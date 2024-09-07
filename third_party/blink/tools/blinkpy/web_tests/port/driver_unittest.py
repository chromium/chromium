# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import optparse
import unittest

from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port.driver import Driver
from blinkpy.web_tests.port.driver import coalesce_repeated_switches
from blinkpy.web_tests.port.factory import command_wrapper
from blinkpy.web_tests.port.server_process_mock import MockServerProcess


class DriverTest(unittest.TestCase):

    # pylint: disable=protected-access

    def make_port(self, **extra_options):
        return Port(
            MockSystemHost(), 'test',
            optparse.Values({
                'configuration': 'Release',
                **extra_options,
            }))

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = command_wrapper(wrapper_string)
        self.assertEqual(wrapper, expected_wrapper)

    def test_command_wrapper(self):
        self._assert_wrapper('valgrind', ['valgrind'])

        # Validate that shlex works as expected.
        command_with_spaces = 'valgrind --smc-check=\'check with spaces!\' --foo'
        expected_parse = [
            'valgrind', '--smc-check=check with spaces!', '--foo'
        ]
        self._assert_wrapper(command_with_spaces, expected_parse)

    def test_test_to_uri(self):
        port = self.make_port()
        driver = Driver(port, None)
        self.assertEqual(
            driver.test_to_uri('foo/bar.html'),
            'file://%s/foo/bar.html' % port.web_tests_dir())
        self.assertEqual(
            driver.test_to_uri('http/tests/foo.html'),
            'http://127.0.0.1:8000/foo.html')
        self.assertEqual(
            driver.test_to_uri('http/tests/https/bar.html'),
            'https://127.0.0.1:8443/https/bar.html')
        self.assertEqual(
            driver.test_to_uri('http/tests/bar.https.html'),
            'https://127.0.0.1:8443/bar.https.html')
        self.assertEqual(
            driver.test_to_uri('http/tests/barhttps.html'),
            'http://127.0.0.1:8000/barhttps.html')
        self.assertEqual(
            driver.test_to_uri('external/wpt/foo/bar.html'),
            'http://web-platform.test:8001/foo/bar.html')
        self.assertEqual(
            driver.test_to_uri('external/wpt/foo/bar.https.html'),
            'https://web-platform.test:8444/foo/bar.https.html')
        self.assertEqual(
            driver.test_to_uri('external/wpt/foo/bar.serviceworker.html'),
            'https://web-platform.test:8444/foo/bar.serviceworker.html')
        self.assertEqual(
            driver.test_to_uri('wpt_internal/foo/bar.html'),
            'http://web-platform.test:8001/wpt_internal/foo/bar.html')
        self.assertEqual(
            driver.test_to_uri('wpt_internal/foo/bar.https.html'),
            'https://web-platform.test:8444/wpt_internal/foo/bar.https.html')
        self.assertEqual(
            driver.test_to_uri('wpt_internal/foo/bar.serviceworker.html'),
            'https://web-platform.test:8444/wpt_internal/foo/bar.serviceworker.html'
        )

    def test_uri_to_test(self):
        port = self.make_port()
        driver = Driver(port, None)
        self.assertEqual(
            driver.uri_to_test(
                'file://%s/foo/bar.html' % port.web_tests_dir()),
            'foo/bar.html')
        self.assertEqual(
            driver.uri_to_test('http://127.0.0.1:8000/foo.html'),
            'http/tests/foo.html')
        self.assertEqual(
            driver.uri_to_test('https://127.0.0.1:8443/https/bar.html'),
            'http/tests/https/bar.html')
        self.assertEqual(
            driver.uri_to_test('https://127.0.0.1:8443/bar.https.html'),
            'http/tests/bar.https.html')
        self.assertEqual(
            driver.uri_to_test('http://web-platform.test:8001/foo/bar.html'),
            'external/wpt/foo/bar.html')
        self.assertEqual(
            driver.uri_to_test(
                'https://web-platform.test:8444/foo/bar.https.html'),
            'external/wpt/foo/bar.https.html')
        self.assertEqual(
            driver.uri_to_test(
                'https://web-platform.test:8444/foo/bar.serviceworker.html'),
            'external/wpt/foo/bar.serviceworker.html')
        self.assertEqual(
            driver.uri_to_test(
                'http://web-platform.test:8001/wpt_internal/foo/bar.html'),
            'wpt_internal/foo/bar.html')
        self.assertEqual(
            driver.uri_to_test(
                'https://web-platform.test:8444/wpt_internal/foo/bar.https.html'
            ), 'wpt_internal/foo/bar.https.html')
        self.assertEqual(
            driver.uri_to_test(
                'https://web-platform.test:8444/wpt_internal/foo/bar.serviceworker.html'
            ), 'wpt_internal/foo/bar.serviceworker.html')

    def test_read_block(self):
        port = self.make_port()
        driver = Driver(port, 0)
        driver._server_process = MockServerProcess(lines=[
            b'ActualHash: foobar',
            b'Content-Type: my_type',
            b'Content-Transfer-Encoding: none',
            b'#EOF',
        ])
        content_block = driver._read_block(0)
        self.assertEqual(content_block.content, b'')
        self.assertEqual(content_block.content_type, b'my_type')
        self.assertEqual(content_block.encoding, b'none')
        self.assertEqual(content_block.content_hash, b'foobar')
        driver._server_process = None

    def test_read_binary_block(self):
        port = self.make_port()
        driver = Driver(port, 0)
        driver._server_process = MockServerProcess(lines=[
            b'ActualHash: actual',
            b'ExpectedHash: expected',
            b'Content-Type: image/png',
            b'Content-Length: 9',
            b'12345678',
            b'#EOF',
        ])
        content_block = driver._read_block(0)
        self.assertEqual(content_block.content_type, b'image/png')
        self.assertEqual(content_block.content_hash, b'actual')
        self.assertEqual(content_block.content, b'12345678\n')
        self.assertEqual(content_block.decoded_content, b'12345678\n')
        driver._server_process = None

    def test_read_base64_block(self):
        port = self.make_port()
        driver = Driver(port, 0)
        driver._server_process = MockServerProcess(lines=[
            b'ActualHash: actual',
            b'ExpectedHash: expected',
            b'Content-Type: image/png',
            b'Content-Transfer-Encoding: base64',
            b'Content-Length: 12',
            b'MTIzNDU2NzgK#EOF',
        ])
        content_block = driver._read_block(0)
        self.assertEqual(content_block.content_type, b'image/png')
        self.assertEqual(content_block.content_hash, b'actual')
        self.assertEqual(content_block.encoding, b'base64')
        self.assertEqual(content_block.content, b'MTIzNDU2NzgK')
        self.assertEqual(content_block.decoded_content, b'12345678\n')

    def test_no_timeout(self):
        port = self.make_port()
        driver = Driver(port, 0, no_timeout=True)
        cmd_line = driver.cmd_line([])
        self.assertEqual(cmd_line[0],
                         '/mock-checkout/out/Release/content_shell')
        self.assertEqual(cmd_line[-1], '-')
        self.assertIn('--no-timeout', cmd_line)

    def test_disable_system_font_check(self):
        port = self.make_port()
        driver = Driver(port, 0)
        self.assertNotIn('--disable-system-font-check', driver.cmd_line([]))
        port = self.make_port(nocheck_sys_deps=True)
        driver = Driver(port, 0)
        self.assertIn('--disable-system-font-check', driver.cmd_line([]))

    def test_check_for_driver_crash(self):
        port = self.make_port()
        driver = Driver(port, 0)

        class FakeServerProcess(object):
            def __init__(self, crashed):
                self.crashed = crashed

            def pid(self):
                return 1234

            def name(self):
                return 'FakeServerProcess'

            def has_crashed(self):
                return self.crashed

            def stop(self,
                     timeout_secs=0.0,
                     kill_tree=True,
                     send_sigterm=False):
                pass

        def assert_crash(driver,
                         error_line,
                         crashed,
                         name,
                         pid,
                         unresponsive=False,
                         leaked=False):
            self.assertEqual(
                driver._check_for_driver_crash(error_line), crashed)
            self.assertEqual(driver._crashed_process_name, name)
            self.assertEqual(driver._crashed_pid, pid)
            self.assertEqual(driver._subprocess_was_unresponsive, unresponsive)
            self.assertEqual(driver._check_for_leak(error_line), leaked)
            driver.stop()

        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '', False, None, None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '#CRASHED\n', True, 'FakeServerProcess', 1234)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '#CRASHED - WebProcess\n', True, 'WebProcess',
                     None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '#CRASHED - WebProcess (pid 8675)\n', True,
                     'WebProcess', 8675)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '#PROCESS UNRESPONSIVE - WebProcess (pid 8675)\n',
                     True, 'WebProcess', 8675, True)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '#CRASHED - renderer (pid 8675)\n', True,
                     'renderer', 8675)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(
            driver,
            '#LEAK - renderer pid 8675 ({"numberOfLiveDocuments":[2,3]})\n',
            False, None, None, False, True)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(True)
        driver._subprocess_was_unresponsive = False
        driver._leaked = False
        assert_crash(driver, '', True, 'FakeServerProcess', 1234)

    def test_creating_a_port_does_not_write_to_the_filesystem(self):
        port = self.make_port()
        Driver(port, 0)
        self.assertEqual(port.host.filesystem.written_files, {})
        self.assertIsNone(port.host.filesystem.last_tmpdir)

    def test_stop_cleans_up_properly(self):
        port = self.make_port()
        port.server_process_constructor = MockServerProcess
        driver = Driver(port, 0)
        driver.start([], None)
        last_tmpdir = port.host.filesystem.last_tmpdir
        self.assertIsNotNone(last_tmpdir)
        driver.stop()
        self.assertFalse(port.host.filesystem.isdir(last_tmpdir))

    def test_two_starts_cleans_up_properly(self):
        port = self.make_port()
        port.server_process_constructor = MockServerProcess
        driver = Driver(port, 0)
        driver.start([], None)
        last_tmpdir = port.host.filesystem.last_tmpdir
        driver._start([])
        self.assertFalse(port.host.filesystem.isdir(last_tmpdir))

    def test_start_actually_starts(self):
        port = self.make_port()
        port.server_process_constructor = MockServerProcess
        driver = Driver(port, 0)
        driver.start([], None)
        self.assertTrue(driver._server_process.started)


class CoalesceRepeatedSwitchesTest(unittest.TestCase):
    def _assert_coalesced_switches(self, input_switches,
                                   expected_coalesced_switches):
        output_switches = coalesce_repeated_switches(input_switches)
        self.assertEquals(output_switches, expected_coalesced_switches)

    def test_no_dupes(self):
        self._assert_coalesced_switches(['--a', '--b', '--c'],
                                        ['--a', '--b', '--c'])

    def test_unknown_duplicates(self):
        self._assert_coalesced_switches(['--a', '--b', '--a'],
                                        ['--a', '--b', '--a'])

    def test_simple_repeated_enable_features(self):
        self._assert_coalesced_switches(
            ['--A', '--enable-features=Y', '--X', '--enable-features=X'],
            ['--A', '--X', '--enable-features=X,Y'])

    def test_multiple_enable_features(self):
        self._assert_coalesced_switches([
            '--A', '--enable-features=Z,X', '--enable-features=Y', '--X',
            '--enable-features=X,Y', '--enable-features=X',
            '--enable-features=X,X'
        ], ['--A', '--X', '--enable-features=X,Y,Z'])

    def test_multiple_disable_features(self):
        self._assert_coalesced_switches([
            '--A', '--disable-features=Z,X', '--disable-features=Y', '--X',
            '--disable-features=X,Y', '--disable-features=X',
            '--disable-features=X,X'
        ], ['--A', '--X', '--disable-features=X,Y,Z'])

    def test_enable_and_disable_features(self):
        # The coalescing of --enable-features and --disable-features is
        # independent (may both enable and disable the same feature).
        self._assert_coalesced_switches([
            '--A', '--disable-features=Z', '--disable-features=E,X',
            '--enable-features=Y', '--X', '--enable-features=X,Y'
        ], ['--A', '--X', '--enable-features=X,Y', '--disable-features=E,X,Z'])
