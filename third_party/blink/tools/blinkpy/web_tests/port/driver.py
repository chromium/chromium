# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import base64
import logging
import re
import shlex
import six
import time

from blinkpy.common.system import path
from blinkpy.common.system.profiler import ProfilerFactory
from blinkpy.web_tests.models.testharness_results import is_all_pass_test_result

_log = logging.getLogger(__name__)

DRIVER_START_TIMEOUT_SECS = 30


def coalesce_repeated_switches(cmd):
    """Combines known repeated command line switches.

    Repetition of a switch notably happens when both per-test switches and the
    additional driver flags specify different --enable-features. For instance:

    --enable-features=X --enable-features=Y

    Conceptually, this indicates to enable features X and Y. However
    Chrome's command line parsing only applies the last seen switch, resulting
    in only feature Y being enabled.

    To solve this, transform it to:

    --enable-features=X,Y
    """

    def parse_csv_switch(prefix, switch, values_set):
        """If |switch| starts with |prefix|, parses it as a comma-separated
        list of values and adds them all to |values_set|. Returns False if the
        switch was not a match for |prefix|."""
        if not switch.startswith(prefix):
            return False

        values = switch[len(prefix):].split(',')
        for value in values:
            values_set.add(value)
        return True

    def add_csv_switch(prefix, values_set, result):
        if len(values_set) == 0:
            return
        sorted_values = sorted(list(values_set))
        result.append('%s%s' % (prefix, ','.join(sorted_values)))

    result = []

    ENABLE_FEATURES_FLAG = '--enable-features='
    DISABLE_FEATURES_FLAG = '--disable-features='

    enabled_set = set()
    disabled_set = set()

    for switch in cmd:
        if parse_csv_switch(ENABLE_FEATURES_FLAG, switch, enabled_set):
            continue
        if parse_csv_switch(DISABLE_FEATURES_FLAG, switch, disabled_set):
            continue
        result.append(switch)

    # Append any coalesced (comma separated) flags to the end.
    add_csv_switch(ENABLE_FEATURES_FLAG, enabled_set, result)
    add_csv_switch(DISABLE_FEATURES_FLAG, disabled_set, result)

    return result


class DriverInput(object):
    def __init__(self, test_name, timeout, image_hash, wpt_print_mode,
                 trace_file, startup_trace_file, args):
        self.test_name = test_name
        self.timeout = timeout  # in ms
        self.image_hash = image_hash
        self.wpt_print_mode = wpt_print_mode
        self.trace_file = trace_file
        self.startup_trace_file = startup_trace_file
        self.args = args


class DriverOutput(object):
    """Groups information about a output from driver for easy passing
    and post-processing of data.
    """

    def __init__(self,
                 text,
                 image,
                 image_hash,
                 audio,
                 crash=False,
                 test_time=0,
                 measurements=None,
                 timeout=False,
                 error=b'',
                 crashed_process_name='??',
                 crashed_pid=None,
                 crash_log=None,
                 crash_site=None,
                 leak=False,
                 leak_log=None,
                 trace_file=None,
                 startup_trace_file=None,
                 pid=None,
                 command=None):
        # FIXME: Args could be renamed to better clarify what they do.
        self.text = text
        self.image = image  # May be empty-string if the test crashes.
        self.image_hash = image_hash
        self.image_diff = None  # image_diff gets filled in after construction.
        self.image_diff_stats = None  # Number of changed pixels, etc.
        self.audio = audio  # Binary format is port-dependent.
        self.crash = crash
        self.crashed_process_name = crashed_process_name
        self.crashed_pid = crashed_pid
        self.crash_log = crash_log
        self.crash_site = crash_site
        self.leak = leak
        self.leak_log = leak_log
        self.trace_file = trace_file
        self.startup_trace_file = startup_trace_file
        self.test_time = test_time
        self.measurements = measurements
        self.timeout = timeout
        self.error = error  # stderr output
        self.pid = pid
        self.command = command
        self.test_type = set()

    def has_stderr(self):
        return bool(self.error)


class DeviceFailure(Exception):
    pass


class TestURIMapper:

    def __init__(self, port):
        self.WPT_DIRS = port.WPT_DIRS
        self._port = port

    # The *_HOST_AND_PORTS tuples are (hostname, insecure_port, secure_port),
    # i.e. the information needed to create HTTP and HTTPS URLs.
    # TODO(burnik): Read from config or args.
    HTTP_DIR = 'http/tests/'
    HTTP_LOCAL_DIR = 'http/tests/local/'
    HTTP_HOST_AND_PORTS = ('127.0.0.1', 8000, 8443)
    WPT_HOST_AND_PORTS = ('web-platform.test', 8001, 8444)
    WPT_H2_PORT = 9000

    def is_http_test(self, test_name):
        return (test_name.startswith(self.HTTP_DIR)
                and not test_name.startswith(self.HTTP_LOCAL_DIR))

    def test_to_uri(self, test_name):
        """Convert a test name to a URI.

        Tests which have an 'https' directory in their paths or '.https.' or
        '.serviceworker.' in their name will be loaded over HTTPS; all other
        tests over HTTP. Example paths loaded over HTTPS:
        http/tests/security/mixedContent/https/test1.html
        http/tests/security/mixedContent/test1.https.html
        external/wpt/encoding/idlharness.any.serviceworker.html
        """
        using_wptserve = self._port.should_use_wptserve(test_name)

        if not self.is_http_test(test_name) and not using_wptserve:
            return path.abspath_to_uri(self._port.host.platform,
                                       self._port.abspath_for_test(test_name))

        if using_wptserve:
            for wpt_path, url_prefix in self.WPT_DIRS.items():
                # The keys of WPT_DIRS do not have trailing slashes.
                wpt_path += '/'
                if test_name.startswith(wpt_path):
                    test_dir_prefix = wpt_path
                    test_url_prefix = url_prefix
                    break
            else:
                # We really shouldn't reach here, but in case we do, fail gracefully.
                _log.error('Unrecognized WPT test name: %s', test_name)
                test_dir_prefix = 'external/wpt/'
                test_url_prefix = '/'
            hostname, insecure_port, secure_port = self.WPT_HOST_AND_PORTS
            if '.www.' in test_name:
                hostname = "www.%s" % hostname
            if '.h2.' in test_name:
                secure_port = self.WPT_H2_PORT
        else:
            test_dir_prefix = self.HTTP_DIR
            test_url_prefix = '/'
            hostname, insecure_port, secure_port = self.HTTP_HOST_AND_PORTS

        relative_path = test_name[len(test_dir_prefix):]

        if ('/https/' in test_name or '.https.' in test_name
                or '.h2.' in test_name or '.serviceworker.' in test_name
                or '.serviceworker-module.' in test_name):
            return 'https://%s:%d%s%s' % (hostname, secure_port,
                                          test_url_prefix, relative_path)
        return 'http://%s:%d%s%s' % (hostname, insecure_port, test_url_prefix,
                                     relative_path)

    def _get_uri_prefixes(self, hostname, insecure_port, secure_port):
        """Returns the HTTP and HTTPS URI prefix for a hostname."""
        return [
            'http://%s:%d/' % (hostname, insecure_port),
            'https://%s:%d/' % (hostname, secure_port)
        ]

    def uri_to_test(self, uri):
        """Return the base web test name for a given URI.

        This returns the test name for a given URI, e.g., if you passed in
        "file:///src/web_tests/fast/html/keygen.html" it would return
        "fast/html/keygen.html".
        """

        if uri.startswith('file:///'):
            prefix = path.abspath_to_uri(self._port.host.platform,
                                         self._port.web_tests_dir())
            if not prefix.endswith('/'):
                prefix += '/'
            return uri[len(prefix):]

        for prefix in self._get_uri_prefixes(*self.HTTP_HOST_AND_PORTS):
            if uri.startswith(prefix):
                return self.HTTP_DIR + uri[len(prefix):]
        for prefix in self._get_uri_prefixes(*self.WPT_HOST_AND_PORTS):
            if uri.startswith(prefix):
                url_path = '/' + uri[len(prefix):]
                for wpt_path, url_prefix in self.WPT_DIRS.items():
                    if url_path.startswith(url_prefix):
                        return wpt_path + '/' + url_path[len(url_prefix):]
        raise NotImplementedError('unknown url type: %s' % uri)


class Driver(TestURIMapper):
    """object for running test(s) using content_shell or other driver."""

    def __init__(self, port, worker_number, no_timeout=False):
        """Initialize a Driver to subsequently run tests.

        Typically this routine will spawn content_shell in a config
        ready for subsequent input.

        port - reference back to the port object.
        worker_number - identifier for a particular worker/driver instance
        """
        super().__init__(port)
        self._worker_number = worker_number
        self._no_timeout = no_timeout

        self._driver_tempdir = None
        # content_shell can report back subprocess crashes by printing
        # "#CRASHED - PROCESSNAME".  Since those can happen at any time
        # and ServerProcess won't be aware of them (since the actual tool
        # didn't crash, just a subprocess) we record the crashed subprocess name here.
        self._crashed_process_name = None
        self._crashed_pid = None

        # content_shell can report back subprocesses that became unresponsive
        # This could mean they crashed.
        self._subprocess_was_unresponsive = False

        # content_shell can report back subprocess DOM-object leaks by printing
        # "#LEAK". This leak detection is enabled only when the flag
        # --enable-leak-detection is passed to content_shell.
        self._leaked = False
        self._leak_log = None

        # stderr reading is scoped on a per-test (not per-block) basis, so we store the accumulated
        # stderr output, as well as if we've seen #EOF on this driver instance.
        # FIXME: We should probably remove _read_first_block and _read_optional_image_block and
        # instead scope these locally in run_test.
        self.error_from_test = bytearray()
        self.err_seen_eof = False
        self._server_process = None
        self._current_cmd_line = None

        self._measurements = {}
        if self._port.get_option('profile'):
            profiler_name = self._port.get_option('profiler')
            self._profiler = ProfilerFactory.create_profiler(
                self._port.host, self._port.path_to_driver(),
                self._port.artifacts_directory(), profiler_name)
        else:
            self._profiler = None

    def __del__(self):
        self.stop()

    def run_test(self, driver_input):
        """Run a single test and return the results.

        Note that it is okay if a test times out or crashes. content_shell
        will be stopped when the test ends, and then restarted for the next
        test when this function is invoked again. As part of the restart, the
        state of Driver will be reset.

        Returns a DriverOutput object.
        """
        start_time = time.time()
        stdin_deadline = start_time + int(driver_input.timeout) / 2000.0
        ok, startup_output = self.start(driver_input.args, stdin_deadline)
        if not ok:
            return startup_output

        return self._run_one_input(driver_input, start_time)

    def _run_one_input(self, driver_input, start_time):
        test_begin_time = time.time()
        self.error_from_test = bytearray()
        self.err_seen_eof = False

        test_command = self._command_from_driver_input(driver_input)
        server_process_command = self._server_process.cmd()

        deadline = test_begin_time + int(driver_input.timeout) / 1000.0
        self._server_process.write(test_command.encode('utf8', 'replace'))
        # First block is either text or audio
        text, audio = self._read_first_block(deadline)
        # The second (optional) block is image data.
        image, actual_image_hash = self._read_optional_image_block(deadline)

        crashed = self.has_crashed()
        timed_out = self._server_process.timed_out
        pid = self._server_process.pid()
        leaked = self._leaked

        if not crashed:
            sanitizer = self._port.output_contains_sanitizer_messages(
                self.error_from_test)
            if sanitizer:
                self.error_from_test = b'OUTPUT CONTAINS "sanitizer",' + \
                    b' so we are treating this test as if it crashed, even though it did not.\n\n' + \
                    self.error_from_test
                crashed = True
                self._crashed_process_name = 'unknown process name'
                self._crashed_pid = 0

        if crashed or timed_out or leaked:
            # We call stop() even if we crashed or timed out in order to get any remaining stdout/stderr output.
            # In the timeout case, we kill the hung process as well.
            # Add a delay to allow process to finish post-run hooks, such as dumping code coverage data.
            out, err = self._server_process.stop(
                timeout_secs=self._port.get_option('driver_kill_timeout_secs'),
                send_sigterm=self._port.get_option('kill_driver_with_sigterm'))
            if out:
                text += out
            if err:
                self.error_from_test += err
            self._server_process = None

        crash_log = None
        crash_site = None
        if crashed:
            self.error_from_test, crash_log, crash_site = self._get_crash_log(
                text, self.error_from_test, newer_than=start_time)

            # If we don't find a crash log use a placeholder error message instead.
            if not crash_log:
                pid_str = str(
                    self._crashed_pid) if self._crashed_pid else 'unknown pid'
                crash_log = 'No crash log found for %s:%s.\n' % (
                    self._crashed_process_name, pid_str)
                # If we were unresponsive append a message informing there may not have been a crash.
                if self._subprocess_was_unresponsive:
                    crash_log += 'Process failed to become responsive before timing out.\n'

                # Print stdout and stderr to the placeholder crash log; we want as much context as possible.
                if self.error_from_test:
                    crash_log += '\nstdout:\n%s\nstderr:\n%s\n' % (
                        text, self.error_from_test)
        command = ("%s %s" %
                   (" ".join(server_process_command), test_command)).encode(
                       'ascii', 'replace')
        if actual_image_hash:
            actual_image_hash = actual_image_hash.decode('utf8', 'replace')
        startup_trace_file = driver_input.startup_trace_file
        if (startup_trace_file
                and not self._port.host.filesystem.isabs(startup_trace_file)):
            startup_trace_file = self._port.host.filesystem.join(
                self._port.host.filesystem.getcwd(), startup_trace_file)
        if startup_trace_file:
            # The startup trace file won't get flushed to disk until the server
            # process stops. In practice, the existence of startup_trace_file
            # means that the server process is restarted after every test
            # anyway, so this just accelerates the inevitable.
            out, err = self._server_process.stop(
                timeout_secs=self._port.get_option('driver_kill_timeout_secs'),
                send_sigterm=self._port.get_option('kill_driver_with_sigterm'))
            if out:
                text += out
            if err:
                self.error_from_test += err
            self._server_process = None
        return DriverOutput(text,
                            image,
                            actual_image_hash,
                            audio,
                            crash=crashed,
                            test_time=time.time() - test_begin_time,
                            measurements=self._measurements,
                            timeout=timed_out,
                            error=self.error_from_test,
                            crashed_process_name=self._crashed_process_name,
                            crashed_pid=self._crashed_pid,
                            crash_log=crash_log,
                            crash_site=crash_site,
                            leak=leaked,
                            leak_log=self._leak_log,
                            trace_file=driver_input.trace_file,
                            startup_trace_file=startup_trace_file,
                            pid=pid,
                            command=command)

    def _get_crash_log(self, stdout, stderr, newer_than):
        # pylint: disable=protected-access
        return self._port._get_crash_log(self._crashed_process_name,
                                         self._crashed_pid, stdout, stderr,
                                         newer_than)

    def has_crashed(self):
        if self._server_process is None:
            return False
        if self._crashed_process_name:
            return True
        if self._server_process.has_crashed():
            self._crashed_process_name = self._server_process.name()
            self._crashed_pid = self._server_process.pid()
            return True
        return False

    def start(self, per_test_args, deadline):
        """Returns a tuple of (whether driver was started, optional startup test result)."""
        new_cmd_line = self.cmd_line(per_test_args)
        if not self._server_process or new_cmd_line != self._current_cmd_line:
            started, output = self._start(per_test_args)
            if started:
                self._run_post_start_tasks()
            return started, output
        return True, None

    def _setup_environ_for_driver(self, environment):
        if self._profiler:
            environment = self._profiler.adjusted_environment(environment)
        return environment

    def _initialize_server_process(self, server_name, cmd_line, environment):
        self._server_process = self._port.server_process_constructor(
            self._port,
            server_name,
            cmd_line,
            environment,
            more_logging=self._port.get_option('driver_logging'))

    def _start(self, per_test_args, wait_for_ready=True):
        """Returns a tuple of (whether driver was started, optional startup test result)."""

        self.stop()
        self._driver_tempdir = self._port.host.filesystem.mkdtemp(
            prefix='%s-' % self._port.driver_name())
        server_name = self._port.driver_name()
        environment = self._port.setup_environ_for_server()
        environment = self._setup_environ_for_driver(environment)
        self._crashed_process_name = None
        self._crashed_pid = None
        self._leaked = False
        cmd_line = self.cmd_line(per_test_args)
        self._initialize_server_process(server_name, cmd_line, environment)
        self._server_process.start()
        self._current_cmd_line = cmd_line

        if wait_for_ready:
            deadline = time.time() + DRIVER_START_TIMEOUT_SECS
            if not self._wait_for_server_process_output(
                    self._server_process, deadline, b'#READY'):
                _log.error('%s took too long to startup.' % server_name)
                # Even though the server hasn't started up, we pretend it has
                # so that the rest of the error-handling code can deal with
                # this as if the test has simply crashed.

        if self._port.get_option(
                'initialize_webgpu_adapter_at_startup_timeout_ms'):
            return self._initialize_webgpu_adapter_at_startup(per_test_args)
        return True, None

    def _initialize_webgpu_adapter_at_startup(self, per_test_args):
        # TODO(crbug.com/953991) - Apparently content_shell isn't
        # "really" ready when it signals #READY; in some WebGPU cases
        # there are intermittent additional delays that we haven't
        # been able to diagnose yet, but that running this particular
        # test seems to address. We should figure out what is going on
        # and remove this workaround, which causes every content shell
        # startup to be slower (if the workaround is triggered via the
        # --initialize-webgpu-adapter-at-startup flag, right above in the
        # code in _start()).
        #
        # TODO(crbug.com/329003665): See if `wpt_internal/webgpu/` is runnable
        # with wptrunner + chromedriver + chrome, which would obviate the need
        # for `000_run_me_first.https.html`.
        init_timeout = self._port.get_option(
            'initialize_webgpu_adapter_at_startup_timeout_ms')
        startup_input = DriverInput(
            "wpt_internal/webgpu/000_run_me_first.https.html",
            timeout=init_timeout,
            image_hash=None,
            wpt_print_mode=None,
            trace_file=None,
            startup_trace_file=None,
            args=per_test_args)
        output = self._run_one_input(startup_input, start_time=time.time())
        if output.text and is_all_pass_test_result(
                output.text.decode(errors='replace')):
            return True, None

        output.text = (b'Failed to initialize WebGPU adapter at startup via '
                       b'wpt_internal_webgpu/000_run_me_first.https.html:\n' +
                       output.text)
        return False, output

    def _wait_for_server_process_output(self, server_process, deadline, text):
        output = b''
        line = server_process.read_stdout_line(deadline)
        output += server_process.pop_all_buffered_stderr()
        while (not server_process.timed_out
               and not server_process.has_crashed()
               and not text in line.rstrip()):
            output += line
            line = server_process.read_stdout_line(deadline)
            output += server_process.pop_all_buffered_stderr()

        if server_process.timed_out:
            _log.error('Timed out while waiting for the %s process: \n"%s"',
                       server_process.name(), output)
            return False
        if server_process.has_crashed():
            _log.error('The %s process crashed while starting: \n"%s"',
                       server_process.name(), output)
            return False

        return True

    def _run_post_start_tasks(self):
        # Remote drivers may override this to delay post-start tasks until the server has ack'd.
        if self._profiler:
            self._profiler.attach_to_pid(self._pid_on_target())

    def _pid_on_target(self):
        # Remote drivers will override this method to return the pid on the device.
        return self._server_process.pid()

    def stop(self, timeout_secs=None):
        # Add a delay to allow process to finish post-run hooks, such as dumping
        # code coverage data; but allow for 0 timeout if explicitly requested.
        if timeout_secs != 0:
            timeout_secs = max(
                timeout_secs or 0,
                self._port.get_option('driver_kill_timeout_secs', 0))

        if self._server_process:
            self._server_process.stop(
                timeout_secs=timeout_secs,
                send_sigterm=self._port.get_option('kill_driver_with_sigterm'))
            self._server_process = None
            if self._profiler:
                self._profiler.profile_after_exit()

        if self._driver_tempdir:
            self._port.host.filesystem.rmtree(str(self._driver_tempdir))
            self._driver_tempdir = None

        self._current_cmd_line = None

    def _base_cmd_line(self):
        return [
            self._port.path_to_driver(),
            '--run-web-tests',
            # To take effect, `--ignore-certificate-errors-spki-list` requires
            # an embedder-defined `--user-data-dir`.
            '--user-data-dir',
        ]

    def cmd_line(self, per_test_args):
        cmd = list(self._port.get_option('wrapper', []))
        cmd += self._base_cmd_line()
        if self._no_timeout:
            cmd.append('--no-timeout')
        cmd.extend(self._port.additional_driver_flags())
        if self._port.get_option('enable_leak_detection'):
            cmd.append('--enable-leak-detection')
        if self._port.get_option('nocheck_sys_deps', False):
            cmd.append('--disable-system-font-check')
        cmd.extend(per_test_args)
        cmd = coalesce_repeated_switches(cmd)
        cmd.append('-')
        return cmd

    def _check_for_driver_crash(self, error_line):
        if error_line == '#CRASHED\n':
            # This is used on Windows to report that the process has crashed
            # See http://trac.webkit.org/changeset/65537.
            self._crashed_process_name = self._server_process.name()
            self._crashed_pid = self._server_process.pid()
            self.error_from_test += error_line.encode('utf-8')
        elif (error_line.startswith('#CRASHED - ')
              or error_line.startswith('#PROCESS UNRESPONSIVE - ')):
            # WebKitTestRunner uses this to report that the WebProcess subprocess crashed.
            match = re.match(r'#(?:CRASHED|PROCESS UNRESPONSIVE) - (\S+)',
                             error_line)
            self._crashed_process_name = (match.group(1)
                                          if match else 'WebProcess')
            match = re.search(r'pid (\d+)', error_line)
            pid = int(match.group(1)) if match else None
            self._crashed_pid = pid
            # FIXME: delete this after we're sure this code is working :)
            _log.debug('%s crash, pid = %s, error_line = %s',
                       self._crashed_process_name, str(pid), error_line)
            if error_line.startswith('#PROCESS UNRESPONSIVE - '):
                self._subprocess_was_unresponsive = True
                self._port.sample_process(self._crashed_process_name,
                                          self._crashed_pid)
            self.error_from_test += error_line.encode('utf-8')
            return True
        return self.has_crashed()

    def _check_for_leak(self, error_line):
        if error_line.startswith('#LEAK - '):
            self._leaked = True
            match = re.match(r'#LEAK - (\S+) pid (\d+) (.+)\n', error_line)
            self._leak_log = (match.group(3)).encode('utf-8')
        return self._leaked

    def _command_from_driver_input(self, driver_input):
        # FIXME: performance tests pass in full URLs instead of test names.
        if (driver_input.test_name.startswith('http://')
                or driver_input.test_name.startswith('https://')
                or driver_input.test_name == ('about:blank')):
            command = driver_input.test_name
        elif (self.is_http_test(driver_input.test_name)
              or self._port.should_use_wptserve(driver_input.test_name)):
            command = self.test_to_uri(driver_input.test_name)
        else:
            command = self._port.abspath_for_test(driver_input.test_name)
        # ' is the separator between arguments.
        if self._port.supports_per_test_timeout():
            command += "'--timeout'%s" % driver_input.timeout
        if driver_input.image_hash:
            command += "'" + driver_input.image_hash
        if driver_input.wpt_print_mode:
            if not driver_input.image_hash:
                command += "'"
            command += "'print"
        if driver_input.trace_file:
            if not driver_input.wpt_print_mode:
                command += "'"
                if not driver_input.image_hash:
                    command += "'"
            command += "'"
            command += driver_input.trace_file

        return command + '\n'

    def _read_first_block(self, deadline):
        # returns (text_content, audio_content)
        block = self._read_block(deadline)
        if block.malloc:
            self._measurements['Malloc'] = float(block.malloc)
        if block.js_heap:
            self._measurements['JSHeap'] = float(block.js_heap)
        if block.content_type == b'audio/wav':
            return (None, block.decoded_content)
        return (block.decoded_content, None)

    def _read_optional_image_block(self, deadline):
        # returns (image, actual_image_hash)
        block = self._read_block(deadline, wait_for_stderr_eof=True)
        if block.content and block.content_type == b'image/png':
            return (block.decoded_content, block.content_hash)
        return (None, block.content_hash)

    def _read_header(self,
                     block,
                     line,
                     header_text,
                     header_attr,
                     header_filter=None):
        if (line.startswith(header_text)
                and getattr(block, header_attr) is None):
            value = line.split()[1]
            if header_filter:
                value = header_filter(value)
            setattr(block, header_attr, value)
            return True
        return False

    def _process_stdout_line(self, block, line):
        if (self._read_header(block, line, b'Content-Type: ', 'content_type')
                or self._read_header(
                    block, line, b'Content-Transfer-Encoding: ', 'encoding')
                or self._read_header(block, line, b'Content-Length: ',
                                     '_content_length', int) or
                self._read_header(block, line, b'ActualHash: ', 'content_hash')
                or self._read_header(block, line, b'DumpMalloc: ', 'malloc')
                or self._read_header(block, line, b'DumpJSHeap: ', 'js_heap')
                or self._read_header(block, line, b'StdinPath', 'stdin_path')):
            return
        # Note, we're not reading ExpectedHash: here, but we could.
        # If the line wasn't a header, we just append it to the content.
        block.content += line

    def _strip_eof(self, line):
        if line and line.endswith(b'#EOF\n'):
            return line[:-5], True
        if line and line.endswith(b'#EOF\r\n'):
            _log.error('Got a CRLF-terminated #EOF - this is a driver bug.')
            return line[:-6], True
        return line, False

    def _read_block(self, deadline, wait_for_stderr_eof=False):
        block = ContentBlock()
        out_seen_eof = False

        while not self.has_crashed():
            if out_seen_eof and (self.err_seen_eof or not wait_for_stderr_eof):
                break

            if self.err_seen_eof:
                out_line = self._server_process.read_stdout_line(deadline)
                err_line = None
            elif out_seen_eof:
                out_line = None
                err_line = self._server_process.read_stderr_line(deadline)
            else:
                out_line, err_line = self._server_process.read_either_stdout_or_stderr_line(
                    deadline)

            if self._server_process.timed_out or self.has_crashed():
                break

            if out_line:
                assert not out_seen_eof
                out_line, out_seen_eof = self._strip_eof(out_line)
            if err_line:
                assert not self.err_seen_eof
                err_line, self.err_seen_eof = self._strip_eof(err_line)
            if out_line:
                if not out_line.endswith(b'\n'):
                    _log.error(
                        'Last character read from DRT stdout line was not a newline!  This indicates either a NRWT or DRT bug.'
                    )
                # pylint: disable=protected-access
                content_length_before_header_check = block._content_length
                self._process_stdout_line(block, out_line)
                # FIXME: Unlike HTTP, DRT dumps the content right after printing a Content-Length header.
                # Don't wait until we're done with headers, just read the binary blob right now.
                if content_length_before_header_check != block._content_length:
                    if block._content_length > 0:
                        block.content = self._server_process.read_stdout(
                            deadline, block._content_length)
                    else:
                        _log.error(
                            'Received content of type %s with Content-Length of 0!  This indicates a bug in %s.',
                            block.content_type, self._server_process.name())

            if err_line:
                if self._check_for_driver_crash(
                        err_line.decode('utf8', 'replace')):
                    break
                if self._check_for_leak(err_line.decode('utf8', 'replace')):
                    break
                self.error_from_test += err_line

        block.decode_content()
        return block


class ContentBlock(object):
    def __init__(self):
        self.content_type = None
        self.encoding = None
        self.content_hash = None
        self._content_length = None
        # Content is treated as binary data even though the text output is usually UTF-8.
        # FIXME: Should be bytearray() once we require Python 2.6.
        # TODO(crbug/1197331): Keeping PY2 as str() for now, as diffing modules
        # need to be looked into for PY3 unified_diff.py and html_diff.py
        if six.PY2:
            self.content = str()
        else:
            self.content = bytearray()
        self.decoded_content = None
        self.malloc = None
        self.js_heap = None
        self.stdin_path = None

    def decode_content(self):
        if self.encoding == b'base64' and self.content is not None:
            self.decoded_content = base64.b64decode(self.content)
        else:
            self.decoded_content = self.content
