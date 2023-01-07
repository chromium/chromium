#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import collections
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(BUILD_TOOLS_DIR)))

import mock

sys.path.append(BUILD_TOOLS_DIR)
import build_version

ProcInfo = collections.namedtuple('ProcInfo', ['returncode', 'output'])

class TestCase(unittest.TestCase):
  def setUp(self):
    self.run_git = mock.patch('lastchange.RunGitCommand').start()

  def tearDown(self):
    mock.patch.stopall()

  def mockGitCommand(self, *args):
    side_effects = []
    for proc_info in args:
      mock_proc = mock.MagicMock()
      mock_proc.returncode = proc_info.returncode
      comm_result = mock_proc.MagicMock()
      comm_result.__getitem__.return_value = proc_info.output
      mock_proc.communicate.return_value = comm_result
      side_effects.append(mock_proc)

    self.run_git.side_effect = side_effects

  def mockDefaultGitCommand(self):
    output = """\
6a8b61d6be4656e682eba005a1dd7f129789129c
[NaCl SDK] Update build_sdk.py to display Cr-Commit-Position in README.

BUG=none
R=bradnelson@google.com, bradnelson@chromium.org

Review URL: https://codereview.chromium.org/495423010

Cr-Commit-Position: refs/heads/master@{#292480}"""
    self.mockGitCommand(ProcInfo(0, output))

  def mockDepthTwoGitCommand(self):
    output0 = """\
ae4b444a0aa09a1fa73e59b180d7d957b9a36bf2
."""

    output1 = """\
6a8b61d6be4656e682eba005a1dd7f129789129c
[NaCl SDK] Update build_sdk.py to display Cr-Commit-Position in README.

BUG=none
R=bradnelson@google.com, bradnelson@chromium.org

Review URL: https://codereview.chromium.org/495423010

Cr-Commit-Position: refs/heads/master@{#292480}"""
    self.mockGitCommand(ProcInfo(0, output0), ProcInfo(0, output1))


  def assertGitShowCalled(self, depth=0):
    cmd = ['show', '-s', '--format=%H%n%B', 'HEAD~%d' % depth]
    self.run_git.assert_called_with(None, cmd)

  def testChromeVersion(self):
    self.mockDefaultGitCommand()
    result = build_version.ChromeVersion()
    self.assertGitShowCalled()
    self.assertEqual(result, 'trunk.292480')

  def testChromeRevision(self):
    self.mockDefaultGitCommand()
    result = build_version.ChromeRevision()
    self.assertGitShowCalled()
    self.assertEqual(result, '292480')

  def testChromeCommitPosition(self):
    self.mockDefaultGitCommand()
    result = build_version.ChromeCommitPosition()
    self.assertGitShowCalled()
    self.assertEqual(
        result,
        '6a8b61d6be4656e682eba005a1dd7f129789129c-refs/heads/master@{#292480}')

  def testChromeCommitPositionDepthTwo(self):
    self.mockDepthTwoGitCommand()
    result = build_version.ChromeCommitPosition()
    self.assertEqual(
        result,
        '6a8b61d6be4656e682eba005a1dd7f129789129c-refs/heads/master@{#292480}')


if __name__ == '__main__':
  unittest.main()
