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

from blinkpy.web_tests.port import mac
from blinkpy.web_tests.port import port_testcase


class MacPortTest(port_testcase.PortTestCase):
    os_name = 'mac'
    os_version = 'mac10.12'
    port_name = 'mac'
    full_port_name = 'mac-mac10.12'
    port_maker = mac.MacPort

    def assert_name(self, port_name, os_version_string, expected):
        port = self.make_port(os_version=os_version_string, port_name=port_name)
        self.assertEqual(expected, port.name())

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())

    def test_get_platform_tags(self):
        port = self.make_port()
        self.assertEqual(port.get_platform_tags(), {'mac', 'mac10.12', 'x86', 'release'})

    def test_driver_name_option(self):
        self.assertTrue(self.make_port()._path_to_driver().endswith('Content Shell'))
        port = self.make_port(options=optparse.Values(dict(driver_name='OtherDriver')))
        self.assertTrue(port._path_to_driver().endswith('OtherDriver'))  # pylint: disable=protected-access

    def test_path_to_image_diff(self):
        self.assertEqual(self.make_port()._path_to_image_diff(), '/mock-checkout/out/Release/image_diff')

    def test_path_to_apache_config_file(self):
        port = self.make_port()
        port._apache_version = lambda: '2.4'  # pylint: disable=protected-access
        self.assertEqual(
            port.path_to_apache_config_file(),
            '/mock-checkout/third_party/blink/tools/apache_config/apache2-httpd-2.4-php7.conf')
