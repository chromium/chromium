#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for test_apps.py."""

import mock
import os
import unittest

import test_apps
import test_runner
import test_runner_errors
import test_runner_test
import xcode_util


_TEST_APP_PATH = '/path/to/test_app.app'
_BUNDLE_ID = 'org.chromium.gtest.test-app'
_MODULE_NAME = 'test_app'
_XCTEST_PATH = '/PlugIns/boringssl_ssl_tests_module.xctest'
_ALL_EG_TEST_NAMES = [('Class1', 'passedTest1'), ('Class1', 'passedTest2')]


class UtilTest(test_runner_test.TestCase):
  """Tests utility functions."""

  @mock.patch('subprocess.check_output', return_value=b'\x01\x00\x00\x00')
  @mock.patch('platform.system', return_value='Darwin')
  def test_is_running_rosetta_true(self, _, __):
    """Tests is_running_rosetta function on arm64 running rosetta."""
    self.assertTrue(test_apps.is_running_rosetta())

  @mock.patch('subprocess.check_output', return_value=b'\x00\x00\x00\x00')
  @mock.patch('platform.system', return_value='Darwin')
  def test_is_running_rosetta_false(self, _, __):
    """Tests is_running_rosetta function on arm64 not running rosetta."""
    self.assertFalse(test_apps.is_running_rosetta())

  @mock.patch('subprocess.check_output', return_value=b'')
  @mock.patch('platform.system', return_value='Darwin')
  def test_is_running_rosetta_not_arm(self, _, __):
    """Tests is_running_rosetta function not invoked in arm."""
    self.assertFalse(test_apps.is_running_rosetta())

  def test_is_not_mac_os(self):
    self.assertFalse(test_apps.is_running_rosetta())


class GetGTestFilterTest(test_runner_test.TestCase):
  """Tests for test_runner.get_gtest_filter."""

  def test_correct_included(self):
    """Ensures correctness of filter."""
    included = [
        'test.1',
        'test.2',
    ]
    expected = 'test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter(included, []), expected)

  def test_correct_excluded(self):
    """Ensures correctness of inverted filter."""
    excluded = [
        'test.1',
        'test.2',
    ]
    expected = '-test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter([], excluded), expected)

  def test_both_included_excluded(self):
    """Ensures correctness when both included, excluded exist."""
    included = ['test.1', 'test.2']
    excluded = ['test.2', 'test.3']
    expected = 'test.1'
    self.assertEqual(test_apps.get_gtest_filter(included, excluded), expected)

    included = ['test.1', 'test.2']
    excluded = ['test.3', 'test.4']
    expected = 'test.1:test.2'
    self.assertEqual(test_apps.get_gtest_filter(included, excluded), expected)

    included = ['test.1', 'test.2', 'test.3']
    excluded = ['test.3']
    expected = 'test.1:test.2'
    self.assertEqual(test_apps.get_gtest_filter(included, excluded), expected)

    included = ['test.1', 'test.2']
    excluded = ['test.1', 'test.2']
    expected = '-*'
    self.assertEqual(test_apps.get_gtest_filter(included, excluded), expected)

  def test_empty_included_excluded(self):
    """Ensures correctness when both included, excluded are empty."""
    with self.assertRaises(AssertionError) as ctx:
      test_apps.get_gtest_filter([], [])
      self.assertEuqals('One of included or excluded list should exist.',
                        ctx.message)



class DeviceXCTestUnitTestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of SimulatorXCTestUnitTestsApp."""

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('xcode_util.xctest_path', return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_fill_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.DeviceXCTestUnitTestsApp(_TEST_APP_PATH)
    expected_xctestrun_node = {
        'TestTargetName': {
            'CommandLineArguments': [
                '--enable-run-ios-unittests-with-xctest',
                '--gmock_verbose=error',
                '--write-compiled-tests-json-to-writable-path'
            ],
            'IsAppHostedTestBundle': True,
            'TestBundlePath': '__TESTHOST__%s' % _XCTEST_PATH,
            'TestHostBundleIdentifier': _BUNDLE_ID,
            'TestHostPath': '%s' % _TEST_APP_PATH,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES':
                    '__TESTHOST__/Frameworks/libXCTestBundleInject.dylib',
                'DYLD_LIBRARY_PATH':
                    '__PLATFORMS__/iPhoneOS.platform/Developer/Library',
                'DYLD_FRAMEWORK_PATH':
                    '__PLATFORMS__/iPhoneOS.platform/Developer/'
                    'Library/Frameworks',
                'XCInjectBundleInto':
                    '__TESTHOST__/%s' % _MODULE_NAME
            }
        }
    }
    xctestrun_node = test_app.fill_xctestrun_node()
    self.assertEqual(xctestrun_node, expected_xctestrun_node)

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('xcode_util.xctest_path', return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_repeat_arg_in_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.DeviceXCTestUnitTestsApp(
        _TEST_APP_PATH, repeat_count=20)
    xctestrun_node = test_app.fill_xctestrun_node()
    self.assertIn(
        '--gtest_repeat=20',
        xctestrun_node.get('TestTargetName', {}).get('CommandLineArguments'))


class SimulatorXCTestUnitTestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of SimulatorXCTestUnitTestsApp."""

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('xcode_util.xctest_path', return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_fill_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.SimulatorXCTestUnitTestsApp(_TEST_APP_PATH)
    expected_xctestrun_node = {
        'TestTargetName': {
            'CommandLineArguments': [
                '--enable-run-ios-unittests-with-xctest',
                '--gmock_verbose=error',
                '--write-compiled-tests-json-to-writable-path'
            ],
            'IsAppHostedTestBundle': True,
            'TestBundlePath': '__TESTHOST__%s' % _XCTEST_PATH,
            'TestHostBundleIdentifier': _BUNDLE_ID,
            'TestHostPath': '%s' % _TEST_APP_PATH,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/usr/lib/'
                    'libXCTestBundleInject.dylib',
                'DYLD_LIBRARY_PATH':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/Library',
                'DYLD_FRAMEWORK_PATH':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                    'Library/Frameworks',
                'XCInjectBundleInto':
                    '__TESTHOST__/%s' % _MODULE_NAME
            }
        }
    }
    xctestrun_node = test_app.fill_xctestrun_node()
    self.assertEqual(xctestrun_node, expected_xctestrun_node)

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('xcode_util.xctest_path', return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_repeat_arg_in_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.SimulatorXCTestUnitTestsApp(
        _TEST_APP_PATH, repeat_count=20)
    xctestrun_node = test_app.fill_xctestrun_node()
    self.assertIn(
        '--gtest_repeat=20',
        xctestrun_node.get('TestTargetName', {}).get('CommandLineArguments'))


class GTestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of GTestsApp."""

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('os.path.exists', return_value=True)
  def test_repeat_count(self, _1, _2):
    """Tests correct arguments present when repeat_count."""
    gtests_app = test_apps.GTestsApp('app_path', repeat_count=2)
    xctestrun_data = gtests_app.fill_xctestrun_node()
    cmd_args = xctestrun_data[gtests_app.module_name +
                              '_module']['CommandLineArguments']
    self.assertTrue('--gtest_repeat=2' in cmd_args)

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('os.path.exists', return_value=True)
  def test_remove_gtest_sharding_env_vars(self, _1, _2):
    gtests_app = test_apps.GTestsApp(
        'app_path', env_vars=['GTEST_SHARD_INDEX=1', 'GTEST_TOTAL_SHARDS=2'])
    assert all(key in gtests_app.env_vars
               for key in ['GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS'])
    gtests_app.remove_gtest_sharding_env_vars()
    assert not any(key in gtests_app.env_vars
                   for key in ['GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS'])

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch('os.path.exists', return_value=True)
  def test_remove_gtest_sharding_env_vars_non_exist(self, _1, _2):
    gtests_app = test_apps.GTestsApp('app_path')
    assert not any(key in gtests_app.env_vars
                   for key in ['GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS'])
    gtests_app.remove_gtest_sharding_env_vars()
    assert not any(key in gtests_app.env_vars
                   for key in ['GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS'])


class EgtestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of EgTestsApp."""

  def setUp(self):
    super(EgtestsAppTest, self).setUp()
    self.mock(test_apps, 'get_bundle_id', lambda _: 'bundle-id')
    self.mock(test_apps, 'is_running_rosetta', lambda: True)
    self.mock(os.path, 'exists', lambda _: True)

  @mock.patch('xcode_util.using_xcode_13_or_higher', return_value=True)
  @mock.patch('test_apps.EgtestsApp.fill_xctest_run', return_value='xctestrun')
  def test_command_with_repeat_count(self, _1, _2):
    """Tests command method can produce repeat_count arguments when available.
    """
    egtests_app = test_apps.EgtestsApp(
        'app_path',
        _ALL_EG_TEST_NAMES,
        host_app_path='host_app_path',
        repeat_count=2)
    cmd = egtests_app.command('outdir', 'id=UUID', 1)
    expected_cmd = [
        'arch', '-arch', 'arm64', 'xcodebuild', 'test-without-building',
        '-xctestrun', 'xctestrun', '-destination', 'id=UUID',
        '-resultBundlePath', 'outdir', '-test-iterations', '2'
    ]
    self.assertEqual(cmd, expected_cmd)

  @mock.patch('xcode_util.using_xcode_13_or_higher', return_value=False)
  @mock.patch('test_apps.EgtestsApp.fill_xctest_run', return_value='xctestrun')
  def test_command_with_repeat_count_incorrect_xcode(self, _1, _2):
    """Tests |command| raises error with repeat_count in lower Xcode version."""
    egtests_app = test_apps.EgtestsApp(
        'app_path',
        _ALL_EG_TEST_NAMES,
        host_app_path='host_app_path',
        repeat_count=2)
    with self.assertRaises(test_runner_errors.XcodeUnsupportedFeatureError):
      cmd = egtests_app.command('outdir', 'id=UUID', 1)

  def test_not_found_egtests_app(self):
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.AppNotFoundError):
      test_apps.EgtestsApp(_TEST_APP_PATH, _ALL_EG_TEST_NAMES)

  def test_not_found_plugins(self):
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.PlugInsNotFoundError):
      xcode_util.xctest_path(_TEST_APP_PATH)

  @mock.patch('os.listdir', autospec=True)
  def test_found_xctest(self, mock_listdir):
    mock_listdir.return_value = [
        '/path/to/test_app.app/PlugIns/any_egtests.xctest'
    ]
    self.assertEqual('/PlugIns/any_egtests.xctest',
                     xcode_util.xctest_path(_TEST_APP_PATH))

  @mock.patch('os.listdir', autospec=True)
  def test_not_found_xctest(self, mock_listdir):
    mock_listdir.return_value = ['random_file']
    egtest = test_apps.EgtestsApp(_TEST_APP_PATH, _ALL_EG_TEST_NAMES)
    with self.assertRaises(test_runner.XCTestPlugInNotFoundError):
      xcode_util.xctest_path(_TEST_APP_PATH)

  @mock.patch('os.listdir', autospec=True)
  def test_additional_inserted_libs(self, mock_listdir):
    mock_listdir.return_value = [
        'random_file', 'main_binary', 'libclang_rt.asan_iossim_dynamic.dylib'
    ]
    egtest = test_apps.EgtestsApp(
        _TEST_APP_PATH,
        _ALL_EG_TEST_NAMES,
        host_app_path='/path/to/host_app.app')
    self.assertEqual(['@executable_path/libclang_rt.asan_iossim_dynamic.dylib'],
                     egtest._additional_inserted_libs())

  def test_xctestRunNode_without_filter(self):
    self.mock(xcode_util, 'xctest_path', lambda _: 'xctest-path')
    self.mock(test_apps.EgtestsApp, '_additional_inserted_libs', lambda _: [])
    egtest_node = test_apps.EgtestsApp(
        _TEST_APP_PATH,
        _ALL_EG_TEST_NAMES).fill_xctestrun_node()['test_app_module']
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def test_xctestRunNode_with_filter_only_identifiers(self):
    self.mock(xcode_util, 'xctest_path', lambda _: 'xctest-path')
    self.mock(test_apps.EgtestsApp, '_additional_inserted_libs', lambda _: [])
    filtered_tests = [
        'TestCase1/testMethod1', 'TestCase1/testMethod2',
        'TestCase2/testMethod1', 'TestCase1/testMethod2'
    ]
    egtest_node = test_apps.EgtestsApp(
        _TEST_APP_PATH, _ALL_EG_TEST_NAMES,
        included_tests=filtered_tests).fill_xctestrun_node()['test_app_module']
    self.assertEqual(filtered_tests, egtest_node['OnlyTestIdentifiers'])
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def test_xctestRunNode_with_filter_skip_identifiers(self):
    self.mock(xcode_util, 'xctest_path', lambda _: 'xctest-path')
    self.mock(test_apps.EgtestsApp, '_additional_inserted_libs', lambda _: [])
    skipped_tests = [
        'TestCase1/testMethod1', 'TestCase1/testMethod2',
        'TestCase2/testMethod1', 'TestCase1/testMethod2'
    ]
    egtest_node = test_apps.EgtestsApp(
        _TEST_APP_PATH, _ALL_EG_TEST_NAMES,
        excluded_tests=skipped_tests).fill_xctestrun_node()['test_app_module']
    self.assertEqual(skipped_tests, egtest_node['SkipTestIdentifiers'])
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)

  def test_xctestRunNode_with_additional_inserted_libs(self):
    asan_dylib = '@executable_path/libclang_rt.asan_iossim_dynamic.dylib'
    self.mock(xcode_util, 'xctest_path', lambda _: 'xctest-path')
    self.mock(test_apps.EgtestsApp, '_additional_inserted_libs',
              lambda _: [asan_dylib])
    egtest_node = test_apps.EgtestsApp(
        _TEST_APP_PATH,
        _ALL_EG_TEST_NAMES).fill_xctestrun_node()['test_app_module']
    self.assertEqual(
        asan_dylib,
        egtest_node['TestingEnvironmentVariables']['DYLD_INSERT_LIBRARIES'])


if __name__ == '__main__':
  unittest.main()
