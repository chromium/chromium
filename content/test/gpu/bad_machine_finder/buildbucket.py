# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with Buildbucket."""

from concurrent import futures
import json
import logging
import re
import subprocess
from typing import List

import dataclasses  # Built-in, but pylint gives an ordering false positive.

# pylint: disable=line-too-long
# Extracts the Swarming URL from lines such as
# [shard #1 (failed) (4m 39s)](https://chromium-swarm.appspot.com/task?id=6bcb05884fb51610)
FAILED_TASK_REGEX = re.compile(r'\[shard #\d+ \(failed\)[^\]]*\]\(([^)]+)\)')
# Extracts the Swarming URL from lines such as
# [shard #1 had an internal swarming failure](https://chromium-swarm.appspot.com/task?id=6bc878810b037010)
INFRA_FAILURE_TASK_REGEX = re.compile(
    r'\[shard #\d+ had an internal swarming failure\]\(([^)]+)\)')
# Extracts the Swarming URL from lines such as
# [shard #6 timed out after 30m 30s](https://chromium-swarm.appspot.com/task?id=6bd7ffc80b748710)
TIMED_OUT_TASK_REGEX = re.compile(
    r'\[shard #\d+ timed out after[^\]]*\]\(([^)]+)\)')
# pylint: enable=line-too-long
# Extracts the Swarming task ID from a Swarming URL, ensuring that it is
# exactly 16 hex characters.
TASK_ID_REGEX = re.compile(r'id=([a-fA-F0-9]{16}$)')

ALL_TASK_REGEXES = [
    FAILED_TASK_REGEX,
    INFRA_FAILURE_TASK_REGEX,
    TIMED_OUT_TASK_REGEX,
]

STATUSES = [
    'FAILURE',
    'INFRA_FAILURE',
]


@dataclasses.dataclass
class Failure:
  buildbucket_id: str
  status: str


def GetBuilderSteps(buildbucket_id: str) -> str:
  """Gets the Buildbucket steps output for the given build.

  Args:
    buildbucket_id: The Buildbucket build to query.

  Returns:
    A string containing the Buildbucket steps information for |buildbucket_id|
    in Markdown.
  """
  bb_cmd = [
      'bb',
      'get',
      '-nocolor',
      '-steps',
      buildbucket_id,
  ]
  return subprocess.check_output(bb_cmd, text=True)


def GetRecentFailuresFromBuilders(builders: List[str],
                                  num_samples: int) -> List[Failure]:
  """Gets the most recent failed builds from the given builders.

  Args:
    builders: The Chromium CI builders to get failed builds for.
    num_samples: The number of recent failed builds to get. This number is
        spread across all builders, e.g. if len(builders) == 2 and
        num_samples == 10, 10 failed builds total will be found rather than
        10 per builder (20 total).

  Returns:
    A list of Failure objects, one for each failed build found.
  """
  bb_cmd = [
      'bb',
      'ls',
      '-n',
      str(num_samples),
      '-fields',
      'id',
      '-nocolor',
  ]
  for b in builders:
    for status in STATUSES:
      bb_cmd.extend([
          '-predicate',
          json.dumps({
              'builder': {
                  'project': 'chromium',
                  'bucket': 'ci',
                  'builder': b,
              },
              'status': status,
          })
      ])

  output = subprocess.check_output(bb_cmd, text=True)

  failures = []
  for line in output.splitlines():
    if not line.strip():
      continue

    url, status, _ = line.split(maxsplit=2)
    failures.append(Failure(url.split('/')[-1], status))
  return failures


def GetFailedSwarmingTasks(buildbucket_id: str) -> List[str]:
  """Gets all failed swarming tasks for a Buildbucket build.

  Args:
    buildbucket_id: The Buildbucket build to query.

  Returns:
    A list of Swarming task IDs that failed in some way in |buildbucket_id|.
  """
  failed_task_urls = []
  step_output = GetBuilderSteps(buildbucket_id)
  for regex in ALL_TASK_REGEXES:
    failed_task_urls.extend(regex.findall(step_output))
  if not failed_task_urls:
    logging.warning('No failed tasks found for build %s', buildbucket_id)
  failed_task_ids = []
  for ftu in failed_task_urls:
    match = TASK_ID_REGEX.search(ftu)
    if not match:
      raise RuntimeError(f'Failed to extract task ID from {ftu}')
    failed_task_ids.append(match.group(1))
  return failed_task_ids


def GetSwarmingTasksForFailures(failures: List[Failure]) -> List[str]:
  """Gets all failed swarming tasks from the given failed builds.

  Args:
    failures: A list of failed builds to pull failed tasks from.

  Returns:
    A list of Swarming task IDs, one for each failed task found.
  """
  jobs = []
  with futures.ThreadPoolExecutor() as pool:
    for f in failures:
      jobs.append(pool.submit(GetFailedSwarmingTasks, f.buildbucket_id))
  failed_task_ids = []
  for j in jobs:
    failed_task_ids.extend(j.result())
  return failed_task_ids
