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
  def testNewUsageThreadTaskRunnerHandleGet(self):
    diff = ['scoped_refptr<SingleThreadTaskRunner> task_runner =',
             '    base::ThreadTaskRunner::GetCurrentDefault()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))

  def testNewUsageSequencedTaskRunnerHandleGet(self):
    diff = ['scoped_refptr<SequencedTaskRunner> task_runner =',
             '    base::SequencedTaskRunner::GetCurrentDefault()']
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('content/renderer/foo.cc', diff)]
    errors = PRESUBMIT._CheckForUseOfGlobalTaskRunnerGetter(input_api,
                                                            MockOutputApi())
    self.assertEqual(1, len(errors))

if __name__ == '__main__':
  unittest.main()
