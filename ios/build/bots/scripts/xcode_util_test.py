# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for xcode_util.py."""

import logging
import mock
import unittest

import test_runner_errors
import test_runner_test
import xcode_util


class XcodeUtilTest(test_runner_test.TestCase):
  """Test class for xcode_util functions."""

  def setUp(self):
    super(XcodeUtilTest, self).setUp()


class InstallTest(XcodeUtilTest):
  """Test class for xcode_util.install function."""

  def setUp(self):
    super(InstallTest, self).setUp()
    self.mac_toolchain = 'mac_toolchain'
    self.xcode_build_version = 'TestXcodeVersion'
    self.xcode_app_path = 'test/path/Xcode.app'
    self.runtime_cache_folder = 'test/path/Runtime'
    self.ios_version = '14.4'

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime', autospec=True)
  @mock.patch('xcode_util._install_xcode', autospec=True)
  def test_legacy_mactoolchain_new_xcode(self, mock_install_xcode,
                                         mock_install_runtime,
                                         mock_move_runtime):
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: False)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: False)

    with self.assertRaises(test_runner_errors.XcodeMacToolchainMismatchError):
      is_legacy_xcode = xcode_util.install(self.mac_toolchain,
                                           self.xcode_build_version,
                                           self.xcode_app_path)
      self.assertTrue(is_legacy_xcode, 'install should return true')

    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', False)
    self.assertFalse(mock_install_runtime.called,
                     '_install_runtime shouldn\'t be called')
    self.assertFalse(mock_move_runtime.called,
                     'move_runtime shouldn\'t be called')

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime', autospec=True)
  @mock.patch('xcode_util._install_xcode', autospec=True)
  def test_legacy_mactoolchain_legacy_xcode(self, mock_install_xcode,
                                            mock_install_runtime,
                                            mock_move_runtime):
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: False)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: True)

    is_legacy_xcode = xcode_util.install(self.mac_toolchain,
                                         self.xcode_build_version,
                                         self.xcode_app_path)

    self.assertTrue(is_legacy_xcode, 'install_should return true')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', False)
    self.assertFalse(mock_install_runtime.called,
                     '_install_runtime shouldn\'t be called')
    self.assertFalse(mock_move_runtime.called,
                     'move_runtime shouldn\'t be called')

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime', autospec=True)
  @mock.patch('xcode_util._install_xcode', autospec=True)
  def test_new_mactoolchain_legacy_xcode(self, mock_install_xcode,
                                         mock_install_runtime,
                                         mock_move_runtime):
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: True)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: True)

    is_legacy_xcode = xcode_util.install(self.mac_toolchain,
                                         self.xcode_build_version,
                                         self.xcode_app_path)

    self.assertTrue(is_legacy_xcode, 'install should return true')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', True)
    self.assertFalse(mock_install_runtime.called,
                     '_install_runtime shouldn\'t be called')
    self.assertFalse(mock_move_runtime.called,
                     'move_runtime shouldn\'t be called')

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime')
  @mock.patch('xcode_util._install_xcode')
  def test_new_mactoolchain_new_xcode(self, mock_install_xcode,
                                      mock_install_runtime, mock_move_runtime):
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: True)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: False)

    is_legacy_xcode = xcode_util.install(
        self.mac_toolchain,
        self.xcode_build_version,
        self.xcode_app_path,
        runtime_cache_folder=self.runtime_cache_folder,
        ios_version=self.ios_version)

    self.assertFalse(is_legacy_xcode, 'install should return False')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', True)
    mock_install_runtime.assert_called_with('mac_toolchain',
                                            'test/path/Runtime',
                                            'TestXcodeVersion', '14.4')
    mock_move_runtime.assert_called_with('test/path/Runtime',
                                         'test/path/Xcode.app', True)


class HelperFunctionTests(XcodeUtilTest):
  """Test class for xcode_util misc util functions."""

  def setUp(self):
    super(HelperFunctionTests, self).setUp()
    self.xcode_runtime_rel_path = (
        'Contents/Developer/'
        'Platforms/iPhoneOS.platform/Library/Developer/'
        'CoreSimulator/Profiles/Runtimes/iOS.simruntime')

  @mock.patch('subprocess.check_output', autospec=True)
  def test_using_new_mac_toolchain(self, mock_check_output):
    mock_check_output.return_value = """
Mac OS / iOS toolchain management

Usage:  mac_toolchain [command] [arguments]

Commands:
  help             prints help about a command
  install          Installs Xcode.
  upload           Uploads Xcode CIPD packages.
  package          Create CIPD packages locally.
  install-runtime  Installs Runtime.


Use "mac_toolchain help [command]" for more information about a command."""
    self.assertTrue(xcode_util._using_new_mac_toolchain('mac_toolchain'))

  @mock.patch('subprocess.check_output', autospec=True)
  def test_using_new_legacy_toolchain(self, mock_check_output):
    mock_check_output.return_value = """
Mac OS / iOS toolchain management

Usage:  mac_toolchain [command] [arguments]

Commands:
  help             prints help about a command
  install          Installs Xcode.
  upload           Uploads Xcode CIPD packages.
  package          Create CIPD packages locally.


Use "mac_toolchain help [command]" for more information about a command."""
    self.assertFalse(xcode_util._using_new_mac_toolchain('mac_toolchain'))

  @mock.patch('os.path.exists', autospec=True, return_value=True)
  def test_is_legacy_xcode_package_legacy(self, mock_exists):
    self.assertTrue(xcode_util._is_legacy_xcode_package('test/path/Xcode.app'))
    mock_exists.assert_called_with('test/path/Xcode.app/' +
                                   self.xcode_runtime_rel_path)

  @mock.patch('os.path.exists', autospec=True, return_value=False)
  def test_is_legacy_xcode_package_not_legacy(self, mock_exists):
    self.assertFalse(xcode_util._is_legacy_xcode_package('test/path/Xcode.app'))
    mock_exists.assert_called_with('test/path/Xcode.app/' +
                                   self.xcode_runtime_rel_path)


class MoveRuntimeTests(XcodeUtilTest):
  """Test class for xcode_util.move_runtime function."""

  def setUp(self):
    super(MoveRuntimeTests, self).setUp()
    self.runtime_cache_folder = 'test/path/Runtime'
    self.xcode_app_path = 'test/path/Xcode.app'

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_into_xcode(self, mock_glob, mock_exists, mock_rmtree,
                                   mock_move):
    mock_glob.return_value = ['test/path/Runtime/iOS.simruntime']
    mock_exists.return_value = False

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            True)

    xcode_runtime_path = ('test/path/Xcode.app/Contents/Developer/'
                          'Platforms/iPhoneOS.platform/Library/Developer/'
                          'CoreSimulator/Profiles/Runtimes/iOS.simruntime')
    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    mock_exists.assert_called_with(xcode_runtime_path)
    self.assertFalse(mock_rmtree.called)
    mock_move.assert_called_with('test/path/Runtime/iOS.simruntime',
                                 xcode_runtime_path)

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_outside_xcode(self, mock_glob, mock_exists, mock_rmtree,
                                      mock_move):
    xcode_runtime_folder = ('test/path/Xcode.app/Contents/Developer/'
                            'Platforms/iPhoneOS.platform/Library/Developer/'
                            'CoreSimulator/Profiles/Runtimes')
    mock_glob.return_value = [xcode_runtime_folder + '/iOS.simruntime']
    mock_exists.return_value = False

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            False)

    mock_glob.assert_called_with(xcode_runtime_folder + '/*.simruntime')
    mock_exists.assert_called_with('test/path/Runtime/iOS.simruntime')
    self.assertFalse(mock_rmtree.called)
    mock_move.assert_called_with(xcode_runtime_folder + '/iOS.simruntime',
                                 'test/path/Runtime/iOS.simruntime')

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_multiple_in_src(self, mock_glob, mock_exists,
                                        mock_rmtree, mock_move):
    mock_glob.return_value = [
        'test/path/Runtime/iOS.simruntime',
        'test/path/Runtime/iOS 13.4.simruntime'
    ]

    with self.assertRaises(test_runner_errors.IOSRuntimeHandlingError):
      xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                              True)
    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    self.assertFalse(mock_exists.called)
    self.assertFalse(mock_rmtree.called)
    self.assertFalse(mock_move.called)

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_remove_from_dst(self, mock_glob, mock_exists,
                                        mock_rmtree, mock_move):
    mock_glob.return_value = ['test/path/Runtime/iOS.simruntime']
    mock_exists.return_value = True

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            True)

    xcode_runtime_path = ('test/path/Xcode.app/Contents/Developer/'
                          'Platforms/iPhoneOS.platform/Library/Developer/'
                          'CoreSimulator/Profiles/Runtimes/iOS.simruntime')
    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    mock_exists.assert_called_with(xcode_runtime_path)
    mock_rmtree.assert_called_with(xcode_runtime_path)
    mock_move.assert_called_with('test/path/Runtime/iOS.simruntime',
                                 xcode_runtime_path)


class MacToolchainInvocationTests(XcodeUtilTest):
  """Test class for xcode_util functions invoking mac_toolchain."""

  def setUp(self):
    super(MacToolchainInvocationTests, self).setUp()
    self.mac_toolchain = 'mac_toolchain'
    self.xcode_build_version = 'TestXcodeVersion'
    self.xcode_app_path = 'test/path/Xcode.app'
    self.runtime_cache_folder = 'test/path/Runtime'
    self.ios_version = '14.4'

  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_runtime(self, mock_check_output):
    xcode_util._install_runtime(self.mac_toolchain, self.runtime_cache_folder,
                                self.xcode_build_version, self.ios_version)
    mock_check_output.assert_called_with([
        'mac_toolchain', 'install-runtime', '-xcode-version',
        'testxcodeversion', '-runtime-version', 'ios-14-4', '-output-dir',
        'test/path/Runtime'
    ],
                                         stderr=-2)

  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_xcode_legacy_mac_toolchain(self, mock_check_output):
    using_new_mac_toolchain = False
    xcode_util._install_xcode(self.mac_toolchain, self.xcode_build_version,
                              self.xcode_app_path, using_new_mac_toolchain)
    mock_check_output.assert_called_with([
        'mac_toolchain', 'install', '-kind', 'ios', '-xcode-version',
        'testxcodeversion', '-output-dir', 'test/path/Xcode.app'
    ],
                                         stderr=-2)

  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_xcode_new_mac_toolchain(self, mock_check_output):
    using_new_mac_toolchain = True
    xcode_util._install_xcode(self.mac_toolchain, self.xcode_build_version,
                              self.xcode_app_path, using_new_mac_toolchain)
    mock_check_output.assert_called_with([
        'mac_toolchain', 'install', '-kind', 'ios', '-xcode-version',
        'testxcodeversion', '-output-dir', 'test/path/Xcode.app',
        '-with-runtime=False'
    ],
                                         stderr=-2)


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
