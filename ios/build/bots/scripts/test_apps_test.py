# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for test_apps.py."""

from test_runner import TestRunner
import mock
import unittest

import test_apps
import test_runner_test


_TEST_APP_PATH = '/path/to/test_app.app'
_HOST_APP_PATH = '/path/to/host_app.app'
_BUNDLE_ID = 'org.chromium.gtest.test-app'
_MODULE_NAME = 'test_app'
_XCTEST_PATH = '/PlugIns/boringssl_ssl_tests_module.xctest'


class GetGTestFilterTest(test_runner_test.TestCase):
  """Tests for test_runner.get_gtest_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = 'test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = '-test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter(tests, invert=True), expected)


class EgtestsAppGetAllTestsTest(test_runner_test.TestCase):
  """Tests to get_all_tests methods of EgtestsApp."""

  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('shard_util.fetch_test_names')
  def testNonTestsFiltered(self, mock_fetch, _):
    mock_fetch.return_value = [
        ('ATestCase', 'testB'),
        ('setUpForTestCase', 'testForStartup'),
        ('ChromeTestCase', 'testServer'),
        ('FindInPageTestCase', 'testURL'),
        ('CTestCase', 'testD'),
    ]
    test_app = test_apps.EgtestsApp(_TEST_APP_PATH)
    tests = test_app.get_all_tests()
    self.assertEqual(set(tests), set(['ATestCase/testB', 'CTestCase/testD']))


class DeviceXCTestUnitTestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of SimulatorXCTestUnitTestsApp."""

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch(
      'test_apps.DeviceXCTestUnitTestsApp._xctest_path',
      return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_fill_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.DeviceXCTestUnitTestsApp(_TEST_APP_PATH)
    expected_xctestrun_node = {
        'TestTargetName': {
            'CommandLineArguments': [
                '--enable-run-ios-unittests-with-xctest',
                '--gmock_verbose=error'
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


class SimulatorXCTestUnitTestsAppTest(test_runner_test.TestCase):
  """Tests to test methods of SimulatorXCTestUnitTestsApp."""

  @mock.patch('test_apps.get_bundle_id', return_value=_BUNDLE_ID)
  @mock.patch(
      'test_apps.SimulatorXCTestUnitTestsApp._xctest_path',
      return_value=_XCTEST_PATH)
  @mock.patch('os.path.exists', return_value=True)
  def test_fill_xctestrun_node(self, *args):
    """Tests fill_xctestrun_node method."""
    test_app = test_apps.SimulatorXCTestUnitTestsApp(_TEST_APP_PATH)
    expected_xctestrun_node = {
        'TestTargetName': {
            'CommandLineArguments': [
                '--enable-run-ios-unittests-with-xctest',
                '--gmock_verbose=error'
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


if __name__ == '__main__':
  unittest.main()
