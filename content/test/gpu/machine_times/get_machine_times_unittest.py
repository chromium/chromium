#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import subprocess
import unittest
import unittest.mock as mock

from machine_times import get_machine_times

from unexpected_passes_common import data_types

# pylint: disable=protected-access


class EnsureBuildbucketAuthUnittest(unittest.TestCase):
  def testValidAuth(self):  # pylint: disable=no-self-use
    """Tests behavior when bb auth is valid."""
    with mock.patch.object(get_machine_times.subprocess, 'check_call'):
      get_machine_times._EnsureBuildbucketAuth()

  def testInvalidAuth(self):
    """Tests behavior when bb auth is invalid."""
    def SideEffect(*args, **kwargs):
      raise subprocess.CalledProcessError(1, [])

    with mock.patch.object(get_machine_times.subprocess,
                           'check_call',
                           side_effect=SideEffect):
      with self.assertRaisesRegex(
          RuntimeError, 'You are not logged into bb - run `bb auth-login`'):
        get_machine_times._EnsureBuildbucketAuth()


class GetTimesForBuilderUnittest(unittest.TestCase):
  def testNoBuildbucketIds(self):
    """Tests behavior when no Buildbucket IDs are found."""
    builder = data_types.BuilderEntry('builder', 'ci', False)
    with mock.patch.object(get_machine_times,
                           '_GetBuildbucketIdsForBuilder',
                           return_value=[]):
      with self.assertLogs(level='WARNING'):
        retval = get_machine_times._GetTimesForBuilder((builder, 1))
        self.assertEqual(retval, {'chromium/ci/builder': {}})

  def testBasic(self):
    """Basic happy path test."""
    builder = data_types.BuilderEntry('builder', 'ci', False)
    step_output = {
        'steps': [
            {
                'name':
                'first step',
                'summaryMarkdown':
                ('Max pending time: 2s (shard #1) '
                 '* [shard #0 (runtime (1s) + overhead (1s): 2s)]'
                 '* [shard #1 (runtime (2s) + overhead (2s): 4s)]'),
            },
            {
                'name':
                'second step',
                'summaryMarkdown':
                ('Max pending time: 4s (shard #1) '
                 '* [shard #0 (runtime (3s) + overhead (3s): 6s)]'
                 '* [shard #1 (runtime (4s) + overhead (4s): 8s)]'),
            },
        ],
    }
    expected_output = {
        'chromium/ci/builder': {
            'first step': [
                (1, 1),
                (2, 2),
            ],
            'second step': [
                (3, 3),
                (4, 4),
            ],
        },
    }
    with mock.patch.object(get_machine_times,
                           '_GetBuildbucketIdsForBuilder',
                           return_value=['1234']):
      with mock.patch.object(get_machine_times,
                             '_GetStepOutputForBuild',
                             return_value=json.dumps(step_output)):
        self.assertEqual(get_machine_times._GetTimesForBuilder((builder, 1)),
                         expected_output)


class GetBuildbucketIdsForBuilderUnittest(unittest.TestCase):
  def testBasic(self):
    """Basic happy path test."""
    builder = data_types.BuilderEntry('builder', 'ci', False)
    mock_process = mock.Mock()
    mock_process.stdout = '1\n2\n3'
    with mock.patch.object(get_machine_times.subprocess,
                           'run',
                           return_value=mock_process) as mock_run:
      self.assertEqual(
          get_machine_times._GetBuildbucketIdsForBuilder(builder, 3),
          ['1', '2', '3'])
      mock_run.assert_called_once_with(
          ['bb', 'ls', '-id', '-3', '-status', 'ended', 'chromium/ci/builder'],
          text=True,
          check=True,
          stdout=subprocess.PIPE)


class GetStepOutputForBuildUnittest(unittest.TestCase):
  def testBasic(self):
    """Basic happy path test."""
    mock_process = mock.Mock()
    mock_process.stdout = 'stdout'
    with mock.patch.object(get_machine_times.subprocess,
                           'run',
                           return_value=mock_process) as mock_run:
      self.assertEqual(get_machine_times._GetStepOutputForBuild('1234'),
                       'stdout')
      mock_run.assert_called_once_with(['bb', 'get', '-json', '-steps', '1234'],
                                       text=True,
                                       check=True,
                                       stdout=subprocess.PIPE)


class GetShardTimesFromStepOutputUnittest(unittest.TestCase):
  def testNonSummaryIgnored(self):
    """Tests that steps without a summary are ignored."""
    step_output = {
        'steps': [
            {
                'name': 'builder cache|check if empty',
            },
        ],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        {})

  def testSummaryFiltering(self):
    """Tests that only steps with certain summaries are used."""
    step_output = {
        'steps': [
            {
                'name':
                'bad step',
                'summaryMarkdown':
                '* [shard #0 (runtime (1s) + overhead (1s): 2s)]',
            },
            {
                'name':
                'Multi shard with pending time',
                'summaryMarkdown':
                ('Max pending time: 38s (shard #5) '
                 '* [shard #0 (runtime (2s) + overhead (2s): 4s)]'),
            },
            {
                'name':
                'Single shard with pending time',
                'summaryMarkdown':
                ('Pending time: 40s '
                 '* [shard #0 (runtime (3s) + overhead (3s): 6s)]'),
            },
            {
                'name':
                'Single shard with no pending time',
                'summaryMarkdown':
                ('Shard runtime 4s '
                 '* [shard #0 (runtime (4s) + overhead (4s): 8s)]'),
            },
        ],
    }
    expected_output = {
        'Multi shard with pending time': [(2, 2)],
        'Single shard with pending time': [(3, 3)],
        'Single shard with no pending time': [(4, 4)],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        expected_output)

  def testPassingMatch(self):
    """Tests that shard times can be extracted from passing shards."""
    step_output = {
        'steps': [
            {
                'name':
                'All passing',
                'summaryMarkdown':
                ('Max pending time: 2s (shard #1) '
                 '* [shard #0 (runtime (1s) + overhead (1s): 2s)]'
                 '* [shard #1 (runtime (2s) + overhead (2s): 4s)]'),
            },
        ],
    }
    expected_output = {
        'All passing': [(1, 1), (2, 2)],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        expected_output)

  def testFailingMatch(self):
    """Tests that shard times can be extracted from failing shards."""
    step_output = {
        'steps': [
            {
                'name':
                'All failing',
                'summaryMarkdown': ('Max pending time: 2s (shard #1)'
                                    '* [shard #0 (failed) (1s)]'
                                    '* [shard #1 (failed) (2s)]'),
            },
        ],
    }
    expected_output = {
        'All failing': [(1, 0), (2, 0)],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        expected_output)

  def testTimeoutMatch(self):
    """Tests that shard times can be extracted from timed out shards."""
    step_output = {
        'steps': [
            {
                'name':
                'All timeout',
                'summaryMarkdown': ('Max pending time: 2s (shard #1)'
                                    '* [shard #0 timed out after 1s]'
                                    '* [shard #1 timed out after 2s]'),
            },
        ],
    }
    expected_output = {
        'All timeout': [(1, 0), (2, 0)],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        expected_output)

  def testSwarmingFailuresIgnored(self):
    """Tests that internal swarming failures are silently ignored."""
    step_output = {
        'steps': [
            {
                'name':
                'All infra failure',
                'summaryMarkdown':
                ('Max pending time: 2s (shard #1)'
                 '* [shard #0 had an internal swarming failure]'
                 '* [shard #1 had an internal swarming failure]'),
            },
        ],
    }
    # assertNoLogs would be useful here, but is only available in Python 3.10
    # and above.
    with mock.patch.object(get_machine_times.logging,
                           'warning',
                           side_effect=RuntimeError):
      self.assertEqual(
          get_machine_times._GetShardTimesFromStepOutput(
              json.dumps(step_output)), {})

  def testNoDataReported(self):
    """Tests that a failure to get shard runtimes is reported to the user."""
    step_output = {
        'steps': [
            {
                'name': 'Missing',
                'summaryMarkdown': 'Max pending time: 1s (shard #0)',
            },
        ],
    }
    with self.assertLogs(level='WARNING'):
      self.assertEqual(
          get_machine_times._GetShardTimesFromStepOutput(
              json.dumps(step_output)), {})

  def testMixedShards(self):
    """Tests shard time extraction with a mix of different shards."""
    step_output = {
        'steps': [
            {
                'name':
                'Mixed',
                'summaryMarkdown':
                ('Max pending time: 3s (shard #2)'
                 '* [shard #0 (runtime (1s) + overhead (1s): 2s)]'
                 '* [shard #1 (failed) (2s)]'
                 '* [shard #2 timed out after 3s]'),
            },
        ],
    }
    expected_output = {
        'Mixed': [(1, 1), (2, 0), (3, 0)],
    }
    self.assertEqual(
        get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output)),
        expected_output)

  def testDuplicateSteps(self):
    """Tests that duplicate shards are not supported."""
    step_output = {
        'id':
        'build-id',
        'steps': [
            {
                'name':
                'I am the real one',
                'summaryMarkdown':
                ('Max pending time: 2s (shard #1) '
                 '* [shard #0 (runtime (1s) + overhead (1s): 2s)]'
                 '* [shard #1 (runtime (2s) + overhead (2s): 4s)]'),
            },
            {
                'name':
                'I am the real one',
                'summaryMarkdown':
                ('Max pending time: 2s (shard #1) '
                 '* [shard #0 (runtime (1s) + overhead (1s): 2s)]'
                 '* [shard #1 (runtime (2s) + overhead (2s): 4s)]'),
            },
        ],
    }
    with self.assertRaises(AssertionError):
      get_machine_times._GetShardTimesFromStepOutput(json.dumps(step_output))


class ConvertSummaryRuntimeToSecondsUnittest(unittest.TestCase):
  def testMinutesAndSeconds(self):
    """Tests conversion with minutes and seconds present."""
    self.assertEqual(get_machine_times._ConvertSummaryRuntimeToSeconds('1m 1s'),
                     61)

  def testSecondsOnly(self):
    """Tests conversion with only seconds present."""
    self.assertEqual(get_machine_times._ConvertSummaryRuntimeToSeconds('1s'), 1)


if __name__ == '__main__':
  unittest.main(verbosity=2)
