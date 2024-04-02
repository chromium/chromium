# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import logging

from blinkpy.common.system.platform_info_mock import MockPlatformInfo
from blinkpy.web_tests.port import ios
from blinkpy.web_tests.port import port_testcase

_log = logging.getLogger(__name__)


class IOSPortTest(port_testcase.PortTestCase):
    port_name = 'ios'
    port_maker = ios.IOSPort

    def assert_name(self,
                    port_name,
                    os_version_string,
                    expected,
                    machine=None):
        port = self.make_port(os_version=os_version_string,
                              port_name=port_name,
                              machine=machine)
        self.assertEqual(expected, port.name())

    def test_operating_system(self):
        self.assertEqual('ios', self.make_port().operating_system())

    def test_get_platform_tags(self):
        port = self.make_port()
        self.assertEqual(port.get_platform_tags(),
                         {'ios', 'ios17-simulator', 'x86', 'release'})

    def test_versions(self):
        self.assert_name('ios', 'ios17', 'ios-ios17-simulator')

    def test_driver_name_option(self):
        self.assertTrue(
            self.make_port().path_to_driver().endswith('content_shell.app'))

    def test_path_to_image_diff(self):
        self.assertEqual(self.make_port()._path_to_image_diff(),
                         '/mock-checkout/out/Release/image_diff')

    # On the iOS port, command line flags are wrapped in a call
    # to 'iossim', so the base class testing logic doesn't apply.
    def test_driver_cmd_line(self):
        options = optparse.Values(
            dict(additional_driver_flag=['--foo=bar', '--foo=baz']))
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertEqual(cmd_line[0], '/mock-checkout/out/Release/iossim')
        self.assertTrue('-d' in cmd_line)
        self.assertEqual(cmd_line[cmd_line.index('-d') + 1], 'iPhone 13')

        # The iOS port adds additional flags to the '-c' option. So we check if
        # '--foo=bar|baz' are in the option.
        additional_flags_index = cmd_line.index('-c') + 1
        self.assertTrue('--foo=bar' in cmd_line[additional_flags_index])
        self.assertTrue('--foo=baz' in cmd_line[additional_flags_index])

    def test_path_to_apache_config_file(self):
        port = self.make_port()
        port._apache_version = lambda: '2.4'  # pylint: disable=protected-access
        self.assertEqual(
            port.path_to_apache_config_file(),
            '/mock-checkout/third_party/blink/tools/apache_config/apache2-httpd-2.4-php7.conf'
        )

    def test_used_expectations_files(self):
        port = self.make_port()
        self.assertEqual(list(port.used_expectations_files()), [
            port.path_to_generic_test_expectations_file(),
            port.host.filesystem.join(port.web_tests_dir(), 'NeverFixTests'),
            port.host.filesystem.join(port.web_tests_dir(),
                                      'StaleTestExpectations'),
            port.host.filesystem.join(port.web_tests_dir(), 'SlowTests'),
            port.host.filesystem.join(port.web_tests_dir(),
                                      'IOSTestExpectations'),
        ])
