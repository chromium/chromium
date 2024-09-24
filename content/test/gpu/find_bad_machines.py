#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to help find bad GPU test machines that need fixing."""

import argparse

from bad_machine_finder import bigquery
from bad_machine_finder import detection
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
  parser.add_argument('--billing-project',
                      default='chrome-unexpected-pass-data',
                      help='The billing project to use for queries')
  parser.add_argument('--stddev-multiplier',
                      type=float,
                      default=3,
                      help=('Used with the stddev outlier detection method. '
                            "Sets how many standard deviations a bot's failure "
                            'rate has to be over the fleet-wide mean for it '
                            'to be considered bad.'))
  parser.add_argument('--random-chance-probability-threshold',
                      type=float,
                      default=0.005,
                      help=('Used with the random chance detection method. '
                            'Sets how unlikely it has to be that a bot '
                            'randomly got at least as many failures as it did '
                            'in order for it to be considered bad.'))

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
  querier = bigquery.Querier(args.billing_project)
  task_stats = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin,
                                              args.sample_period)
  # We currently only support one mixin at a time.
  mixin_stats = task_stats[args.mixin]

  bad_machine_list = detection.BadMachineList()
  bad_machine_list.Merge(
      detection.DetectViaStdDevOutlier(mixin_stats, args.stddev_multiplier))
  bad_machine_list.Merge(
      detection.DetectViaRandomChance(mixin_stats,
                                      args.random_chance_probability_threshold))

  print('Bad machines:')
  bot_ids = sorted(list(bad_machine_list.bad_machines.keys()))
  for b in bot_ids:
    print(f'  {b}')
    for reason in bad_machine_list.bad_machines[b]:
      print(f'    * {reason}')


if __name__ == '__main__':
  main()
