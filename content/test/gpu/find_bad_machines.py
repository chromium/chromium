#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to help find bad GPU test machines that need fixing."""

import argparse

from bad_machine_finder import buildbucket
from bad_machine_finder import swarming
from bad_machine_finder import test_specs


def ParseArgs() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description='Find machines that are likely contributing to test failures')
  parser.add_argument('--num-samples',
                      type=int,
                      default=50,
                      help='The number of failed builds to look at')
  parser.add_argument('--mixin',
                      required=True,
                      help='The name of the mixin to get data for')

  args = parser.parse_args()

  _VerifyArgs(parser, args)

  return args


def _VerifyArgs(parser: argparse.ArgumentParser,
                args: argparse.Namespace) -> None:
  if args.num_samples <= 0:
    parser.error('--num-samples must be greater than 0')


def main() -> None:
  args = ParseArgs()
  builders = test_specs.GetBuildersWithMixin(args.mixin)
  failures = buildbucket.GetRecentFailuresFromBuilders(builders,
                                                       args.num_samples)
  failed_tasks = buildbucket.GetSwarmingTasksForFailures(failures)
  bot_counts = swarming.GetBotCountsFromTasks(failed_tasks)

  kv_pairs = list(bot_counts.items())
  kv_pairs.sort(key=lambda kv: kv[1], reverse=True)

  print('Failed task counts:')
  for bot_id, failed_task_count in kv_pairs:
    print(f'{bot_id}: {failed_task_count}')


if __name__ == '__main__':
  main()
