# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import select
import socket

from blinkpy.web_tests.port.server_process import ServerProcess
from blinkpy.web_tests.port import driver

_log = logging.getLogger(__name__)

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
        self._port.check_simulator_is_booted()

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
