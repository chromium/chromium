#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile)

class DCheckTest(unittest.TestCase):
  def testDCheckInNavigationRequest(self):
    diff = [
        '  DCHECK(foo);',
        '  DCHECK_EQ(foo, bar);',
        '  DCHECK_NE(foo, bar);'
    ]
    input_api = MockInputApi()
    input_api.files = [
        MockAffectedFile(
            'content/browser/renderer_host/navigation_request.cc', diff)
    ]
    errors = PRESUBMIT._WarnAgainstDCHECK(input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(3, len(errors[0].items))
    self.assertIn('DCHECK is discouraged in this file', errors[0].message)
    self.assertIn('CHECKs are cheap and are preferred when possible',
                  errors[0].message)

  def testDCheckInOtherFile(self):
    diff = ['  DCHECK(foo);']
    input_api = MockInputApi()
    input_api.files = [
        MockAffectedFile('content/browser/foo.cc', diff)
    ]
    errors = PRESUBMIT._WarnAgainstDCHECK(input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testDCheckIsOnIgnored(self):
    diff = ['#if DCHECK_IS_ON()']
    input_api = MockInputApi()
    input_api.files = [
        MockAffectedFile(
            'content/browser/renderer_host/navigation_request.cc', diff)
    ]
    errors = PRESUBMIT._WarnAgainstDCHECK(input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

if __name__ == '__main__':
  unittest.main()
