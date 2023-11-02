#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(SCRIPT_DIR, 'data')
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(PARENT_DIR)))

sys.path.append(PARENT_DIR)

import sel_ldr
import mock


class TestSelLdr(unittest.TestCase):
  def testRequiresArg(self):
    with mock.patch('sys.stderr'):
      self.assertRaises(SystemExit, sel_ldr.main, [])

  def testUsesHelper(self):
    with mock.patch('subprocess.call') as call:
      with mock.patch('os.path.exists'):
        with mock.patch('os.path.isfile'):
          with mock.patch('create_nmf.ParseElfHeader') as parse_header:
            parse_header.return_value = ('x8-64', False)
            with mock.patch('getos.GetPlatform') as get_platform:
              # assert that when we are running on linux
              # the helper is used.
              get_platform.return_value = 'linux'
              sel_ldr.main(['foo.nexe'])
              parse_header.assert_called_once_with('foo.nexe')
              self.assertEqual(call.call_count, 1)
              cmd = call.call_args[0][0]
              self.assertTrue('helper_bootstrap' in cmd[0])

              # assert that when not running on linux the
              # helper is not used.
              get_platform.reset_mock()
              parse_header.reset_mock()
              call.reset_mock()
              get_platform.return_value = 'win'
              sel_ldr.main(['foo.nexe'])
              parse_header.assert_called_once_with('foo.nexe')
              self.assertEqual(call.call_count, 1)
              cmd = call.call_args[0][0]
              self.assertTrue('helper_bootstrap' not in cmd[0])


if __name__ == '__main__':
  unittest.main()
