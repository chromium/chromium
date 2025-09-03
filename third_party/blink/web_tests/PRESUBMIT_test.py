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

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
sys.path.append(_DIR_SOURCE_ROOT)

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockAffectedFile


class PresubmitTest(unittest.TestCase):
    def testTestsCorrespondingToAffectedBaselines(self):
        D = 'third_party/blink/web_tests'
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            MockAffectedFile(f'{D}/subdir/without-behavior-change.html', []),
            MockAffectedFile(f'{D}/subdir/with-behavior-change.html', []),
            MockAffectedFile(f'{D}/subdir/with-variants.html', []),
            MockAffectedFile(f'{D}/subdir/multiglobals.any.js', []),
            MockAffectedFile(
                f'{D}/flag-specific/fake-flag/'
                'subdir/with-behavior-change-expected.txt', []),
            MockAffectedFile(
                f'{D}/platform/fake-platform/virtual/fake-suite/'
                'subdir/with-variants_query-param-expected.txt', []),
            MockAffectedFile(f'{D}/subdir/multiglobals.any-expected.txt', []),
            MockAffectedFile(
                f'{D}/subdir/multiglobals.any.worker-expected.txt', []),
        ])
        tests = PRESUBMIT._TestsCorrespondingToAffectedBaselines(
            mock_input_api)
        self.assertEqual(
            set(tests),
            {
                os.path.normpath('subdir/multiglobals.any.js'),
                os.path.normpath('subdir/with-behavior-change.html'),
                # `with-variants.html` is not checked; see inline comments.
            })

    def testCheckForDoctypeHTML(self):
        """This verifies that we correctly identify missing DOCTYPE html tags.
        """
        D = 'third_party/blink/web_tests/external/wpt'
        file1 = MockAffectedFile(f"{D}/some/dir/file1.html", [
            "<!DOCTYPE html>", "<html>", "<body>", "<p>Test</p>", "</body>",
            "</html>"
        ])
        file2 = MockAffectedFile(
            "some/dir2/file2.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file3 = MockAffectedFile(f"{D}/file3.html", [
            "<!--Some comment-->", "<!docTYPE    htML>", "<html>", "<body>",
            "<p>Test</p>", "</body>", "</html>"
        ])
        file4 = MockAffectedFile("dir/file4.html",
                                 ["<script></script>", "<!DOCTYPE html>"])
        file5 = MockAffectedFile("file5.html", [])
        file6 = MockAffectedFile(
            f"{D}/file6.not_html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file7 = MockAffectedFile(f"{D}/file7.html", [
            "<!DOCTYPE html   >", "<html>", "<body>", "<p>Test</p>", "</body>",
            "</html>"
        ])
        file8 = MockAffectedFile("file8.html", [
            "<!DOCTYPE html FOOBAR>", "<html>", "<body>", "<p>Test</p>",
            "</body>", "</html>"
        ])
        file9 = MockAffectedFile(
            f"{D}/some/dir/quirk-file9.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        file10 = MockAffectedFile(
            f"{D}/old/file10.html",
            ["<html>", "<body>", "<p>New content</p>", "</body>", "</html>"],
            ["<html>", "<body>", "<p>Old content</p>", "</body>", "</html>"],
            action="M")

        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            file1, file2, file3, file4, file5, file6, file7, file8, file9,
            file10
        ])
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
        D = 'third_party/blink/web_tests/external/wpt'
        error_file = MockAffectedFile(
            f"{D}/external/wpt/some/dir/doctype_error.html",
            ["<html>", "<body>", "<p>Test</p>", "</body>", "</html>"])
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([error_file])

        messages = PRESUBMIT._CheckForDoctypeHTML(mock_input_api,
                                                  MockOutputApi())

        self.assertEqual(1, len(messages))
        self.assertEqual("warning", messages[0].type)

    def testCheckForUnlistedDirsInBuildGn(self):
        mock_input_api = MockInputApi()
        web_tests_dir = os.path.join('third_party', 'blink', 'web_tests')
        build_file = MockAffectedFile(os.path.join(
            web_tests_dir, 'BUILD.gn'), [
                'group("web_tests") {',
                '  data = [',
                '    # === List Test Cases folders here ===',
                '    "a/"',
                '',
                '    # === List Case Folders Ends ===',
                '  ]'
                '}',
            ])
        mock_input_api.InitFiles([
            build_file,
            MockAffectedFile(os.path.join(web_tests_dir, 'a', 'a.html'), []),
            MockAffectedFile(os.path.join(web_tests_dir, 'b', 'b.html'), []),
            MockAffectedFile(
                os.path.join(web_tests_dir, 'platform', 'c', 'c-expected.txt'),
                []),
        ])

        messages = PRESUBMIT._CheckForUnlistedTestFolder(
            mock_input_api, MockOutputApi())
        self.assertEqual(1, len(messages))
        self.assertEqual(['b'], messages[0].items)
        self.assertRegex(messages[0].message, r'Please add b to BUILD\.gn')


if __name__ == "__main__":
    unittest.main()
