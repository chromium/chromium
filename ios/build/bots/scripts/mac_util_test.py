#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for mac_util_test.py."""

import json
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


class TestKillUsbMuxd(test_runner_test.TestCase):

  @patch('subprocess.check_call')
  def test_run_kill_usbmuxd_succeeds(self, mock_check_call):
    mock_check_call.return_value = MagicMock()
    mac_util.kill_usbmuxd()
    mock_check_call.assert_called_with(
        ['sudo', '/usr/bin/killall', '-v', 'usbmuxd'])


class TestStopUsbMuxd(test_runner_test.TestCase):

  @patch('subprocess.check_call')
  def test_run_stop_usbmuxd_succeeds(self, mock_check_call):
    mock_check_call.return_value = MagicMock()
    mac_util.stop_usbmuxd()
    mock_check_call.assert_called_with(
        ['sudo', '/bin/launchctl', 'stop', 'com.apple.usbmuxd'])


class TestPlistAsDict(test_runner_test.TestCase):

  @patch('subprocess.check_output')
  def test_complex_dict(self, mock_check_output: MagicMock):

    mock_return = '{"key1":"val1","key2":{"key3":[1,2,3],"key4":false}}'.encode(
        'utf-8')
    mock_check_output.return_value = mock_return

    plist, error = mac_util.plist_as_dict('PATH')

    expected_plist = {
        'key1': 'val1',
        'key2': {
            'key3': [1, 2, 3],
            'key4': False
        }
    }
    self.assertIsNone(error)
    self.assertDictEqual(plist, expected_plist)

  @patch('subprocess.check_output')
  def test_subprocess_call(self, mock_check_output: MagicMock):
    mock_check_output.return_value = '{}'.encode('utf-8')
    mac_util.plist_as_dict('PATH')
    mock_check_output.assert_called_once_with(
        ['plutil', '-convert', 'json', '-o', '-', 'PATH'])

  @patch('subprocess.check_output')
  def test_subprocess_raises_error(self, mock_check_output: MagicMock):
    mock_check_output.side_effect = subprocess.CalledProcessError(
        cmd='cmd', returncode=1)
    plist, error = mac_util.plist_as_dict('PATH')
    self.assertIsNone(plist)
    self.assertIsInstance(error, subprocess.CalledProcessError)

  @patch('subprocess.check_output')
  def test_json_decode_raises_error(self, mock_check_output: MagicMock):
    mock_json_loads = MagicMock()
    mock_json_loads.side_effect = json.JSONDecodeError('message', 'doc', 0)
    self.mock(json, 'loads', mock_json_loads)
    plist, error = mac_util.plist_as_dict('PATH')
    self.assertIsNone(plist)
    self.assertIsInstance(error, json.JSONDecodeError)


if __name__ == '__main__':
  unittest.main()
