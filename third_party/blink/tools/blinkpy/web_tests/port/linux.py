# Copyright (C) 2010 Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

import logging
import os

from blinkpy.common.exit_codes import SYS_DEPS_EXIT_STATUS
from blinkpy.web_tests.breakpad.dump_reader_multipart import DumpReaderLinux
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import win

_log = logging.getLogger(__name__)


class LinuxPort(base.Port):
    port_name = 'linux'

    SUPPORTED_VERSIONS = ('linux', )

    FALLBACK_PATHS = {}
    FALLBACK_PATHS['linux'] = (['linux'] +
                               win.WinPort.latest_platform_fallback_path())

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md'

    XVFB_START_STOP_TIMEOUT = 5.0  # Wait up to 5 seconds for Xvfb to start or stop.

    def __init__(self, host, port_name, **kwargs):
        super(LinuxPort, self).__init__(host, port_name, **kwargs)
        (self._version, ) = self.SUPPORTED_VERSIONS
        self._architecture = 'x86_64'

        if not self.get_option('disable_breakpad'):
            self._dump_reader = DumpReaderLinux(host, self.build_path())
        self._original_home = None
        self._original_display = None
        self._xvfb_process = None
        self._xvfb_stdout = None
        self._xvfb_stderr = None

        # See //testing/xvfb.py for an explanation of parsing -help output.
        try:
            output = self.host.executive.run_command(['Xvfb', '-help'],
                                                     debug_logging=False)
            self._xvfb_supports_maxclients = (type(output) is str
                                              and '-maxclients' in output)
        except Exception:
            self._xvfb_supports_maxclients = False

    def additional_driver_flags(self):
        flags = super(LinuxPort, self).additional_driver_flags()
        if not self.get_option('disable_breakpad'):
            flags += [
                '--enable-crash-reporter',
                '--crash-dumps-dir=%s' %
                self._dump_reader.crash_dumps_directory()
            ]
        return flags

    def configuration_specifier_macros(self):
        if self.flag_specific_config_name():
            return {self.port_name: list(self.SUPPORTED_VERSIONS)}
        return super().configuration_specifier_macros()

    def check_build(self, needs_http, printer):
        result = super(LinuxPort, self).check_build(needs_http, printer)

        if result:
            _log.error('For complete Linux build requirements, please see:')
            _log.error('')
            _log.error(
                '    https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md'
            )
        return result

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        if self.get_option('disable_breakpad'):
            return None
        return self._dump_reader.look_for_new_crash_logs(
            crashed_processes, start_time)

    def clobber_old_port_specific_results(self):
        if not self.get_option('disable_breakpad'):
            self._dump_reader.clobber_old_results()

    def operating_system(self):
        return 'linux'

    def use_system_httpd(self):
        if (self.host.platform.is_linux() and
                self.host.platform.get_machine() == 'x86_64'):
            return False
        # use the system httpd on linux-arm64 and freebsd
        return True

    def path_to_apache(self):
        if not self.use_system_httpd():
            return self._path_from_chromium_base(
                'third_party', 'apache-linux', 'bin', 'httpd')
        # The Apache binary path can vary depending on OS and distribution
        # See http://wiki.apache.org/httpd/DistrosDefaultLayout
        for path in ['/usr/sbin/httpd', '/usr/sbin/apache2']:
            if self._filesystem.exists(path):
                return path
        _log.error('Could not find apache. Not installed or unknown path.')
        return None

    def setup_test_run(self):
        super(LinuxPort, self).setup_test_run()
        if self.get_option('use_xvfb'):
            if not self._start_xvfb():
                return SYS_DEPS_EXIT_STATUS
        self._setup_dummy_home_dir()

    def clean_up_test_run(self):
        super(LinuxPort, self).clean_up_test_run()
        if self.get_option('use_xvfb'):
            self._stop_xvfb(save_logs=False)
        self._clean_up_dummy_home_dir()

    #
    # PROTECTED METHODS
    #

    def _setup_dummy_home_dir(self):
        """Creates a dummy home directory for running the test.

        This is a workaround for crbug.com/595504; see crbug.com/612730.
        If crbug.com/612730 is resolved in another way, then this may be
        unnecessary.
        """
        self._original_home = self.host.environ.get('HOME')
        self._original_cipd_cache_dir = self.host.environ.get('CIPD_CACHE_DIR')
        dummy_home = str(self._filesystem.mkdtemp())
        self.host.environ['HOME'] = dummy_home
        # When using a dummy home directory, CIPD cache directory needs to be
        # specified explicitly to make vpython work.
        self.host.environ['CIPD_CACHE_DIR'] = os.path.join(
            dummy_home, '.vpython_cipd_cache')
        self._setup_files_in_dummy_home_dir(dummy_home)

    def _setup_files_in_dummy_home_dir(self, dummy_home):
        # Note: This may be unnecessary.
        fs = self._filesystem
        for filename in ['.Xauthority']:
            original_path = fs.join(self._original_home, filename)
            if not fs.exists(original_path):
                continue
            fs.copyfile(original_path, fs.join(dummy_home, filename))
        # Prevent fontconfig etc. from reconstructing the cache and symlink rr
        # trace directory.
        for dirpath in [['.cache'], ['.local', 'share', 'rr']]:
            original_path = fs.join(self._original_home, *dirpath)
            if not fs.exists(original_path):
                continue
            fs.maybe_make_directory(fs.join(dummy_home, *dirpath[:-1]))
            fs.symlink(original_path, fs.join(dummy_home, *dirpath))

    def _clean_up_dummy_home_dir(self):
        """Cleans up the dummy dir and resets the HOME environment variable."""
        dummy_home = self.host.environ['HOME']
        assert dummy_home != self._original_home
        self._filesystem.rmtree(dummy_home)
        self.host.environ['HOME'] = self._original_home
        if self._original_cipd_cache_dir:
            self.host.environ['CIPD_CACHE_DIR'] = self._original_cipd_cache_dir
        else:
            del self.host.environ['CIPD_CACHE_DIR']

    def _start_xvfb(self):
        display = self._find_display()
        if not display:
            _log.critical('Failed to find a free display to start Xvfb.')
            return False

        # Parts of Xvfb use a hard-coded "/tmp" for its temporary directory.
        # This can cause a failure when those parts expect to hardlink against
        # files that were created in "TEMPDIR" / "TMPDIR".
        #
        # See: https://crbug.com/715848
        env = self.host.environ.copy()
        if env.get('TMPDIR') and env['TMPDIR'] != '/tmp':
            _log.info('Overriding TMPDIR to "/tmp" for Xvfb, was: %s',
                      env['TMPDIR'])
            env['TMPDIR'] = '/tmp'

        _log.debug('Starting Xvfb with display "%s".', display)
        self._xvfb_stdout, _ = self.host.filesystem.open_binary_tempfile()
        self._xvfb_stderr, _ = self.host.filesystem.open_binary_tempfile()
        self._xvfb_process = self.host.executive.popen(
            ['Xvfb', display] + self.xvfb_flags(),
            stdout=self._xvfb_stdout,
            stderr=self._xvfb_stderr,
            env=env)

        # By setting DISPLAY here, the individual worker processes will
        # get the right DISPLAY. Note, if this environment could be passed
        # when creating workers, then we wouldn't need to modify DISPLAY here.
        self._original_display = self.host.environ.get('DISPLAY')
        self.host.environ['DISPLAY'] = display

        # Check that xvfb has started correctly via probing using xdpyinfo.
        # While xvfb is running, the poll() method will return None;
        # https://docs.python.org/2/library/subprocess.html#subprocess.Popen.poll
        start_time = self.host.time()
        while self.host.time() - start_time < self.XVFB_START_STOP_TIMEOUT:
            if self._xvfb_process.poll() is not None:
                break
            # We don't explicitly set the display, as we want to check the
            # environment value.
            exit_code = self.host.executive.run_command(['xdpyinfo'],
                                                        return_exit_code=True)
            if exit_code == 0:
                _log.debug('Successfully started Xvfb with display "%s".',
                           display)
                return True
            _log.warn(
                'xdpyinfo check failed with exit code %s while starting Xvfb on "%s".',
                exit_code, display)
            self.host.sleep(0.1)

        retcode = self._xvfb_process.poll()
        self._stop_xvfb(save_logs=True)
        _log.critical(
            'Failed to start Xvfb on display "%s" (xvfb retcode: %r).',
            display, retcode)
        return False

    def xvfb_flags(self):
        flags = ['-screen', '0', '1280x800x24', '-ac', '-dpi', '96']
        # Raise the Xvfb connection limit if the default limit (256 connections)
        # is in danger of being exceeded by 4 connections per test.
        if self._xvfb_supports_maxclients:
            flags += ['-maxclients', '1024']
        return flags

    def _find_display(self):
        """Tries to find a free X display, looping if necessary."""
        # The "xvfb-run" command uses :99 by default.
        for display_number in range(99, 120):
            if self.host.filesystem.exists('/tmp/.X%d-lock' % display_number):
                continue
            display = ':%d' % display_number
            exit_code = self.host.executive.run_command(
                ['xdpyinfo', '-display', display], return_exit_code=True)
            if exit_code == 1:
                return display
        return None

    def _stop_xvfb(self, save_logs):
        if self._original_display:
            self.host.environ['DISPLAY'] = self._original_display
        if self._xvfb_process and self._xvfb_process.poll() is None:
            self._xvfb_process.terminate()
            start_time = self.host.time()
            while self.host.time() - start_time < self.XVFB_START_STOP_TIMEOUT:
                if self._xvfb_process.poll() is not None:
                    _log.debug('Xvfb exited with code %d.',
                               self._xvfb_process.poll())
                    break
                self.host.sleep(0.1)
            else:
                _log.debug('Killing Xvfb process pid %d.',
                           self._xvfb_process.pid)
                self._xvfb_process.kill()
                self._xvfb_process.wait()

        for name, tmp_file in [('stdout', self._xvfb_stdout),
                               ('stderr', self._xvfb_stderr)]:
            if not tmp_file:
                continue
            tmp_file.close()
            if not self.host.filesystem.exists(tmp_file.name):
                continue
            if save_logs:
                with self.host.filesystem.open_text_file_for_reading(
                        tmp_file.name) as log:
                    for line in log:
                        _log.warn('Xvfb %s: %s', name, line)
            self.host.filesystem.remove(tmp_file.name)
        self._xvfb_stdout = self._xvfb_stderr = self._xvfb_process = None
