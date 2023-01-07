#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(BUILD_TOOLS_DIR)))

sys.path.append(BUILD_TOOLS_DIR)

import mock
import test_projects

class TestMain(unittest.TestCase):
  """Tests for main() entry point of the script."""

  def testInvalidArgs(self):
    with mock.patch('sys.stderr'):
      with self.assertRaises(SystemExit):
        test_projects.main(['--foo'])


if __name__ == '__main__':
  unittest.main()
