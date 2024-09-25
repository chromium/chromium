#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to help find bad GPU test machines that need fixing."""

import argparse
from typing import Dict

from bad_machine_finder import bigquery
from bad_machine_finder import detection
from bad_machine_finder import swarming
from bad_machine_finder import tasks
from bad_machine_finder import test_specs

MIXIN_GROUPS = {
    'gpu': [
        # ChromeOS amd64-generic omitted since it is run on GCE instances.
        # ChromeOS volteer omitted since it runs in Skylab.
        # gpu_samsung_a13_stable omitted until devices are available
        # motorola_moto_g_power_5g omitted since the configuration has been
        #    dropped
        # win10_nvidia_rtx_4070_super_stable omitted until additional machines
        #    are ready
        'chromium_nexus_5x_oreo',
        'chromium_pixel_2_pie',
        'gpu_nvidia_shield_tv_stable',
        'gpu_pixel_4_stable',
        'gpu_pixel_6_experimental',
        'gpu_pixel_6_stable',
        'gpu_samsung_a23_stable',
        'gpu_samsung_s23_stable',
        'gpu_samsung_s24_stable',
        'linux_amd_rx_5500_xt',
        'linux_nvidia_gtx_1660_experimental',
        'linux_nvidia_gtx_1660_stable',
        'linux_intel_uhd_630_experimental',
        'linux_intel_uhd_630_stable',
        'linux_intel_uhd_770_stable',
        'mac_arm64_apple_m1_gpu_experimental',
        'mac_arm64_apple_m1_gpu_stable',
        'mac_arm64_apple_m2_retina_gpu_experimental',
        'mac_arm64_apple_m2_retina_gpu_stable',
        'mac_mini_intel_gpu_experimental',
        'mac_mini_intel_gpu_stable',
        'mac_pro_amd_gpu',
        'mac_retina_amd_gpu_experimental',
        'mac_retina_amd_gpu_stable',
        'mac_retina_nvidia_gpu_experimental',
        'mac_retina_nvidia_gpu_stable',
        'win10_amd_rx_5500_xt_stable',
        'win10_intel_uhd_630_experimental',
        'win10_intel_uhd_630_stable',
        'win10_intel_uhd_770_stable',
        'win10_nvidia_gtx_1660_experimental',
        'win10_nvidia_gtx_1660_stable',
        'win11_qualcomm_adreno_690_stable',
    ],
}


def ParseArgs() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description='Find machines that are likely contributing to test failures')
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

  mixin_group = parser.add_mutually_exclusive_group(required=True)
  mixin_group.add_argument('--mixin',
                           action='append',
                           dest='mixins',
                           help=('The name of the mixin to get data for. Can '
                                 'be specified multiple times.'))
  mixin_group.add_argument('--mixin-group',
                           choices=sorted(list(MIXIN_GROUPS.keys())),
                           help='A preset group of mixins to check.')

  args = parser.parse_args()

  _VerifyArgs(parser, args)

  return args


def _VerifyArgs(parser: argparse.ArgumentParser,
                args: argparse.Namespace) -> None:
  if args.sample_period <= 0:
    parser.error('--sample-period must be greater than 0')


def _GetDimensionsByMixin(
    args: argparse.Namespace) -> Dict[str, test_specs.DimensionSet]:
  if args.mixin_group:
    mixins = MIXIN_GROUPS[args.mixin_group]
  else:
    mixins = args.mixins

  dimensions_by_mixin = {
      mixin_name: test_specs.GetMixinDimensions(mixin_name)
      for mixin_name in mixins
  }

  return dimensions_by_mixin


def _AnalyzeMixin(mixin_stats: tasks.MixinStats,
                  args: argparse.Namespace) -> detection.BadMachineList:
  bad_machine_list = detection.BadMachineList()
  bad_machine_list.Merge(
      detection.DetectViaStdDevOutlier(mixin_stats, args.stddev_multiplier))
  bad_machine_list.Merge(
      detection.DetectViaRandomChance(mixin_stats,
                                      args.random_chance_probability_threshold))
  return bad_machine_list


def main() -> None:
  args = ParseArgs()
  dimensions_by_mixin = _GetDimensionsByMixin(args)
  querier = bigquery.Querier(args.billing_project)
  task_stats = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin,
                                              args.sample_period)

  for mixin_name, mixin_stats in task_stats.items():
    bad_machine_list = _AnalyzeMixin(mixin_stats, args)
    if not bad_machine_list.bad_machines:
      continue

    print(f'\nBad machines for {mixin_name}')
    bot_ids = sorted(list(bad_machine_list.bad_machines.keys()))
    for b in bot_ids:
      print(f'  {b}')
      for reason in bad_machine_list.bad_machines[b]:
        print(f'    * {reason}')


if __name__ == '__main__':
  main()
