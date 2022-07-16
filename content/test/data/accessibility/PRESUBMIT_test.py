#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
      '..', '..'))
from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile)

class AccessibilityEventsTestIncludesAndroidTest(unittest.TestCase):
  # Test that no warning is raised when the Android file is also modified.
  def testAndroidChangeIncluded(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/event/foo.html',
          [''], action='A'),
        MockAffectedFile(
          'accessibility/WebContentsAccessibilityEventsTest.java',
          [''], action='M')
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (0, len(msgs), msgs))

  # Test that a warning is raised when the Android file is not modified.
  def testAndroidChangeMissing(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/event/foo.html',
          [''], action='A'),
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (1, len(msgs), msgs))

  # Test that Android change is not required when no html file is added/removed.
  def testIgnoreNonHtmlFiles(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/event/foo.txt',
          [''], action='A'),
        MockAffectedFile('content/test/data/accessibility/event/foo.cc',
          [''], action='A'),
        MockAffectedFile('content/test/data/accessibility/event/foo.h',
          [''], action='A'),
        MockAffectedFile('content/test/data/accessibility/event/foo.py',
          [''], action='A')
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (0, len(msgs), msgs))

  # Test that Android change is not required for unrelated html files.
  def testIgnoreNonRelatedHtmlFiles(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/aria/foo.html',
          [''], action='A'),
        MockAffectedFile('content/test/data/accessibility/html/foo.html',
          [''], action='A'),
        MockAffectedFile('chrome/tests/data/accessibility/foo.html',
          [''], action='A')
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (0, len(msgs), msgs))

  # Test that only modifying an html file will not trigger the warning.
  def testIgnoreModifiedFiles(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/event/foo.html',
          [''], action='M')
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (0, len(msgs), msgs))

  # Test that deleting an html file will trigger the warning.
  def testAndroidChangeMissingOnDeletedFile(self):
    mock_input_api = MockInputApi()

    mock_input_api.files = [
        MockAffectedFile('content/test/data/accessibility/event/foo.html',
          [], action='D')
    ]

    msgs = PRESUBMIT.CheckAccessibilityEventsTestIncludesAndroid(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(msgs),
                     'Expected %d messages, found %d: %s'
                     % (1, len(msgs), msgs))

if __name__ == '__main__':
  unittest.main()