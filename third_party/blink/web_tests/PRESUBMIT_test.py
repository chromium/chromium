#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import sys
import os
import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockAffectedFile


class PresubmitTest(unittest.TestCase):
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

        mock_input_api = MockInputApi()
        mock_input_api.files = [
            file1, file2, file3, file4, file5, file6, file7, file8, file9
        ]
        errors = PRESUBMIT._CheckForDoctypeHTML(mock_input_api,
                                                MockOutputApi())

        self.assertEqual(4, len(errors))
        for i, file in enumerate([file2, file4, file5, file8]):
            self.assertIn("\"%s\"" % file.LocalPath(), errors[i].message)


if __name__ == "__main__":
    unittest.main()
