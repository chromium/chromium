# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os
import subprocess
import sys
import mock

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import PLUGIN_SERVICE_ADDRESS, PLUGIN_PROXY_SERVICE_PORT, REMOTE_PLUGIN_PROXY_PORT
from usbmuxd_proxy import PluginServiceProxyWrapper


class PluginServiceProxyWrapperTest(unittest.TestCase):

  def setUp(self):
    self.plugin_service_proxy = PluginServiceProxyWrapper(
        PLUGIN_SERVICE_ADDRESS, PLUGIN_PROXY_SERVICE_PORT,
        REMOTE_PLUGIN_PROXY_PORT)

  def test_start_iproxy(self):
    with mock.patch.object(subprocess, 'Popen') as mock_popen:
      mock_popen.return_value.returncode = 0
      self.plugin_service_proxy.start_iproxy()
      mock_popen.assert_called_once_with(
          ['iproxy', PLUGIN_PROXY_SERVICE_PORT, REMOTE_PLUGIN_PROXY_PORT])


if __name__ == '__main__':
  unittest.main()
