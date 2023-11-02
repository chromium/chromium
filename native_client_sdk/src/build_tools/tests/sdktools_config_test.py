#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
import os
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)

import sdk_tools.config as config

class TestSdkToolsConfig(unittest.TestCase):
  def testInvalidSyntax(self):
    invalid_json = "# oops\n"
    cfg = config.Config()
    self.assertRaises(config.Error, lambda: cfg.LoadJson(invalid_json))

  def testEmptyConfig(self):
    """Test that empty config contains just empty sources list."""
    expected = '{\n  "sources": []\n}'
    cfg = config.Config()
    json_output = cfg.ToJson()
    self.assertEqual(json_output, expected)

  def testIntegerSetting(self):
    json_input = '{ "setting": 3 }'
    cfg = config.Config()
    cfg.LoadJson(json_input)
    self.assertEqual(cfg.setting, 3)

  def testReadWrite(self):
    json_input1 = '{\n  "sources": [], \n  "setting": 3\n}'
    json_input2 = '{\n  "setting": 3\n}'
    for json_input in (json_input1, json_input2):
      cfg = config.Config()
      cfg.LoadJson(json_input)
      json_output = cfg.ToJson()
      self.assertEqual(json_output, json_input1)

  def testAddSource(self):
    cfg = config.Config()
    cfg.AddSource('http://localhost/foo')
    json_output = cfg.ToJson()
    expected = '{\n  "sources": [\n    "http://localhost/foo"\n  ]\n}'
    self.assertEqual(json_output, expected)


if __name__ == '__main__':
  unittest.main()
