# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with Swarming."""

import collections
from concurrent import futures
import json
import os
import subprocess
from typing import Dict, List

import gpu_path_util


def GetBotIdFromTask(task_id: str) -> str:
  """Retrieves which swarming bot a task ran on.

  Args:
    task_id: The ID of the Swarming task to query.

  Returns:
    The name of the Swarming bot that was used to run |task_id|.
  """
  swarming_cmd = [
      os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'tools', 'luci-go',
                   'swarming'),
      'collect',
      '-json-output',
      '-',
      '-quiet',
      '-S',
      'chromium-swarm.appspot.com',
      task_id,
  ]
  task_output = json.loads(subprocess.check_output(swarming_cmd, text=True))
  bot_dimensions = task_output[task_id]['results']['bot_dimensions']
  for dimension in bot_dimensions:
    if dimension['key'] == 'id':
      return dimension['value'][0]
  raise RuntimeError(f'Could not find bot ID for task {task_id}')


def GetBotCountsFromTasks(tasks: List[str]) -> Dict[str, int]:
  """Tallies how many times each bot ran one of the given tasks.

  Args:
    tasks: The Swarming task IDs to check.

  Returns:
    A dict mapping swarming bot names to the number of times that bot ran one
    of the tasks in |tasks|.
  """
  jobs = []
  with futures.ThreadPoolExecutor() as pool:
    for t in tasks:
      jobs.append(pool.submit(GetBotIdFromTask, t))
  bot_counts = collections.defaultdict(int)
  for j in jobs:
    bot_counts[j.result()] += 1
  return bot_counts
