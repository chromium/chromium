#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for mac_util_test.py."""

import subprocess
import unittest
from unittest.mock import patch, MagicMock
import distutils.version

import mac_util
import test_runner_test


class TestIsMacOS13OrHigher(test_runner_test.TestCase):

  @patch('mac_util.version')
  def test_is_macos_13_or_higher_with_version_12(self, mock_version):
    mock_version.return_value = '12.0.0'
    self.assertFalse(mac_util.is_macos_13_or_higher())

  @patch('mac_util.version')
  def test_is_macos_13_or_higher_with_version_13(self, mock_version):
    mock_version.return_value = '13.0.0'
    self.assertTrue(mac_util.is_macos_13_or_higher())

  @patch('mac_util.version')
  def test_is_macos_13_or_higher_with_version_14(self, mock_version):
    mock_version.return_value = '14.0.0'
    self.assertTrue(mac_util.is_macos_13_or_higher())


class TestRunCodesignCheck(test_runner_test.TestCase):

  @patch('subprocess.check_call')
  def test_run_codesign_check_succeeds(self, mock_check_call):
    mock_check_call.return_value = MagicMock()
    success, message = mac_util.run_codesign_check("testdir/Xcode.app")
    self.assertEqual(success, True)
    self.assertEqual(message, None)

  @patch('subprocess.check_call')
  def test_run_codesign_check_fails(self, mock_check_call):
    error = subprocess.CalledProcessError(1, "codesign check return error")
    mock_check_call.side_effect = error
    success, return_error = mac_util.run_codesign_check("testdir/Xcode.app")
    self.assertEqual(success, False)
    self.assertEqual(return_error, error)


if __name__ == '__main__':
  unittest.main()
