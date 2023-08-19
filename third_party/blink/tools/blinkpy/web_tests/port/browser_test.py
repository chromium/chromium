# Copyright (C) 2014 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
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

from blinkpy.common import exit_codes
from blinkpy.web_tests.port import browser_test_driver
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import mac
from blinkpy.web_tests.port import win


def get_port_class_name(port_name):
    if 'linux' in port_name:
        return 'BrowserTestLinuxPort'
    elif 'mac' in port_name:
        return 'BrowserTestMacPort'
    elif 'win' in port_name:
        return 'BrowserTestWinPort'
    return None


class BrowserTestPortOverrides(object):
    """Set of overrides that every browser test platform port should have. This
    class should not be instantiated as certain functions depend on base. Port
    to work.
    """

    def _driver_class(self):
        return browser_test_driver.BrowserTestDriver

    def web_tests_dir(self):
        """Overridden function from the base port class. Redirects everything
        to src/chrome/test/data/printing/layout_tests.
        """
        # pylint: disable=no-member
        return self._path_from_chromium_base('chrome', 'test', 'data',
                                             'printing', 'layout_tests')

    def check_sys_deps(self):
        """This function is meant to be a no-op since we don't want to actually
        check for system dependencies.
        """
        return exit_codes.OK_EXIT_STATUS

    def driver_name(self):
        return 'browser_tests'

    def _default_timeout_ms(self):
        return 10000

    def virtual_test_suites(self):
        return []


class BrowserTestLinuxPort(BrowserTestPortOverrides, linux.LinuxPort):
    pass


class BrowserTestMacPort(BrowserTestPortOverrides, mac.MacPort):
    def path_to_driver(self, target=None):
        return self.build_path(self.driver_name(), target=target)

    def _default_timeout_ms(self):
        return 20000


class BrowserTestWinPort(BrowserTestPortOverrides, win.WinPort):
    def _default_timeout_ms(self):
        return 20000
