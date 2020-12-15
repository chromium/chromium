# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying the ResultDB BigQuery tables."""

import json
import logging
import multiprocessing.pool
import os
import subprocess

from typ import expectations_parser
from unexpected_passes import builders as builders_module
from unexpected_passes import data_types

DEFAULT_NUM_SAMPLES = 100
MAX_ROWS = (2**31) - 1

# Largely written by nodir@ and modified by bsheedy@
# This query gets us all results for tests that have had results with a
# RetryOnFailure or Failure expectation in the past |@num_samples| builds on
# |@builder_name| for the test |suite| type we're looking at. Whether these are
# CI or try results depends on whether |builder_type| is "ci" or "try".
GPU_BQ_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      exported.id,
      ARRAY_AGG(STRUCT(
          exported.id,
          test_id,
          status,
          (
            SELECT value
            FROM tr.tags
            WHERE key = "step_name") as step_name,
          ARRAY(
            SELECT value
            FROM tr.tags
            WHERE key = "typ_tag") as typ_tags,
          ARRAY(
            SELECT value
            FROM tr.tags
            WHERE key = "raw_typ_expectation") as typ_expectations
      )) as test_results,
      FROM `luci-resultdb.chromium.gpu_{builder_type}_test_results` tr
      WHERE
        status != "SKIP"
        AND STRUCT("builder", @builder_name) IN UNNEST(variant)
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.{suite}\.")
      GROUP BY exported.id
      ORDER BY ANY_VALUE(partition_time) DESC
      LIMIT @num_builds
    ),
    tests AS (
      SELECT ARRAY_AGG(tr) test_results
      FROM builds b, b.test_results tr
      WHERE
        "RetryOnFailure" IN UNNEST(typ_expectations)
        OR "Failure" IN UNNEST(typ_expectations)
      GROUP BY test_id, step_name
    )
SELECT tr.*
FROM tests t, t.test_results tr
"""

# The suite reported to Telemetry for selecting which suite to run is not
# necessarily the same one that is reported to typ/ResultDB, so map any special
# cases here.
TELEMETRY_SUITE_TO_RDB_SUITE_EXCEPTION_MAP = {
    'info_collection': 'info_collection_test',
    'power': 'power_measurement_integration_test',
    'trace_test': 'trace_integration_test',
}


def FillExpectationMapForCiBuilders(expectation_map, builders, suite, project,
                                    num_samples):
  """Fills |expectation_map| for CI builders.

  See _FillExpectationMapForBuilders() for more information.
  """
  logging.info('Filling test expectation map with CI results')
  return _FillExpectationMapForBuilders(expectation_map, builders, 'ci', suite,
                                        project, num_samples)


def FillExpectationMapForTryBuilders(expectation_map, builders, suite, project,
                                     num_samples):
  """Fills |expectation_map| for try builders.

  See _FillExpectationMapForBuilders() for more information.
  """
  logging.info('Filling test expectation map with try results')
  return _FillExpectationMapForBuilders(expectation_map, builders, 'try', suite,
                                        project, num_samples)


def _FillExpectationMapForBuilders(expectation_map, builders, builder_type,
                                   suite, project, num_samples):
  """Fills |expectation_map| with results from |builders|.

  Args:
    expectation_map: A dict in the format returned by
        expectations.CreateTestExpectationMap(). Will be modified in-place.
    builders: A list of strings containing the names of builders to query.
    builder_type: A string containing the type of builder to query, either "ci"
        or "try".
    suite: A string containing the name of the suite that is being queried for.
    project: A string containing the billing project to use for BigQuery.
    num_samples: An integer containing the number of builds to pull results
        from.

  Returns:
    A dict containing any results that were retrieved that did not have a
    matching expectation in |expectation_map| in the following format:
    {
      |builder_type|:|builder_name| (str): [
        result1 (data_types.Result),
        result2 (data_types.Result),
        ...
      ],
    }
  """
  all_unmatched_results = {}
  # This is not officially documented, but comes up frequently when looking for
  # information on Python thread pools. It has the same API as
  # multiprocessing.Pool(), but uses threads instead of subprocesses. This
  # results in us being able to avoid jumping through some hoops with regards to
  # pickling that come with multiple processes. Since each thread is going to be
  # I/O bound on the query, there shouldn't be a noticeable performance loss
  # despite the default CPython interpreter not being truly multithreaded.
  pool = multiprocessing.pool.ThreadPool()
  curried_query = lambda b: QueryBuilder(b, builder_type, suite, project,
                                         num_samples)
  results_list = pool.map(curried_query, builders)
  for (builder_name, results) in results_list:
    prefixed_builder_name = '%s:%s' % (builder_type, builder_name)
    unmatched_results = _AddResultListToMap(expectation_map,
                                            prefixed_builder_name, results)
    if unmatched_results:
      all_unmatched_results[prefixed_builder_name] = unmatched_results
  return all_unmatched_results


def _AddResultListToMap(expectation_map, builder, results):
  """Adds |results| to |expectation_map|.

  Args:
    expectation_map: A dict in the format returned by
        expectations.CreateTestExpectationMap(). Will be modified in-place.
    builder: A string containing the builder |results| came from. Should be
        prefixed with something to distinguish between identically named CI and
        try builders.
    results: A list of data_types.Result objects corresponding to the ResultDB
        data queried for |builder|.

  Returns:
    A list of data_types.Result objects who did not have a matching expectation
    in |expectation_map|.
  """
  failure_results = set()
  pass_results = set()
  unmatched_results = []
  for r in results:
    if r.actual_result == 'Pass':
      pass_results.add(r)
    else:
      failure_results.add(r)

  # Remove any cases of failure -> pass from the passing set. If a test is
  # flaky, we get both pass and failure results for it, so we need to remove the
  # any cases of a pass result having a corresponding, earlier failure result.
  modified_failing_retry_results = set()
  for r in failure_results:
    modified_failing_retry_results.add(
        data_types.Result(r.test, r.tags, 'Pass', r.step, r.build_id))
  pass_results -= modified_failing_retry_results

  for r in pass_results | failure_results:
    found_matching = _AddResultToMap(r, builder, expectation_map)
    if not found_matching:
      unmatched_results.append(r)

  return unmatched_results


def _AddResultToMap(result, builder, expectation_map):
  """Adds a single |result| to |expectation_map|.

  Args:
    result: A data_types.Result object to add.
    builder: A string containing the name of the builder |result| came from.
    expectation_map: A dict in the format returned by
        expectations.CreateTestExpectationMap(). Will be modified in-place.

  Returns:
    True if an expectation in |expectation_map| applied to |result|, otherwise
    False.
  """
  found_matching_expectation = False
  # We need to use fnmatch since wildcards are supported, so there's no point in
  # checking the test name key right now. The AppliesToResult check already does
  # an fnmatch check.
  for expectations in expectation_map.itervalues():
    for e, builder_map in expectations.iteritems():
      if e.AppliesToResult(result):
        found_matching_expectation = True
        step_map = builder_map.setdefault(builder, {})
        stats = step_map.setdefault(result.step, data_types.BuildStats())
        if result.actual_result == 'Pass':
          stats.AddPassedBuild()
        else:
          stats.AddFailedBuild(result.build_id)
  return found_matching_expectation


def QueryBuilder(builder, builder_type, suite, project, num_samples):
  """Queries ResultDB for results from |builder|.

  Args:
    builder: A string containing the name of the builder to query.
    builder_type: A string containing the type of builder to query, either "ci"
        or "try".
    suite: A string containing the name of the suite that is being queried for.
    project: A string containing the billing project to use for BigQuery.
    num_samples: An integer containing the number of builds to pull results
        from.

  Returns:
    A tuple (builder, results). |builder| is simply the value of the input
    |builder| argument, returned to facilitate parallel execution. |results| is
    the results returned by the query converted into a list of data_types.Result
    objects.
  """
  num_samples = num_samples or DEFAULT_NUM_SAMPLES
  assert num_samples > 0

  # WebGL 1 and 2 tests are technically the same suite, but have different
  # expectation files. This leads to us getting both WebGL 1 and 2 results when
  # we only have expectations for one of them, which causes all the results from
  # the other to be reported as not having a matching expectation.
  # TODO(crbug.com/1140283): Remove this once WebGL expectations are merged
  # and there's no need to differentiate them.
  if 'webgl_conformance' in suite:
    webgl_version = suite[-1]
    suite = 'webgl_conformance'
    check_webgl_version =\
        lambda tags: 'webgl-version-%s' % webgl_version in tags
  else:
    check_webgl_version = lambda tags: True

  # Most test names are |suite|_integration_test, but there are several that
  # are not reported that way in typ, and by extension ResultDB, so adjust that
  # here.
  suite = TELEMETRY_SUITE_TO_RDB_SUITE_EXCEPTION_MAP.get(
      suite, suite + '_integration_test')

  query = GPU_BQ_QUERY_TEMPLATE.format(builder_type=builder_type, suite=suite)
  cmd = [
      'bq',
      'query',
      '--max_rows=%d' % MAX_ROWS,
      '--format=json',
      '--project_id=%s' % project,
      '--use_legacy_sql=false',
      '--parameter=builder_name::%s' % builder,
      '--parameter=num_builds:INT64:%d' % num_samples,
      query,
  ]
  with open(os.devnull, 'w') as devnull:
    try:
      stdout = subprocess.check_output(cmd, stderr=devnull)
    except subprocess.CalledProcessError as e:
      logging.error(e.output)
      raise

  query_results = json.loads(stdout)
  results = []
  if not query_results:
    # Don't bother logging if we know this is a fake CI builder.
    if not (builder_type == 'ci'
            and builder in builders_module.FAKE_CI_BUILDERS):
      logging.warning(
          'Did not get results for "%s", but this may be because its results '
          'do not apply to any expectations for this suite.', builder)
    return (builder, results)

  for r in query_results:
    if not check_webgl_version(r['typ_tags']):
      continue
    build_id = _StripPrefixFromBuildId(r['id'])
    test_name = _StripPrefixFromTestId(r['test_id'])
    actual_result = _ConvertActualResultToExpectationFileFormat(r['status'])
    tags = r['typ_tags']
    step = r['step_name']
    results.append(
        data_types.Result(test_name, tags, actual_result, step, build_id))
  logging.debug('Got %d results for %s builder %s', len(results), builder_type,
                builder)
  return (builder, results)


def _StripPrefixFromBuildId(build_id):
  # Build IDs provided by ResultDB are prefixed with "build-"
  split_id = build_id.split('-')
  assert len(split_id) == 2
  return split_id[-1]


def _StripPrefixFromTestId(test_id):
  # GPU test IDs provided by ResultDB are the test name as known by the test
  # runner prefixed by
  # "ninja://<target>/gpu_tests.<suite>_integration_test.<class>.", e.g.
  #     "ninja://chrome/test:telemetry_gpu_integration_test/
  #      gpu_tests.pixel_integration_test.PixelIntegrationTest."
  split_id = test_id.split('.', 3)
  assert len(split_id) == 4
  return split_id[-1]


def _ConvertActualResultToExpectationFileFormat(actual_result):
  # The result reported to ResultDB is in the format PASS/FAIL, while the
  # expected results in an expectation file are in the format Pass/Failure.
  return expectations_parser.RESULT_TAGS[actual_result]
