#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import shutil
import tempfile
import unittest

import merge_js_lib as merger
from parameterized import parameterized


class MergeJSLibTest(unittest.TestCase):

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

        source_map = ''
        if test_script['exists']:
          # Create an inline sourcemap with just the required keys.
          source_map_data_url = base64.b64encode(
              json.dumps({
                  'sources': [os.path.join(*test_script['location'])],
                  'sourceRoot': ''
              }).encode('utf-8'))

          source_map = 'data:application/json;base64,' + \
              source_map_data_url.decode('utf-8')

        with open(file_path, 'w') as f:
          f.write(test_script_file % (i, test_script['url'], source_map))

        expected_files.append(file_path)
        if test_script['exists']:
          expected_files.append(
              os.path.join(scripts_dir, 'parsed_scripts',
                           *test_script['location']))

      if len(expected_files) > 0:
        expected_files.append(
            os.path.join(scripts_dir, 'parsed_scripts', 'parsed_scripts.json'))

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

  def test_trailing_curly_brace_stripped(self):
    test_script_file = """{
  "text":"test\\ncontents\\n0",
  "url":"//a/b/c/1.js",
  "sourceMapURL":"data:application/json;base64,eyJzb3VyY2VzIjogWyJhL2IvYy8xLmpzIl0sICJzb3VyY2VSb290IjogIiJ9"
}}"""

    scripts_dir = None

    try:
      scripts_dir = tempfile.mkdtemp()
      file_path = os.path.join(scripts_dir, '0.js.json')
      with open(file_path, 'w') as f:
        f.write(test_script_file)
      expected_files = [
          file_path,
          os.path.join(scripts_dir, 'parsed_scripts', 'a', 'b', 'c', '1.js'),
          os.path.join(scripts_dir, 'parsed_scripts', 'parsed_scripts.json')
      ]

      merger.write_parsed_scripts(scripts_dir, source_dir='')
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

/*
 * function comment should be excluded.
 */
export const add = (a, b) => a + b; // should not be excluded

/* should be excluded */

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
    "6":{"start":{"line":6,"column":0},"end":{"line":6,"column":0}},
    "7":{"start":{"line":7,"column":0},"end":{"line":7,"column":2}},
    "8":{"start":{"line":8,"column":0},"end":{"line":8,"column":39}},
    "9":{"start":{"line":9,"column":0},"end":{"line":9,"column":3}},
    "10":{"start":{"line":10,"column":0},"end":{"line":10,"column":61}},
    "11":{"start":{"line":11,"column":0},"end":{"line":11,"column":0}},
    "12":{"start":{"line":12,"column":0},"end":{"line":12,"column":24}},
    "13":{"start":{"line":13,"column":0},"end":{"line":13,"column":0}}
  },
  "s":{
    "1": 1,
    "2": 1,
    "3": 1,
    "4": 1,
    "5": 1,
    "6": 1,
    "7": 1,
    "8": 1,
    "9": 1,
    "10": 1,
    "11": 1,
    "12": 1,
    "13": 1
  }
}
        }"""

    expected_output_file = """{
            "%s": {
                "path": "%s",
                "all": false,
                "statementMap": {
                    "10": {
                        "start": {
                            "line": 10,
                            "column": 0
                        },
                        "end": {
                            "line": 10,
                            "column": 61
                        }
                    }
                },
                "s": {
                    "10": 1
                }
            }
        }"""

    try:
      test_dir = tempfile.mkdtemp()
      file_path = os.path.join(test_dir, 'coverage.json').replace('\\', '/')
      example_test_file_path = os.path.join(test_dir,
                                            'fileA.js').replace('\\', '/')
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

  def test_paths_are_remapped_and_removed(self):
    test_file_data = """{
          "/path/to/checkout/chrome/browser/fileA.js": {
            "path": "/path/to/checkout/chrome/browser/fileA.js"
          },
          "/path/to/checkout/out/dir/chrome/browser/fileB.js": {
            "path": "/path/to/checkout/out/dir/chrome/browser/fileB.js"
          },
          "/some/random/path/fileC.js": {
            "path": "/some/random/path/fileC.js"
          }
        }"""

    expected_after_remap = {
        'chrome/browser/fileA.js': {
            'path': 'chrome/browser/fileA.js'
        }
    }

    try:
      test_dir = tempfile.mkdtemp()
      coverage_file_path = os.path.join(test_dir,
                                        'coverage.json').replace('\\', '/')

      with open(coverage_file_path, 'w', encoding='utf-8', newline='') as f:
        f.write(test_file_data)

      merger.remap_paths_to_relative(coverage_file_path, '/path/to/checkout',
                                     '/path/to/checkout/out/dir')

      with open(coverage_file_path, 'rb') as f:
        coverage_json = json.load(f)
        self.assertEqual(coverage_json, expected_after_remap)

    finally:
      shutil.rmtree(test_dir)

  @parameterized.expand([
      ('// test', True),
      ('/* test', True),
      ('*/ test', True),
      (' * test', True),
      ('import test', True),
      (' x = 5 /* comment */', False),
      ('x = 5', False),
  ])
  def test_should_exclude(self, line, exclude):
    self.assertEqual(merger.should_exclude(line), exclude)


if __name__ == '__main__':
  unittest.main()
