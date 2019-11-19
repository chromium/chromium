# Copyright (C) 2011 Google Inc. All rights reserved.
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

import logging
import optparse

from blinkpy.common.exit_codes import SYS_DEPS_EXIT_STATUS
from blinkpy.common.system.executive_mock import MockExecutive, MockProcess
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import port_testcase


class LinuxPortTest(port_testcase.PortTestCase, LoggingTestCase):
    os_name = 'linux'
    os_version = 'trusty'
    port_name = 'linux'
    full_port_name = 'linux-trusty'
    port_maker = linux.LinuxPort

    def assert_version_properties(self, port_name, os_version, expected_name,
                                  expected_version,
                                  driver_file_output=None):
        host = MockSystemHost(os_name=self.os_name, os_version=(os_version or self.os_version))
        host.filesystem.isfile = lambda x: 'content_shell' in x
        if driver_file_output:
            host.executive = MockExecutive(driver_file_output)
        port = self.make_port(host=host, port_name=port_name, os_version=os_version)
        self.assertEqual(port.name(), expected_name)
        self.assertEqual(port.version(), expected_version)

    def test_versions(self):
        self.assertTrue(self.make_port().name() in ('linux-trusty',))

        self.assert_version_properties('linux', 'trusty', 'linux-trusty', 'trusty')
        self.assert_version_properties('linux-trusty', None, 'linux-trusty', 'trusty')
        with self.assertRaises(AssertionError):
            self.assert_version_properties('linux-utopic', None, 'ignored', 'ignored', 'ignored')

    def assert_baseline_paths(self, port_name, os_version, *expected_paths):
        port = self.make_port(port_name=port_name, os_version=os_version)
        self.assertEqual(
            port.baseline_version_dir(),
            port._absolute_baseline_path(expected_paths[0]))  # pylint: disable=protected-access
        self.assertEqual(len(port.baseline_search_path()), len(expected_paths))
        for i, path in enumerate(expected_paths):
            self.assertTrue(port.baseline_search_path()[i].endswith(path))

    def test_get_platform_tags(self):
        port = self.make_port()
        self.assertEqual(port.get_platform_tags(), {'linux', 'trusty', 'x86_64', 'release'})

    def test_baseline_paths(self):
        self.assert_baseline_paths('linux', 'trusty', 'linux', '/win')
        self.assert_baseline_paths('linux-trusty', None, 'linux', '/win')

    def test_check_illegal_port_names(self):
        # FIXME: Check that, for now, these are illegal port names.
        # Eventually we should be able to do the right thing here.
        with self.assertRaises(AssertionError):
            linux.LinuxPort(MockSystemHost(), port_name='linux-x86')

    def test_operating_system(self):
        self.assertEqual('linux', self.make_port().operating_system())

    def test_driver_name_option(self):
        # pylint: disable=protected-access
        self.assertTrue(self.make_port()._path_to_driver().endswith('content_shell'))
        port = self.make_port(options=optparse.Values({'driver_name': 'OtherDriver'}))
        self.assertTrue(port._path_to_driver().endswith('OtherDriver'))

    def test_path_to_image_diff(self):
        # pylint: disable=protected-access
        self.assertEqual(self.make_port()._path_to_image_diff(), '/mock-checkout/out/Release/image_diff')

    def test_dummy_home_dir_is_created_and_cleaned_up(self):
        def run_command_fake(args):
            if args[0:2] == ['xdpyinfo', '-display']:
                return 1
            return 0

        port = self.make_port()
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)
        port.host.environ['HOME'] = '/home/user'
        port.host.filesystem.files['/home/user/.Xauthority'] = ''

        # Set up the test run; the temporary home directory should be set up.
        port.setup_test_run()
        temp_home_dir = port.host.environ['HOME']
        self.assertNotEqual(temp_home_dir, '/home/user')
        self.assertTrue(port.host.filesystem.isdir(temp_home_dir))
        self.assertTrue(port.host.filesystem.isfile(port.host.filesystem.join(temp_home_dir, '.Xauthority')))

        # Clean up; HOME should be reset and the temp dir should be cleaned up.
        port.clean_up_test_run()
        self.assertEqual(port.host.environ['HOME'], '/home/user')
        self.assertFalse(port.host.filesystem.exists(temp_home_dir))

    def test_setup_test_run_starts_xvfb(self):
        def run_command_fake(args):
            if args[0:2] == ['xdpyinfo', '-display']:
                return 1
            return 0

        port = self.make_port()
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)

        self.assertIsNone(port.setup_test_run())
        self.assertEqual(
            port.host.executive.calls,
            [
                ['xdpyinfo', '-display', ':99'],
                ['Xvfb', ':99', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96'],
                ['xdpyinfo'],
            ])
        env = port.setup_environ_for_server()
        self.assertEqual(env['DISPLAY'], ':99')

    def test_setup_test_run_starts_xvfb_clears_tmpdir(self):
        def run_command_fake(args):
            if args[0:2] == ['xdpyinfo', '-display']:
                return 1
            return 0

        port = self.make_port()
        port.host.environ['TMPDIR'] = '/foo/bar'
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)

        self.assertIsNone(port.setup_test_run())
        self.assertEqual(
            port.host.executive.calls,
            [
                ['xdpyinfo', '-display', ':99'],
                ['Xvfb', ':99', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96'],
                ['xdpyinfo'],
            ])
        self.assertEqual(port.host.executive.full_calls[1].kwargs['env'].get('TMPDIR'), '/tmp')
        env = port.setup_environ_for_server()
        self.assertEqual(env['DISPLAY'], ':99')

    def test_setup_test_runs_finds_free_display(self):
        def run_command_fake(args):
            if args == ['xdpyinfo', '-display', ':102']:
                return 1
            return 0

        port = self.make_port()
        port.host.filesystem.files['/tmp/.X99-lock'] = ''
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)

        self.assertIsNone(port.setup_test_run())
        self.assertEqual(
            port.host.executive.calls,
            [
                # Do not call `xdpyinfo -display :99` because the lock exists.
                ['xdpyinfo', '-display', ':100'],
                ['xdpyinfo', '-display', ':101'],
                ['xdpyinfo', '-display', ':102'],
                ['Xvfb', ':102', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96'],
                ['xdpyinfo'],
            ])
        env = port.setup_environ_for_server()
        self.assertEqual(env['DISPLAY'], ':102')

    def test_setup_test_runs_multiple_checks_when_slow_to_start(self):
        count = [0]

        def run_command_fake(args):
            if args[0:2] == ['xdpyinfo', '-display']:
                return 1
            # The variable `count` is a list rather than an int so that this
            # function can increment the value.
            if args == ['xdpyinfo'] and count[0] < 3:
                count[0] += 1
                return 1
            return 0

        port = self.make_port()
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)

        self.assertIsNone(port.setup_test_run())
        self.assertEqual(
            port.host.executive.calls,
            [
                ['xdpyinfo', '-display', ':99'],
                ['Xvfb', ':99', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96'],
                ['xdpyinfo'],
                ['xdpyinfo'],
                ['xdpyinfo'],
                ['xdpyinfo'],
            ])
        env = port.setup_environ_for_server()
        self.assertEqual(env['DISPLAY'], ':99')

    def test_setup_test_runs_eventually_times_out(self):
        def run_command_fake(args):
            if args[0] == 'xdpyinfo':
                return 1
            return 0

        host = MockSystemHost(os_name=self.os_name, os_version=self.os_version)
        port = self.make_port(host=host)
        port.host.executive = MockExecutive(run_command_fn=run_command_fake)
        self.set_logging_level(logging.DEBUG)

        self.assertEqual(port.setup_test_run(), SYS_DEPS_EXIT_STATUS)
        self.assertEqual(
            port.host.executive.calls,
            [
                ['xdpyinfo', '-display', ':99'],
                ['Xvfb', ':99', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96'],
            ] + [['xdpyinfo']] * 51)
        env = port.setup_environ_for_server()
        self.assertEqual(env['DISPLAY'], ':99')
        self.assertLog(
            ['DEBUG: Starting Xvfb with display ":99".\n'] +
            ['WARNING: xdpyinfo check failed with exit code 1 while starting Xvfb on ":99".\n'] * 51 +
            [
                'DEBUG: Killing Xvfb process pid 42.\n',
                'CRITICAL: Failed to start Xvfb on display ":99" (xvfb retcode: None).\n',
            ])

    def test_setup_test_runs_terminates_if_xvfb_proc_fails(self):
        def run_command_fake(args):
            if args[0] == 'xdpyinfo':
                return 1
            return 0

        host = MockSystemHost(os_name=self.os_name, os_version=self.os_version)
        port = self.make_port(host=host)
        # Xvfb is started via Executive.popen, which returns an object for the
        # process. Here we set up a fake process object that acts as if it has
        # exited with return code 1 immediately.
        proc = MockProcess(stdout='', stderr='', returncode=3)
        port.host.executive = MockExecutive(
            run_command_fn=run_command_fake, proc=proc)
        self.set_logging_level(logging.DEBUG)

        self.assertEqual(port.setup_test_run(), SYS_DEPS_EXIT_STATUS)
        self.assertEqual(
            port.host.executive.calls,
            [
                ['xdpyinfo', '-display', ':99'],
                ['Xvfb', ':99', '-screen', '0', '1280x800x24', '-ac', '-dpi', '96']
            ])
        self.assertLog([
            'DEBUG: Starting Xvfb with display ":99".\n',
            'CRITICAL: Failed to start Xvfb on display ":99" (xvfb retcode: 3).\n'
        ])
