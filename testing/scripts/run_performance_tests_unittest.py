# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import json

import run_performance_tests
from run_performance_tests import TelemetryCommandGenerator

# The path where the output of a wpt run was written. This is the file that
# gets processed by BaseWptScriptAdapter.
OUTPUT_JSON_FILENAME = "out.json"


class TelemetryCommandGeneratorTest(unittest.TestCase):
  def setUp(self):
    fake_args = [
        './run_benchmark',
        '--isolated-script-test-output=output.json'
    ]
    self._fake_options = run_performance_tests.parse_arguments(fake_args)

  def testStorySelectionBeginEnd(self):
    story_selection_config = json.loads(
        '{"begin": 11, "end": 21, "abridged": false}')
    generator = TelemetryCommandGenerator(
        'benchmark_name', self._fake_options, story_selection_config
    )
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-begin-index=11', command)
    self.assertIn('--story-shard-end-index=21', command)
    self.assertNotIn('--run-abridged-story-set', command)


  def testStorySelectionAbridgedDefault(self):
    story_selection_config = json.loads(
        '{"begin": 11, "end": 21}')
    generator = TelemetryCommandGenerator(
        'benchmark_name', self._fake_options, story_selection_config
    )
    command = generator.generate('output_dir')
    self.assertIn('--run-abridged-story-set', command)

  def testStorySelectionIndexSectionsSingleIndex(self):
    story_selection_config = json.loads(
        '{"sections": [{"begin": 11, "end": 21}, {"begin": 25, "end": 26}]}')
    generator = TelemetryCommandGenerator(
        'benchmark_name', self._fake_options, story_selection_config
    )
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-indexes=11-21,25', command)

  def testStorySelectionIndexSectionsOpenEnds(self):
    story_selection_config = json.loads(
        '{"sections": [{"end": 10}, {"begin": 15, "end": 16}, {"begin": 20}]}')
    generator = TelemetryCommandGenerator(
        'benchmark_name', self._fake_options, story_selection_config
    )
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-indexes=-10,15,20-', command)


  def testStorySelectionIndexSectionsIllegalRange(self):
    with self.assertRaises(ValueError):
      story_selection_config = json.loads(
          '{"sections": [{"begin": 15, "end": 16}, {"foo": "bar"}]}')
      generator = TelemetryCommandGenerator(
          'benchmark_name', self._fake_options, story_selection_config
      )
      generator.generate('output_dir')

  def testStorySelectionIndexSectionsEmpty(self):
    story_selection_config = json.loads(
        '{"sections": []}')
    generator = TelemetryCommandGenerator(
        'benchmark_name', self._fake_options, story_selection_config
    )
    command = generator.generate('output_dir')
    self.assertNotIn('--story-shard-indexes=', command)