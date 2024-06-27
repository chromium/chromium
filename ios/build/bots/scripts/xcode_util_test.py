#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for xcode_util.py."""

import logging
import mock
import os
import unittest

import test_runner_errors
import test_runner_test
import iossim_util
import xcode_util


_XCODEBUILD_VERSION_OUTPUT_12 = b"""Xcode 12.4
Build version 12D4e
"""
_XCODEBUILD_VERSION_OUTPUT_13 = b"""Xcode 13.0
Build version 13A5155e
"""
_XCODEBUILD_VERSION_OUTPUT_15 = b"""Xcode 15.0
Build version 15A5209g
"""

_XCODEBUILD_VERSION_OUTPUT_16 = b"""Xcode 16.0
Build version 16A5171c
"""

ADD_SIMULATOR_RUNTIME_OUTPUT = 'ramdomid (iOS 15.0)'

RUNTIME_15_0 = {
    "build": "20C52",
    "deletable": True,
    "identifier": "F58C93A6-E6F2-4AC3-BBED-CA9195B186D3",
    "kind": "Disk Image",
    "signatureState": "Verified",
    "sizeBytes": 6407069874,
    "state": "Ready",
    "version": "15.0"
}


class XcodeUtilTest(test_runner_test.TestCase):
  """Test class for xcode_util functions."""

  def setUp(self):
    super(XcodeUtilTest, self).setUp()

  @mock.patch(
      'subprocess.check_output', return_value=_XCODEBUILD_VERSION_OUTPUT_13)
  def test_version(self, _):
    """Tests xcode_util.version()"""
    version, build_version = xcode_util.version()
    self.assertEqual(version, '13.0')
    self.assertEqual(build_version, '13a5155e')

  @mock.patch(
      'subprocess.check_output', return_value=_XCODEBUILD_VERSION_OUTPUT_12)
  def test_using_xcode_12(self, _):
    """Tests xcode_util.using_xcode_11_or_higher"""
    self.assertTrue(xcode_util.using_xcode_11_or_higher())
    self.assertFalse(xcode_util.using_xcode_13_or_higher())
    self.assertFalse(xcode_util.using_xcode_15_or_higher())
    self.assertFalse(xcode_util.using_xcode_16_or_higher())

  @mock.patch(
      'subprocess.check_output', return_value=_XCODEBUILD_VERSION_OUTPUT_13)
  def test_using_xcode_13(self, _):
    """Tests xcode_util.using_xcode_13_or_higher"""
    self.assertTrue(xcode_util.using_xcode_11_or_higher())
    self.assertTrue(xcode_util.using_xcode_13_or_higher())
    self.assertFalse(xcode_util.using_xcode_15_or_higher())
    self.assertFalse(xcode_util.using_xcode_16_or_higher())

  @mock.patch(
      'subprocess.check_output', return_value=_XCODEBUILD_VERSION_OUTPUT_15)
  def test_using_xcode_15(self, _):
    """Tests xcode_util.using_xcode_15_or_higher"""
    self.assertTrue(xcode_util.using_xcode_11_or_higher())
    self.assertTrue(xcode_util.using_xcode_13_or_higher())
    self.assertTrue(xcode_util.using_xcode_15_or_higher())
    self.assertFalse(xcode_util.using_xcode_16_or_higher())

  @mock.patch(
      'subprocess.check_output', return_value=_XCODEBUILD_VERSION_OUTPUT_16)
  def test_using_xcode_16(self, _):
    """Tests xcode_util.using_xcode_16_or_higher"""
    self.assertTrue(xcode_util.using_xcode_11_or_higher())
    self.assertTrue(xcode_util.using_xcode_13_or_higher())
    self.assertTrue(xcode_util.using_xcode_15_or_higher())
    self.assertTrue(xcode_util.using_xcode_16_or_higher())


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
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_mactoolchain_new_xcode(self, mock_macos_13_or_higher,
                                         mock_install_xcode,
                                         mock_install_runtime,
                                         mock_move_runtime):
    mock_macos_13_or_higher.return_value = False
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
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_mactoolchain_legacy_xcode(self, mock_macos_13_or_higher,
                                            mock_install_xcode,
                                            mock_install_runtime,
                                            mock_move_runtime):
    mock_macos_13_or_higher.return_value = False
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
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_new_mactoolchain_legacy_xcode(self, mock_macos_13_or_higher,
                                         mock_install_xcode,
                                         mock_install_runtime,
                                         mock_move_runtime):
    mock_macos_13_or_higher.return_value = False
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
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_new_mactoolchain_new_xcode(self, mock_macos_13_or_higher,
                                      mock_install_xcode, mock_install_runtime,
                                      mock_move_runtime):
    mock_macos_13_or_higher.return_value = False
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

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime')
  @mock.patch('xcode_util._install_xcode')
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_new_mactoolchain_new_xcode_no_runtime(self, mock_macos_13_or_higher,
                                                 mock_install_xcode,
                                                 mock_install_runtime,
                                                 mock_move_runtime):
    mock_macos_13_or_higher.return_value = False
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: True)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: False)

    is_legacy_xcode = xcode_util.install(
        self.mac_toolchain,
        self.xcode_build_version,
        self.xcode_app_path,
        runtime_cache_folder=None,
        ios_version=None)

    self.assertFalse(is_legacy_xcode, 'install should return False')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', True)
    self.assertFalse(mock_install_runtime.called)
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime')
  @mock.patch('xcode_util._install_xcode')
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def test_new_mactoolchain_new_xcode_macos13_good_xcode_cache(
      self, mock_os_path_exists, mock_macos_13_or_higher, mock_install_xcode,
      mock_install_runtime, mock_move_runtime):
    mock_macos_13_or_higher.return_value = True
    mock_os_path_exists.return_value = False
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: True)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: False)

    is_legacy_xcode = xcode_util.install(
        self.mac_toolchain,
        self.xcode_build_version,
        self.xcode_app_path,
        runtime_cache_folder=None,
        ios_version=None)

    self.assertTrue(is_legacy_xcode, 'install should return True')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', True)
    self.assertFalse(mock_install_runtime.called)
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util._install_runtime')
  @mock.patch('xcode_util._install_xcode')
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.mkdir', autospec=True)
  def test_new_mactoolchain_new_xcode_macos13_codesign_failed(
      self, mock_mkdir, mock_rmtree, mock_os_path_exists,
      mock_macos_13_or_higher, mock_install_xcode, mock_install_runtime,
      mock_move_runtime):
    mock_macos_13_or_higher.return_value = True
    mock_os_path_exists.return_value = True
    self.mock(xcode_util, '_using_new_mac_toolchain', lambda cmd: True)
    self.mock(xcode_util, '_is_legacy_xcode_package', lambda path: False)

    is_legacy_xcode = xcode_util.install(
        self.mac_toolchain,
        self.xcode_build_version,
        self.xcode_app_path,
        runtime_cache_folder=None,
        ios_version=None)

    mock_rmtree.assert_called_with(self.xcode_app_path)
    mock_mkdir.assert_called_with(self.xcode_app_path)
    self.assertTrue(is_legacy_xcode, 'install should return True')
    mock_install_xcode.assert_called_with('mac_toolchain', 'TestXcodeVersion',
                                          'test/path/Xcode.app', True)
    self.assertFalse(mock_install_runtime.called)
    self.assertFalse(mock_move_runtime.called)

  def test_install_runtime_dmg_with_builtin_runtime(self):
    with mock.patch('xcode_util.is_runtime_builtin', return_value=True):
      with mock.patch('iossim_util.delete_simulator_runtime_and_wait'
                     ) as mock_delete_simulator_runtime_and_wait:
        with mock.patch(
            'xcode_util._install_runtime_dmg') as mock__install_runtime_dmg:
          with mock.patch('iossim_util.add_simulator_runtime'
                         ) as mock_add_simulator_runtime:
            with mock.patch('iossim_util.override_default_iphonesim_runtime'
                           ) as mock_override_default_iphonesim_runtime:
              result = xcode_util.install_runtime_dmg(
                  mac_toolchain='mac_toolchain',
                  runtime_cache_folder='/path/to/runtime_cache_folder',
                  ios_version='15.0',
                  xcode_build_version='14a123')

    self.assertFalse(mock_delete_simulator_runtime_and_wait.called)
    self.assertFalse(mock__install_runtime_dmg.called)
    self.assertFalse(mock_add_simulator_runtime.called)
    self.assertFalse(mock_override_default_iphonesim_runtime.called)

  @mock.patch('xcode_util.using_xcode_16_or_higher', return_value=False)
  def test_install_runtime_dmg_with_non_builtin_runtime(self, _):
    with mock.patch('xcode_util.is_runtime_builtin', return_value=False):
      with mock.patch(
          'iossim_util.delete_least_recently_used_simulator_runtimes'
      ) as mock_delete_least_recently_used_simulator_runtimes:
        with mock.patch(
            'xcode_util.get_latest_runtime_build_cipd',
            return_value='20C52') as mock_get_latest_runtime_build_cipd:
          with mock.patch(
              'iossim_util.get_simulator_runtime_info_by_build',
              return_value=None) as mock_get_simulator_runtime_info_by_build:
            with mock.patch(
                'xcode_util._install_runtime_dmg') as mock__install_runtime_dmg:
              with mock.patch(
                  'iossim_util.add_simulator_runtime',
                  return_value=ADD_SIMULATOR_RUNTIME_OUTPUT
              ) as mock_add_simulator_runtime:
                with mock.patch(
                    'xcode_util.get_runtime_dmg_name',
                    return_value='/path/to/runtime_cache_folder/test.dmg'):
                  with mock.patch(
                      'iossim_util.override_default_iphonesim_runtime'
                  ) as mock_override_default_iphonesim_runtime:
                    with mock.patch('os.environ.get', return_value=True):
                      result = xcode_util.install_runtime_dmg(
                          mac_toolchain='mac_toolchain',
                          runtime_cache_folder='/path/to/runtime_cache_folder',
                          ios_version='15.0',
                          xcode_build_version='15a123')

    mock_delete_least_recently_used_simulator_runtimes.assert_called_once_with()
    mock_get_simulator_runtime_info_by_build.assert_called_once_with('20C52')
    mock__install_runtime_dmg.assert_called_once_with(
        'mac_toolchain', '/path/to/runtime_cache_folder', '15.0', '15a123')
    mock_add_simulator_runtime.assert_called_once_with(
        '/path/to/runtime_cache_folder/test.dmg')
    mock_override_default_iphonesim_runtime.assert_called_once_with(
        ADD_SIMULATOR_RUNTIME_OUTPUT, '15.0')

  @mock.patch('xcode_util.using_xcode_16_or_higher', return_value=False)
  def test_install_runtime_dmg_already_exists(self, _):
    with mock.patch('xcode_util.is_runtime_builtin', return_value=False):
      with mock.patch(
          'iossim_util.delete_least_recently_used_simulator_runtimes'
      ) as mock_delete_least_recently_used_simulator_runtimes:
        with mock.patch(
            'xcode_util.get_latest_runtime_build_cipd',
            return_value='20C52') as mock_get_latest_runtime_build_cipd:
          with mock.patch(
              'iossim_util.get_simulator_runtime_info_by_build',
              return_value=RUNTIME_15_0
          ) as mock_get_simulator_runtime_info_by_build:
            with mock.patch(
                'xcode_util._install_runtime_dmg') as mock__install_runtime_dmg:
              with mock.patch('os.environ.get', return_value=True):
                result = xcode_util.install_runtime_dmg(
                    mac_toolchain='mac_toolchain',
                    runtime_cache_folder='/path/to/runtime_cache_folder',
                    ios_version='15.0',
                    xcode_build_version='15a123')

    mock_delete_least_recently_used_simulator_runtimes.assert_called_once_with()
    mock_get_simulator_runtime_info_by_build.assert_called_once_with('20C52')
    mock__install_runtime_dmg.assert_not_called()


class HelperFunctionTests(XcodeUtilTest):
  """Test class for xcode_util misc util functions."""

  def setUp(self):
    super(HelperFunctionTests, self).setUp()
    self.xcode_runtime_dir_rel_path = (
        'Contents/Developer/'
        'Platforms/iPhoneOS.platform/Library/Developer/'
        'CoreSimulator/Profiles/Runtimes')
    self.xcode_runtime_rel_path = (
        'Contents/Developer/'
        'Platforms/iPhoneOS.platform/Library/Developer/'
        'CoreSimulator/Profiles/Runtimes/iOS.simruntime')

  @mock.patch('subprocess.check_output', autospec=True)
  def test_using_new_mac_toolchain(self, mock_check_output):
    mock_check_output.return_value = b"""
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
    mock_check_output.return_value = b"""
Mac OS / iOS toolchain management

Usage:  mac_toolchain [command] [arguments]

Commands:
  help             prints help about a command
  install          Installs Xcode.
  upload           Uploads Xcode CIPD packages.
  package          Create CIPD packages locally.


Use "mac_toolchain help [command]" for more information about a command."""
    self.assertFalse(xcode_util._using_new_mac_toolchain('mac_toolchain'))

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_is_legacy_xcode_package_legacy(self, mock_glob, mock_rmtree):
    test_xcode_path = 'test/path/Xcode.app/'
    runtime_names = ['iOS.simruntime', 'iOS 12.4.simruntime']
    xcode_runtime_paths = [
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     runtime_name) for runtime_name in runtime_names
    ]
    mock_glob.return_value = xcode_runtime_paths
    self.assertTrue(xcode_util._is_legacy_xcode_package(test_xcode_path))
    mock_glob.assert_called_with(
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     '*.simruntime'))
    self.assertFalse(mock_rmtree.called)

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_is_legacy_xcode_package_no_runtime(self, mock_macos_13_or_higher,
                                              mock_glob, mock_rmtree):
    mock_macos_13_or_higher.return_value = False
    test_xcode_path = 'test/path/Xcode.app/'
    xcode_runtime_paths = []
    mock_glob.return_value = xcode_runtime_paths
    self.assertFalse(xcode_util._is_legacy_xcode_package(test_xcode_path))
    mock_glob.assert_called_with(
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     '*.simruntime'))
    self.assertFalse(mock_rmtree.called)

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_is_legacy_xcode_package_single_runtime(self, mock_macos_13_or_higher,
                                                  mock_glob, mock_rmtree):
    mock_macos_13_or_higher.return_value = False
    test_xcode_path = 'test/path/Xcode.app/'
    runtime_names = ['iOS.simruntime']
    xcode_runtime_paths = [
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     runtime_name) for runtime_name in runtime_names
    ]
    mock_glob.return_value = xcode_runtime_paths
    self.assertFalse(xcode_util._is_legacy_xcode_package(test_xcode_path))
    mock_glob.assert_called_with(
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     '*.simruntime'))
    mock_rmtree.assert_called_with(
        os.path.join(test_xcode_path, self.xcode_runtime_dir_rel_path,
                     'iOS.simruntime'))

  def test_is_runtime_builtin_with_builtin_runtime(self):
    with mock.patch(
        'iossim_util.get_simulator_runtime_info',
        return_value={
            'kind': 'Bundled with Xcode',
        }):
      result = xcode_util.is_runtime_builtin('15.0')

    self.assertTrue(result)

  def test_is_runtime_builtin_with_non_builtin_runtime(self):
    with mock.patch(
        'iossim_util.get_simulator_runtime_info',
        return_value={
            'kind': 'Disk Image',
        }):
      result = xcode_util.is_runtime_builtin('15.0')

    self.assertFalse(result)

  def test_is_runtime_builtin_with_no_runtime_info(self):
    with mock.patch(
        'iossim_util.get_simulator_runtime_info', return_value=None):
      result = xcode_util.is_runtime_builtin('15.0')

    self.assertFalse(result)

  def test_convert_ios_version_to_cipd_ref(self):
    expected_result = "ios-14-4"
    actual_result = xcode_util.convert_ios_version_to_cipd_ref("14.4")
    self.assertEqual(expected_result, actual_result)

  def test_get_latest_runtime_build_cipd_with_ios_version(self):
    output1 = """
      Tags:
        ios_runtime_build:21C62
        ios_runtime_version:ios-17-2"""
    output2 = """
      Tags:
        ios_runtime_build:21A342
        ios_runtime_version:ios-15-0"""
    with mock.patch('xcode_util.describe_cipd_ref') as mock_describe_cipd_ref:
      mock_describe_cipd_ref.side_effect = [output1, output2]
      result = xcode_util.get_latest_runtime_build_cipd('14c18', '15.0')
    self.assertEqual(result, '21A342')

  def test_get_latest_runtime_build_cipd_with_xcode_version(self):
    output = """
      Tags:
        ios_runtime_build:21A342
        ios_runtime_version:ios-15-0"""
    with mock.patch(
        'xcode_util.describe_cipd_ref',
        return_value='ios_runtime_build:21A342'):
      result = xcode_util.get_latest_runtime_build_cipd('14c18', '15.0')
    self.assertEqual(result, '21A342')

  def test_get_latest_runtime_build_cipd_return_no_match(self):
    with mock.patch('xcode_util.describe_cipd_ref') as mock_describe_cipd_ref:
      mock_describe_cipd_ref.side_effect = ['', '']
      result = xcode_util.get_latest_runtime_build_cipd('14c18', '15.0')
    self.assertIsNone(result)


class MoveRuntimeTests(XcodeUtilTest):
  """Test class for xcode_util.move_runtime function."""

  def setUp(self):
    super(MoveRuntimeTests, self).setUp()
    self.runtime_cache_folder = 'test/path/Runtime'
    self.xcode_app_path = 'test/path/Xcode.app'

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_into_xcode(self, mock_glob, mock_rmtree, mock_move):

    mock_glob.side_effect = [['test/path/Runtime/iOS.simruntime'], []]

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            True)

    xcode_runtime_path = ('test/path/Xcode.app/Contents/Developer/'
                          'Platforms/iPhoneOS.platform/Library/Developer/'
                          'CoreSimulator/Profiles/Runtimes/iOS.simruntime')
    calls = [
        mock.call('test/path/Runtime/*.simruntime'),
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/*.simruntime'))
    ]
    mock_glob.assert_has_calls(calls)
    self.assertFalse(mock_rmtree.called)
    mock_move.assert_called_with('test/path/Runtime/iOS.simruntime',
                                 xcode_runtime_path)

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_outside_xcode(self, mock_glob, mock_rmtree, mock_move):
    xcode_runtime_folder = ('test/path/Xcode.app/Contents/Developer/'
                            'Platforms/iPhoneOS.platform/Library/Developer/'
                            'CoreSimulator/Profiles/Runtimes')
    mock_glob.side_effect = [[xcode_runtime_folder + '/iOS.simruntime'], []]

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            False)

    calls = [
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/*.simruntime')),
        mock.call('test/path/Runtime/*.simruntime')
    ]
    mock_glob.assert_has_calls(calls)
    self.assertFalse(mock_rmtree.called)
    mock_move.assert_called_with(xcode_runtime_folder + '/iOS.simruntime',
                                 'test/path/Runtime/iOS.simruntime')

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_multiple_in_src(self, mock_glob, mock_rmtree,
                                        mock_move):
    mock_glob.side_effect = [[
        'test/path/Runtime/iOS.simruntime',
        'test/path/Runtime/iOS 13.4.simruntime'
    ], []]

    with self.assertRaises(test_runner_errors.IOSRuntimeHandlingError):
      xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                              True)
    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    self.assertFalse(mock_rmtree.called)
    self.assertFalse(mock_move.called)

  @mock.patch('shutil.move', autospec=True)
  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_move_runtime_remove_from_dst(self, mock_glob, mock_rmtree,
                                        mock_move):

    mock_glob.side_effect = [['test/path/Runtime/iOS.simruntime'],
                             [('test/path/Xcode.app/Contents/Developer/'
                               'Platforms/iPhoneOS.platform/Library/Developer/'
                               'CoreSimulator/Profiles/Runtimes/iOS.simruntime')
                             ]]

    xcode_util.move_runtime(self.runtime_cache_folder, self.xcode_app_path,
                            True)

    xcode_runtime_path = ('test/path/Xcode.app/Contents/Developer/'
                          'Platforms/iPhoneOS.platform/Library/Developer/'
                          'CoreSimulator/Profiles/Runtimes/iOS.simruntime')
    calls = [
        mock.call('test/path/Runtime/*.simruntime'),
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/*.simruntime'))
    ]
    mock_glob.assert_has_calls(calls)
    mock_rmtree.assert_called_with(xcode_runtime_path)
    mock_move.assert_called_with('test/path/Runtime/iOS.simruntime',
                                 xcode_runtime_path)


  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  def test_remove_runtimes(self, mock_glob, mock_rmtree):

    mock_glob.return_value = [
        ('test/path/Xcode.app/Contents/Developer/'
         'Platforms/iPhoneOS.platform/Library/Developer/'
         'CoreSimulator/Profiles/Runtimes/iOS.simruntime'),
        ('test/path/Xcode.app/Contents/Developer/'
         'Platforms/iPhoneOS.platform/Library/Developer/'
         'CoreSimulator/Profiles/Runtimes/iOS 15.0.simruntime')
    ]

    xcode_util.remove_runtimes(self.xcode_app_path)

    calls = [
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/*.simruntime'))
    ]
    mock_glob.assert_has_calls(calls)
    calls = [
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/iOS.simruntime')),
        mock.call(('test/path/Xcode.app/Contents/Developer/'
                   'Platforms/iPhoneOS.platform/Library/Developer/'
                   'CoreSimulator/Profiles/Runtimes/iOS 15.0.simruntime'))
    ]
    mock_rmtree.assert_has_calls(calls)


class MacToolchainInvocationTests(XcodeUtilTest):
  """Test class for xcode_util functions invoking mac_toolchain."""

  def setUp(self):
    super(MacToolchainInvocationTests, self).setUp()
    self.mac_toolchain = 'mac_toolchain'
    self.xcode_build_version = 'TestXcodeVersion'
    self.xcode_app_path = 'test/path/Xcode.app'
    self.runtime_cache_folder = 'test/path/Runtime'
    self.ios_version = '14.4'

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_runtime_no_cache(self, mock_check_output, mock_glob,
                                    mock_exists, mock_rmtree):
    mock_glob.return_value = []
    mock_exists.return_value = False

    xcode_util._install_runtime(self.mac_toolchain, self.runtime_cache_folder,
                                self.xcode_build_version, self.ios_version)

    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    calls = [
        mock.call('test/path/Runtime/.cipd'),
        mock.call('test/path/Runtime/.xcode_versions'),
    ]
    mock_exists.assert_has_calls(calls)
    self.assertFalse(mock_rmtree.called)
    mock_check_output.assert_called_with([
        'mac_toolchain', 'install-runtime', '-xcode-version',
        'testxcodeversion', '-runtime-version', 'ios-14-4', '-output-dir',
        'test/path/Runtime'
    ],
                                         stderr=-2)

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_runtime_has_cache(self, mock_check_output, mock_glob,
                                     mock_exists, mock_rmtree):
    mock_glob.return_value = ['test/path/Runtime/iOS.simruntime']
    xcode_util._install_runtime(self.mac_toolchain, self.runtime_cache_folder,
                                self.xcode_build_version, self.ios_version)

    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    self.assertFalse(mock_exists.called)
    self.assertFalse(mock_rmtree.called)
    mock_check_output.assert_called_with([
        'mac_toolchain', 'install-runtime', '-xcode-version',
        'testxcodeversion', '-runtime-version', 'ios-14-4', '-output-dir',
        'test/path/Runtime'
    ],
                                         stderr=-2)

  @mock.patch('shutil.rmtree', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('glob.glob', autospec=True)
  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_runtime_incorrect_cache(self, mock_check_output, mock_glob,
                                           mock_exists, mock_rmtree):
    mock_glob.return_value = []
    mock_exists.return_value = True

    xcode_util._install_runtime(self.mac_toolchain, self.runtime_cache_folder,
                                self.xcode_build_version, self.ios_version)

    mock_glob.assert_called_with('test/path/Runtime/*.simruntime')
    calls = [
        mock.call('test/path/Runtime/.cipd'),
        mock.call('test/path/Runtime/.xcode_versions'),
    ]
    mock_exists.assert_has_calls(calls)
    mock_rmtree.assert_has_calls(calls)

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

  @mock.patch('subprocess.check_call', autospec=True)
  def test_install_runtime_dmg(self, mock_check_output):
    xcode_util._install_runtime_dmg(self.mac_toolchain,
                                    self.runtime_cache_folder, self.ios_version,
                                    self.xcode_build_version)

    mock_check_output.assert_called_with([
        'mac_toolchain', 'install-runtime-dmg', '-runtime-version', 'ios-14-4',
        '-xcode-version', self.xcode_build_version, '-output-dir',
        'test/path/Runtime'
    ],
                                         stderr=-2)


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
