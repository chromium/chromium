#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for determining which GPU tests are unexpectedly passing.

This script depends on the `bb` tool, which is available as part of depot tools,
and the `bq` tool, which is available as part of the Google Cloud SDK
https://cloud.google.com/sdk/docs/quickstarts.

Example usage:

unexpected_pass_finder.py \
  --project <BigQuery billing project> \
  --suite <test suite to check> \

Concrete example:

unexpected_pass_finder.py \
  --project luci-resultdb-dev \
  --suite pixel

You would typically want to pass in --remove-stale-expectations as well in order
to have the script automatically remove any expectations it determines are no
longer necessary. If a particular expectation proves to be erroneously flagged
and removed (e.g. due to a very low flake rate that doesn't get caught
consistently by the script), expectations can be omitted from automatic removal
using an inline `# finder:disable` comment for a single expectation or a pair of
`# finder:disable`/`# finder:enable` comments for a block of expectations.
"""

from __future__ import print_function

import argparse
import logging
import os
import sys

CHROMIUM_SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..')
sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'testing'))

from unexpected_passes import gpu_builders
from unexpected_passes import gpu_expectations
from unexpected_passes import gpu_queries
from unexpected_passes_common import builders
from unexpected_passes_common import result_output

SUITE_TO_EXPECTATIONS_MAP = {
    'power': 'power_measurement',
    'webgl_conformance1': 'webgl_conformance',
    'webgl_conformance2': 'webgl2_conformance',
}

SUITE_TO_TELEMETRY_SUITE_MAP = {
    'webgl_conformance1': 'webgl_conformance',
    'webgl_conformance2': 'webgl_conformance',
}


def ParseArgs():
  parser = argparse.ArgumentParser(
      description=('Script for finding cases of stale expectations that can '
                   'be removed/modified.'))
  input_group = parser.add_mutually_exclusive_group()
  input_group.add_argument(
      '--expectation-file',
      help='A path to an expectation file to read from. If not specified and '
      '--test is not used, will automatically determine based off the '
      'provided suite.')
  input_group.add_argument(
      '--test',
      action='append',
      dest='tests',
      default=[],
      help='The name of a test to check for unexpected passes. Can be passed '
      'multiple times to specify multiple tests. Will be treated as if it was '
      'expected to be flaky on all configurations.')
  parser.add_argument(
      '--suite',
      required=True,
      # Could probably autogenerate this list using the same
      # method as Telemetry's run_browser_tests.py once there is no need to
      # distinguish WebGL 1 from WebGL 2.
      choices=[
          'context_lost',
          'depth_capture',
          'hardware_accelerated_feature',
          'gpu_process',
          'info_collection',
          'maps',
          'mediapipe',
          'pixel',
          'power',
          'screenshot_sync',
          'trace_test',
          'webcodecs',
          'webgl_conformance1',
          'webgl_conformance2',
      ],
      help='The test suite being checked.')
  parser.add_argument('--project',
                      required=True,
                      help='The billing project to use for BigQuery queries. '
                      'Must have access to the ResultDB BQ tables, e.g. '
                      '"luci-resultdb.chromium.gpu_ci_test_results".')
  parser.add_argument('--num-samples',
                      type=int,
                      default=100,
                      help='The number of recent builds to query.')
  parser.add_argument('--output-format',
                      choices=[
                          'html',
                          'print',
                      ],
                      default='html',
                      help='How to output script results.')
  parser.add_argument('--remove-stale-expectations',
                      action='store_true',
                      default=False,
                      help='Automatically remove any expectations that are '
                      'determined to be stale from the expectation file.')
  parser.add_argument('--modify-semi-stale-expectations',
                      action='store_true',
                      default=False,
                      help='If any semi-stale expectations are found, prompt '
                      'the user about the modification of each one.')
  parser.add_argument('-v',
                      '--verbose',
                      action='count',
                      default=0,
                      help='Increase logging verbosity, can be passed multiple '
                      'times.')
  parser.add_argument('-q',
                      '--quiet',
                      action='store_true',
                      default=False,
                      help='Disable logging for non-errors.')
  parser.add_argument('--large-query-mode',
                      action='store_true',
                      default=False,
                      help='Run the script in large query mode. This incurs '
                      'a significant performance hit, but allows the use of '
                      'larger sample sizes on large test suites by partially '
                      'working around a hard memory limit in BigQuery.')

  args = parser.parse_args()
  if args.quiet:
    args.verbose = -1
  SetLoggingVerbosity(args.verbose)

  if not (args.tests or args.expectation_file):
    args.expectation_file = os.path.join(
        os.path.dirname(__file__), 'gpu_tests', 'test_expectations',
        '%s_expectations.txt' %
        SUITE_TO_EXPECTATIONS_MAP.get(args.suite, args.suite))

  if args.remove_stale_expectations and not args.expectation_file:
    raise argparse.ArgumentError('--remove-stale-expectations',
                                 'Can only be used with expectation files')

  return args


def SetLoggingVerbosity(verbosity_level):
  if verbosity_level == -1:
    level = logging.ERROR
  elif verbosity_level == 0:
    level = logging.WARNING
  elif verbosity_level == 1:
    level = logging.INFO
  else:
    level = logging.DEBUG
  logging.getLogger().setLevel(level)


def main():
  args = ParseArgs()

  builders_instance = gpu_builders.GpuBuilders()
  builders.RegisterInstance(builders_instance)
  expectations_instance = gpu_expectations.GpuExpectations()

  test_expectation_map = expectations_instance.CreateTestExpectationMap(
      args.expectation_file, args.tests)
  ci_builders = builders_instance.GetCiBuilders(
      SUITE_TO_TELEMETRY_SUITE_MAP.get(args.suite, args.suite))

  querier = gpu_queries.GpuBigQueryQuerier(args.suite, args.project,
                                           args.num_samples,
                                           args.large_query_mode)
  # Unmatched results are mainly useful for script maintainers, as they don't
  # provide any additional information for the purposes of finding unexpectedly
  # passing tests or unused expectations.
  unmatched = querier.FillExpectationMapForCiBuilders(test_expectation_map,
                                                      ci_builders)
  try_builders = builders_instance.GetTryBuilders(ci_builders)
  unmatched.update(
      querier.FillExpectationMapForTryBuilders(test_expectation_map,
                                               try_builders))
  unused_expectations = test_expectation_map.FilterOutUnusedExpectations()
  stale, semi_stale, active = test_expectation_map.SplitByStaleness()
  result_output.OutputResults(stale, semi_stale, active, unmatched,
                              unused_expectations, args.output_format)

  affected_urls = set()
  stale_message = ''
  if args.remove_stale_expectations:
    stale_expectations = []
    for _, expectation_map in stale.iteritems():
      stale_expectations.extend(expectation_map.keys())
    stale_expectations.extend(unused_expectations)
    affected_urls |= expectations_instance.RemoveExpectationsFromFile(
        stale_expectations, args.expectation_file)
    stale_message += ('Stale expectations removed from %s. Stale comments, '
                      'etc. may still need to be removed.\n' %
                      args.expectation_file)

  if args.modify_semi_stale_expectations:
    affected_urls |= expectations_instance.ModifySemiStaleExpectations(
        semi_stale, args.expectation_file)
    stale_message += ('Semi-stale expectations modified in %s. Stale '
                      'comments, etc. may still need to be removed.\n' %
                      args.expectation_file)

  if stale_message:
    print(stale_message)
  if affected_urls:
    orphaned_urls = expectations_instance.FindOrphanedBugs(affected_urls)
    result_output.OutputAffectedUrls(affected_urls, orphaned_urls)


if __name__ == '__main__':
  main()
