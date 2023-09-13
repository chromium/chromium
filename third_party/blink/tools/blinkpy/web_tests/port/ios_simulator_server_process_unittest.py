# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys
import subprocess
import unittest

from blinkpy.common.system.system_host import SystemHost
from blinkpy.web_tests.port import ios_simulator_server_process
from blinkpy.web_tests.port.factory import PortFactory

TEST_MESSAGE = b'writing test of a web test name'


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
        proc.write(b'')

        self.assertIsNone(proc.poll())
        self.assertFalse(proc.has_crashed())

        # Check if the iOS simulator server process creates a test file for
        # communicating between run_web_test.py and a content shell.
        web_test_file_path = proc._get_web_test_file_path()
        self.assertIsNotNone(web_test_file_path)

        proc.write(TEST_MESSAGE)

        test_file = open(web_test_file_path, 'r')
        read_line = test_file.readline().strip()
        self.assertEqual(read_line.encode(), TEST_MESSAGE)

        proc.stop(0)
