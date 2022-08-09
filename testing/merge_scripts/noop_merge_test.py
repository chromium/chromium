#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import sys
import tempfile
import unittest

import common_merge_script_tests

THIS_DIR = os.path.dirname(__file__)

sys.path.insert(
    0, os.path.abspath(os.path.join(THIS_DIR, '..', 'resources')))
import noop_merge


class NoopMergeTest(unittest.TestCase):

  # pylint: disable=super-with-arguments
  def setUp(self):
    super(NoopMergeTest, self).setUp()
    self.temp_dir = tempfile.mkdtemp()
  # pylint: enable=super-with-arguments

  # pylint: disable=super-with-arguments
  def tearDown(self):
    shutil.rmtree(self.temp_dir)
    super(NoopMergeTest, self).tearDown()
  # pylint: enable=super-with-arguments

  def test_copies_first_json(self):
    input_json = os.path.join(self.temp_dir, 'input.json')
    input_json_contents = {'foo': ['bar', 'baz']}
    with open(input_json, 'w') as f:
      json.dump(input_json_contents, f)
    output_json = os.path.join(self.temp_dir, 'output.json')
    self.assertEqual(0, noop_merge.noop_merge(output_json, [input_json]))
    with open(output_json) as f:
      self.assertEqual(input_json_contents, json.load(f))

  def test_no_jsons(self):
    output_json = os.path.join(self.temp_dir, 'output.json')
    self.assertEqual(0, noop_merge.noop_merge(output_json, []))
    with open(output_json) as f:
      self.assertEqual({}, json.load(f))

  def test_multiple_jsons(self):
    input_json1 = os.path.join(self.temp_dir, 'input1.json')
    input_json1_contents = {'test1': ['foo1', 'bar1']}
    with open(input_json1, 'w') as f:
      json.dump(input_json1_contents, f)
    input_json2 = os.path.join(self.temp_dir, 'input2.json')
    input_json2_contents = {'test2': ['foo2', 'bar2']}
    with open(input_json2, 'w') as f:
      json.dump(input_json2_contents, f)
    output_json = os.path.join(self.temp_dir, 'output.json')
    self.assertNotEqual(
        0, noop_merge.noop_merge(output_json, [input_json1, input_json2]))


class CommandLineTest(common_merge_script_tests.CommandLineTest):
  # pylint: disable=super-with-arguments
  def __init__(self, methodName='runTest'):
    super(CommandLineTest, self).__init__(methodName, noop_merge)
  # pylint: enable=super-with-arguments

if __name__ == '__main__':
  unittest.main()
