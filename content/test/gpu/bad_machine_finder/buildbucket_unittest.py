# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from bad_machine_finder import buildbucket


class GetRecentFailuresFromBuildersUnittest(unittest.TestCase):

  def testCommandGeneration(self):  # pylint: disable=no-self-use
    """Tests that the generated command is correct."""
    # pylint: disable=line-too-long
    expected_command = [
        'bb',
        'ls',
        '-n',
        '5',
        '-fields',
        'id',
        '-nocolor',
        '-predicate',
        '{"builder": {"project": "chromium", "bucket": "ci", "builder": "builder-1"}, "status": "FAILURE"}',
        '-predicate',
        '{"builder": {"project": "chromium", "bucket": "ci", "builder": "builder-1"}, "status": "INFRA_FAILURE"}',
        '-predicate',
        '{"builder": {"project": "chromium", "bucket": "ci", "builder": "builder-2"}, "status": "FAILURE"}',
        '-predicate',
        '{"builder": {"project": "chromium", "bucket": "ci", "builder": "builder-2"}, "status": "INFRA_FAILURE"}',
    ]
    # pylint: enable=line-too-long
    with mock.patch.object(buildbucket.subprocess,
                           'check_output') as subprocess_mock:
      buildbucket.GetRecentFailuresFromBuilders(['builder-1', 'builder-2'], 5)
      subprocess_mock.assert_called_once_with(expected_command, text=True)

  def testOutputParsing(self):
    """Tests that bb output is properly parsed and returned."""
    # pylint: disable=line-too-long
    bb_output = """\
http://ci.chromium.org/b/8737608613340544369 FAILURE         'chromium/ci/Mac FYI Experimental Retina Release (AMD)'

http://ci.chromium.org/b/8737638557026093953 FAILURE         'chromium/ci/Mac FYI Retina Release (AMD)'

http://ci.chromium.org/b/8737642267188210897 INFRA_FAILURE   'chromium/ci/Mac FYI Experimental Retina Release (AMD)'

http://ci.chromium.org/b/8737649523261209377 FAILURE         'chromium/ci/Mac FYI Experimental Retina Release (AMD)'

http://ci.chromium.org/b/8737682546980849201 FAILURE         'chromium/ci/Mac FYI Experimental Retina Release (AMD)'
"""
    # pylint: enable=line-too-long
    with mock.patch.object(buildbucket.subprocess,
                           'check_output',
                           return_value=bb_output):
      failures = buildbucket.GetRecentFailuresFromBuilders([
          'Mac FYI Experimental Retina Release (AMD)',
          'Mac FYI Retina Release (AMD)'
      ], 5)
    expected_failures = [
        buildbucket.Failure('8737608613340544369', 'FAILURE'),
        buildbucket.Failure('8737638557026093953', 'FAILURE'),
        buildbucket.Failure('8737642267188210897', 'INFRA_FAILURE'),
        buildbucket.Failure('8737649523261209377', 'FAILURE'),
        buildbucket.Failure('8737682546980849201', 'FAILURE'),
    ]
    self.assertEqual(failures, expected_failures)


class GetFailedSwarmingTasksUnittests(unittest.TestCase):

  def testOutputParsing(self):
    """Tests that bb output is properly parsed and returned."""
    # pylint: disable=line-too-long
    bb_output = """\
Successful task, should not be parsed
* [shard #1 (runtime (4m 39s) + overhead (20s): 5m 09s)](https://chromium-swarm.appspot.com/task?id=6bdb10fd08c5b410)
Failed task, should be parsed
* [shard #1 (failed) (4m 39s)](https://chromium-swarm.appspot.com/task?id=6bcb05884fb51610)
Infra failure task, should be parsed
* [shard #1 had an internal swarming failure](https://chromium-swarm.appspot.com/task?id=6bc878810b037010)
Timeout task, should be parsed
* [shard #6 timed out after 30m 30s](https://chromium-swarm.appspot.com/task?id=6bd7ffc80b748710)
"""
    # pylint: enable=line-too-long
    with mock.patch.object(buildbucket,
                           'GetBuilderSteps',
                           return_value=bb_output):
      failed_task_ids = buildbucket.GetFailedSwarmingTasks('id')

    expected_ids = [
        # Failure.
        '6bcb05884fb51610',
        # Infra failure.
        '6bc878810b037010',
        # Timeout.
        '6bd7ffc80b748710',
    ]
    self.assertEqual(failed_task_ids, expected_ids)

  def testMalformedTaskId(self):
    """Tests behavior when a malformed Swarming task ID is found."""
    # pylint: disable=line-too-long
    outputs = [
        # Completely malformed.
        """\
* [shard #1 (failed) (4m 39s)](https://chromium-swarm.appspot.com/task?id=malformed)
""",
        # Too short.
        """\
* [shard #1 (failed) (4m 39s)](https://chromium-swarm.appspot.com/task?id=6bcb05884fb5161)
""",
        # Too long.
        """\
* [shard #1 (failed) (4m 39s)](https://chromium-swarm.appspot.com/task?id=6bcb05884fb516100)
""",
    ]
    # pylint: enable-line-too-long
    for bb_output in outputs:
      with mock.patch.object(buildbucket,
                             'GetBuilderSteps',
                             return_value=bb_output):
        with self.assertRaisesRegex(
            RuntimeError, r'Failed to extract task ID from '
            r'https://chromium-swarm\.appspot\.com/task\?id=.*'):
          buildbucket.GetFailedSwarmingTasks('id')


class GetSwarmingTasksForFailuresUnittests(unittest.TestCase):

  def testBasic(self):
    """Tests behavior along the basic/happy path."""

    def SideEffect(buildbucket_id: str):
      return [f'failed_task_{buildbucket_id}']

    with mock.patch.object(buildbucket,
                           'GetFailedSwarmingTasks',
                           side_effect=SideEffect):
      failed_task_ids = buildbucket.GetSwarmingTasksForFailures([
          buildbucket.Failure('build_1', 'FAILURE'),
          buildbucket.Failure('build_2', 'INFRA_FAILURE'),
      ])

    self.assertEqual(failed_task_ids,
                     ['failed_task_build_1', 'failed_task_build_2'])
