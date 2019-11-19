#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test_runner.py."""

import collections
import glob
import logging
import mock
import os
import subprocess
import tempfile
import unittest

import test_runner


class TestCase(unittest.TestCase):
  """Test case which supports installing mocks. Uninstalls on tear down."""

  def __init__(self, *args, **kwargs):
    """Initializes a new instance of this class."""
    super(TestCase, self).__init__(*args, **kwargs)

    # Maps object to a dict which maps names of mocked members to their
    # original values.
    self._mocks = collections.OrderedDict()

  def mock(self, obj, member, mock):
    """Installs mock in place of the named member of the given obj.

    Args:
      obj: Any object.
      member: String naming the attribute of the object to mock.
      mock: The mock to install.
    """
    self._mocks.setdefault(obj, collections.OrderedDict()).setdefault(
        member, getattr(obj, member))
    setattr(obj, member, mock)

  def tearDown(self, *args, **kwargs):
    """Uninstalls mocks."""
    super(TestCase, self).tearDown(*args, **kwargs)

    for obj in self._mocks:
      for member, original_value in self._mocks[obj].iteritems():
        setattr(obj, member, original_value)


class GetKIFTestFilterTest(TestCase):
  """Tests for test_runner.get_kif_test_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = 'NAME:test1|test2'

    self.assertEqual(test_runner.get_kif_test_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = '-NAME:test1|test2'

    self.assertEqual(
        test_runner.get_kif_test_filter(tests, invert=True), expected)


class GetGTestFilterTest(TestCase):
  """Tests for test_runner.get_gtest_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = 'test.1:test.2'

    self.assertEqual(test_runner.get_gtest_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = '-test.1:test.2'

    self.assertEqual(
        test_runner.get_gtest_filter(tests, invert=True), expected)


class InstallXcodeTest(TestCase):
  """Tests install_xcode."""

  def setUp(self):
    super(InstallXcodeTest, self).setUp()
    self.mock(test_runner, 'xcode_select', lambda _: None)
    self.mock(os.path, 'exists', lambda _: True)

  def test_success(self):
    self.assertTrue(test_runner.install_xcode('test_build', 'true', 'path'))

  def test_failure(self):
    self.assertFalse(test_runner.install_xcode('test_build', 'false', 'path'))


class SimulatorTestRunnerTest(TestCase):
  """Tests for test_runner.SimulatorTestRunner."""

  def setUp(self):
    super(SimulatorTestRunnerTest, self).setUp()

    def install_xcode(build, mac_toolchain_cmd, xcode_app_path):
      return True

    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(test_runner, 'install_xcode', install_xcode)
    self.mock(test_runner.subprocess, 'check_output',
              lambda _: 'fake-bundle-id')
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(test_runner.TestRunner, 'set_sigterm_handler',
      lambda self, handler: 0)
    self.mock(os, 'listdir', lambda _: [])

  def test_app_not_found(self):
    """Ensures AppNotFoundError is raised."""

    self.mock(os.path, 'exists', lambda p: not p.endswith('fake-app'))

    with self.assertRaises(test_runner.AppNotFoundError):
      test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        '', # Empty xcode-build
        'out-dir',
      )

  def test_iossim_not_found(self):
    """Ensures SimulatorNotFoundError is raised."""
    self.mock(os.path, 'exists', lambda p: not p.endswith('fake-iossim'))

    with self.assertRaises(test_runner.SimulatorNotFoundError):
      test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'xcode-build',
        'out-dir',
      )

  def test_init(self):
    """Ensures instance is created."""
    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'xcode-build',
        'out-dir',
    )

    self.assertTrue(tr)

  def test_startup_crash(self):
    """Ensures test is relaunched once on startup crash."""
    def set_up(self):
      return

    @staticmethod
    def _run(cmd, shards=None):
      return collections.namedtuple('result', ['crashed', 'crashed_test'])(
          crashed=True, crashed_test=None)

    def tear_down(self):
      return

    self.mock(test_runner.SimulatorTestRunner, 'set_up', set_up)
    self.mock(test_runner.TestRunner, '_run', _run)
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', tear_down)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'xcode-build',
        'out-dir',
        xctest=True,
    )
    with self.assertRaises(test_runner.AppLaunchError):
      tr.launch()

  def test_run(self):
    """Ensures the _run method is correct with test sharding."""
    def shard_xctest(object_path, shards, test_cases=None):
      return [['a/1', 'b/2'], ['c/3', 'd/4'], ['e/5']]

    def run_tests(self, test_shard=None):
      out = []
      for test in test_shard:
        testname = test.split('/')
        out.append('Test Case \'-[%s %s]\' started.' %
                   (testname[0], testname[1]))
        out.append('Test Case \'-[%s %s]\' passed (0.1 seconds)' %
                   (testname[0], testname[1]))
      return (out, 0, 0)

    tr = test_runner.SimulatorTestRunner(
      'fake-app',
      'fake-iossim',
      'platform',
      'os',
      'xcode-version',
      'xcode-build',
      'out-dir',
      xctest=True,
    )
    self.mock(test_runner, 'shard_xctest', shard_xctest)
    self.mock(test_runner.SimulatorTestRunner, 'run_tests', run_tests)

    tr.xctest_path = 'fake.xctest'
    cmd = tr.get_launch_command()
    result = tr._run(cmd=cmd, shards=3)
    self.assertIn('a/1', result.passed_tests)
    self.assertIn('b/2', result.passed_tests)
    self.assertIn('c/3', result.passed_tests)
    self.assertIn('d/4', result.passed_tests)
    self.assertIn('e/5', result.passed_tests)

  def test_run_with_system_alert(self):
    """Ensures SystemAlertPresentError is raised when warning 'System alert
      view is present, so skipping all tests' is in the output."""
    with self.assertRaises(test_runner.SystemAlertPresentError):
      tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'xcode-build',
        'out-dir',
        xctest=True,
      )
      tr.xctest_path = 'fake.xctest'
      cmd = ['echo', 'System alert view is present, so skipping all tests!']
      result = tr._run(cmd=cmd)

  def test_get_launch_command(self):
    """Ensures launch command is correct with test_filters, test sharding and
      test_cases."""
    tr = test_runner.SimulatorTestRunner(
      'fake-app',
      'fake-iossim',
      'platform',
      'os',
      'xcode-version',
      'xcode-build',
      'out-dir',
    )
    tr.xctest_path = 'fake.xctest'
    # Cases test_filter is not empty, with empty/non-empty self.test_cases.
    tr.test_cases = []
    cmd = tr.get_launch_command(['a'], invert=False)
    self.assertIn('-t', cmd)
    self.assertIn('a', cmd)

    tr.test_cases = ['a', 'b']
    cmd = tr.get_launch_command(['a'], invert=False)
    self.assertIn('-t', cmd)
    self.assertIn('a', cmd)
    self.assertNotIn('b', cmd)

    # Cases test_filter is empty, with empty/non-empty self.test_cases.
    tr.test_cases = []
    cmd = tr.get_launch_command(test_filter=None, invert=False)
    self.assertNotIn('-t', cmd)

    tr.test_cases = ['a', 'b']
    cmd = tr.get_launch_command(test_filter=None, invert=False)
    self.assertIn('-t', cmd)
    self.assertIn('a', cmd)
    self.assertIn('b', cmd)

  def test_relaunch(self):
    """Ensures test is relaunched on test crash until tests complete."""
    def set_up(self):
      return

    @staticmethod
    def _run(cmd, shards=None):
      result = collections.namedtuple(
          'result', [
              'crashed',
              'crashed_test',
              'failed_tests',
              'flaked_tests',
              'passed_tests',
          ],
      )
      if '-e' not in cmd:
        # First run, has no test filter supplied. Mock a crash.
        return result(
            crashed=True,
            crashed_test='c',
            failed_tests={'b': ['b-out'], 'c': ['Did not complete.']},
            flaked_tests={'d': ['d-out']},
            passed_tests=['a'],
        )
      else:
        return result(
            crashed=False,
            crashed_test=None,
            failed_tests={},
            flaked_tests={},
            passed_tests=[],
        )

    def tear_down(self):
      return

    self.mock(test_runner.SimulatorTestRunner, 'set_up', set_up)
    self.mock(test_runner.TestRunner, '_run', _run)
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', tear_down)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'xcode-build',
        'out-dir',
    )
    tr.launch()
    self.assertTrue(tr.logs)


class DeviceTestRunnerTest(TestCase):
  def setUp(self):
    super(DeviceTestRunnerTest, self).setUp()

    def install_xcode(build, mac_toolchain_cmd, xcode_app_path):
      return True

    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(test_runner, 'install_xcode', install_xcode)
    self.mock(test_runner.subprocess, 'check_output',
              lambda _: 'fake-bundle-id')
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(tempfile, 'mkstemp', lambda: '/tmp/tmp_file')

    self.tr = test_runner.DeviceTestRunner(
        'fake-app',
        'xcode-version',
        'xcode-build',
        'out-dir',
    )
    self.tr.xctestrun_data = {'TestTargetName':{}}

  def test_with_test_filter_without_test_cases(self):
    """Ensures tests in the run with test_filter and no test_cases."""
    self.tr.set_xctest_filters(['a', 'b'], invert=False)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['OnlyTestIdentifiers'],
        ['a', 'b']
    )

  def test_invert_with_test_filter_without_test_cases(self):
    """Ensures tests in the run invert with test_filter and no test_cases."""
    self.tr.set_xctest_filters(['a', 'b'], invert=True)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['SkipTestIdentifiers'],
        ['a', 'b']
    )

  def test_with_test_filter_with_test_cases(self):
    """Ensures tests in the run with test_filter and test_cases."""
    self.tr.test_cases = ['a', 'b', 'c', 'd']
    self.tr.set_xctest_filters(['a', 'b', 'irrelevant test'], invert=False)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['OnlyTestIdentifiers'],
        ['a', 'b']
    )

  def test_invert_with_test_filter_with_test_cases(self):
    """Ensures tests in the run invert with test_filter and test_cases."""
    self.tr.test_cases = ['a', 'b', 'c', 'd']
    self.tr.set_xctest_filters(['a', 'b', 'irrelevant test'], invert=True)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['OnlyTestIdentifiers'],
        ['c', 'd']
    )

  def test_without_test_filter_without_test_cases(self):
    """Ensures tests in the run with no test_filter and no test_cases."""
    self.tr.set_xctest_filters(test_filter=None, invert=False)
    self.assertIsNone(
        self.tr.xctestrun_data['TestTargetName'].get('OnlyTestIdentifiers'))

  def test_invert_without_test_filter_without_test_cases(self):
    """Ensures tests in the run invert with no test_filter and no test_cases."""
    self.tr.set_xctest_filters(test_filter=None, invert=True)
    self.assertIsNone(
        self.tr.xctestrun_data['TestTargetName'].get('OnlyTestIdentifiers'))

  def test_without_test_filter_with_test_cases(self):
    """Ensures tests in the run with no test_filter but test_cases."""
    self.tr.test_cases = ['a', 'b', 'c', 'd']
    self.tr.set_xctest_filters(test_filter=None, invert=False)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['OnlyTestIdentifiers'],
        ['a', 'b', 'c', 'd']
    )

  def test_invert_without_test_filter_with_test_cases(self):
    """Ensures tests in the run invert with no test_filter but test_cases."""
    self.tr.test_cases = ['a', 'b', 'c', 'd']
    self.tr.set_xctest_filters(test_filter=None, invert=True)
    self.assertEqual(
        self.tr.xctestrun_data['TestTargetName']['OnlyTestIdentifiers'],
        ['a', 'b', 'c', 'd']
    )

  @mock.patch('subprocess.check_output', autospec=True)
  def test_get_test_names(self, mock_subprocess):
    otool_output = (
        'imp 0x102492020 -[BrowserViewControllerTestCase testJavaScript]'
        'name 0x105ee8b84 testFixForCrbug801165'
        'types 0x105f0c842 v16 @ 0:8'
        'name 0x105ee8b9a testOpenURLFromNTP'
        'types 0x105f0c842 v16 @ 0:8'
        'imp 0x102493b30 -[BrowserViewControllerTestCase testOpenURLFromNTP]'
        'name 0x105ee8bad testOpenURLFromTab'
        'types 0x105f0c842 v16 @ 0:8'
        'imp 0x102494180 -[BrowserViewControllerTestCase testOpenURLFromTab]'
        'name 0x105ee8bc0 testOpenURLFromTabSwitcher'
        'types 0x105f0c842 v16 @ 0:8'
        'imp 0x102494f70 -[BrowserViewControllerTestCase testTabSwitch]'
        'types 0x105f0c842 v16 @ 0:8'
        'imp 0x102494f70 -[BrowserViewControllerTestCase helper]'
        'imp 0x102494f70 -[BrowserViewControllerTestCCCCCCCCC testMethod]'
    )
    mock_subprocess.return_value = otool_output
    tests = test_runner.get_test_names('')
    self.assertEqual(
        [
            ('BrowserViewControllerTestCase', 'testJavaScript'),
            ('BrowserViewControllerTestCase', 'testOpenURLFromNTP'),
            ('BrowserViewControllerTestCase', 'testOpenURLFromTab'),
            ('BrowserViewControllerTestCase', 'testTabSwitch')
        ],
        tests
    )


if __name__ == '__main__':
  logging.basicConfig(format='[%(asctime)s:%(levelname)s] %(message)s',
    level=logging.DEBUG, datefmt='%I:%M:%S')
  unittest.main()
