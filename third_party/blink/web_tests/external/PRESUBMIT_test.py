#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

import PRESUBMIT


class MockInputApi(object):
  """A minimal mock InputApi for _LintWPT."""
  def __init__(self):
    self.affected_files = []
    self.os_path = os.path
    self.python_executable = sys.executable
    self.subprocess = subprocess

  def AbsoluteLocalPaths(self):
    return self.affected_files

  def PresubmitLocalPath(self):
    return os.path.abspath(os.path.dirname(__file__))


class MockOutputApi(object):
  """A minimal mock OutputApi for _LintWPT."""
  def PresubmitError(self, message, long_text=''):
    return (message, long_text)


class LintWPTTest(unittest.TestCase):
  def setUp(self):
    self._test_file = os.path.join(
      os.path.dirname(__file__), 'wpt', '_DO_NOT_SUBMIT_.html')

  def tearDown(self):
    os.remove(self._test_file)

  def testWPTLintSuccess(self):
    with open(self._test_file, 'w') as f:
      f.write('<body>Hello, world!</body>')
    mock_input = MockInputApi()
    mock_output = MockOutputApi()
    mock_input.affected_files = [os.path.abspath(self._test_file)]
    errors = PRESUBMIT._LintWPT(mock_input, mock_output)
    self.assertEqual(len(errors), 0)

  def testWPTLintErrors(self):
    # Private LayoutTests APIs are not allowed.
    with open(self._test_file, 'w') as f:
      f.write('<script>testRunner.notifyDone()</script>')
    mock_input = MockInputApi()
    mock_output = MockOutputApi()
    mock_input.affected_files = [os.path.abspath(self._test_file)]
    errors = PRESUBMIT._LintWPT(mock_input, mock_output)
    self.assertEqual(len(errors), 1)


if __name__ == '__main__':
  unittest.main()
