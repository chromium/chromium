# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import select
import socket
import time

from blinkpy.web_tests.port.server_process import ServerProcess
from blinkpy.web_tests.port import driver

_log = logging.getLogger(__name__)

BOOT_STATE = 'Booted'

CONN_WAITING_TIMEOUT = 20


# Custom version of ServerProcess that runs processes on the iOS simulator.
class IOSSimulatorServerProcess(ServerProcess):
    def __init__(self,
                 port_obj,
                 name,
                 cmd,
                 env=None,
                 treat_no_data_as_crash=False,
                 more_logging=False):
        super(IOSSimulatorServerProcess,
              self).__init__(port_obj, name, cmd, env, treat_no_data_as_crash,
                             more_logging)
        self._boot_simulator()

    def _start(self):
        if self._proc:
            raise ValueError('%s already running' % self._name)
        self._reset()

        # The iOS simulator doesn't allow to use stdin stream for packaged apps.
        # Additionally, the stdout from run-test-suite not only includes
        # additional data from the iOS test infrastructure but also combines
        # stderr and stdout. As a result, when executing content_shell on the
        # iOS simulator, it becomes impractical to employ stdin for transmitting
        # a list of tests or to dependably utilize stdout for outputting
        # results. To address this issue in the context of web tests, we employ
        # a workaround by redirecting stdin and stdout to a TCP socket connected
        # to the web test runner. Similar to Fuchsia, the runner also uses the
        # --stdio-redirect flag to specify the address and port for stdin and
        # stdout redirection.
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listen_socket.bind(('127.0.0.1', self._port.stdio_redirect_port()))
        listen_socket.listen(1)

        proc = self._host.executive.popen(self._cmd,
                                          stdin=self._host.executive.PIPE,
                                          stdout=self._host.executive.PIPE,
                                          stderr=self._host.executive.PIPE,
                                          env=self._env)

        # Wait for incoming connection from the iOS content_shell.
        fd = listen_socket.fileno()

        read_fds, _, _ = select.select([fd], [], [], CONN_WAITING_TIMEOUT)
        while fd not in read_fds:
            _log.error(
                'Timed out waiting connection from content_shell. Try to launch the content_shell again.'
            )
            proc.kill()
            # TODO(gyuyoung): There is sometimes no incoming connection when
            # attempting to launch a content shell. We need to investigate why
            # launching the content shell occasionally fails. To resolve the
            # issue, consider repeatedly attempting to launch the content shell
            # until it successfully launches.
            proc = self._host.executive.popen(self._cmd,
                                              stdin=self._host.executive.PIPE,
                                              stdout=self._host.executive.PIPE,
                                              stderr=self._host.executive.PIPE,
                                              env=self._env)
            read_fds, _, _ = select.select([fd], [], [], CONN_WAITING_TIMEOUT)

        stdio_socket, _ = listen_socket.accept()
        fd = stdio_socket.fileno()  # pylint: disable=no-member
        stdin_pipe = os.fdopen(os.dup(fd), 'wb', 0)
        stdout_pipe = os.fdopen(os.dup(fd), 'rb', 0)
        stdio_socket.close()

        proc.stdin = stdin_pipe
        proc.stdout = stdout_pipe

        self._set_proc(proc)

    def _boot_simulator(self):
        device = self._get_device(self._port.device_name())
        state = device.get('state')
        if state != BOOT_STATE:
            _log.info('No simulator is booted. Booting a simulator...')
            udid = device.get('udid')
            self._run_simctl('boot ' + udid)
            time.sleep(2)  # Wait for 2 seconds before checking the state.

            while True:
                device = self._get_device(self._port.device_name())
                state = device.get('state')
                if state == BOOT_STATE:
                    break
                time.sleep(2)  # Wait for 2 seconds before checking again.

    def _get_device(self, device_name):
        devices = json.loads(self._run_simctl('list -j devices available'))
        if len(devices) == 0:
            raise RuntimeError('No available device in the iOS simulator.')
        runtime = self._latest_runtime()
        return next(
            (d
             for d in devices['devices'][runtime] if d['name'] == device_name),
            None)

    def _latest_runtime(self):
        runtimes = json.loads(self._run_simctl('list -j runtimes available'))
        valid_runtimes = [
            runtime for runtime in runtimes['runtimes']
            if 'identifier' in runtime and runtime['identifier'].startswith(
                'com.apple.CoreSimulator.SimRuntime')
        ]
        if len(valid_runtimes) == 0:
            raise RuntimeError('No valid runtime in the iOS simulator.')

        valid_runtimes.sort(key=lambda runtime: float(runtime['version']),
                            reverse=True)
        return valid_runtimes[0]['identifier']

    def _run_simctl(self, command):
        prefix_commands = ['/usr/bin/xcrun', 'simctl']
        command_array = prefix_commands + command.split()
        return self._host.executive.run_command(command_array)
