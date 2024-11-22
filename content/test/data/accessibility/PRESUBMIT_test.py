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
from PRESUBMIT_test_mocks import MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

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

    # Test Android change is not required when no html file is added/removed.
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

    # Test that Android change is not required when no platform expectations
    # files are changed.
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

    # Test that Android change is not required when no blink expectations
    # files are changed.
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


class CheckAccessibilityTestExpectationFilenamesTest(unittest.TestCase):

    # Test that files with the correct naming are not flagged,
    # nor are modified files.
    def testValidFilenames(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo-expected-android-external.txt", []),
            MockFile("content/test/data/accessibility/event/bar-expected-android.txt", []),
            MockFile("content/test/data/accessibility/html/baz-expected-android-assist-data.txt", []),
            MockFile("content/test/data/accessibility/accname/qux-expected-auralinux.txt", []),
            MockFile("content/test/data/accessibility/css/foobar-expected-mac.txt", []),
            MockFile("content/test/data/accessibility/tree/barbaz-expected-uia-win.txt", []),
            MockFile("content/test/data/accessibility/table/quxfoo-expected-win.txt", []),

             # Existing files don't require updating, so they are not checked.
            MockFile("content/test/data/accessibility/aria/existing_file_bad_name.txt", [], action='M'),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenames(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test that newly added files with incorrect naming are flagged,
    # but only text files.
    def testInvalidFilenames(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo-expected-android-foo.txt", [], action='A'),
            MockFile("content/test/data/accessibility/event/bar-expected-ios.txt", [], action='A'),
            MockFile("content/test/data/accessibility/html/baz-actual-android.txt", [], action='A'),
            MockFile("content/test/data/accessibility/event/invalid.txt", [], action='A'),
            MockFile("content/test/data/accessibility/event/invalid-expected-win.html", [], action='A'),
            MockFile("content/test/data/accessibility/event/invalid-actual-win.html", [], action='A'),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenames(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertIn("foo-expected-android-foo.txt", results[0].items[0])
        self.assertIn("bar-expected-ios.txt", results[0].items[1])
        self.assertIn("baz-actual-android.txt", results[0].items[2])
        self.assertIn("invalid.txt", results[0].items[3])

    # Test files outside the //content/test/data/accessibility directory
    # are not relevant.
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/foo.txt", []),
            MockFile("foo/bar.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenames(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


class CheckAccessibilityHtmlSvgPairTest(unittest.TestCase):

    # Test that files paired properly give no warning, and that if there
    # is only an html file there is no warning.
    def testValidPairs(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo.html", []),
            MockFile("content/test/data/accessibility/aria/foo.svg", []),
            MockFile("content/test/data/accessibility/event/bar.html", []),
            MockFile("content/test/data/accessibility/event/bar.svg", []),
            MockFile("content/test/data/accessibility/baz.html", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlSvgPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test that files with the same name and different directories give,
    # and files with different base names in the same directory do not.
    def testInvalidPairs(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            # Different directories, should give warning
            MockFile("content/test/data/accessibility/aria/foo.html", []),
            MockFile("content/test/data/accessibility/event/foo.svg", []),

            # Different base names, should not give warning
            MockFile("content/test/data/accessibility/aria/bar.html", []),
            MockFile("content/test/data/accessibility/aria/baz.svg", []),

            # Different directories, should give warning
            MockFile("content/test/data/accessibility/event/same.html", []),
            MockFile("content/test/data/accessibility/aria/same.svg", []),

        ]
        results = PRESUBMIT.CheckAccessibilityHtmlSvgPair(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(2, len(results[0].items))
        self.assertIn("content/test/data/accessibility/aria/foo.html", results[0].items[0])
        self.assertIn("content/test/data/accessibility/event/foo.svg", results[0].items[0])
        self.assertIn("content/test/data/accessibility/event/same.html", results[0].items[1])
        self.assertIn("content/test/data/accessibility/aria/same.svg", results[0].items[1])

    # Test that unrelated files in different directories do not give warning
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/foo.html", []),
            MockFile("foo/bar.svg", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlSvgPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


if __name__ == '__main__':
    unittest.main()
