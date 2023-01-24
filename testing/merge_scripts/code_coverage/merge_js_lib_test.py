#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import mock
import os
import shutil
import tempfile
import unittest

import merge_js_lib as merger


class MergeJSLibTest(unittest.TestCase):
    def test_single_func_invocation_merge(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 50,
            'count': 1
        }, {
            'startOffset': 15,
            'endOffset': 49,
            'count': 1
        }, {
            'startOffset': 41,
            'endOffset': 46,
            'count': 0
        }]

        expected_output_segments = [{
            'end': 41,
            'count': 1
        }, {
            'end': 46,
            'count': 0
        }, {
            'end': 50,
            'count': 1
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_multiple_subtractive_ranges(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 151,
            'count': 1
        }, {
            'startOffset': 15,
            'endOffset': 151,
            'count': 2
        }, {
            'startOffset': 85,
            'endOffset': 114,
            'count': 0
        }, {
            'startOffset': 119,
            'endOffset': 145,
            'count': 0
        }]

        expected_output_segments = [
            {
                'count': 1,
                'end': 15
            },
            {
                'count': 2,
                'end': 85
            },
            {
                'count': 0,
                'end': 114
            },
            {
                'count': 2,
                'end': 119
            },
            {
                'count': 0,
                'end': 145
            },
            {
                'count': 2,
                'end': 151
            },
        ]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_multiple_disjoint_segments_output_congiguous_ranges(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 119,
            'count': 1
        }, {
            'startOffset': 15,
            'endOffset': 58,
            'count': 1
        }, {
            'startOffset': 50,
            'endOffset': 55,
            'count': 0
        }, {
            'startOffset': 76,
            'endOffset': 119,
            'count': 1
        }, {
            'startOffset': 111,
            'endOffset': 116,
            'count': 0
        }]

        expected_output_segments = [{
            'count': 1,
            'end': 50
        }, {
            'count': 0,
            'end': 55
        }, {
            'count': 1,
            'end': 111
        }, {
            'count': 0,
            'end': 116
        }, {
            'count': 1,
            'end': 119
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_overlap_segments_output_disjoint(self):
        input_segments = [{
            'count': 1,
            'startOffset': 0,
            'endOffset': 100
        }, {
            'count': 2,
            'startOffset': 50,
            'endOffset': 100
        }]

        expected_output_segments = [{
            'count': 1,
            'end': 50
        }, {
            'count': 2,
            'end': 100
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_disjoint_segments_stay_disjoint(self):
        input_segments = [{
            'count': 1,
            'startOffset': 0,
            'endOffset': 100
        }, {
            'count': 2,
            'startOffset': 100,
            'endOffset': 200
        }]

        expected_output_segments = [{
            'count': 1,
            'end': 100
        }, {
            'count': 2,
            'end': 200
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_first_element_is_ignored(self):
        input_segments = [
            {
                'count': 5,
                'startOffset': 0,
                'endOffset': 1000
            },
            {
                'count': 7,
                'startOffset': 50,
                'endOffset': 100
            },
            {
                'count': 6,
                'startOffset': 150,
                'endOffset': 300
            },
            {
                'count': 0,
                'startOffset': 400,
                'endOffset': 500
            },
            {
                'count': 20,
                'startOffset': 600,
                'endOffset': 700
            },
        ]

        expected_output_segments = [{
            'count': 5,
            'end': 50
        }, {
            'count': 7,
            'end': 100
        }, {
            'count': 5,
            'end': 150
        }, {
            'count': 6,
            'end': 300
        }, {
            'count': 5,
            'end': 400
        }, {
            'count': 0,
            'end': 500
        }, {
            'count': 5,
            'end': 600
        }, {
            'count': 20,
            'end': 700
        }, {
            'count': 5,
            'end': 1000
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_nonoverlapping_segments_with_gaps(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 100,
            'count': 1,
        }, {
            'startOffset': 0,
            'endOffset': 10,
            'count': 10,
        }, {
            'startOffset': 90,
            'endOffset': 100,
            'count': 20,
        }]

        expected_output_segments = [{
            'count': 10,
            'end': 10
        }, {
            'count': 1,
            'end': 90
        }, {
            'count': 20,
            'end': 100
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_nonoverlapping_segments_not_from_zero(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 100,
            'count': 0
        }, {
            'startOffset': 10,
            'endOffset': 20,
            'count': 10
        }, {
            'startOffset': 90,
            'endOffset': 100,
            'count': 20
        }]

        expected_output_segments = [{
            'count': 0,
            'end': 10
        }, {
            'count': 10,
            'end': 20
        }, {
            'count': 0,
            'end': 90
        }, {
            'count': 20,
            'end': 100
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_overlapping_segments_present_disjoint_segments(self):
        input_segments = [{
            'startOffset': 0,
            'endOffset': 88,
            'count': 1
        }, {
            'startOffset': 34,
            'endOffset': 57,
            'count': 3
        }, {
            'startOffset': 46,
            'endOffset': 50,
            'count': 0
        }, {
            'startOffset': 69,
            'endOffset': 77,
            'count': 0
        }]

        expected_output_segments = [{
            'count': 1,
            'end': 34
        }, {
            'count': 3,
            'end': 46
        }, {
            'count': 0,
            'end': 50
        }, {
            'count': 3,
            'end': 57
        }, {
            'count': 1,
            'end': 69
        }, {
            'count': 0,
            'end': 77
        }, {
            'count': 1,
            'end': 88
        }]

        output_segments = merger._convert_to_disjoint_segments(input_segments)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_disjoint_segments_accumulate(self):
        segment_a = [{'count': 1, 'end': 100}, {'count': 2, 'end': 200}]

        segment_b = [{'count': 1, 'end': 100}, {'count': 2, 'end': 200}]

        expected_output_segments = [{
            'count': 2,
            'end': 100
        }, {
            'count': 4,
            'end': 200
        }]

        output_segments = merger._merge_segments(segment_a, segment_b)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_overlapping_segments_split_output(self):
        segment_a = [{
            'count': 1,
            'end': 100
        }, {
            'count': 2,
            'end': 200
        }, {
            'count': 5,
            'end': 300
        }]

        segment_b = [{'count': 1, 'end': 100}, {'count': 4, 'end': 250}]

        expected_output_segments = [{
            'count': 2,
            'end': 100
        }, {
            'count': 6,
            'end': 200
        }, {
            'count': 9,
            'end': 250
        }, {
            'count': 5,
            'end': 300
        }]

        output_segments = merger._merge_segments(segment_a, segment_b)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_nonoverlapping_segments_concatenate(self):
        segment_a = [{'count': 1, 'end': 100}, {'count': 2, 'end': 200}]

        segment_b = [{
            'count': 0,
            'end': 200
        }, {
            'count': 1,
            'end': 300
        }, {
            'count': 4,
            'end': 500
        }]

        expected_output_segments = [{
            'count': 1,
            'end': 100
        }, {
            'count': 2,
            'end': 200
        }, {
            'count': 1,
            'end': 300
        }, {
            'count': 4,
            'end': 500
        }]

        output_segments = merger._merge_segments(segment_a, segment_b)
        self.assertListEqual(output_segments, expected_output_segments)

    def test_write_parsed_scripts(self):
        test_files = [{
            'url': '//a/b/c/1.js',
            'location': ['a', 'b', 'c', '1.js'],
            'exists': True
        }, {
            'url': '//d/e/f/5.js',
            'location': ['d', 'e', 'f', '5.js'],
            'exists': True
        }, {
            'url': '//a/b/d/7.js',
            'location': ['a', 'b', 'd', '7.js'],
            'exists': True
        }, {
            'url': 'chrome://test_webui/file.js',
            'exists': False
        }, {
            'url': 'file://testing/file.js',
            'exists': False
        }]

        test_script_file = """{
"text": "test\\ncontents\\n%d",
"url": "%s",
"sourceMapURL":"%s"
}"""

        scripts_dir = None
        expected_files = []

        try:
            scripts_dir = tempfile.mkdtemp()
            for i, test_script in enumerate(test_files):
                file_path = os.path.join(scripts_dir, '%d.js.json' % i)

                source_map = ""
                if test_script['exists']:
                    # Create an inline sourcemap with just the required keys.
                    source_map_data_url = base64.b64encode(
                        json.dumps({
                            "sources":
                            [os.path.join(*test_script['location'])],
                            "sourceRoot":
                            ""
                        }).encode('utf-8'))

                    source_map = 'data:application/json;base64,' + \
                        source_map_data_url.decode('utf-8')

                with open(file_path, 'w') as f:
                    f.write(test_script_file %
                            (i, test_script['url'], source_map))

                expected_files.append(file_path)
                if test_script['exists']:
                    expected_files.append(
                        os.path.join(scripts_dir, 'parsed_scripts',
                                     *test_script['location']))

            if len(expected_files) > 0:
                expected_files.append(
                    os.path.join(scripts_dir, 'parsed_scripts',
                                 'parsed_scripts.json'))

            merger.write_parsed_scripts(scripts_dir, source_dir='')
            actual_files = []

            for root, _, files in os.walk(scripts_dir):
                for file_name in files:
                    actual_files.append(os.path.join(root, file_name))

            self.assertCountEqual(expected_files, actual_files)
        finally:
            shutil.rmtree(scripts_dir)

    def test_write_parsed_scripts_negative_cases(self):
        test_files = [{
            'url': '//a/b/c/1.js',
            'contents': """{
"url": "%s"
}"""
        }, {
            'url': '//d/e/f/1.js',
            'contents': """{
"text": "test\\ncontents\\n%s"
}"""
        }]

        scripts_dir = None
        expected_files = []
        try:
            scripts_dir = tempfile.mkdtemp()
            for i, test_script in enumerate(test_files):
                file_path = os.path.join(scripts_dir, '%d.js.json' % i)
                expected_files.append(file_path)
                with open(file_path, 'w') as f:
                    f.write(test_script['contents'] % test_script['url'])

            merger.write_parsed_scripts(scripts_dir)

            actual_files = []
            for root, _, files in os.walk(scripts_dir):
                for file_name in files:
                    actual_files.append(os.path.join(root, file_name))

            self.assertCountEqual(expected_files, actual_files)
        finally:
            shutil.rmtree(scripts_dir)

    def test_non_data_urls_are_ignored(self):
        test_script_file = """{
"text": "test\\ncontents",
"url": "http://test_url",
"sourceMapURL":"%s"
}"""

        scripts_dir = None
        expected_files = []

        try:
            scripts_dir = tempfile.mkdtemp()
            file_path = os.path.join(scripts_dir, 'external_map.js.json')
            expected_files = [file_path]

            # Write a script with an external URL as the sourcemap, this should
            # exclude it from being written to disk.
            with open(file_path, 'w') as f:
                f.write(test_script_file % 'external.map')

            merger.write_parsed_scripts(scripts_dir, source_dir='')
            actual_files = []

            for root, _, files in os.walk(scripts_dir):
                for file_name in files:
                    actual_files.append(os.path.join(root, file_name))

            self.assertCountEqual(expected_files, actual_files)
        finally:
            shutil.rmtree(scripts_dir)

    def test_uninteresting_lines_are_excluded(self):
        """This contrived istanbul coverage file represents the coverage from
        the following example file:
        """
        example_test_file = """// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './iframe.js';
export const add = (a, b) => a + b;
"""

        test_istanbul_file = """{
"%s":{
  "path":"%s",
  "all":false,
  "statementMap":{
    "1":{"start":{"line":1,"column":0},"end":{"line":1,"column":38}},
    "2":{"start":{"line":2,"column":0},"end":{"line":2,"column":73}},
    "3":{"start":{"line":3,"column":0},"end":{"line":3,"column":29}},
    "4":{"start":{"line":4,"column":0},"end":{"line":4,"column":0}},
    "5":{"start":{"line":5,"column":0},"end":{"line":5,"column":21}},
    "6":{"start":{"line":6,"column":0},"end":{"line":6,"column":35}}
  },
  "s":{
    "1": 1,
    "2": 1,
    "3": 1,
    "4": 1,
    "5": 1,
    "6": 1
  }
}
        }"""

        expected_output_file = """{
            "%s": {
                "path": "%s",
                "all": false,
                "statementMap": {
                    "6": {
                        "start": {
                            "line": 6,
                            "column": 0
                        },
                        "end": {
                            "line": 6,
                            "column": 35
                        }
                    }
                },
                "s": {
                    "6": 1
                }
            }
        }"""

        try:
            test_dir = tempfile.mkdtemp()
            file_path = os.path.join(test_dir,
                                     'coverage.json').replace('\\', '/')
            example_test_file_path = os.path.join(test_dir,
                                                  'fileA.js').replace(
                                                      '\\', '/')
            expected_output = json.loads(
                expected_output_file %
                (example_test_file_path, example_test_file_path))

            # Set up the tests files so that exclusions can be performed.
            with open(file_path, 'w') as f:
                f.write(test_istanbul_file %
                        (example_test_file_path, example_test_file_path))
            with open(example_test_file_path, 'w') as f:
                f.write(example_test_file)

            # Perform the exclusion.
            merger.exclude_uninteresting_lines(file_path)

            # Assert the final `coverage.json` file matches the expected output.
            with open(file_path, 'rb') as f:
                coverage_json = json.load(f)
                self.assertEqual(coverage_json, expected_output)

        finally:
            shutil.rmtree(test_dir)


if __name__ == '__main__':
    unittest.main()
