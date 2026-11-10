# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import unittest
from unittest import mock

from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import fuchsia


class _MockPort:
    def __init__(self, target_host):
        self.host = MockSystemHost()
        self._target_host = target_host

    def get_target_host(self):
        return self._target_host

    def results_directory(self):
        return '/mock-results'


class FuchsiaServerProcessTest(unittest.TestCase):
    def test_start_converts_port_forward_error_to_device_failure(self):
        target_host = mock.Mock()
        target_host.setup_forwarded_port.side_effect = (
            subprocess.CalledProcessError(255, ['ssh', 'echo', 'true'])
        )
        port = _MockPort(target_host)
        proc = fuchsia.FuchsiaServerProcess(port, 'content_shell', ['cmd'])

        with mock.patch('socket.socket') as mock_socket_cls:
            mock_socket = mock_socket_cls.return_value
            mock_socket.getsockname.return_value = ('127.0.0.1', 12345)

            with self.assertRaises(driver.DeviceFailure):
                proc._start()

            target_host.setup_forwarded_port.assert_called_once_with(12345)
            mock_socket.close.assert_called_once_with()
            target_host.run_test_component.assert_not_called()
