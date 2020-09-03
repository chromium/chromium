#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Script for determining which tests are unexpectedly passing.

This is particularly of use for GPU tests, where flakiness is heavily suppressed
but may be forgotten to be unsuppressed once the root cause is fixed.

This script depends on the `rdb` tool, which is available as part of depot
tools, and the `bq` tool, which is available as part of the Google Cloud SDK
https://cloud.google.com/sdk/docs/quickstarts.

Example usage:

find_unexpected_passing_tests.py \
  --builder <builder to check, can be repeated, optional> \
  --num-samples <number of builds to query, optional> \
  --project <billing project>

Concrete example:

find_unexpected_passing_tests.py \
  --builder "Win10 FYI x64 Release (NVIDIA)" \
  --num-samples 10 \
  --project luci-resultdb-dev

The --project argument can be any project you are associated with in the
Google Cloud console https://console.cloud.google.com/ (see drop-down menu in
the top left corner) that has sufficient permissions to query BigQuery.
"""

import argparse
import json
import os
import subprocess

QUERY_TEMPLATE = """\
WITH builds AS (
  SELECT
    id,
    start_time,
    builder.bucket,
    builder.builder
  FROM
    `cr-buildbucket.chromium.builds`
  WHERE
    builder.builder = "{builder}"
    # Ignore branch builders
    AND (builder.bucket = "ci" OR builder.bucket = "try")
    # Optimization
    AND create_time >= TIMESTAMP_SUB(CURRENT_TIMESTAMP(), INTERVAL 30 DAY)
)

SELECT * FROM builds ORDER BY start_time DESC LIMIT {num_samples}
"""

DEFAULT_BUILDERS = [
    # CI
    'Linux FYI Release (Intel HD 630)',
    'Linux FYI Release (NVIDIA)',
    'Mac FYI Release (Intel)',
    'Mac FYI Retina Release (AMD)',
    'Win10 FYI x64 Release (Intel HD 630)',
    'Win10 FYI x64 Release (NVIDIA)',
]

NINJA_TARGET_PREFIXES = [
    'chrome/test:telemetry_gpu_integration_test',
]

TEST_SUITE_PREFIXES = [
    '/gpu_tests.gpu_process_integration_test.GpuProcessIntegrationTest.',
    '/gpu_tests.hardware_accelerated_feature_integration_test.',
    '/gpu_tests.info_collection_test.InfoCollectionTest.',
    '/gpu_tests.pixel_integration_test.PixelIntegrationTest.',
    '/gpu_tests.trace_integration_test.TraceIntegrationTest.',
    ('/gpu_tests.webgl_conformance_integration_test.'
     'WebGLConformanceIntegrationTest.'),
]


def TryStripTestId(test_id):
  """Tries to strip off unnecessary information from a ResultDB test ID.

  Args:
    test_id: A ResultDB testId value.

  Returns:
    |test_id| with unnecessary information stripped off if possible.
  """
  test_id = test_id.replace('ninja://', '')
  for target in NINJA_TARGET_PREFIXES:
    test_id = test_id.replace(target, '')
  for subtest in TEST_SUITE_PREFIXES:
    test_id = test_id.replace(subtest, '')
  return test_id


def PrintUnexpectedPasses(unexpected_passes, args):
  """Prints out unexpected pass query results.

  Args:
    unexpected_passes: The output of GetUnexpectedPasses().
    args: The parsed arguments from an argparse.ArgumentParser.
  """
  for builder, passes in unexpected_passes.iteritems():
    passed_all = {}
    passed_some = {}
    for suite, tests in passes.iteritems():
      for test, num_passes in tests.iteritems():
        if num_passes == args.num_samples:
          passed_all.setdefault(suite, []).append(test)
        else:
          passed_some.setdefault(suite, []).append((test, num_passes))

    # Alphabetize for readability.
    for tests in passed_all.values():
      tests.sort()
    for tests in passed_some.values():
      tests.sort()

    print '##### %s #####' % builder
    if passed_all:
      print '----- Tests that passed in all runs -----'
      for suite, tests in passed_all.iteritems():
        print '%s:' % suite
        for test in tests:
          print '    %s' % test
        print ''
    if passed_some:
      print '----- Tests that passed in some runs -----'
      for suite, tests in passed_some.iteritems():
        print '%s:' % suite
        for (test, num_passes) in tests:
          print '    %s: %d/%d' % (test, num_passes, args.num_samples)
    print '\n\n'


def ConvertGpuToVendorName(gpu):
  """Converts a given GPU dimension string to a GPU vendor.

  E.g. a GPU containing "8086" will be mapped to "Intel".

  Args:
    gpu: A string containing a GPU dimension

  Returns:
    A string containing the GPU vendor.
  """
  if not gpu:
    return 'No GPU'
  elif '8086' in gpu:
    return 'Intel'
  elif '10de' in gpu:
    return 'NVIDIA'
  elif '1002' in gpu:
    return 'AMD'
  return gpu


def GetTestSuiteFromVariant(variant):
  """Gets a human-readable test suite from a ResultDB variant.

  Args:
    variant: A dict containing a variant definition from ResultDB

  Returns:
    A string containing the test suite.
  """
  suite_name = variant.get('test_suite', 'default_suite')
  gpu = variant.get('gpu')
  os = variant.get('os')
  gpu = ConvertGpuToVendorName(gpu)
  return '%s on %s on %s' % (suite_name, gpu, os)


def GetUnexpectedPasses(builds, args):
  """Gets the unexpected test passes from the given builds.

  Args:
    builds: The output of GetBuildbucketIds().
    args: The parsed arguments from an argparse.ArgumentParser.

  Returns:
    A dict in the following form:
    {
      builder (string): {
        suite variant (string): {
          test (string): num_passes (int),
        },
      },
    }
  """
  retval = {}
  for builder, buildbucket_ids in builds.iteritems():
    print 'Querying ResultDB for builder %s' % builder
    cmd = [
        'rdb',
        'query',
        '-json',
        '-u',  # Only get data for unexpected results.
    ]
    for bb_id in buildbucket_ids:
      cmd.append('build-%s' % bb_id)

    with open(os.devnull, 'w') as devnull:
      stdout = subprocess.check_output(cmd, stderr=devnull)

    # stdout should be a newline-separated list of JSON strings.
    for str_result in stdout.splitlines():
      result = json.loads(str_result)
      if 'testExoneration' not in result:
        continue
      if ('Unexpected passes' not in result['testExoneration']
          ['explanationHtml']):
        continue
      test_suite = GetTestSuiteFromVariant(
          result['testExoneration']['variant']['def'])
      test_id = TryStripTestId(result['testExoneration']['testId'])
      retval.setdefault(builder, {}).setdefault(test_suite,
                                                {}).setdefault(test_id, 0)
      retval[builder][test_suite][test_id] += 1
  return retval


def GetBuildbucketIds(args):
  """Gets the Buildbucket IDs for the given args.

  Args:
    args: The parsed arguments from an argparse.ArgumentParser.

  Returns:
    A dict of builder (string) to list of Buildbucket IDs (string).
  """
  retval = {}
  for builder in args.builders:
    print 'Querying BigQuery for builder %s' % builder
    query = QUERY_TEMPLATE.format(builder=builder, num_samples=args.num_samples)
    cmd = [
        'bq',
        'query',
        '--format=json',
        '--project_id=%s' % args.project,
        '--use_legacy_sql=false',
        query,
    ]
    with open(os.devnull, 'w') as devnull:
      stdout = subprocess.check_output(cmd, stderr=devnull)

    query_results = json.loads(stdout)
    assert len(query_results)
    for result in query_results:
      retval.setdefault(builder, []).append(result['id'])
  return retval


def ParseArgs():
  parser = argparse.ArgumentParser(
      description='Script to find tests which are unexpectedly passing, i.e. '
      'whose test suppressions can probably be removed/relaxed.')
  parser.add_argument('--project',
                      required=True,
                      help='A billing project to use for BigQuery queries.')
  parser.add_argument('--builder',
                      action='append',
                      dest='builders',
                      default=[],
                      help='A builder to query results from. Can be specified '
                      'multiple times to use multiple builders. If omitted, '
                      'will use a default set of builders.')
  parser.add_argument('--num-samples',
                      type=int,
                      default=100,
                      help='The number of recent builds to query.')

  args = parser.parse_args()
  assert args.num_samples > 0
  args.builders = args.builders or DEFAULT_BUILDERS
  return args


def main():
  args = ParseArgs()
  builds = GetBuildbucketIds(args)
  unexpected_passes = GetUnexpectedPasses(builds, args)
  PrintUnexpectedPasses(unexpected_passes, args)


if __name__ == '__main__':
  main()
