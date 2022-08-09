#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import six
import sys
import unittest

THIS_DIR = os.path.dirname(os.path.abspath(__file__))

# For results_merger.
sys.path.insert(0, os.path.join(THIS_DIR, '..', 'resources'))
import results_merger


GOOD_JSON_TEST_RESULT_0 = {
  'tests': {
    'car': {
      'honda': {
        'expected': 'PASS',
        'actual': 'PASS'
      },
      'toyota': {
        'expected': 'FAIL',
        'actual': 'FAIL'
      }
    },
    'computer': {
      'dell': {
        'expected': 'PASS',
        'actual': 'PASS'
      }
    },
  },
  'interrupted': False,
  'version': 3,
  'seconds_since_epoch': 1406662289.76,
  'num_failures_by_type': {
     'FAIL': 0,
     'PASS': 2
  },
  'layout_tests_dir': 'abc'
}

GOOD_JSON_TEST_RESULT_1 = {
  'tests': {
    'car': {
      'tesla': {
        'expected': 'PASS',
        'actual': 'PASS'
      },
    },
    'burger': {
      'mcdonald': {
        'expected': 'PASS',
        'actual': 'PASS'
      }
    },
  },
  'interrupted': False,
  'version': 3,
  'seconds_since_epoch': 1406662283.11,
  'num_failures_by_type': {
     'FAIL': 0,
     'PASS': 2
  },
  'layout_tests_dir': '123'
}

GOOD_JSON_TEST_RESULT_2 = {
  'tests': {
    'car': {
      'mercedes': {
        'expected': 'PASS',
        'actual': 'FAIL'
      },
    },
    'burger': {
      'in n out': {
        'expected': 'PASS',
        'actual': 'PASS'
      }
    },
  },
  'interrupted': True,
  'version': 3,
  'seconds_since_epoch': 1406662200.01,
  'num_failures_by_type': {
     'FAIL': 1,
     'PASS': 1
  }
}

GOOD_JSON_TEST_RESULT_MERGED = {
  'tests': {
    'car': {
      'tesla': {
        'expected': 'PASS',
        'actual': 'PASS'
      },
      'mercedes': {
        'expected': 'PASS',
        'actual': 'FAIL'
      },
      'honda': {
        'expected': 'PASS',
        'actual': 'PASS'
      },
      'toyota': {
        'expected': 'FAIL',
        'actual': 'FAIL'
      }
    },
    'computer': {
      'dell': {
        'expected': 'PASS',
        'actual': 'PASS'
      }
    },
    'burger': {
      'mcdonald': {
        'expected': 'PASS',
        'actual': 'PASS'
      },
      'in n out': {
        'expected': 'PASS',
        'actual': 'PASS'
      }
    }
  },
  'interrupted': True,
  'version': 3,
  'seconds_since_epoch': 1406662200.01,
  'num_failures_by_type': {
    'FAIL': 1,
    'PASS': 5
  },
  'layout_tests_dir': '123'
}


def extend(initial, add):
  out = copy.deepcopy(initial)
  out.update(add)
  return out


def remove(initial, keys):
  out = copy.deepcopy(initial)
  for k in keys:
    del out[k]
  return out



# These unittests are run in PRESUBMIT, but not by recipe_simulation_test, hence
# to avoid false alert on missing coverage by recipe_simulation_test, we mark
# these code as no cover.
class MergingTest(unittest.TestCase):  # pragma: no cover
  maxDiff = None  # Show full diff if assertion fail

  def test_merge_tries(self):
    self.assertEqual(
        {'a': 'A', 'b': {'c': 'C'}},
        results_merger.merge_tries(
            {'a': 'A', 'b': {}}, {'b': {'c': 'C'}}))

  def test_merge_tries_unmergable(self):
    with six.assertRaisesRegex(self, results_merger.MergeException, "a:b"):
        results_merger.merge_tries(
            {'a': {'b': 'A'}}, {'a': {'b': 'C'}})

  def test_merge_metadata(self):
    metadata1 = {'metadata': {'tags': ['foo', 'bar']}}
    metadata2 = {'metadata': {'tags': ['foo', 'bat']}}
    merged_results = results_merger.merge_test_results(
        [extend(GOOD_JSON_TEST_RESULT_0, metadata1),
         extend(GOOD_JSON_TEST_RESULT_1, metadata2)])
    self.assertEqual(
        merged_results['metadata']['tags'], ['foo', 'bat'])

  def test_merge_json_test_results_nop(self):
    good_json_results = (
        GOOD_JSON_TEST_RESULT_0,
        GOOD_JSON_TEST_RESULT_1,
        GOOD_JSON_TEST_RESULT_2,
        GOOD_JSON_TEST_RESULT_MERGED)
    for j in good_json_results:
      # Clone so we can check the input dictionaries are not modified
      a = copy.deepcopy(j)
      self.assertEqual(results_merger.merge_test_results([a]), j)
      self.assertEqual(a, j)

  def test_merge_json_test_results_invalid_version(self):
    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          extend(GOOD_JSON_TEST_RESULT_0, {'version': 5}),
          ])

    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          GOOD_JSON_TEST_RESULT_0,
          extend(GOOD_JSON_TEST_RESULT_1, {'version': 5}),
          ])

  def test_merge_json_test_results_missing_version(self):
    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          remove(GOOD_JSON_TEST_RESULT_0, ['version']),
          ])

    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          GOOD_JSON_TEST_RESULT_0,
          remove(GOOD_JSON_TEST_RESULT_1, ['version']),
          ])

  def test_merge_json_test_results_invalid_extra(self):
    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          extend(GOOD_JSON_TEST_RESULT_0, {'extra': True}),
          ])

    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          GOOD_JSON_TEST_RESULT_0,
          extend(GOOD_JSON_TEST_RESULT_1, {'extra': True}),
          ])

  def test_merge_json_test_results_missing_required(self):
    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          remove(GOOD_JSON_TEST_RESULT_0, ['interrupted']),
          ])

    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          GOOD_JSON_TEST_RESULT_0,
          remove(GOOD_JSON_TEST_RESULT_1, ['interrupted']),
          ])

  def test_merge_json_test_results_multiple(self):
    self.assertEqual(
        results_merger.merge_test_results([
            GOOD_JSON_TEST_RESULT_0,
            GOOD_JSON_TEST_RESULT_1,
            GOOD_JSON_TEST_RESULT_2,
            ]),
        GOOD_JSON_TEST_RESULT_MERGED)

  def test_merge_json_test_results_optional_matches(self):
    self.assertEqual(
        results_merger.merge_test_results([
            extend(GOOD_JSON_TEST_RESULT_0, {'path_delimiter': '.'}),
            extend(GOOD_JSON_TEST_RESULT_1, {'path_delimiter': '.'}),
            extend(GOOD_JSON_TEST_RESULT_2, {'path_delimiter': '.'}),
            ]),
        extend(GOOD_JSON_TEST_RESULT_MERGED, {'path_delimiter': '.'}))

  def test_merge_json_test_results_optional_differs(self):
    with self.assertRaises(results_merger.MergeException):
      results_merger.merge_test_results([
          extend(GOOD_JSON_TEST_RESULT_0, {'path_delimiter': '.'}),
          extend(GOOD_JSON_TEST_RESULT_1, {'path_delimiter': '.'}),
          extend(GOOD_JSON_TEST_RESULT_2, {'path_delimiter': '/'}),
          ])

  def test_merge_json_test_results_optional_count(self):
    self.assertEqual(
        results_merger.merge_test_results([
            extend(GOOD_JSON_TEST_RESULT_0, {'fixable': 1}),
            extend(GOOD_JSON_TEST_RESULT_1, {'fixable': 2}),
            extend(GOOD_JSON_TEST_RESULT_2, {'fixable': 3}),
            ]),
        extend(GOOD_JSON_TEST_RESULT_MERGED, {'fixable': 6}))

  def test_merge_nothing(self):
    self.assertEqual(
        results_merger.merge_test_results([]),
        {})

# TODO(tansell): Make this test fail properly, currently fails with an
# AttributeError.
#  def test_merge_test_name_conflict(self):
#    self.maxDiff = None  # Show full diff if assertion fail
#    with self.assertRaises(results_merger.MergeException):
#      results_merger.merge_test_results(
#        [GOOD_JSON_TEST_RESULT_0, GOOD_JSON_TEST_RESULT_0])



if __name__ == '__main__':
  unittest.main()  # pragma: no cover
