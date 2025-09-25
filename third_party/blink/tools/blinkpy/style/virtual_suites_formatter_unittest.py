# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from blinkpy.style.virtual_suites_formatter import format_json_with_comments


class VirtualSuitesFormatterTest(unittest.TestCase):

    def test_sort_simple(self):
        """Tests basic sorting of objects and comments."""
        input_data = [
            "comment for b",
            {
                "prefix": "b"
            },
            "comment for a",
            {
                "prefix": "a"
            },
        ]
        expected_output = [
            "comment for a",
            {
                "prefix": "a"
            },
            "comment for b",
            {
                "prefix": "b"
            },
        ]
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), expected_output)

    def test_with_header_comments(self):
        """Tests that header comments are preserved."""
        input_data = [
            "header1", "header2", "__BEGIN_SUITES__", "comment for b", {
                "prefix": "b"
            }, "comment for a", {
                "prefix": "a"
            }
        ]
        expected_output = [
            "header1", "header2", "__BEGIN_SUITES__", "comment for a", {
                "prefix": "a"
            }, "comment for b", {
                "prefix": "b"
            }
        ]
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), expected_output)

    def test_multiline_comment_before_suite(self):
        """Tests that multi-line comments for a suite are not separated."""
        input_data = [
            "header1", "__BEGIN_SUITES__", "comment for b line 1",
            "comment for b line 2", {
                "prefix": "b"
            }, "comment for a", {
                "prefix": "a"
            }
        ]
        expected_output = [
            "header1", "__BEGIN_SUITES__", "comment for a", {
                "prefix": "a"
            }, "comment for b line 1", "comment for b line 2", {
                "prefix": "b"
            }
        ]
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), expected_output)

    def test_empty_list(self):
        """Tests an empty list."""
        input_data = []
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), [])

    def test_only_comments(self):
        """Tests a list with only comments."""
        input_data = ["comment1", "comment2"]
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), input_data)

    def test_only_objects(self):
        """Tests a list with only objects."""
        input_data = [{"prefix": "b"}, {"prefix": "a"}]
        expected_output = [{"prefix": "a"}, {"prefix": "b"}]
        sorted_json = format_json_with_comments(input_data)
        self.assertEqual(json.loads(sorted_json), expected_output)

