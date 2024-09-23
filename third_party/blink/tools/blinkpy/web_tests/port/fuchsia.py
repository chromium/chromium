# Copyright (C) 2018 Google Inc. All rights reserved.
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
import select
import socket
import subprocess
import sys
import threading
import time

from argparse import Namespace
from blinkpy.common import exit_codes
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.path_finder import get_chromium_src_dir
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import factory
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import server_process


# Imports Fuchsia runner modules. This is done dynamically only when FuchsiaPort
# is instantiated to avoid dependency on Fuchsia runner on other platforms.
def _import_fuchsia_runner():
    sys.path.insert(0,
                    os.path.join(get_chromium_src_dir(), 'build/fuchsia/test'))

    # pylint: disable=import-error
    # pylint: disable=invalid-name
    # pylint: disable=redefined-outer-name
    global SDK_ROOT, SDK_TOOLS_DIR, get_ssh_address, run_continuous_ffx_command, run_ffx_command
    from common import SDK_ROOT, SDK_TOOLS_DIR, get_ssh_address, run_continuous_ffx_command, run_ffx_command
    global get_host_arch, get_ssh_prefix
    from compatible_utils import get_host_arch, get_ssh_prefix
    global ports_forward, port_forward
    from test_server import ports_forward, port_forward
    global run_symbolizer
    from ffx_integration import run_symbolizer
    # pylint: enable=import-error
    # pylint: enable=invalid-name
    # pylint: disable=redefined-outer-name


# Path to the content shell package relative to the build directory.
CONTENT_SHELL_PACKAGE_PATH = 'gen/content/shell/content_shell/content_shell.far'

# HTTP path prefixes for the HTTP server.
# WEB_TEST_PATH_PREFIX should be matched to the local directory name of
# web_tests because some tests and test_runner find test root directory
# with it.
WEB_TESTS_PATH_PREFIX = '/third_party/blink/' + WEB_TESTS_LAST_COMPONENT

# Paths to the directory where the fonts are copied to. Must match the path in
# content/shell/app/blink_test_platform_support_fuchsia.cc .
FONTS_DEVICE_PATH = '/system/fonts'

PROCESS_START_TIMEOUT = 60

_log = logging.getLogger(__name__)


def _subprocess_log_thread(pipe, prefix):
    try:
        while True:
            line = pipe.readline()
            if not line:
                return
            _log.error('%s: %s', prefix, line.decode('utf-8'))
    finally:
        pipe.close()


class SubprocessOutputLogger(object):
    def __init__(self, process, prefix):
        self._process = process
        self._thread = threading.Thread(
            target=_subprocess_log_thread, args=(process.stdout, prefix))
        self._thread.daemon = True
        self._thread.start()

    def __del__(self):
        self.close()

    def close(self):
        self._process.kill()


class _TargetHost(object):

    def __init__(self, ports, target_id):
        self._target_id = target_id
        # Tell SSH to forward all server ports from the Fuchsia device to
        # the host.
        self._host_port_pair = get_ssh_address(self._target_id)
        # Reverse forward all ports listed in |ports| to the device.
        forwarding_ports = []
        for port in ports:
            forwarding_ports.append((port, port))
        ports_forward(self._host_port_pair, forwarding_ports)

    def run_command(self, command):
        ssh_prefix = get_ssh_prefix(self._host_port_pair)
        return subprocess.Popen(ssh_prefix + command,
                                stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

    def run_test_component(self, url, cmd_line):
        # TODO(crbug.com/1381116): migrate off of `ffx test run` when possible
        command = ['test', 'run', url, '--']
        command.extend(cmd_line)
        return run_continuous_ffx_command(command,
                                          self._target_id,
                                          encoding=None,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT)

    def setup_forwarded_port(self, port):
        return port_forward(self._host_port_pair, port)


class FuchsiaPort(base.Port):
    port_name = 'fuchsia'

    SUPPORTED_VERSIONS = ('fuchsia', )

    FALLBACK_PATHS = {
        'fuchsia':
        ['fuchsia'] + linux.LinuxPort.latest_platform_fallback_path()
    }

    def __init__(self, host, port_name, target_host=None, **kwargs):
        super(FuchsiaPort, self).__init__(host, port_name, **kwargs)
        _import_fuchsia_runner()

        self._operating_system = 'fuchsia'
        self._version = 'fuchsia'
        self._architecture = 'x86_64' if get_host_arch() == 'x64' else 'arm64'

        self.server_process_constructor = FuchsiaServerProcess

        # Used to implement methods that depend on the host platform.
        self._host_port = factory.PortFactory(host).get(**kwargs)

        self._target_host = target_host
        self._zircon_logger = None
        self._symbolizer = os.path.join(SDK_TOOLS_DIR, 'symbolizer')
        self._build_id_dir = os.path.join(SDK_ROOT, '.build-id')

    def _driver_class(self):
        return ChromiumFuchsiaDriver

    def path_to_driver(self, target=None):
        return self.build_path(CONTENT_SHELL_PACKAGE_PATH, target=target)

    def __del__(self):
        if self._zircon_logger:
            self._zircon_logger.close()

    def _cpu_cores(self):
        # TODO(crbug.com/1340573): Four parallel jobs always gives reasonable
        # performance, while using larger numbers may actually slow things.
        # Hard-code eight virtual CPU cores, so that four jobs will be run.
        return 8

    def setup_test_run(self):
        super(FuchsiaPort, self).setup_test_run()
        try:
            target_id = self.get_option('fuchsia_target_id')
            self._target_host = _TargetHost(self.SERVER_PORTS, target_id)

            klog_proc = self._target_host.run_command(['dlog', '-f'])
            symbolized_klog_proc = run_symbolizer([self.get_build_ids_path()],
                                                  klog_proc.stdout,
                                                  subprocess.PIPE,
                                                  raw_bytes=True)
            self._zircon_logger = SubprocessOutputLogger(
                symbolized_klog_proc, 'Zircon')
        except:
            return exit_codes.NO_DEVICES_EXIT_STATUS

    def clean_up_test_run(self):
        if self._target_host:
            self._target_host = None

    def child_kwargs(self):
        return {"target_host": self._target_host}

    def num_workers(self, requested_num_workers):
        # Allow for multi-process / multi-threading overhead in the browser
        # by allocating two CPU cores per-worker.
        return min(self._cpu_cores() / 2, requested_num_workers)

    def _default_timeout_ms(self):
        # Use 20s timeout instead of the default 6s. This is necessary because
        # the tests are executed in qemu, so they run slower compared to other
        # platforms.
        return 20000

    def start_http_server(self, additional_dirs, number_of_drivers):
        additional_dirs['/third_party/blink/PerformanceTests'] = \
            self._perf_tests_dir()
        additional_dirs[WEB_TESTS_PATH_PREFIX] = \
            self._path_finder.web_tests_dir()
        additional_dirs['/gen'] = self.generated_sources_directory()
        additional_dirs['/third_party/blink'] = \
            self._path_from_chromium_base('third_party', 'blink')
        super(FuchsiaPort, self).start_http_server(additional_dirs,
                                                   number_of_drivers)
        # Wait for the ssh proxy to be ready.
        for _ in range(5):
            if self.get_target_host().run_command(
                ['curl', 'http://127.0.0.1:8000/']).wait() == 0:
                break
            time.sleep(1)
        # But still continue the tests if it's not working.


    def operating_system(self):
        return self._operating_system

    def path_to_apache(self):
        return self._host_port.path_to_apache()

    def path_to_apache_config_file(self):
        return self._host_port.path_to_apache_config_file()

    def default_smoke_test_only(self):
        return True

    def get_target_host(self):
        return self._target_host

    def get_build_ids_path(self):
        package_path = self.path_to_driver()
        return os.path.join(os.path.dirname(package_path), 'ids.txt')


class ChromiumFuchsiaDriver(driver.Driver):
    def __init__(self, port, worker_number, no_timeout=False):
        super(ChromiumFuchsiaDriver, self).__init__(port, worker_number,
                                                    no_timeout)

    def _initialize_server_process(self, server_name, cmd_line, environment):
        self._server_process = self._port.server_process_constructor(
            self._port,
            server_name,
            cmd_line,
            environment,
            more_logging=self._port.get_option('driver_logging'))

    def _base_cmd_line(self):
        return [
            '--run-web-tests',
            '--user-data-dir',
            '--use-vulkan',
            '--enable-gpu-rasterization',
            '--force-device-scale-factor=1',
            '--enable-features=Vulkan',
            '--gpu-watchdog-timeout-seconds=60',
        ]

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumFuchsiaDriver,
                        self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            relative_test_filename = \
                os.path.relpath(command,
                                self._port._path_finder.chromium_base())
            command = 'http://127.0.0.1:8000' + '/' + relative_test_filename
        return command


# Custom version of ServerProcess that runs processes on a remote device.
class FuchsiaServerProcess(server_process.ServerProcess):
    def __init__(self,
                 port_obj,
                 name,
                 cmd,
                 env=None,
                 treat_no_data_as_crash=False,
                 more_logging=False):
        super(FuchsiaServerProcess, self).__init__(
            port_obj, name, cmd, env, treat_no_data_as_crash, more_logging)
        self._symbolizer_proc = None

    def _start(self):
        if self._proc:
            raise ValueError('%s already running' % self._name)
        self._reset()

        # Fuchsia doesn't support stdin stream for packaged apps, and stdout
        # from run-test-suite not only has extra emissions from the Fuchsia test
        # infrastructure, it also merges stderr and stdout together. Combined,
        # these mean that when running content_shell on Fuchsia it's not
        # possible to use stdin to pass list of tests or to reliably use stdout
        # to emit results. To workaround this issue for web tests we redirect
        # stdin and stdout to a TCP socket connected to the web test runner. The
        # runner uses --stdio-redirect to specify address and port for stdin and
        # stdout redirection.
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.bind(('127.0.0.1', 0))
        listen_socket.listen(1)
        stdio_port = int(listen_socket.getsockname()[1])
        forwarded_stdio_port = \
            self._port.get_target_host().setup_forwarded_port(stdio_port)

        command = self._cmd + \
            ['--no-sandbox',
             '--stdio-redirect=127.0.0.1:%d' % forwarded_stdio_port]

        proc = self._port.get_target_host().run_test_component(
            "fuchsia-pkg://fuchsia.com/content_shell#meta/content_shell.cm",
            command)

        # Wait for incoming connection from content_shell.
        fd = listen_socket.fileno()
        read_fds, _, _ = select.select([fd], [], [], PROCESS_START_TIMEOUT)
        if fd not in read_fds:
            listen_socket.close()
            proc.kill()
            raise driver.DeviceFailure(
                'Timed out waiting connection from content_shell.')

        # Python's interfaces for sockets and pipes are different. To masquerade
        # the socket as a pipe dup() the file descriptor and pass it to
        # os.fdopen().
        stdio_socket, _ = listen_socket.accept()
        fd = stdio_socket.fileno()  # pylint: disable=no-member
        stdin_pipe = os.fdopen(os.dup(fd), "wb", 0)
        stdout_pipe = os.fdopen(os.dup(fd), "rb", 0)
        stdio_socket.close()

        assert not proc.stdin
        proc.stdin = stdin_pipe

        # stdout from `proc` is the merged stdout/stderr produced by
        # run-test-suite, which contains only stderr since we run stdout through
        # the socket above. Run this combined output through the symbolizer as
        # if it were stderr.
        merged_stdout_stderr = proc.stdout
        proc.stdout = stdout_pipe

        # Run symbolizer to filter the stderr stream.
        self._symbolizer_proc = run_symbolizer(
            [self._port.get_build_ids_path()],
            merged_stdout_stderr,
            subprocess.PIPE,
            raw_bytes=True)
        proc.stderr = self._symbolizer_proc.stdout

        self._set_proc(proc)

    def stop(self, timeout_secs=0.0, kill_tree=False, send_sigterm=False):
        result = super(FuchsiaServerProcess,
                       self).stop(timeout_secs, kill_tree, send_sigterm)
        if self._symbolizer_proc:
            self._symbolizer_proc.kill()
        return result
