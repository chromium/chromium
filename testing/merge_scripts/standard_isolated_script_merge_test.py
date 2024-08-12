#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest
from unittest import mock

import common_merge_script_tests
import standard_isolated_script_merge

TWO_COMPLETED_SHARDS = {
    u'shards': [
        {
            u'state': u'COMPLETED',
        },
        {
            u'state': u'COMPLETED',
        },
    ],
}


class StandardIsolatedScriptMergeTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.test_files = []
    self.summary = None

  # pylint: disable=super-with-arguments
  def tearDown(self):
    shutil.rmtree(self.temp_dir)
    super(StandardIsolatedScriptMergeTest, self).tearDown()

  # pylint: enable=super-with-arguments

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

  def _stage(self, summary, files):
    self.summary = self._write_temp_file('summary.json', summary)
    for path, content in files.items():
      abs_path = self._write_temp_file(path, content)
      self.test_files.append(abs_path)


class OutputTest(StandardIsolatedScriptMergeTest):

  def test_success_and_failure(self):
    self._stage(
        TWO_COMPLETED_SHARDS, {
            '0/output.json': {
                'successes': ['fizz', 'baz'],
            },
            '1/output.json': {
                'successes': ['buzz', 'bar'],
                'failures': ['failing_test_one']
            }
        })

    output_json_file = os.path.join(self.temp_dir, 'output.json')
    standard_isolated_script_merge.StandardIsolatedScriptMerge(
        output_json_file, self.summary, self.test_files)

    with open(output_json_file, 'r') as f:
      results = json.load(f)
      self.assertEqual(results['successes'], ['fizz', 'baz', 'buzz', 'bar'])
      self.assertEqual(results['failures'], ['failing_test_one'])
      self.assertTrue(results['valid'])

  def test_missing_shard(self):
    self._stage(TWO_COMPLETED_SHARDS, {
        '0/output.json': {
            'successes': ['fizz', 'baz'],
        },
    })
    output_json_file = os.path.join(self.temp_dir, 'output.json')
    standard_isolated_script_merge.StandardIsolatedScriptMerge(
        output_json_file, self.summary, self.test_files)

    with open(output_json_file, 'r') as f:
      results = json.load(f)
      self.assertEqual(results['successes'], ['fizz', 'baz'])
      self.assertEqual(results['failures'], [])
      self.assertTrue(results['valid'])
      self.assertEqual(results['global_tags'], ['UNRELIABLE_RESULTS'])
      self.assertEqual(results['missing_shards'], [1])


class InputParsingTest(StandardIsolatedScriptMergeTest):
  # pylint: disable=super-with-arguments
  def setUp(self):
    super(InputParsingTest, self).setUp()

    self.merge_test_results_args = []

    def mock_merge_test_results(results_list):
      self.merge_test_results_args.append(results_list)
      return {
          'foo': [
              'bar',
              'baz',
          ],
      }

    m = mock.patch(
        'standard_isolated_script_merge.results_merger.merge_test_results',
        side_effect=mock_merge_test_results)
    m.start()
    self.addCleanup(m.stop)

  # pylint: enable=super-with-arguments

  def test_simple(self):
    self._stage(
        TWO_COMPLETED_SHARDS, {
            '0/output.json': {
                'result0': ['bar', 'baz'],
            },
            '1/output.json': {
                'result1': {
                    'foo': 'bar'
                }
            }
        })

    output_json_file = os.path.join(self.temp_dir, 'output.json')
    exit_code = standard_isolated_script_merge.StandardIsolatedScriptMerge(
        output_json_file, self.summary, self.test_files)

    self.assertEqual(0, exit_code)
    self.assertEqual([
        [{
            'result0': [
                'bar',
                'baz',
            ],
        }, {
            'result1': {
                'foo': 'bar',
            },
        }],
    ], self.merge_test_results_args)

  def test_no_jsons(self):
    self._stage({
        u'shards': [],
    }, {})

    json_files = []
    output_json_file = os.path.join(self.temp_dir, 'output.json')
    exit_code = standard_isolated_script_merge.StandardIsolatedScriptMerge(
        output_json_file, self.summary, json_files)

    self.assertEqual(0, exit_code)
    self.assertEqual([[]], self.merge_test_results_args)


class CommandLineTest(common_merge_script_tests.CommandLineTest):

  # pylint: disable=super-with-arguments
  def __init__(self, methodName='runTest'):
    super(CommandLineTest, self).__init__(methodName,
                                          standard_isolated_script_merge)

  # pylint: enable=super-with-arguments


if __name__ == '__main__':
  unittest.main()
