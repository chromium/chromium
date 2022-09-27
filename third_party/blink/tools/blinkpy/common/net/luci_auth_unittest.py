# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.luci_auth import LuciAuth


class LuciAuthTest(unittest.TestCase):
    def test_run_on_linux(self):
        host = MockHost(os_name='linux')
        host.filesystem.maybe_make_directory(
            '/mock-checkout/third_party/depot_tools')

        luci_auth = LuciAuth(host)
        luci_auth.get_access_token()
        self.assertListEqual(
            host.executive.calls,
            [['/mock-checkout/third_party/depot_tools/luci-auth', 'token']])

    def test_run_on_windows(self):
        host = MockHost(os_name='win')
        host.filesystem.maybe_make_directory(
            '/mock-checkout/third_party/depot_tools')

        luci_auth = LuciAuth(host)
        luci_auth.get_access_token()
        self.assertEqual(
            host.executive.calls,
            [['/mock-checkout/third_party/depot_tools/luci-auth.bat', 'token']
             ])
