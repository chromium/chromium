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

import argparse
import logging
import os
import sys

from unexpected_passes import builders
from unexpected_passes import expectations
from unexpected_passes import queries
from unexpected_passes import result_output

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
          'pixel',
          'power',
          'screenshot_sync',
          'trace_test',
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


def WarnUserOfIncompleteRollout():
  response = raw_input('WARNING: This script relies on data from ResultDB, '
                       'which is not enabled on all builders yet. As such, '
                       'results from this script should not be trusted yet. '
                       'Do you want to continue? y/N ')
  if response.lower() != 'y':
    sys.exit(0)


def main():
  args = ParseArgs()
  # TODO(crbug.com/1108016): Remove this warning once ResultDB is enabled on all
  # builders and there is enough data for the results to be trusted.
  WarnUserOfIncompleteRollout()
  test_expectation_map = expectations.CreateTestExpectationMap(
      args.expectation_file, args.tests)
  ci_builders = builders.GetCiBuilders(
      SUITE_TO_TELEMETRY_SUITE_MAP.get(args.suite, args.suite))
  # Unmatched results are mainly useful for script maintainers, as they don't
  # provide any additional information for the purposes of finding unexpectedly
  # passing tests or unused expectations.
  unmatched = queries.FillExpectationMapForCiBuilders(test_expectation_map,
                                                      ci_builders, args.suite,
                                                      args.project,
                                                      args.num_samples)
  try_builders = builders.GetTryBuilders(ci_builders)
  unmatched.update(
      queries.FillExpectationMapForTryBuilders(test_expectation_map,
                                               try_builders, args.suite,
                                               args.project, args.num_samples))
  unused_expectations = expectations.FilterOutUnusedExpectations(
      test_expectation_map)
  stale, semi_stale, active = expectations.SplitExpectationsByStaleness(
      test_expectation_map)
  result_output.OutputResults(stale, semi_stale, active, unmatched,
                              unused_expectations, args.output_format)

  if args.remove_stale_expectations:
    stale_expectations = []
    for _, expectation_map in stale.iteritems():
      stale_expectations.extend(expectation_map.keys())
    stale_expectations.extend(unused_expectations)
    removed_urls = expectations.RemoveExpectationsFromFile(
        stale_expectations, args.expectation_file)
    print('Stale expectations removed from %s. Stale comments, etc. may still '
          'need to be removed.' % args.expectation_file)
    result_output.OutputRemovedUrls(removed_urls)


if __name__ == '__main__':
  main()
