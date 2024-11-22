#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import unittest
import sys

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..', '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi

class AccessibilityEventsTestsAreIncludedForAndroidTest(unittest.TestCase):
    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncluded(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/event/foo-expected-mac.txt',
                [''],
                action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityEventsTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required when no html file is added/removed.
    def testIgnoreNonHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/event/foo.txt',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.cc',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.h',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/event/foo.py',
                             [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required for unrelated html files.
    def testIgnoreNonRelatedHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('chrome/tests/data/accessibility/foo.html', [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that only modifying an html file will not trigger the warning.
    def testIgnoreModifiedFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/event/foo-expected-win.txt',
                [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityEventsTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))


class AccessibilityTreeTestsAreIncludedForAndroidTest(unittest.TestCase):
    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncluded(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityTreeTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that no warning is raised when the Android file is also modified.
    def testAndroidChangeIncludedManyFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/accname/foo.html', [''],
                action='A'),
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/css/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile(
                'accessibility/WebContentsAccessibilityTreeTest.java', [''],
                action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that a warning is raised when the Android file is not modified.
    def testAndroidChangeMissing(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile(
                'content/test/data/accessibility/aria/foo-expected-win.txt',
                [''],
                action='A'),
            MockAffectedFile(
                'content/test/data/accessibility/aria/foo-expected-blink.txt',
                [''],
                action='A'),
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            1, len(msgs),
            'Expected %d messages, found %d: %s' % (1, len(msgs), msgs))

    # Test that Android change is not required when no platform expectations files are changed.
    def testAndroidChangNotMissing(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/accname/foo.txt',
                             [''],
                             action='A'),
            MockAffectedFile(
                'content/test/data/accessibility/html/foo-expected-blink.txt',
                [''],
                action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/aria/foo.cc',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/css/foo.h', [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/tree/foo.py',
                             [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required when no blink expectations files are changed.
    def testAndroidChangNotMissing(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/accname/foo.txt',
                             [''],
                             action='A'),
            MockAffectedFile(
                'content/test/data/accessibility/html/foo-expected-win.txt',
                [''],
                action='A'),
            MockAffectedFile('content/test/data/accessibility/html/foo.html',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/aria/foo.cc',
                             [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/css/foo.h', [''],
                             action='A'),
            MockAffectedFile('content/test/data/accessibility/tree/foo.py',
                             [''],
                             action='A')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that Android change is not required for unrelated html files.
    def testIgnoreNonRelatedHtmlFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/event/foo.html',
                             [''],
                             action='A'),
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))

    # Test that only modifying an html file will not trigger the warning.
    def testIgnoreModifiedFiles(self):
        mock_input_api = MockInputApi()

        mock_input_api.files = [
            MockAffectedFile('content/test/data/accessibility/aria/foo.html',
                             [''],
                             action='M')
        ]

        msgs = PRESUBMIT.CheckAccessibilityTreeTestsAreIncludedForAndroid(
            mock_input_api, MockOutputApi())
        self.assertEqual(
            0, len(msgs),
            'Expected %d messages, found %d: %s' % (0, len(msgs), msgs))


if __name__ == '__main__':
    unittest.main()
