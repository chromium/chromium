#!/usr/bin/env vpython
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import os
import shutil
import tempfile
import unittest

import merge_js_lib as merger

_MODULES_PATH = os.path.join(
    '..', '..', '..', 'third_party', 'node', 'node_modules', 'v8-to-istanbul')
_COVERAGE_MODULES_EXIST = os.path.exists(_MODULES_PATH)

class ConvertToIstanbulTest(unittest.TestCase):
  _TEST_SOURCE_A = """function add(a, b) {
  return a + b;
}

function subtract(a, b) {
  return a - b;
}

subtract(5, 2);
"""

  _TEST_COVERAGE_A = """{
  "result": [
    {
      "scriptId":"72",
      "url":"//file.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":101,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"add",
          "ranges":[
            {"startOffset":0,"endOffset":38,"count":0}
          ],
          "isBlockCoverage":false
        },
        {
          "functionName":"subtract",
          "ranges":[
            {"startOffset":40,"endOffset":83,"count":1}
          ],
          "isBlockCoverage":true
        }
      ]
    }
  ]
}
"""

  _TEST_COVERAGE_INVALID = """{
  "scriptId":"72",
  "url":"//file.js",
  "functions":[
    {
      "functionName":"",
      "ranges":[
        {"startOffset":0,"endOffset":101,"count":1}
      ],
      "isBlockCoverage":true
    },
    {
      "functionName":"add",
      "ranges":[
        {"startOffset":0,"endOffset":38,"count":0}
      ],
      "isBlockCoverage":false
    },
    {
      "functionName":"subtract",
      "ranges":[
        {"startOffset":40,"endOffset":83,"count":1}
      ],
      "isBlockCoverage":true
    }
  ]
}
"""

  _TEST_SOURCE_B = """const {subtract} = require('./test1.js');

function add(a, b) {
  return a + b;
}

subtract(5, 2);

"""

  _TEST_SOURCE_C = """exports.subtract = function(a, b) {
  return a - b;
}
"""

  _TEST_COVERAGE_B = """{
  "result":[
    {
      "scriptId":"72",
      "url":"//test.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":99,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"add",
          "ranges":[
            {"startOffset":43,"endOffset":81,"count":0}
          ],
          "isBlockCoverage":false
        }
      ]
    },
    {
      "scriptId":"73",
      "url":"//test1.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":54,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"exports.subtract",
          "ranges":[
            {"startOffset":19,"endOffset":53,"count":1}
          ],
          "isBlockCoverage":true
        }
      ]
    }
  ]
}
"""

  _TEST_COVERAGE_NO_LEADING_SLASH = """{
  "result":[
    {
      "scriptId":"72",
      "url":"file:///usr/local/google/home/benreich/v8-to-istanbul/test.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":99,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"add",
          "ranges":[
            {"startOffset":43,"endOffset":81,"count":0}
          ],
          "isBlockCoverage":false
        }
      ]
    },
    {
      "scriptId":"73",
      "url":"//test1.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":54,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"exports.subtract",
          "ranges":[
            {"startOffset":19,"endOffset":53,"count":1}
          ],
          "isBlockCoverage":true
        }
      ]
    }
  ]
}
"""

  _TEST_COVERAGE_DUPLICATE_SINGLE = """{
  "result":[
    {
      "scriptId":"73",
      "url":"//test1.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":54,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"exports.subtract",
          "ranges":[
            {"startOffset":19,"endOffset":53,"count":1}
          ],
          "isBlockCoverage":true
        }
      ]
    }
  ]
}
"""

  _TEST_COVERAGE_DUPLICATE_DOUBLE = """{
  "result":[
    {
      "scriptId":"72",
      "url":"//test.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":99,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"add",
          "ranges":[
            {"startOffset":43,"endOffset":81,"count":0}
          ],
          "isBlockCoverage":false
        }
      ]
    },
    {
      "scriptId":"73",
      "url":"//test1.js",
      "functions":[
        {
          "functionName":"",
          "ranges":[
            {"startOffset":0,"endOffset":54,"count":1}
          ],
          "isBlockCoverage":true
        },
        {
          "functionName":"exports.subtract",
          "ranges":[
            {"startOffset":19,"endOffset":53,"count":1}
          ],
          "isBlockCoverage":true
        }
      ]
    }
  ]
}
"""

  def setUp(self):
    self.task_output_dir = tempfile.mkdtemp()
    self.coverage_dir = os.path.join(self.task_output_dir, 'coverages')
    self.source_dir = os.path.join(self.task_output_dir, 'source')

    os.makedirs(self.coverage_dir)
    os.makedirs(self.source_dir)

  def tearDown(self):
    shutil.rmtree(self.task_output_dir)

  def list_files(self, absolute_path):
    actual_files = []
    for root, _, files in os.walk(absolute_path):
      actual_files.extend([
          os.path.join(root, file_name) for file_name in files
      ])

    return actual_files

  def _write_files(self, root_dir, *file_path_contents):
    for data in file_path_contents:
      file_path, contents = data
      with open(os.path.join(root_dir, file_path), 'w') as f:
        f.write(contents)

  def write_sources(self, *file_path_contents):
    self._write_files(self.source_dir, *file_path_contents)

  def write_coverages(self, *file_path_contents):
    self._write_files(self.coverage_dir, *file_path_contents)

  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_happy_path(self):
    self.write_sources(('file.js', self._TEST_SOURCE_A))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_A))

    merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 1)

  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_no_coverages_in_file(self):
    coverage_file = """{
  "result": []
}
"""

    self.write_sources(('file.js', self._TEST_SOURCE_A))
    self.write_coverages(('test_coverage.cov.json', coverage_file))

    merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 0)

  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_invalid_coverage_file(self):
    self.write_sources(('file.js', self._TEST_SOURCE_A))
    self.write_coverages(
      ('test_coverage.cov.json', self._TEST_COVERAGE_INVALID))

    self.assertRaises(merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir))

  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_multiple_coverages_single_file(self):
    self.write_sources(('test.js', self._TEST_SOURCE_B))
    self.write_sources(('test1.js', self._TEST_SOURCE_C))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_B))

    merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 2)

  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_multiple_coverages_no_leading_double_slash(self):
    self.write_sources(('test.js', self._TEST_SOURCE_B))
    self.write_sources(('test1.js', self._TEST_SOURCE_C))
    self.write_coverages(
        ('test_coverage.cov.json', self._TEST_COVERAGE_NO_LEADING_SLASH))

    merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 1)


  @unittest.skipUnless(_COVERAGE_MODULES_EXIST, 'requires JS coverage modules')
  def test_multiple_duplicate_coverages_flattened(self):
    self.write_sources(('test.js', self._TEST_SOURCE_B))
    self.write_sources(('test1.js', self._TEST_SOURCE_C))
    self.write_coverages(
        ('test_coverage_1.cov.json', self._TEST_COVERAGE_B))
    self.write_coverages(
        ('test_coverage_2.cov.json', self._TEST_COVERAGE_DUPLICATE_DOUBLE))

    merger.convert_raw_coverage_to_istanbul(
        [self.coverage_dir], self.source_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 2)


if __name__ == '__main__':
    unittest.main()
