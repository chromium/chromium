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

import functools
import mock
import optparse
import sys
import unittest

from blinkpy.common.system import filesystem_mock
from blinkpy.common.system import output_capture
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.web_tests.port import port_testcase
from blinkpy.web_tests.port import win


class WinPortTest(port_testcase.PortTestCase):
    port_name = 'win'
    full_port_name = 'win-win11'
    port_maker = win.WinPort
    os_name = 'win'
    os_version = 'win11'

    def test_setup_environ_for_server(self):
        port = self.make_port()
        port._executive = MockExecutive(should_log=True)
        output = output_capture.OutputCapture()
        orig_environ = port.host.environ.copy()
        env = output.assert_outputs(self, port.setup_environ_for_server)
        self.assertEqual(orig_environ['PATH'], port.host.environ.get('PATH'))

    def assert_name(self, port_name, os_version_string, expected):
        port = self.make_port(
            port_name=port_name, os_version=os_version_string)
        self.assertEqual(expected, port.name())

    def test_get_platform_tags(self):
        port = self.make_port()
        self.assertEqual(port.get_platform_tags(),
                         {'win', 'win11', 'x86', 'release'})

    def test_versions(self):
        port = self.make_port()
        self.assertIn(port.name(), ('win-win11'))

        self.assert_name(None, '10.20h2', 'win-win10.20h2')
        self.assert_name('win', '10.20h2', 'win-win10.20h2')
        self.assert_name('win-win10.20h2', '10.20h2', 'win-win10.20h2')
        self.assert_name('win-win10.20h2', '10.1909', 'win-win10.20h2')

        self.assert_name(None, '8', 'win-win10.20h2')
        self.assert_name(None, '8.1', 'win-win10.20h2')
        self.assert_name('win', '8', 'win-win10.20h2')
        self.assert_name('win', '8.1', 'win-win10.20h2')

        self.assert_name(None, '7sp1', 'win-win10.20h2')
        self.assert_name(None, '7sp0', 'win-win10.20h2')
        self.assert_name(None, 'vista', 'win-win10.20h2')
        self.assert_name('win', '7sp1', 'win-win10.20h2')
        self.assert_name('win', '7sp0', 'win-win10.20h2')
        self.assert_name('win', 'vista', 'win-win10.20h2')

        self.assert_name(None, 'win11', 'win-win11')
        self.assert_name('win', 'win11', 'win-win11')

        self.assert_name(None, 'future', 'win-win11')
        self.assert_name('win', 'future', 'win-win11')
        self.assert_name('win-win11', 'future', 'win-win11')

        with self.assertRaises(AssertionError):
            self.assert_name(None, 'w2k', 'win-win11')

    def assert_baseline_paths(self, port_name, *expected_paths):
        port = self.make_port(port_name=port_name)
        self.assertEqual(port.baseline_version_dir(),
                         port._absolute_baseline_path(expected_paths[0]))  # pylint: disable=protected-access
        self.assertEqual(len(port.baseline_search_path()), len(expected_paths))
        for i, path in enumerate(expected_paths):
            self.assertTrue(port.baseline_search_path()[i].endswith(path))

    def test_baseline_path(self):
        self.assert_baseline_paths('win-win10.20h2', 'win10', 'win')
        self.assert_baseline_paths('win-win11', 'win')

    def test_operating_system(self):
        self.assertEqual('win', self.make_port().operating_system())

    @unittest.skipIf(sys.platform != 'win32',
                     'Needs WindowsError to be defined')
    def test_python3_command(self):
        def run_command_fn(valid_python, args):
            if args[0] != valid_python:
                raise WindowsError('%s != %s' % (valid_python, args[0]))
            return 0

        with mock.patch('os.getenv', return_value='.BAT;.EXE;.COM'):
            # Simple case: one of the extensions match.
            port = self.make_port()
            port._executive = MockExecutive(run_command_fn=functools.partial(
                run_command_fn, 'python3.EXE'))
            self.assertEqual('python3.EXE', port.python3_command())

            # Ensure that the code checks for python3 without an extension.
            port = self.make_port()
            port._executive = MockExecutive(
                run_command_fn=functools.partial(run_command_fn, 'python3'))
            self.assertEqual('python3', port.python3_command())

            # If there are no matches, a WindowsError should be raised.
            port = self.make_port()
            port._executive = MockExecutive(
                run_command_fn=functools.partial(run_command_fn, None))
            with self.assertRaises(WindowsError):
                port.python3_command()

    @unittest.skipIf(sys.platform != 'win32', 'Smoketest for Windows only')
    def test_python3_command_smoketest(self):
        # This is a smoketest to make sure that depot_tools has ensured a valid
        # Python 3 is available. If this test fails on a machine, it means that
        # there is something wrong with the Python 3 setup.
        self.assertIsNotNone(self.make_port().python3_command())

    def test_driver_name_option(self):
        self.assertTrue(
            self.make_port().path_to_driver().endswith('content_shell.exe'))
        port = self.make_port(
            options=optparse.Values({'driver_name': 'OtherDriver'}))
        self.assertTrue(port.path_to_driver().endswith('OtherDriver.exe'))

    def test_path_to_image_diff(self):
        self.assertEqual(self.make_port()._path_to_image_diff(),
                         '/mock-checkout/out/Release/image_diff.exe')

    def test_path_to_apache_config_file(self):
        self.assertEqual(
            self.make_port().path_to_apache_config_file(),
            '/mock-checkout/third_party/blink/tools/apache_config/win-httpd.conf'
        )

    def test_relative_test_filename(self):
        port = self.make_port()
        relative_path = port._filesystem.join(port.web_tests_dir(), 'foo',
                                              'bar')
        self.assertEqual(port.relative_test_filename(relative_path), 'foo/bar')
        absolute_path = 'C:\\foo\\bar\\not_relative'
        # Non-Windows platforms won't see the given path as absolute, so mock
        # that out.
        if sys.platform != 'win32':
            mock_filesystem = filesystem_mock.MockFileSystem()
            mock_filesystem.abspath = lambda p: p
            port._filesystem = mock_filesystem
            port.host.filesystem = mock_filesystem
        self.assertEqual(port.relative_test_filename(absolute_path),
                         '/C:/foo/bar/not_relative')
