#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to help find bad GPU test machines that need fixing."""

import argparse

from bad_machine_finder import bigquery
from bad_machine_finder import swarming
from bad_machine_finder import test_specs


def ParseArgs() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description='Find machines that are likely contributing to test failures')
  parser.add_argument('--mixin',
                      required=True,
                      help='The name of the mixin to get data for')
  parser.add_argument('--sample-period',
                      type=int,
                      default=7,
                      help='The number of days to sample data from')

  args = parser.parse_args()

  _VerifyArgs(parser, args)

  return args


def _VerifyArgs(parser: argparse.ArgumentParser,
                args: argparse.Namespace) -> None:
  if args.sample_period <= 0:
    parser.error('--sample-period must be greater than 0')


def main() -> None:
  args = ParseArgs()
  dimensions = test_specs.GetMixinDimensions(args.mixin)
  dimensions_by_mixin = {args.mixin: dimensions}
  querier = bigquery.Querier('chrome-unexpected-pass-data')
  task_stats = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin,
                                              args.sample_period)
  # We currently only support one mixin at a time.
  task_stats = task_stats[args.mixin]

  bot_failure_rate_pairs = []
  for bot_id, bot_stats in task_stats.IterBots():
    failed_task_rate = (float(bot_stats.failed_tasks) / bot_stats.total_tasks)
    bot_failure_rate_pairs.append((bot_id, failed_task_rate))

  bot_failure_rate_pairs.sort(key=lambda kv: kv[1], reverse=True)
  print('Task failure rates:')
  for bot_id, failure_rate in bot_failure_rate_pairs:
    print(f'{bot_id}: {failure_rate}')


if __name__ == '__main__':
  main()
