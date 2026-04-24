#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.insert(0, os.path.abspath(os.path.dirname(os.path.dirname(__file__))))
import PRESUBMIT_test_mocks


class CheckNoShortKeywordInNetTest(unittest.TestCase):

    def testNoShortKeywordPlatformIndependent(self):
        lines = ['short my_var = 0;']
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockAffectedFile('net/base/filename.cc',
                                                  lines)
        ]
        errors = PRESUBMIT.CheckNoShortKeywordInNet(
            mock_input_api, PRESUBMIT_test_mocks.MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertIn(
            'Do not use the "short" keyword in platform-independent'
            ' //net code.', errors[0].message)

    def testNoShortKeywordPlatformSpecificSuffix(self):
        lines = ['short my_var = 0;']
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockAffectedFile('net/base/filename_win.cc',
                                                  lines)
        ]
        errors = PRESUBMIT.CheckNoShortKeywordInNet(
            mock_input_api, PRESUBMIT_test_mocks.MockOutputApi())
        self.assertEqual(0, len(errors))

    def testNoShortKeywordPlatformSpecificFolder(self):
        lines = ['short my_var = 0;']
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockAffectedFile('net/android/filename.cc',
                                                  lines)
        ]
        errors = PRESUBMIT.CheckNoShortKeywordInNet(
            mock_input_api, PRESUBMIT_test_mocks.MockOutputApi())
        self.assertEqual(0, len(errors))

    def testAllowSubstringsAndCapitalized(self):
        lines = [
            'int short_ = 0;',
            'int time_short = 0;',
            'class Short {',
        ]
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockAffectedFile('net/base/filename.cc',
                                                  lines)
        ]
        errors = PRESUBMIT.CheckNoShortKeywordInNet(
            mock_input_api, PRESUBMIT_test_mocks.MockOutputApi())
        self.assertEqual(0, len(errors))


if __name__ == '__main__':
    unittest.main()
