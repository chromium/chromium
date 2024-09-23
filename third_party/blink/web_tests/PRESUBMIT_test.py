#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import functools
import json
import unittest
import sys
import os
import posixpath
from unittest import mock
import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockAffectedFile


class PresubmitTest(unittest.TestCase):
    def testTestsCorrespondingToAffectedBaselines(self):
        test_files = [
            '/chromium/src/third_party/blink/web_tests/'
            'path/to/test-without-behavior-change.html',
            '/chromium/src/third_party/blink/web_tests/'
            'path/to/test-with-behavior-change.html',
            '/chromium/src/third_party/blink/web_tests/'
            'path/to/test-with-variants.html',
            '/chromium/src/third_party/blink/web_tests/'
            'path/to/test-multiglobals.any.js',
        ]
        mock_input_api = MockInputApi()
        mock_input_api.os_path = posixpath
        mock_input_api.presubmit_local_path = '/chromium/src/third_party/blink/web_tests'
        mock_input_api.glob = mock.Mock(
            side_effect=functools.partial(fnmatch.filter, test_files))
        mock_input_api.files = [
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'path/to/test-without-behavior-change.html', []),
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'path/to/test-with-behavior-change.html', []),
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'flag-specific/fake-flag/'
                'path/to/test-with-behavior-change-expected.txt', []),
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'platform/fake-platform/virtual/fake-suite/'
                'path/to/test-with-variants_query-param-expected.txt', []),
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'path/to/test-multiglobals.any-expected.txt', []),
            MockAffectedFile(
                '/chromium/src/third_party/blink/web_tests/'
                'path/to/test-multiglobals.any.worker-expected.txt', []),
        ]
        tests = PRESUBMIT._TestsCorrespondingToAffectedBaselines(
            mock_input_api)
        self.assertEqual(
            set(tests),
            {
                'path/to/test-multiglobals.any.js',
                'path/to/test-with-behavior-change.html',
                # `path/to/test-with-variants.html` is not checked; see inline
                # comments.
            })

    def testCheckForDoctypeHTML(self):
        """This verifies that we correctly identify missing DOCTYPE html tags.
        """

        file1 = MockAffectedFile("some/dir/file1.html", [
            "<!DOCTYPE html>", "<html>", "<body>", "<p>Test</p>", "</body>",
            "</html>"
        ])
        file2 = MockAffectedFile(
            "some/dir2/file2.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file3 = MockAffectedFile("file3.html", [
            "<!--Some comment-->", "<!docTYPE    htML>", "<html>", "<body>",
            "<p>Test</p>", "</body>", "</html>"
        ])
        file4 = MockAffectedFile("dir/file4.html",
                                 ["<script></script>", "<!DOCTYPE html>"])
        file5 = MockAffectedFile("file5.html", [])
        file6 = MockAffectedFile(
            "file6.not_html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file7 = MockAffectedFile("file7.html", [
            "<!DOCTYPE html   >", "<html>", "<body>", "<p>Test</p>", "</body>",
            "</html>"
        ])
        file8 = MockAffectedFile("file8.html", [
            "<!DOCTYPE html FOOBAR>", "<html>", "<body>", "<p>Test</p>",
            "</body>", "</html>"
        ])
        file9 = MockAffectedFile(
            "some/dir/quirk-file9.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file10 = MockAffectedFile(
            "old/file10.html",
            ["<html>", "<body>", "<p>New content</p>", "</body>", "</html>"],
            ["<html>", "<body>", "<p>Old content</p>", "</body>", "</html>"],
            action="M")

        mock_input_api = MockInputApi()
        mock_input_api.files = [
            file1, file2, file3, file4, file5, file6, file7, file8, file9,
            file10
        ]
        messages = PRESUBMIT._CheckForDoctypeHTML(mock_input_api,
                                                  MockOutputApi())

        self.assertEqual(4, len(messages))
        for i, file in enumerate([file2, file4, file5, file8]):
            self.assertEqual("error", messages[i].type)
            self.assertIn("\"%s\"" % file.LocalPath(), messages[i].message)

    def testCheckForDoctypeHTMLExceptions(self):
        """This test makes sure that we don't raise <!DOCTYPE html> errors
        for WPT importer.
        """
        error_file = MockAffectedFile(
            "/chromium/src/third_party/blink/web_tests/"
            "external/wpt/some/dir/doctype_error.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        mock_input_api = MockInputApi()
        mock_input_api.os_path = posixpath
        mock_input_api.presubmit_local_path = "/chromium/src/third_party/blink/web_tests"
        mock_input_api.files = [error_file]

        messages = PRESUBMIT._CheckForDoctypeHTML(mock_input_api,
                                                  MockOutputApi())

        self.assertEqual(1, len(messages))
        self.assertEqual("warning", messages[0].type)


if __name__ == "__main__":
    unittest.main()
