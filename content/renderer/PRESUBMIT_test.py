#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile)

class GetTest(unittest.TestCase):
  def testNewUsageSingleThreadTaskRunnerGetCurrentDefault(self):
    diff = ['scoped_refptr<SingleThreadTaskRunner> task_runner =',
            '    base::SingleThreadTaskRunner::GetCurrentDefault()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))

  def testNewUsageSingleThreadTaskRunnerHandleGetCurrentBestEffort(self):
    diff = ['scoped_refptr<SingleThreadTaskRunner> task_runner =',
            '    base::SingleThreadTaskRunner::GetCurrentBestEffort()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))

  def testNewUsageSequencedTaskRunnerGetCurrentDefault(self):
    diff = ['scoped_refptr<SequencedTaskRunner> task_runner =',
             '    base::SequencedTaskRunner::GetCurrentDefault()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))


def testNewUsageSequencedTaskRunnerGetCurrentBestEffort(self):
    diff = ['scoped_refptr<SequencedTaskRunner> task_runner =',
            '    base::SequencedTaskRunner::GetCurrentBestEffort()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))

if __name__ == '__main__':
  unittest.main()
