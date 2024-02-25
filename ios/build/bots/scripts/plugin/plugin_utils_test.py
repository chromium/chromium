#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for plugin_util.py."""

import os
import sys
import unittest

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import VIDEO_RECORDER_PLUGIN_OPTIONS
from test_plugins import VideoRecorderPlugin, FileCopyPlugin
import plugin_utils

TEST_DEVICE_ID = 'device id'
TEST_OUT_DIR = 'out/dir'


class UnitTest(unittest.TestCase):

  def test_get_video_plugin_from_args(self):
    plugins = plugin_utils.init_plugins_from_args(
        TEST_OUT_DIR,
        video_plugin_option=VIDEO_RECORDER_PLUGIN_OPTIONS.failed_only.name)
    self.assertIsInstance(plugins[0], VideoRecorderPlugin)

  def test_no_plugin_specified_from_args(self):
    plugins = plugin_utils.init_plugins_from_args(TEST_OUT_DIR)
    self.assertTrue(len(plugins) == 0)

  def test_get_clang_coverage_plugin_from_args(self):
    plugins = plugin_utils.init_plugins_from_args(
        TEST_OUT_DIR, use_clang_coverage=True)
    self.assertIsInstance(plugins[0], FileCopyPlugin)
    self.assertEqual(plugins[0].glob_pattern, 'data/*.profraw')


if __name__ == '__main__':
  unittest.main()
