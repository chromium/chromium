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
import multiprocessing
import os
import select
import socket
import subprocess
import sys
import threading

from argparse import Namespace
from blinkpy.common import exit_codes
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.path_finder import get_chromium_src_dir
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import factory
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import server_process

# Modules loaded dynamically in _import_fuchsia_runner().
# pylint: disable=invalid-name
fuchsia_target = None
qemu_target = None
symbolizer = None

# pylint: enable=invalid-name


# Imports Fuchsia runner modules. This is done dynamically only when FuchsiaPort
# is instantiated to avoid dependency on Fuchsia runner on other platforms.
def _import_fuchsia_runner():
    sys.path.insert(0, os.path.join(get_chromium_src_dir(), 'build/fuchsia'))

    # pylint: disable=import-error
    # pylint: disable=invalid-name
    # pylint: disable=redefined-outer-name
    global ConnectPortForwardingTask
    from common import ConnectPortForwardingTask
    global _GetPathToBuiltinTarget, _LoadTargetClass, InitializeTargetArgs
    from common_args import _GetPathToBuiltinTarget, _LoadTargetClass, InitializeTargetArgs
    global fuchsia_target
    import target as fuchsia_target
    global qemu_target
    import qemu_target
    global symbolizer
    import symbolizer
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

PROCESS_START_TIMEOUT = 20

_log = logging.getLogger(__name__)


def _subprocess_log_thread(pipe, prefix):
    try:
        while True:
            line = pipe.readline()
            if not line:
                return
            _log.error('%s: %s', prefix, line.decode('utf8'))
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
    def __init__(self, build_path, build_ids_path, ports_to_forward, target,
                 results_directory):
        try:
            self._pkg_repo = None
            self._target = target
            self._target.Start()
            self._setup_target(build_path, build_ids_path, ports_to_forward,
                               results_directory)
        except:
            self.cleanup()
            raise

    def _setup_target(self, build_path, build_ids_path, ports_to_forward,
                      results_directory):
        # Tell SSH to forward all server ports from the Fuchsia device to
        # the host.
        forwarding_flags = [
            '-O',
            'forward',  # Send SSH mux control signal.
            '-N',  # Don't execute command
            '-T'  # Don't allocate terminal.
        ]
        for port in ports_to_forward:
            forwarding_flags += ['-R', '%d:localhost:%d' % (port, port)]
        self._proxy = self._target.RunCommandPiped([],
                                                   ssh_args=forwarding_flags,
                                                   stdout=subprocess.PIPE,
                                                   stderr=subprocess.STDOUT)

        package_path = os.path.join(build_path, CONTENT_SHELL_PACKAGE_PATH)
        self._target.StartSystemLog([package_path])

        self._pkg_repo = self._target.GetPkgRepo()
        self._pkg_repo.__enter__()

        self._target.InstallPackage([package_path])

        # Process will be forked for each worker, which may make QemuTarget
        # unusable (e.g. waitpid() for qemu process returns ECHILD after
        # fork() ). Save command runner before fork()ing, to use it later to
        # connect to the target.
        self.target_command_runner = self._target.GetCommandRunner()

    def run_command(self, command):
        return self.target_command_runner.RunCommandPiped(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

    def cleanup(self):
        try:
            if self._pkg_repo:
                self._pkg_repo.__exit__(None, None, None)
        finally:
            if self._target:
                self._target.Stop()

    def setup_forwarded_port(self, port):
        return ConnectPortForwardingTask(self._target, port)


class FuchsiaPort(base.Port):
    port_name = 'fuchsia'

    SUPPORTED_VERSIONS = ('fuchsia', )

    FALLBACK_PATHS = {
        'fuchsia':
        ['fuchsia'] + linux.LinuxPort.latest_platform_fallback_path()
    }

    def __init__(self, host, port_name, **kwargs):
        super(FuchsiaPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'fuchsia'
        self._version = 'fuchsia'
        self._target_device = self.get_option('device')

        self._architecture = 'x86_64' if self._target_cpu(
        ) == 'x64' else 'arm64'

        self.server_process_constructor = FuchsiaServerProcess

        # Used to implement methods that depend on the host platform.
        self._host_port = factory.PortFactory(host).get(**kwargs)

        self._target_host = self.get_option('fuchsia_target')
        self._zircon_logger = None
        _import_fuchsia_runner()

    def _driver_class(self):
        return ChromiumFuchsiaDriver

    def _path_to_driver(self, target=None):
        return self._build_path_with_target(target, CONTENT_SHELL_PACKAGE_PATH)

    def __del__(self):
        if self._zircon_logger:
            self._zircon_logger.close()

    def _target_cpu(self):
        return self.get_option('fuchsia_target_cpu')

    def _cpu_cores(self):
        # Revise the processor count on arm64, the trybots on arm64 are in
        # dockers and cannot use all processors.
        # For x64, fvdl always assumes hyperthreading is supported by intel
        # processors, but the cpu_count returns the number regarding if the core
        # is a physical one or a hyperthreading one, so the number should be
        # divided by 2 to avoid creating more threads than the processor
        # supports.
        if self._target_cpu() == 'x64':
            return max(int(multiprocessing.cpu_count() / 2) - 1, 4)
        return 4

    def setup_test_run(self):
        super(FuchsiaPort, self).setup_test_run()
        try:
            target_args = InitializeTargetArgs()
            target_args.out_dir = self._build_path()
            target_args.target_cpu = self._target_cpu()
            target_args.fuchsia_out_dir = self.get_option('fuchsia_out_dir')
            target_args.ssh_config = self.get_option('fuchsia_ssh_config')
            target_args.host = self.get_option('fuchsia_host')
            target_args.port = self.get_option('fuchsia_port')
            target_args.node_name = self.get_option('fuchsia_node_name')
            target_args.cpu_cores = self._cpu_cores()
            target_args.logs_dir = self.results_directory()
            target = _LoadTargetClass(
                _GetPathToBuiltinTarget(
                    self._target_device)).CreateFromArgs(target_args)
            self._target_host = _TargetHost(self._build_path(),
                                            self.get_build_ids_path(),
                                            self.SERVER_PORTS, target,
                                            self.results_directory())

            if self.get_option('zircon_logging'):
                klog_proc = self._target_host.run_command(['dlog', '-f'])
                symbolized_klog_proc = symbolizer.RunSymbolizer(klog_proc.stdout,
                    subprocess.PIPE, [self.get_build_ids_path()])
                self._zircon_logger = SubprocessOutputLogger(symbolized_klog_proc,
                    'Zircon')

            # Save fuchsia_target in _options, so it can be shared with other
            # workers.
            self._options.fuchsia_target = self._target_host

        except fuchsia_target.FuchsiaTargetException as e:
            _log.error('Failed to start qemu: %s.', str(e))
            return exit_codes.NO_DEVICES_EXIT_STATUS

    def clean_up_test_run(self):
        if self._target_host:
            self._target_host.cleanup()
            self._target_host = None

    def num_workers(self, requested_num_workers):
        # Run a single qemu instance.
        return min(self._cpu_cores(), requested_num_workers)

    def _default_timeout_ms(self):
        # Use 20s timeout instead of the default 6s. This is necessary because
        # the tests are executed in qemu, so they run slower compared to other
        # platforms.
        return 20000

    def start_http_server(self, additional_dirs, number_of_drivers):
        additional_dirs['/third_party/blink/PerformanceTests'] = \
            self._perf_tests_dir()
        additional_dirs[WEB_TESTS_PATH_PREFIX] = self.web_tests_dir()
        additional_dirs['/gen'] = self.generated_sources_directory()
        additional_dirs['/third_party/blink'] = \
            self._path_from_chromium_base('third_party', 'blink')
        super(FuchsiaPort, self).start_http_server(additional_dirs,
                                                   number_of_drivers)

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
        package_path = self._path_to_driver()
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
        cmd = [
            'run',
            'fuchsia-pkg://fuchsia.com/content_shell#meta/content_shell.cmx'
        ]
        if self._port._target_device == 'qemu':
            cmd.append('--ozone-platform=headless')
        # Use Scenic on AEMU
        else:
            cmd.extend([
                '--ozone-platform=scenic', '--use-vulkan',
                '--enable-gpu-rasterization', '--force-device-scale-factor=1',
                '--use-gl=stub', '--enable-features=Vulkan',
                '--gpu-watchdog-timeout-seconds=60'
            ])
        return cmd

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumFuchsiaDriver,
                        self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            relative_test_filename = \
                os.path.relpath(command, self._port.web_tests_dir())
            command = 'http://127.0.0.1:8000' + WEB_TESTS_PATH_PREFIX + \
                '/' + relative_test_filename
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

        # Fuchsia doesn't support stdin stream for packaged applications, so the
        # stdin stream for content_shell is routed through a separate TCP
        # socket. The socket is reverse-forwarded from the device to the script
        # over SSH.
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.bind(('127.0.0.1', 0))
        listen_socket.listen(1)
        stdin_port = int(listen_socket.getsockname()[1])
        forwarded_stdin_port = \
            self._port.get_target_host().setup_forwarded_port(stdin_port)

        command = ['%s=%s' % (k, v) for k, v in self._env.items()] + \
            self._cmd + \
            ['--no-sandbox', '--stdin-redirect=127.0.0.1:%d' %
             (forwarded_stdin_port)]

        proc = self._port.get_target_host().run_command(command)
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
        stdin_socket, _ = listen_socket.accept()
        fd = stdin_socket.fileno()  # pylint: disable=no-member
        stdin_pipe = os.fdopen(os.dup(fd), "wb", 0)
        stdin_socket.close()

        proc.stdin.close()
        proc.stdin = stdin_pipe
        # Run symbolizer to filter the stderr stream.
        self._symbolizer_proc = symbolizer.RunSymbolizer(
            proc.stderr, subprocess.PIPE, [self._port.get_build_ids_path()])
        proc.stderr = self._symbolizer_proc.stdout

        self._set_proc(proc)

    def stop(self, timeout_secs=0.0, kill_tree=False):
        result = super(FuchsiaServerProcess, self).stop(
            timeout_secs, kill_tree)
        if self._symbolizer_proc:
            self._symbolizer_proc.kill()
        return result
