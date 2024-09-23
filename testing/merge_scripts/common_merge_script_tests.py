# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest


class CommandLineTest(unittest.TestCase):
  # pylint: disable=super-with-arguments
  def __init__(self, methodName, module):
    super(CommandLineTest, self).__init__(methodName)
    self._module = module

  # pylint: disable=super-with-arguments

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp(prefix='common_merge_script_tests')

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def test_accepts_task_output_dir(self):
    task_output_dir = os.path.join(self.temp_dir, 'task_output_dir')
    shard0_dir = os.path.join(task_output_dir, '0')
    os.makedirs(shard0_dir)
    summary_json = os.path.join(task_output_dir, 'summary.json')
    with open(summary_json, 'w') as summary_file:
      summary_contents = {
          u'shards': [
              {
                  u'state': u'COMPLETED',
              },
          ],
      }
      json.dump(summary_contents, summary_file)

    shard0_json = os.path.join(shard0_dir, 'output.json')
    with open(shard0_json, 'w') as shard0_file:
      json.dump({}, shard0_file)

    output_json = os.path.join(self.temp_dir, 'merged.json')

    raw_args = [
        '--task-output-dir',
        task_output_dir,
        '--summary-json',
        summary_json,
        '--output-json',
        output_json,
        shard0_json,
    ]
    self.assertEqual(0, self._module.main(raw_args))
