#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest

import merge_js_lib as merger
import node

_HERE_DIR = Path(__file__).parent.resolve()
_SOURCE_MAP_PROCESSOR = (_HERE_DIR.parent.parent.parent / 'tools' /
                         'code_coverage' / 'js_source_maps' /
                         'create_js_source_maps' /
                         'create_js_source_maps.js').resolve()


@unittest.skipIf(os.name == 'nt', 'Not intended to work on Windows')
class ConvertToIstanbulTest(unittest.TestCase):
  _TEST_SOURCE_A = """function add(a, b) {
  return a + b;
}

function subtract(a, b) {
  return a - b;
}

subtract(5, 2);
"""
  _INVALID_MAPPING_A = (
      '//# sourceMappingURL=data:application/json;base64,'
      'eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbImZvby50cyJdLCJuYW1lcyI6W10sIm1hcHBpb'
      'mdzIjoiOzs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Oz'
      's7OztBQUFBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUN'
      'BIiwiZmlsZSI6Ii91c3IvbG9jYWwvZ29vZ2xlL2hvbWUvc3Jpbml2YXNoZWdkZS9jaHJv'
      'bWl1bS9zcmMvZm9vX3ByZS50cyIsInNvdXJjZVJvb3QiOiIvdXNyL2xvY2FsL2dvb2dsZ'
      'S9ob21lL3NyaW5pdmFzaGVnZGUvY2hyb21pdW0vc3JjIiwic291cmNlc0NvbnRlbnQiOl'
      'siZnVuY3Rpb24gYWRkKGEsIGIpIHtcbiAgcmV0dXJuIGEgKyBiO1xufVxuXG5mdW5jdGl'
      'vbiBzdWJ0cmFjdChhLCBiKSB7XG4gIHJldHVybiBhIC0gYjtcbn1cblxuc3VidHJhY3Qo'
      'NSwgMik7XG4iXX0=')

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
    self.out_dir = os.path.join(self.task_output_dir, 'out')
    self.sourceRoot = '/'

    os.makedirs(self.coverage_dir)
    os.makedirs(self.source_dir)
    os.makedirs(self.out_dir)

  def tearDown(self):
    shutil.rmtree(self.task_output_dir)

  def list_files(self, absolute_path):
    actual_files = []
    for root, _, files in os.walk(absolute_path):
      actual_files.extend(
          [os.path.join(root, file_name) for file_name in files])

    return actual_files

  def _write_files(self, root_dir, *file_path_contents):
    for data in file_path_contents:
      file_path, contents = data
      with open(os.path.join(root_dir, file_path), 'w') as f:
        f.write(contents)

  def _write_transformations(self, source_dir, out_dir, original_file_name,
                             input_file_name, output_file_name):
    original_file = os.path.join(source_dir, original_file_name)
    input_file = os.path.join(source_dir, input_file_name)
    output_file = os.path.join(out_dir, output_file_name)
    node.RunNode([
        str(_SOURCE_MAP_PROCESSOR),
        '--originals={}'.format(' '.join([original_file])),
        '--inputs={}'.format(' '.join([input_file])),
        '--outputs={}'.format(' '.join([output_file])),
        '--inline-sourcemaps',
        '--sourceRoot={}'.format(self.sourceRoot),
    ])

  def write_sources(self, *file_path_contents):
    url_to_path_map = {}
    for path_url, contents in file_path_contents:
      file_path, url = path_url
      url_to_path_map[file_path] = url
      self._write_files(self.source_dir, (url, contents))
      self._write_files(self.out_dir, (url, contents))
      self._write_transformations(self.source_dir, self.out_dir, url, url, url)
    with open(os.path.join(self.out_dir, 'parsed_scripts.json'),
              'w',
              encoding='utf-8') as f:
      f.write(json.dumps(url_to_path_map))

  def write_coverages(self, *file_path_contents):
    self._write_files(self.coverage_dir, *file_path_contents)

  def test_happy_path(self):
    self.write_sources((('//file.js', 'file.js'), self._TEST_SOURCE_A))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_A))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 1)

  def test_invalid_mapping(self):
    self.write_sources((('//file.js', 'file.js'), self._TEST_SOURCE_A))
    self._write_files(
        self.out_dir,
        ('file.js', self._TEST_SOURCE_A + '\n' + self._INVALID_MAPPING_A))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_A))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 0)

  def test_no_coverages_in_file(self):
    coverage_file = """{
      "result": []
    }
    """

    self.write_sources((('//file.js', 'file.js'), self._TEST_SOURCE_A))
    self.write_coverages(('test_coverage.cov.json', coverage_file))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 0)

  def test_invalid_coverage_file(self):
    self.write_sources((('//file.js', 'file.js'), self._TEST_SOURCE_A))
    self.write_coverages(
        ('test_coverage.cov.json', self._TEST_COVERAGE_INVALID))

    with self.assertRaises(RuntimeError):
      merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                              self.task_output_dir)

  def test_multiple_coverages_single_file(self):
    self.write_sources((('//test.js', 'test.js'), self._TEST_SOURCE_B),
                       (('//test1.js', 'test1.js'), self._TEST_SOURCE_C))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_B))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 2)

  def test_multiple_coverages_no_leading_double_slash(self):
    self.write_sources((('//test.js', 'test.js'), self._TEST_SOURCE_B),
                       (('//test1.js', 'test1.js'), self._TEST_SOURCE_C))
    self.write_coverages(
        ('test_coverage.cov.json', self._TEST_COVERAGE_NO_LEADING_SLASH))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 1)

  def test_multiple_duplicate_coverages_flattened(self):
    self.write_sources((('//test.js', 'test.js'), self._TEST_SOURCE_B),
                       (('//test1.js', 'test1.js'), self._TEST_SOURCE_C))
    self.write_coverages(('test_coverage_1.cov.json', self._TEST_COVERAGE_B))
    self.write_coverages(
        ('test_coverage_2.cov.json', self._TEST_COVERAGE_DUPLICATE_DOUBLE))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 2)

  def test_original_source_missing(self):
    self.write_sources((('//file.js', 'file.js'), self._TEST_SOURCE_A))
    self.write_coverages(('test_coverage.cov.json', self._TEST_COVERAGE_A))
    os.remove(os.path.join(self.source_dir, 'file.js'))

    merger.convert_raw_coverage_to_istanbul([self.coverage_dir], self.out_dir,
                                            self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 0)

  def test_multiple_coverages_in_multiple_shards(self):
    coverage_dir_1 = os.path.join(self.coverage_dir, 'coverage1')
    coverage_dir_2 = os.path.join(self.coverage_dir, 'coverage2')
    os.makedirs(coverage_dir_1)
    os.makedirs(coverage_dir_2)

    self.write_sources((('//test.js', 'test.js'), self._TEST_SOURCE_B),
                       (('//test1.js', 'test1.js'), self._TEST_SOURCE_C))
    self._write_files(coverage_dir_1,
                      ('test_coverage_1.cov.json', self._TEST_COVERAGE_B))
    self._write_files(
        coverage_dir_2,
        ('test_coverage_2.cov.json', self._TEST_COVERAGE_DUPLICATE_DOUBLE))

    merger.convert_raw_coverage_to_istanbul([coverage_dir_1, coverage_dir_2],
                                            self.out_dir, self.task_output_dir)

    istanbul_files = self.list_files(
        os.path.join(self.task_output_dir, 'istanbul'))
    self.assertEqual(len(istanbul_files), 2)


if __name__ == '__main__':
  unittest.main()
