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
            MockFile("content/test/data/accessibility/event/bar-expected-android-exp.txt", []),
            MockFile("content/test/data/accessibility/accname/qux-expected-auralinux.txt", []),
            MockFile("content/test/data/accessibility/accname/qux-expected-auralinux-2.txt", []),
            MockFile("content/test/data/accessibility/event/bar-expected-blink.txt", []),
            MockFile("content/test/data/accessibility/accname/qux-expected-blink-cros.txt", []),
            MockFile("content/test/data/accessibility/accname/qux-expected-fuchsia.txt", []),
            MockFile("content/test/data/accessibility/css/foobar-expected-mac.txt", []),
            MockFile("content/test/data/accessibility/css/foobar-expected-mac-before-11.txt", []),
            MockFile("content/test/data/accessibility/tree/barbaz-expected-uia-win.txt", []),
            MockFile("content/test/data/accessibility/table/quxfoo-expected-win.txt", []),

            # Files in the /mac/ and /win/ia2 sub-directories end in "-expected.txt".
            MockFile("content/test/data/accessibility/mac/barbaz-expected.txt", []),
            MockFile("content/test/data/accessibility/win/ia2/quxfoo-expected.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenames(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test that newly added or modified files with incorrect naming are flagged,
    # but only text files, and not deletions.
    def testInvalidFilenames(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo-expected-android-foo.txt", [], action='A'),
            MockFile("content/test/data/accessibility/event/bar-expected-ios.txt", [], action='A'),
            MockFile("content/test/data/accessibility/html/baz-actual-android.txt", [], action='M'),
            MockFile("content/test/data/accessibility/html/baz-actual-android.txt", [], action='D'),
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


class CheckAccessibilityTestExpectationFilenamesMacWinTest(unittest.TestCase):

    # Test that files with the correct naming are not flagged,
    # nor are modified files.
    def testValidFilenames(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/mac/foo-expected.txt", []),
            MockFile("content/test/data/accessibility/win/bar-expected.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenamesMacWin(
            mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test that newly added or modified files with incorrect naming are flagged,
    # but only text files, and not deletions.
    def testInvalidFilenames(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/mac/foo-expected-mac.txt", [], action='A'),
            MockFile("content/test/data/accessibility/win/bar-expected-win.txt", [], action='M'),
            MockFile("content/test/data/accessibility/win/bar-expected-win.txt", [], action='D'),
            MockFile("content/test/data/accessibility/mac/foo-expected-mac.html", [], action='A'),
            MockFile("content/test/data/accessibility/win/bar-expected-win.html", [], action='A'),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenamesMacWin(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(2, len(results[0].items))
        self.assertIn("foo-expected-mac.txt", results[0].items[0])
        self.assertIn("bar-expected-win.txt", results[0].items[1])

    # Test files outside the /mac/ or /win/ sub-directories are not relevant.
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/foo.txt", []),
            MockFile("foo/mac/bar.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityTestExpectationFilenamesMacWin(
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


# TODO(accessibility): These tests work fine locally but not on try bots. Find
# a solution to this so the test can be enabled.
@unittest.skip("These read files directly and don't play nicely with bots.")
class AccessibilityHtmlFileTestTest(unittest.TestCase):

    ########################################
    # Tests that verify no false positives #
    ########################################

    # Test no warning if html filename is included in test suite for /node/
    def testValidReferencesForNode(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/accname/foo.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_node_browsertest.cc", ["foo.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if html filename is included in test suite for /mac/
    def testValidReferencesForMac(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/mac/foobar.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_scripts_browsertest.cc", ["foobar.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test Android does not care about /mac/
    def testValidReferencesForMacWithAndroid(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/mac/foobar.html", [], action='A'),
            MockFile("content/test/data/accessibility/mac/foobar-expected-android.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_scripts_browsertest.cc", ["foobar.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if html filename is included in test suite for /event/
    def testValidReferencesForEvent(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/event/bar.html", [], action='A'),
            MockFile("content/test/data/accessibility/event/bar-expected-win.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_events_browsertest.cc", ["bar.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if html filename is included in test suite for /event/ with Android
    def testValidReferencesForEventWithAndroid(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/event/bar.html", [], action='A'),
            MockFile("content/test/data/accessibility/event/bar-expected-android.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_events_browsertest.cc", ["bar.html"]),
            MockFile("content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityEventsTest.java", ["bar.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if html filename is included in test suite for any other directory
    def testValidReferencesForOtherFolder(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/foo/asdf.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_tree_browsertest.cc", ["asdf.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test Android does not care about any other directory
    def testValidReferencesForOtherFolderWithAndroid(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/foo/asdf.html", [], action='A'),
            MockFile("content/test/data/accessibility/foo/asdf-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_tree_browsertest.cc", ["asdf.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if html filename is included in test suite for other Android relevant directories
    def testValidReferencesForInterestingFoldersForAndroid(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/accname/foo.html", [], action='A'),
            MockFile("content/test/data/accessibility/accname/foo-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_node_browsertest.cc", [
                "foo.html",
            ], action='A'),
            MockFile("content/test/data/accessibility/css/bar.html", [], action='A'),
            MockFile("content/test/data/accessibility/css/bar-expected-android.txt", [], action='A'),
            MockFile("content/test/data/accessibility/aria/baz.html", [], action='A'),
            MockFile("content/test/data/accessibility/aria/baz-expected-android-assist-data.txt", [], action='A'),
            MockFile("content/test/data/accessibility/html/beep.html", [], action='A'),
            MockFile("content/test/data/accessibility/html/beep-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_tree_browsertest.cc", [
                "foo.html",
                "bar.html",
                "baz.html",
                "beep.html",
                ]),
            MockFile("content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityTreeTest.java", [
                "foo.html"
                "bar.html"
                "baz.html"
                "beep.html"
                ]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning for unrelated files and directories
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/foo.html", [], action='A'),
            MockFile("foo/bar.html", [], action='A'),
            MockFile("foo/bar-expected-android.txt", [], action='A'),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    ########################################
    #  Tests that verify correct warnings  #
    ########################################

    # Test warning when no filename added in /node/
    def testMissingReferenceForNode(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/accname/foo.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_node_browsertest.cc", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertIn("foo.html", results[0].items[0])
        self.assertIn("dump_accessibility_node_browsertest.cc", results[0].items[0])

    # Test warning when no filename added in /mac/
    def testMissingReferenceForMac(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/mac/foo.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_scripts_browsertest.cc", ["foobaz.html"]),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertIn("foo.html", results[0].items[0])
        self.assertIn("dump_accessibility_scripts_browsertest.cc", results[0].items[0])

    # Test warning when no filename added in other subdirectory
    def testMissingReferenceForOtherFolder(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/other/bar.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_tree_browsertest.cc", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertIn("bar.html", results[0].items[0])
        self.assertIn("dump_accessibility_tree_browsertest.cc", results[0].items[0])

    # Test warning when no filename added in /event/
    def testMissingReferenceForEvent(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/event/bar.html", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_events_browsertest.cc", []),
            MockFile("content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityEventsTest.java", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(results[0].items))
        self.assertIn("bar.html", results[0].items[0])
        self.assertIn("dump_accessibility_events_browsertest.cc", results[0].items[0])
        self.assertNotIn("WebContentsAccessibilityEventsTest.java", results[0].items[0])

    # Test warning when no filename added in /event/ with Android
    def testMissingReferenceForEventWithAndroid(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/event/bar-with-dashes.html", [], action='A'),
            MockFile("content/test/data/accessibility/event/bar-with-dashes-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_events_browsertest.cc", []),
            MockFile("content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityEventsTest.java", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(2, len(results[0].items))
        self.assertIn("bar-with-dashes.html", results[0].items[0])
        self.assertNotIn("dump_accessibility_events_browsertest.cc", results[0].items[0])
        self.assertIn("WebContentsAccessibilityEventsTest.java", results[0].items[0])
        self.assertEqual(2, len(results[0].items))
        self.assertIn("bar-with-dashes.html", results[0].items[1])
        self.assertIn("dump_accessibility_events_browsertest.cc", results[0].items[1])
        self.assertNotIn("WebContentsAccessibilityEventsTest.java", results[0].items[1])

    # Test warning when no filename added in interesting directories for Android
    def testMissingReferenceForInterestingAndroidFolder(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/accname/foo.html", [], action='A'),
            MockFile("content/test/data/accessibility/accname/foo-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_node_browsertest.cc", [
                "foo.html",
            ], action='A'),
            MockFile("content/test/data/accessibility/css/bar.html", [], action='A'),
            MockFile("content/test/data/accessibility/css/bar-expected-android-external.txt", [], action='A'),
            MockFile("content/test/data/accessibility/aria/baz.html", [], action='A'),
            MockFile("content/test/data/accessibility/aria/baz-expected-android-assist-data.txt", [], action='A'),
            MockFile("content/test/data/accessibility/html/beep.html", [], action='A'),
            MockFile("content/test/data/accessibility/html/beep-expected-android-external.txt", [], action='A'),
            MockFile("content/browser/accessibility/dump_accessibility_tree_browsertest.cc", [
                "foo.html",
                "bar.html",
                "baz.html",
                "beep.html",
            ]),
            MockFile("content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityTreeTest.java", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlFileTest(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(4, len(results[0].items))
        self.assertIn("foo.html", results[0].items[0])
        self.assertNotIn("dump_accessibility_tree_browsertest.cc", results[0].items[0])
        self.assertIn("WebContentsAccessibilityTreeTest.java", results[0].items[0])
        self.assertIn("bar.html", results[0].items[1])
        self.assertNotIn("dump_accessibility_tree_browsertest.cc", results[0].items[1])
        self.assertIn("WebContentsAccessibilityTreeTest.java", results[0].items[1])
        self.assertIn("baz.html", results[0].items[2])
        self.assertNotIn("dump_accessibility_tree_browsertest.cc", results[0].items[2])
        self.assertIn("WebContentsAccessibilityTreeTest.java", results[0].items[2])
        self.assertIn("beep.html", results[0].items[3])
        self.assertNotIn("dump_accessibility_tree_browsertest.cc", results[0].items[3])
        self.assertIn("WebContentsAccessibilityTreeTest.java", results[0].items[3])


class CheckAccessibilityHtmlExpectationsPairTest(unittest.TestCase):

    # Test no warning if valid pairs exist
    def testValidPairs(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo.html", []),
            MockFile("content/test/data/accessibility/aria/foo-expected-android.txt", []),
            MockFile("content/test/data/accessibility/event/bar.html", []),
            MockFile("content/test/data/accessibility/event/bar-expected-win.txt", []),
            MockFile("content/test/data/accessibility/other/same.html", []),
            MockFile("content/test/data/accessibility/other/same-expected-uia.txt", []),
            MockFile("content/test/data/accessibility/other/same-expected-blink.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlExpectationsPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test no warning if simply modifying files
    def testModifyNotChecked(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo.html", [], action='M'),
            MockFile("content/test/data/accessibility/event/bar-expected-win.txt", [], action='M'),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlExpectationsPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test warnings raised if any pairing is missing
    def testInvalidPairs(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/foo.html", []),
            MockFile("content/test/data/accessibility/event/bar-expected-win.txt", []),
            MockFile("content/test/data/accessibility/other/baz.html", []),
            MockFile("content/test/data/accessibility/other/same-expected-chromium.txt", []),
            MockFile("content/test/data/accessibility/accname/beep.html", []),
            MockFile("content/test/data/accessibility/accname/beep-android.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlExpectationsPair(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(5, len(results[0].items))
        self.assertIn("foo.html", results[0].items[0])
        self.assertIn("missing corresponding -expected-*.txt", results[0].items[0])
        self.assertIn("baz.html", results[0].items[1])
        self.assertIn("missing corresponding -expected-*.txt", results[0].items[1])
        self.assertIn("beep.html", results[0].items[2])
        self.assertIn("missing corresponding -expected-*.txt", results[0].items[2])
        self.assertIn("bar-expected-win.txt", results[0].items[3])
        self.assertIn("missing corresponding .html", results[0].items[3])
        self.assertIn("same-expected-chromium.txt", results[0].items[4])
        self.assertIn("missing corresponding .html", results[0].items[4])

    # Test no warnings for unrelated files/directories
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/foo.html", []),
            MockFile("foo/bar-expected-foo.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlExpectationsPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

     # Test no warnings for frames, which are referenced in other tests but
     # are not tested directly themselves.
    def testFrameFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/frames/foo.html", []),
            MockFile("content/test/data/accessibility/aria/frames/some-expectation.txt", []),
            MockFile("content/test/data/accessibility/html/frame/baz.html", []),
            MockFile("content/test/data/accessibility/html/frame/same-expected-chromium.txt", []),
        ]
        results = PRESUBMIT.CheckAccessibilityHtmlExpectationsPair(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))



class CheckFrameHtmlFilesDontHaveExpectations(unittest.TestCase):

    # Test no warning if no text files exist
    def testNoExpectationFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/frames/foo.html", []),
            MockFile("content/test/data/accessibility/html/frame/bar.html", []),
        ]
        results = PRESUBMIT.CheckFrameHtmlFilesDontHaveExpectations(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    # Test warnings raised if any expectation file exists
    def testExpectationFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/data/accessibility/aria/frames/foo.html", []),
            MockFile("content/test/data/accessibility/aria/frames/foo-expected-blink.txt", [], action='A'),
            MockFile("content/test/data/accessibility/html/frame/bar.html", []),
            MockFile("content/test/data/accessibility/html/frame/bar-expected-android.txt", [], action='M'),
        ]
        results = PRESUBMIT.CheckFrameHtmlFilesDontHaveExpectations(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual(2, len(results[0].items))
        self.assertIn("foo-expected-blink.txt", results[0].items[0])
        self.assertIn("bar-expected-android.txt", results[0].items[1])

    # Test no warnings for unrelated files/directories
    def testNonMatchingFiles(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockFile("content/test/aria/frames/foo-expected-blink.txt", [], action='A'),
            MockFile("html/frame/foo/bar-expected-foo.txt", [], action='A'),
        ]
        results = PRESUBMIT.CheckFrameHtmlFilesDontHaveExpectations(mock_input_api, MockOutputApi())
        self.assertEqual(0, len(results))


if __name__ == '__main__':
    unittest.main()
