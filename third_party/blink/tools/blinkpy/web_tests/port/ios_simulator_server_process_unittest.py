# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import socket
import subprocess
import sys
import threading
import unittest

from blinkpy.common.system.system_host import SystemHost
from blinkpy.web_tests.port import ios_simulator_server_process
from blinkpy.web_tests.port.factory import PortFactory

TEST_FILE_NAME = b'dom/parent_node/append-on-document.html'


class TestIOSSimulatorServerProcess(unittest.TestCase):
    def _is_ios_simulator_installed(self):
        try:
            devices = json.loads(
                subprocess.check_output([
                    '/usr/bin/xcrun', 'simctl', 'list', '-j', 'devices',
                    'available'
                ]))

            if len(devices) != 0:
                return True
            else:
                return False
        except subprocess.CalledProcessError:
            return False

    def _start_simulator_server(
            self,
            proc: ios_simulator_server_process.IOSSimulatorServerProcess):
        proc.write(TEST_FILE_NAME)

    def test_write(self):
        # Test only when the iOS simulator is installed on Mac.
        if sys.platform != 'darwin' or not self._is_ios_simulator_installed():
            return

        cmd = [
            sys.executable, '-c',
            'import sys; import time; time.sleep(0.02); print "stdout"; sys.stdout.flush(); print >>sys.stderr, "stderr"'
        ]
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get('ios')
        proc = ios_simulator_server_process.IOSSimulatorServerProcess(
            port, 'python', cmd)
        server = threading.Thread(target=self._start_simulator_server,
                                  args=(proc, ),
                                  name='simulator-server',
                                  daemon=True)
        # Start to the simulator server, and it sends a test file name to the
        # client.
        server.start()

        self.assertIsNone(proc.poll())
        self.assertFalse(proc.has_crashed())

        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            # Connect to the server.
            client_socket.connect(('127.0.0.1', port.stdio_redirect_port()))

            # Check if the received data is the expected test file name.
            received_data = client_socket.recv(1024)
            self.assertEqual(received_data, TEST_FILE_NAME)
        except socket.error as e:
            raise ValueError('Connection error: {%s}' % e)
        finally:
            client_socket.close()
            server.join()

        proc.stop(0)
