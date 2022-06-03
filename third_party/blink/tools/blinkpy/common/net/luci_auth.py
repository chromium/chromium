# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An interface to luci-auth.

The main usage is to get the OAuth access token for the service account on LUCI.
"""

from blinkpy.common.path_finder import PathFinder


class LuciAuth(object):
    def __init__(self, host):
        self._host = host
        finder = PathFinder(host.filesystem)
        luci_auth_bin = ('luci-auth.bat'
                         if host.platform.is_win() else 'luci-auth')
        self._luci_auth_path = host.filesystem.join(finder.depot_tools_base(),
                                                    luci_auth_bin)

    def get_access_token(self):
        # ScriptError will be raised if luci-auth fails.
        output = self._host.executive.run_command(
            [self._luci_auth_path, 'token'])
        return output.strip()
