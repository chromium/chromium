#!/usr/bin/env vpython
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import json
import logging
import os
import shutil
import sys
import tempfile
import unittest

import common_merge_script_tests

THIS_DIR = os.path.dirname(os.path.abspath(__file__))

# For 'standard_gtest_merge.py'.
sys.path.insert(
    0, os.path.abspath(os.path.join(THIS_DIR, '..', 'resources')))

import mock

import standard_gtest_merge


# gtest json output for successfully finished shard #0.
GOOD_GTEST_JSON_0 = {
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'disabled_tests': [
    'ConditionVariableTest.TimeoutAcrossSetTimeOfDay',
    'FileTest.TouchGetInfo',
    'MessageLoopTestTypeDefault.EnsureDeletion',
  ],
  'global_tags': ['CPU_64_BITS', 'MODE_DEBUG', 'OS_LINUX', 'OS_POSIX'],
  'per_iteration_data': [{
    'AlignedMemoryTest.DynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.ScopedDynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
  }],
  'test_locations': {
    'AlignedMemoryTest.DynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 123,
    },
    'AlignedMemoryTest.ScopedDynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 456,
    },
    # This is a test from a different shard, but this happens in practice and we
    # should not fail if information is repeated.
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
  },
}


# gtest json output for successfully finished shard #1.
GOOD_GTEST_JSON_1 = {
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'disabled_tests': [
    'ConditionVariableTest.TimeoutAcrossSetTimeOfDay',
    'FileTest.TouchGetInfo',
    'MessageLoopTestTypeDefault.EnsureDeletion',
  ],
  'global_tags': ['CPU_64_BITS', 'MODE_DEBUG', 'OS_LINUX', 'OS_POSIX'],
  'per_iteration_data': [{
    'AlignedMemoryTest.StackAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.StaticAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
  }],
  'test_locations': {
    'AlignedMemoryTest.StackAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 789,
    },
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
  },
}


TIMED_OUT_GTEST_JSON_1 = {
  'disabled_tests': [],
  'global_tags': [],
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'per_iteration_data': [{
    'AlignedMemoryTest.StackAlignment': [{
      'elapsed_time_ms': 54000,
      'losless_snippet': True,
      'output_snippet': 'timed out',
      'output_snippet_base64': '',
      'status': 'FAILURE',
    }],
    'AlignedMemoryTest.StaticAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': '',
      'output_snippet_base64': '',
      'status': 'NOTRUN',
    }],
  }],
  'test_locations': {
    'AlignedMemoryTest.StackAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 789,
    },
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
  },
}

# GOOD_GTEST_JSON_0 and GOOD_GTEST_JSON_1 merged.
GOOD_GTEST_JSON_MERGED = {
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'disabled_tests': [
    'ConditionVariableTest.TimeoutAcrossSetTimeOfDay',
    'FileTest.TouchGetInfo',
    'MessageLoopTestTypeDefault.EnsureDeletion',
  ],
  'global_tags': ['CPU_64_BITS', 'MODE_DEBUG', 'OS_LINUX', 'OS_POSIX'],
  'missing_shards': [],
  'per_iteration_data': [{
    'AlignedMemoryTest.DynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.ScopedDynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.StackAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.StaticAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
  }],
  'swarming_summary': {
    u'shards': [
      {
        u'state': u'COMPLETED',
        u'outputs_ref': {
          u'view_url': u'blah',
        },
      }
      ],
  },
  'test_locations': {
    'AlignedMemoryTest.StackAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 789,
    },
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
    'AlignedMemoryTest.DynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 123,
    },
    'AlignedMemoryTest.ScopedDynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 456,
    },
  },
}


# Only shard #1 finished. UNRELIABLE_RESULTS is set.
BAD_GTEST_JSON_ONLY_1_SHARD = {
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'disabled_tests': [
    'ConditionVariableTest.TimeoutAcrossSetTimeOfDay',
    'FileTest.TouchGetInfo',
    'MessageLoopTestTypeDefault.EnsureDeletion',
  ],
  'global_tags': [
    'CPU_64_BITS',
    'MODE_DEBUG',
    'OS_LINUX',
    'OS_POSIX',
    'UNRELIABLE_RESULTS',
  ],
  'missing_shards': [0],
  'per_iteration_data': [{
    'AlignedMemoryTest.StackAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.StaticAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
  }],
  'test_locations': {
    'AlignedMemoryTest.StackAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 789,
    },
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
  },
}


# GOOD_GTEST_JSON_0 and TIMED_OUT_GTEST_JSON_1 merged.
TIMED_OUT_GTEST_JSON_MERGED = {
  'all_tests': [
    'AlignedMemoryTest.DynamicAllocation',
    'AlignedMemoryTest.ScopedDynamicAllocation',
    'AlignedMemoryTest.StackAlignment',
    'AlignedMemoryTest.StaticAlignment',
  ],
  'disabled_tests': [
    'ConditionVariableTest.TimeoutAcrossSetTimeOfDay',
    'FileTest.TouchGetInfo',
    'MessageLoopTestTypeDefault.EnsureDeletion',
  ],
  'global_tags': ['CPU_64_BITS', 'MODE_DEBUG', 'OS_LINUX', 'OS_POSIX'],
  'missing_shards': [],
  'per_iteration_data': [{
    'AlignedMemoryTest.DynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.ScopedDynamicAllocation': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': 'blah\\n',
      'output_snippet_base64': 'YmxhaAo=',
      'status': 'SUCCESS',
    }],
    'AlignedMemoryTest.StackAlignment': [{
      'elapsed_time_ms': 54000,
      'losless_snippet': True,
      'output_snippet': 'timed out',
      'output_snippet_base64': '',
      'status': 'FAILURE',
    }],
    'AlignedMemoryTest.StaticAlignment': [{
      'elapsed_time_ms': 0,
      'losless_snippet': True,
      'output_snippet': '',
      'output_snippet_base64': '',
      'status': 'NOTRUN',
    }],
  }],
  'swarming_summary': {
    u'shards': [
      {
        u'state': u'COMPLETED',
      },
      {
        u'state': u'TIMED_OUT',
      },
      ],
  },
  'test_locations': {
    'AlignedMemoryTest.StackAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 789,
    },
    'AlignedMemoryTest.StaticAlignment': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 12,
    },
    'AlignedMemoryTest.DynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 123,
    },
    'AlignedMemoryTest.ScopedDynamicAllocation': {
      'file': 'foo/bar/allocation_test.cc',
      'line': 456,
    },
  },
}


class _StandardGtestMergeTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def _write_temp_file(self, path, content):
    abs_path = os.path.join(self.temp_dir, path.replace('/', os.sep))
    if not os.path.exists(os.path.dirname(abs_path)):
      os.makedirs(os.path.dirname(abs_path))
    with open(abs_path, 'w') as f:
      if isinstance(content, dict):
        json.dump(content, f)
      else:
        assert isinstance(content, str)
        f.write(content)
    return abs_path


class LoadShardJsonTest(_StandardGtestMergeTest):

  def test_double_digit_jsons(self):
    jsons_to_merge = []
    for i in xrange(15):
      json_dir = os.path.join(self.temp_dir, str(i))
      json_path = os.path.join(json_dir, 'output.json')
      if not os.path.exists(json_dir):
        os.makedirs(json_dir)
      with open(json_path, 'w') as f:
        json.dump({'all_tests': ['LoadShardJsonTest.test%d' % i]}, f)
      jsons_to_merge.append(json_path)

    content, err = standard_gtest_merge.load_shard_json(
      0, None, jsons_to_merge)
    self.assertEqual({'all_tests': ['LoadShardJsonTest.test0']}, content)
    self.assertIsNone(err)

    content, err = standard_gtest_merge.load_shard_json(
      12, None, jsons_to_merge)
    self.assertEqual({'all_tests': ['LoadShardJsonTest.test12']}, content)
    self.assertIsNone(err)

  def test_double_task_id_jsons(self):
    jsons_to_merge = []
    for i in xrange(15):
      json_dir = os.path.join(self.temp_dir, 'deadbeef%d' % i)
      json_path = os.path.join(json_dir, 'output.json')
      if not os.path.exists(json_dir):
        os.makedirs(json_dir)
      with open(json_path, 'w') as f:
        json.dump({'all_tests': ['LoadShardJsonTest.test%d' % i]}, f)
      jsons_to_merge.append(json_path)

    content, err = standard_gtest_merge.load_shard_json(
      0, 'deadbeef0', jsons_to_merge)
    self.assertEqual({'all_tests': ['LoadShardJsonTest.test0']},
                     content)
    self.assertIsNone(err)

    content, err = standard_gtest_merge.load_shard_json(
      12, 'deadbeef12', jsons_to_merge)
    self.assertEqual({'all_tests': ['LoadShardJsonTest.test12']},
                     content)
    self.assertIsNone(err)


class MergeShardResultsTest(_StandardGtestMergeTest):
  """Tests for merge_shard_results function."""

  def setUp(self):
    super(MergeShardResultsTest, self).setUp()
    self.summary = None
    self.test_files = []

  def stage(self, summary, files):
    self.summary = self._write_temp_file('summary.json', summary)
    for path, content in files.iteritems():
      abs_path = self._write_temp_file(path, content)
      self.test_files.append(abs_path)

  def call(self):
    stdout = cStringIO.StringIO()
    with mock.patch('sys.stdout', stdout):
      merged = standard_gtest_merge.merge_shard_results(
          self.summary, self.test_files)
      return merged, stdout.getvalue().strip()

  def assertUnicodeEquals(self, expectation, result):
    def convert_to_unicode(key_or_value):
      if isinstance(key_or_value, str):
        return unicode(key_or_value)
      if isinstance(key_or_value, dict):
        return {convert_to_unicode(k): convert_to_unicode(v)
                for k, v in key_or_value.items()}
      if isinstance(key_or_value, list):
        return [convert_to_unicode(x) for x in key_or_value]
      return key_or_value

    unicode_expectations = convert_to_unicode(expectation)
    unicode_result = convert_to_unicode(result)
    self.assertEquals(unicode_expectations, unicode_result)

  def test_ok(self):
    # Two shards, both successfully finished.
    self.stage({
      u'shards': [
        {
          u'state': u'COMPLETED',
        },
        {
          u'state': u'COMPLETED',
        },
      ],
    },
    {
      '0/output.json': GOOD_GTEST_JSON_0,
      '1/output.json': GOOD_GTEST_JSON_1,
    })
    merged, stdout = self.call()
    merged['swarming_summary'] = {
      'shards': [
        {
          u'state': u'COMPLETED',
          u'outputs_ref': {
            u'view_url': u'blah',
          },
        }
      ],
    }
    self.assertUnicodeEquals(GOOD_GTEST_JSON_MERGED, merged)
    self.assertEqual('', stdout)

  def test_timed_out(self):
    # Two shards, both successfully finished.
    self.stage({
      'shards': [
        {
          'state': 'COMPLETED',
        },
        {
          'state': 'TIMED_OUT',
        },
      ],
    },
    {
      '0/output.json': GOOD_GTEST_JSON_0,
      '1/output.json': TIMED_OUT_GTEST_JSON_1,
    })
    merged, stdout = self.call()

    self.assertUnicodeEquals(TIMED_OUT_GTEST_JSON_MERGED, merged)
    self.assertIn(
        'Test runtime exceeded allocated time\n', stdout)

  def test_missing_summary_json(self):
    # summary.json is missing, should return None and emit warning.
    self.summary = os.path.join(self.temp_dir, 'summary.json')
    merged, output = self.call()
    self.assertEqual(None, merged)
    self.assertIn('@@@STEP_WARNINGS@@@', output)
    self.assertIn('summary.json is missing or can not be read', output)

  def test_unfinished_shards(self):
    # Only one shard (#1) finished. Shard #0 did not.
    self.stage({
      u'shards': [
        None,
        {
          u'state': u'COMPLETED',
        },
      ],
    },
    {
      u'1/output.json': GOOD_GTEST_JSON_1,
    })
    merged, stdout = self.call()
    merged.pop('swarming_summary')
    self.assertUnicodeEquals(BAD_GTEST_JSON_ONLY_1_SHARD, merged)
    self.assertIn(
        '@@@STEP_WARNINGS@@@\nsome shards did not complete: 0\n', stdout)
    self.assertIn(
        '@@@STEP_LOG_LINE@some shards did not complete: 0@'
        'Missing results from the following shard(s): 0@@@\n', stdout)

  def test_missing_output_json(self):
    # Shard #0 output json is missing.
    self.stage({
      u'shards': [
        {
          u'state': u'COMPLETED',
        },
        {
          u'state': u'COMPLETED',
        },
      ],
    },
    {
      u'1/output.json': GOOD_GTEST_JSON_1,
    })
    merged, stdout = self.call()
    merged.pop('swarming_summary')
    self.assertUnicodeEquals(BAD_GTEST_JSON_ONLY_1_SHARD, merged)
    self.assertIn(
        'No result was found: '
        'shard 0 test output was missing', stdout)

  def test_large_output_json(self):
    # a shard is too large.
    self.stage({
      u'shards': [
        {
          u'state': u'COMPLETED',
        },
        {
          u'state': u'COMPLETED',
        },
      ],
    },
    {
      '0/output.json': GOOD_GTEST_JSON_0,
      '1/output.json': GOOD_GTEST_JSON_1,
    })
    old_json_limit = standard_gtest_merge.OUTPUT_JSON_SIZE_LIMIT
    len0 = len(json.dumps(GOOD_GTEST_JSON_0))
    len1 = len(json.dumps(GOOD_GTEST_JSON_1))
    large_shard = "0" if len0 > len1 else "1"
    try:
      # Override max output.json size just for this test.
      standard_gtest_merge.OUTPUT_JSON_SIZE_LIMIT = min(len0,len1)
      merged, stdout = self.call()
      merged.pop('swarming_summary')
      self.assertUnicodeEquals(BAD_GTEST_JSON_ONLY_1_SHARD, merged)
      self.assertIn(
          'No result was found: '
          'shard %s test output exceeded the size limit' % large_shard, stdout)
    finally:
      standard_gtest_merge.OUTPUT_JSON_SIZE_LIMIT = old_json_limit


class CommandLineTest(common_merge_script_tests.CommandLineTest):

  def __init__(self, methodName='runTest'):
    super(CommandLineTest, self).__init__(methodName, standard_gtest_merge)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
