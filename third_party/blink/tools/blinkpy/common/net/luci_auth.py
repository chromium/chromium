# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An interface to luci-auth.

The main usage is to get the OAuth access token for the service account on LUCI.
"""

from blinkpy.common.path_finder import PathFinder


class LuciAuth(object):
    def __init__(self, host):
        self._host = host
        self._finder = PathFinder(host.filesystem)

    @property
    def _luci_auth_executable(self):
        luci_auth_bin = ('luci-auth.bat'
                         if self._host.platform.is_win() else 'luci-auth')
        depot_tools_base = self._finder.depot_tools_base()
        if depot_tools_base:
            return self._host.filesystem.join(depot_tools_base, luci_auth_bin)
        # If `//third_party/depot_tools` is not found, try using `luci-auth` in
        # the current `PATH`.
        return luci_auth_bin

    def get_access_token(self):
        # ScriptError will be raised if luci-auth fails.
        output = self._host.executive.run_command(
            [self._luci_auth_executable, 'token'], debug_logging=False)
        return output.strip()
