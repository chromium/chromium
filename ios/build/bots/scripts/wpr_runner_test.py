#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for wpr_runner.py."""

import glob
import logging
import os
import subprocess
import unittest
from unittest.mock import Mock, call

import iossim_util
import test_runner
import test_runner_test
import wpr_runner


class WprProxySimulatorTestRunnerTest(test_runner_test.TestCase):
  """Tests for test_runner.WprProxySimulatorTestRunner."""

  def setUp(self):
    super(WprProxySimulatorTestRunnerTest, self).setUp()

    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(test_runner.subprocess,
              'check_output', lambda _: b'fake-bundle-id')
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(test_runner.TestRunner, 'set_sigterm_handler',
              lambda self, handler: 0)
    self.mock(test_runner.SimulatorTestRunner, 'getSimulator',
              lambda _: 'fake-id')
    self.mock(test_runner.SimulatorTestRunner, 'deleteSimulator',
              lambda a, b: True)
    self.mock(wpr_runner.WprProxySimulatorTestRunner,
              'copy_trusted_certificate', lambda a, b: True)
    self.mock(iossim_util, 'get_simulator',
              lambda a, b: 'E4E66320-177A-450A-9BA1-488D85B7278E')

  def test_app_not_found(self):
    """Ensures AppNotFoundError is raised."""

    self.mock(os.path, 'exists', lambda p: not p.endswith('bad-host-app-path'))

    with self.assertRaises(test_runner.AppNotFoundError):
      wpr_runner.WprProxySimulatorTestRunner(
          'fake-app',
          'bad-host-app-path',
          'fake-iossim',
          'replay-path',
          'platform',
          'os',
          'wpr-tools-path',
          'out-dir',
      )

  def test_replay_path_not_found(self):
    """Ensures ReplayPathNotFoundError is raised."""

    self.mock(os.path, 'exists', lambda p: not p.endswith('bad-replay-path'))

    with self.assertRaises(wpr_runner.ReplayPathNotFoundError):
      wpr_runner.WprProxySimulatorTestRunner(
          'fake-app',
          'fake-host-app',
          'fake-iossim',
          'bad-replay-path',
          'platform',
          'os',
          'wpr-tools-path',
          'out-dir',
      )

  def test_wpr_tools_not_found(self):
    """Ensures WprToolsNotFoundError is raised."""

    self.mock(os.path, 'exists', lambda p: not p.endswith('bad-tools-path'))

    with self.assertRaises(wpr_runner.WprToolsNotFoundError):
      wpr_runner.WprProxySimulatorTestRunner(
          'fake-app',
          'fake-host-app',
          'fake-iossim',
          'replay-path',
          'platform',
          'os',
          'bad-tools-path',
          'out-dir',
      )

  def test_cert_not_found(self):
    """Ensures CertPathNotFoundError is raised"""

    runner = wpr_runner.WprProxySimulatorTestRunner(
        'fake-app',
        'fake-host-app',
        'fake-iossim',
        'replay-path',
        'platform',
        'os',
        'wpr-tools-path',
        'out-dir',
    )

    class FakeOsPath():

      def __init__(self):
        self.numCalls = 0

      def __call__(self, a):
        if self.numCalls == 0:
          self.numCalls += 1
          return True
        self.numCalls += 1
        return not a.endswith('wpr-tools-path/web_page_replay_go/wpr_cert.pem')

    self.mock(os.path, 'exists', FakeOsPath())
    self.mock(subprocess, 'check_call', lambda *args: None)

    self.unmock(wpr_runner.WprProxySimulatorTestRunner,
                'copy_trusted_certificate')

    with self.assertRaises(wpr_runner.CertPathNotFoundError):
      runner.copy_trusted_certificate('fake-udid')

  def test_copy_cert(self):
    """Ensures right commands are issued to copy cert"""

    runner = wpr_runner.WprProxySimulatorTestRunner(
        'fake-app',
        'fake-host-app',
        'fake-iossim',
        'replay-path',
        'platform',
        'os',
        'wpr-tools-path',
        'out-dir',
    )

    self.unmock(wpr_runner.WprProxySimulatorTestRunner,
                'copy_trusted_certificate')

    check_call_mock = Mock()

    self.mock(subprocess, 'check_call', check_call_mock)

    runner.copy_trusted_certificate('UDID')

    calls = [
        call(['xcrun', 'simctl', 'boot', 'UDID']),
        call([
            'xcrun', 'simctl', 'keychain', 'UDID', 'add-root-cert',
            'wpr-tools-path/web_page_replay_go/wpr_cert.pem'
        ]),
        call(['xcrun', 'simctl', 'shutdown', 'UDID'])
    ]

    check_call_mock.assert_has_calls(calls)

    # ensure subprocess.check_call was only called 3 times
    self.assertEqual(check_call_mock.call_count, 3)

  def test_init(self):
    """Ensures instance is created."""
    tr = wpr_runner.WprProxySimulatorTestRunner(
        'fake-app',
        'fake-host-app',
        'fake-iossim',
        'replay-path',
        'platform',
        'os',
        'wpr-tools-path',
        'out-dir',
    )

    self.assertTrue(tr)

  def run_wpr_test(self, test_filter=[], invert=False):
    """Wrapper that mocks the _run method and returns its result."""

    class FakeStdout:

      def __init__(self):
        self.line_index = 0
        self.lines = [
            b'Test Case \'-[a 1]\' started.',
            b'Test Case \'-[a 1]\' has uninteresting logs.',
            b'Test Case \'-[a 1]\' passed (0.1 seconds)',
            b'Test Case \'-[b 2]\' started.',
            b'Test Case \'-[b 2]\' passed (0.1 seconds)',
            b'Test Case \'-[c 3]\' started.',
            b'Test Case \'-[c 3]\' has interesting failure info.',
            b'Test Case \'-[c 3]\' failed (0.1 seconds)',
        ]

      def readline(self):
        if self.line_index < len(self.lines):
          return_line = self.lines[self.line_index]
          self.line_index += 1
          return return_line
        else:
          return None

    class FakeProcess:

      def __init__(self):
        self.stdout = FakeStdout()
        self.returncode = 0

      def stdout(self):
        return self.stdout

      def wait(self):
        return

    def popen(recipe_cmd, env, stdout, stderr):
      return FakeProcess()

    tr = wpr_runner.WprProxySimulatorTestRunner(
        'fake-app',
        'fake-host-app',
        'fake-iossim',
        'replay-path',
        'platform',
        'os',
        'wpr-tools-path',
        'out-dir',
    )
    self.mock(wpr_runner.WprProxySimulatorTestRunner, 'wprgo_start',
              lambda a, b: None)
    self.mock(wpr_runner.WprProxySimulatorTestRunner, 'wprgo_stop',
              lambda _: None)
    self.mock(wpr_runner.WprProxySimulatorTestRunner, 'get_wpr_test_command',
              lambda a, b, c: ["command", "arg"])

    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(glob, 'glob', lambda _: ["file1", "file2"])
    self.mock(subprocess, 'Popen', popen)

    tr.xctest_path = 'fake.xctest'
    cmd = {'invert': invert, 'test_filter': test_filter}
    return tr._run(cmd=cmd, clones=1)

  def test_run_no_filter(self):
    """Ensures the _run method can handle passed and failed tests."""
    result = self.run_wpr_test()
    self.assertIn('file1.a/1', result.passed_tests())
    self.assertIn('file1.b/2', result.passed_tests())
    self.assertIn('file1.c/3', result.failed_tests())
    self.assertIn('file2.a/1', result.passed_tests())
    self.assertIn('file2.b/2', result.passed_tests())
    self.assertIn('file2.c/3', result.failed_tests())

  def test_run_with_filter(self):
    """Ensures the _run method works with a filter."""
    result = self.run_wpr_test(test_filter=["file1"], invert=False)
    self.assertIn('file1.a/1', result.passed_tests())
    self.assertIn('file1.b/2', result.passed_tests())
    self.assertIn('file1.c/3', result.failed_tests())
    self.assertNotIn('file2.a/1', result.passed_tests())
    self.assertNotIn('file2.b/2', result.passed_tests())
    self.assertNotIn('file2.c/3', result.failed_tests())

  def test_run_with_inverted_filter(self):
    """Ensures the _run method works with an inverted filter."""
    result = self.run_wpr_test(test_filter=["file1"], invert=True)
    self.assertNotIn('file1.a/1', result.passed_tests())
    self.assertNotIn('file1.b/2', result.passed_tests())
    self.assertNotIn('file1.c/3', result.failed_tests())
    self.assertIn('file2.a/1', result.passed_tests())
    self.assertIn('file2.b/2', result.passed_tests())
    self.assertIn('file2.c/3', result.failed_tests())


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
