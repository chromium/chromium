#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
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
General disables can be handled via `finder:disable-general` and
`finder:enable-general`. Disabling removal only if the expectation is found to
be unused can be handled via `finder:disable-unused` and `finder:enable-unused`.
Disabling removal only if the expectation is found to be stale can be handled
via `finder:disable-stale` and `finder:enable-stale`.
"""

import argparse
import datetime
import os

from gpu_path_util import setup_telemetry_paths  # pylint: disable=unused-import
from gpu_path_util import setup_testing_paths  # pylint: disable=unused-import

from gpu_tests import gpu_integration_test

from unexpected_passes import gpu_builders
from unexpected_passes import gpu_expectations
from unexpected_passes import gpu_queries
from unexpected_passes_common import argument_parsing
from unexpected_passes_common import builders
from unexpected_passes_common import expectations
from unexpected_passes_common import result_output


def ParseArgs() -> argparse.Namespace:
  name_mapping = gpu_integration_test.GenerateTestNameMapping()
  test_suites = list(name_mapping.keys())
  test_suites.sort()

  parser = argparse.ArgumentParser(
      description=('Script for finding cases of stale expectations that can '
                   'be removed/modified.'))
  argument_parsing.AddCommonArguments(parser)

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
  parser.add_argument('--suite',
                      required=True,
                      choices=test_suites,
                      help='The test suite being checked.')

  args = parser.parse_args()
  argument_parsing.PerformCommonPostParseSetup(args)
  suite_class = name_mapping[args.suite]

  if not (args.tests or args.expectation_file):
    expectation_files = suite_class.ExpectationsFiles()
    if not expectation_files:
      raise RuntimeError(
          'Suite %s does not specify an expectation file and is thus not '
          'compatible with this script.' % args.suite)
    if len(expectation_files) > 1:
      raise RuntimeError(
          'Suite %s specifies %d expectation files when only 1 is supported.' %
          len(expectation_files))
    args.expectation_file = expectation_files[0]

  if args.remove_stale_expectations and not args.expectation_file:
    parser.error(
        '--remove-stale-expectations can only be used with expectation files')

  # Change to whatever repo the test suite claims the expectation file lives in.
  # This allows the script to work for most suites if run from outside of
  # chromium/src. Similarly, it allows suites such as WebGPU CTS that have
  # expectation files in a different repo to be work when run from chromium/src.
  os.chdir(suite_class.GetExpectationsFilesRepoPath())

  return args


# pylint: disable=too-many-locals
def main() -> None:
  args = ParseArgs()

  builders_instance = gpu_builders.GpuBuilders(args.suite,
                                               args.include_internal_builders)
  builders.RegisterInstance(builders_instance)
  expectations_instance = gpu_expectations.GpuExpectations()
  expectations.RegisterInstance(expectations_instance)

  test_expectation_map = expectations_instance.CreateTestExpectationMap(
      args.expectation_file, args.tests,
      datetime.timedelta(days=args.expectation_grace_period))
  ci_builders = builders_instance.GetCiBuilders()

  querier = gpu_queries.GpuBigQueryQuerier(args.suite, args.project,
                                           args.num_samples,
                                           args.keep_unmatched_results)
  # Unmatched results are mainly useful for script maintainers, as they don't
  # provide any additional information for the purposes of finding unexpectedly
  # passing tests or unused expectations.
  unmatched = querier.FillExpectationMapForBuilders(test_expectation_map,
                                                    ci_builders)
  try_builders = builders_instance.GetTryBuilders(ci_builders)
  unmatched.update(
      querier.FillExpectationMapForBuilders(test_expectation_map, try_builders))
  unused_expectations = test_expectation_map.FilterOutUnusedExpectations()
  stale, semi_stale, active = test_expectation_map.SplitByStaleness()
  if args.result_output_file:
    with open(args.result_output_file, 'w') as outfile:
      result_output.OutputResults(stale, semi_stale, active, unmatched,
                                  unused_expectations, args.output_format,
                                  outfile)
  else:
    result_output.OutputResults(stale, semi_stale, active, unmatched,
                                unused_expectations, args.output_format)

  affected_urls = set()
  stale_message = ''
  if args.remove_stale_expectations:
    for expectation_file, expectation_map in stale.items():
      affected_urls |= expectations_instance.RemoveExpectationsFromFile(
          expectation_map.keys(), expectation_file,
          expectations.RemovalType.STALE)
      stale_message += ('Stale expectations removed from %s. Stale comments, '
                        'etc. may still need to be removed.\n' %
                        expectation_file)
    for expectation_file, unused_list in unused_expectations.items():
      affected_urls |= expectations_instance.RemoveExpectationsFromFile(
          unused_list, expectation_file, expectations.RemovalType.UNUSED)
      stale_message += ('Unused expectations removed from %s. Stale comments, '
                        'etc. may still need to be removed.\n' %
                        expectation_file)

  if args.narrow_semi_stale_expectation_scope:
    affected_urls |= expectations_instance.NarrowSemiStaleExpectationScope(
        semi_stale)
    stale_message += ('Semi-stale expectations narrowed in %s. Stale comments, '
                      'etc. may still need still need to be removed.\n' %
                      args.expectation_file)

  if stale_message:
    print(stale_message)
  if affected_urls:
    orphaned_urls = expectations_instance.FindOrphanedBugs(affected_urls)
    if args.bug_output_file:
      with open(args.bug_output_file, 'w') as bug_outfile:
        result_output.OutputAffectedUrls(affected_urls,
                                         orphaned_urls,
                                         bug_outfile,
                                         auto_close_bugs=args.auto_close_bugs)
    else:
      result_output.OutputAffectedUrls(affected_urls,
                                       orphaned_urls,
                                       auto_close_bugs=args.auto_close_bugs)
# pylint: enable=too-many-locals


if __name__ == '__main__':
  main()
