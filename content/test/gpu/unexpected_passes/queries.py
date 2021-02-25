# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying the ResultDB BigQuery tables."""

import copy
import json
import logging
import multiprocessing.pool
import os
import subprocess
import threading
import time

from typ import expectations_parser
from unexpected_passes import builders as builders_module
from unexpected_passes import data_types
from unexpected_passes import multiprocessing_utils

DEFAULT_NUM_SAMPLES = 100
MAX_ROWS = (2**31) - 1
ASYNC_RESULT_SLEEP_DURATION = 5

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
        {test_filter_clause}
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

# Very similar to above, but used for getting the names of tests that are of
# interest for use as a filter.
TEST_FILTER_QUERY_TEMPLATE = """\
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
      LIMIT 50
    ),
    tests AS (
      SELECT ARRAY_AGG(tr) test_results
      FROM builds b, b.test_results tr
      WHERE
        "RetryOnFailure" IN UNNEST(typ_expectations)
        OR "Failure" IN UNNEST(typ_expectations)
      GROUP BY test_id, step_name
    )
SELECT DISTINCT tr.test_id
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
                                    num_samples, large_query_mode):
  """Fills |expectation_map| for CI builders.

  See _FillExpectationMapForBuilders() for more information.
  """
  logging.info('Filling test expectation map with CI results')
  return _FillExpectationMapForBuilders(expectation_map, builders, 'ci', suite,
                                        project, num_samples, large_query_mode)


def FillExpectationMapForTryBuilders(expectation_map, builders, suite, project,
                                     num_samples, large_query_mode):
  """Fills |expectation_map| for try builders.

  See _FillExpectationMapForBuilders() for more information.
  """
  logging.info('Filling test expectation map with try results')
  return _FillExpectationMapForBuilders(expectation_map, builders, 'try', suite,
                                        project, num_samples, large_query_mode)


def _FillExpectationMapForBuilders(expectation_map, builders, builder_type,
                                   suite, project, num_samples,
                                   large_query_mode):
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
    large_query_mode: A boolean indicating whether large query mode should be
        used. In this mode, an initial, smaller query is made and its results
        are used to perform additional filtering on a second, larger query in
        BigQuery. This works around hitting a hard memory limit when running the
        ORDER BY clause.

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

  # We use two separate pools since each is better for a different task. Adding
  # retrieved results to the expectation map is computationally intensive, so
  # properly parallelizing it results in large speedup. Python's default
  # interpreter does not support true multithreading, and the multiprocessing
  # module throws a fit when using custom data types due to pickling, so use
  # pathos' ProcessPool for this, which is like multiprocessing but handles all
  # the pickling automatically.
  #
  # However, ProcessPool appears to add a non-trivial amount of overhead when
  # passing data back and forth, so use a thread pool for triggering BigQuery
  # queries. Each query is already started in its own subprocess, so the lack
  # of multithreading is not an issue. multiprocessing.pool.ThreadPool() is not
  # officially documented, but comes up frequently when looking for information
  # on Python thread pools and is used in other places in the Chromium code
  # base.
  #
  # Using two pools also allows us to start processing data while queries are
  # still running since the latter spends most of its time waiting for the
  # query to complete.
  #
  # Since the ThreadPool is going to be idle most of the time, we can use many
  # more threads than we have logical cores.
  thread_count = 4 * multiprocessing.cpu_count()
  query_pool = multiprocessing.pool.ThreadPool(thread_count)
  result_pool = multiprocessing_utils.GetProcessPool()

  running_queries = set()
  running_adds = set()
  running_adds_lock = threading.Lock()

  def pass_query_result_to_add(result):
    bn, r = result
    arg = (expectation_map, builder_type, bn, r)
    running_adds_lock.acquire()
    running_adds.add(result_pool.apipe(_AddResultToMapMultiprocessing, arg))
    running_adds_lock.release()

  for b in builders:
    arg = (b, builder_type, suite, project, num_samples, large_query_mode)
    running_queries.add(
        query_pool.apply_async(QueryBuilder,
                               arg,
                               callback=pass_query_result_to_add))

  # We check the AsyncResult objects here because the provided callback only
  # gets called on success, and exceptions are not raised until the result is
  # retrieved. This can be removed whenever this is switched to Python 3, as
  # apply_async has an error_callback parameter there.
  while True:
    completed_queries = set()
    for rq in running_queries:
      if rq.ready():
        completed_queries.add(rq)
        rq.get()
    running_queries -= completed_queries
    if not len(running_queries):
      break
    time.sleep(ASYNC_RESULT_SLEEP_DURATION)

  # At this point, no more AsyncResults should be getting added to
  # |running_adds|, so we don't need to bother with the lock.
  add_results = []
  while True:
    completed_adds = set()
    for ra in running_adds:
      if ra.ready():
        completed_adds.add(ra)
        add_results.append(ra.get())
    running_adds -= completed_adds
    if not len(running_adds):
      break
    time.sleep(ASYNC_RESULT_SLEEP_DURATION)

  tmp_expectation_map = {}

  for (unmatched_results, prefixed_builder_name, merge_map) in add_results:
    _MergeExpectationMaps(tmp_expectation_map, merge_map, expectation_map)
    if unmatched_results:
      all_unmatched_results[prefixed_builder_name] = unmatched_results

  expectation_map.clear()
  expectation_map.update(tmp_expectation_map)

  return all_unmatched_results


def _MergeExpectationMaps(base_map, merge_map, reference_map=None):
  """Merges |merge_map| into |base_map|.

  Args:
    base_map: A dict to be updated with the contents of |merge_map|. Will be
        modified in place.
    merge_map: A dict in the format returned by
        expectations.CreateTestExpectationMap() whose contents will be merged
        into |base_map|.
    reference_map: A dict containing the information that was originally in
        |base_map|. Used for ensuring that a single expectation/builder/step
        combination is only ever updated once. If None, a copy of |base_map|
        will be used.
  """
  # We should only ever encounter a single updated BuildStats for an
  # expectation/builder/step combination. Use the reference map to determine
  # if a particular BuildStats has already been updated or not.
  reference_map = reference_map or copy.deepcopy(base_map)
  for key, value in merge_map.iteritems():
    if key not in base_map:
      base_map[key] = value
    else:
      if isinstance(value, dict):
        _MergeExpectationMaps(base_map[key], value, reference_map.get(key, {}))
      else:
        assert isinstance(value, data_types.BuildStats)
        # Ensure we haven't updated this BuildStats already. If the reference
        # map doesn't have a corresponding BuildStats, then base_map shouldn't
        # have initially either, and thus it would have been added before
        # reaching this point. Otherwise, the two values must match, meaning
        # that base_map's BuildStats hasn't been updated yet.
        reference_stats = reference_map.get(key, None)
        assert reference_stats is not None
        assert reference_stats == base_map[key]
        base_map[key] = value


def _AddResultToMapMultiprocessing(inputs):
  expectation_map, builder_type, builder_name, results = inputs
  prefixed_builder_name = '%s:%s' % (builder_type, builder_name)
  unmatched_results = _AddResultListToMap(expectation_map,
                                          prefixed_builder_name, results)
  return unmatched_results, prefixed_builder_name, expectation_map


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


def QueryBuilder(builder, builder_type, suite, project, num_samples,
                 large_query_mode):
  """Queries ResultDB for results from |builder|.

  Args:
    builder: A string containing the name of the builder to query.
    builder_type: A string containing the type of builder to query, either "ci"
        or "try".
    suite: A string containing the name of the suite that is being queried for.
    project: A string containing the billing project to use for BigQuery.
    num_samples: An integer containing the number of builds to pull results
        from.
    large_query_mode: A boolean indicating whether large query mode should be
        used. In this mode, an initial, smaller query is made and its results
        are used to perform additional filtering on a second, larger query in
        BigQuery. This works around hitting a hard memory limit when running the
        ORDER BY clause.

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

  test_filter_clause = _GetTestFilterClauseForBuilder(builder, builder_type,
                                                      suite, project,
                                                      large_query_mode)
  if test_filter_clause is None:
    # No affected tests on this builder, so early return.
    return (builder, [])

  query = GPU_BQ_QUERY_TEMPLATE.format(builder_type=builder_type,
                                       test_filter_clause=test_filter_clause,
                                       suite=suite)

  query_results = _RunBigQueryCommandForJsonOutput(query, project, {
      '': {
          'builder_name': builder
      },
      'INT64': {
          'num_builds': num_samples
      }
  })
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


def _GetTestFilterClauseForBuilder(builder, builder_type, suite, project,
                                   large_query_mode):
  """Returns a SQL clause to only include relevant tests.

  Args:
    builder: A string containing the name of the builder to query.
    builder_type: A string containing the type of builder to query, either "ci"
        or "try".
    suite: A string containing the name of the suite that is being queried for.
    project: A string containing the billing project to use for BigQuery.
    large_query_mode: A boolean indicating whether large query mode should be
        used. In this mode, an initial, smaller query is made and its results
        are used to perform additional filtering on a second, larger query in
        BigQuery. This works around hitting a hard memory limit when running the
        ORDER BY clause.

  Returns:
    None if |large_query_mode| is True and no tests are relevant to the
    specified |builder|. Otherwise, a string containing a valid SQL clause.
  """
  if not large_query_mode:
    # Look for all tests that match the given suite.
    return """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.%s\.")""" % suite

  query = TEST_FILTER_QUERY_TEMPLATE.format(builder_type=builder_type,
                                            suite=suite)
  query_results = _RunBigQueryCommandForJsonOutput(
      query, project, {'': {
          'builder_name': builder
      }})
  test_ids = ['"%s"' % r['test_id'] for r in query_results]
  if not test_ids:
    return None
  # Only consider specific test cases that were found to have active
  # expectations in the above query.
  test_filter_clause = 'AND test_id IN UNNEST([%s])' % ', '.join(test_ids)
  return test_filter_clause


def _RunBigQueryCommandForJsonOutput(query, project, parameters):
  """Runs the given BigQuery query and returns its output as JSON.

  Args:
    query: A string containing a valid BigQuery query to run.
    project: A string containing the billing project to use for BigQuery.
    parameters: A dict specifying parameters to substitute in the query in
        the format {type: {key: value}}. For example, the dict:
        {'INT64': {'num_builds': 5}}
        would result in --parameter=num_builds:INT64:5 being passed to BigQuery.

  Returns:
    The result of |query| as JSON.
  """
  cmd = _GenerateBigQueryCommand(project, parameters)
  with open(os.devnull, 'w') as devnull:
    p = subprocess.Popen(cmd,
                         stdout=subprocess.PIPE,
                         stderr=devnull,
                         stdin=subprocess.PIPE)
    # We pass in the query via stdin instead of including it on the commandline
    # because we can run into command length issues in large query mode.
    stdout, _ = p.communicate(query)
    if p.returncode:
      error_msg = 'Error running command %s. stdout: %s' % (cmd, stdout)
      if 'memory' in stdout:
        error_msg += ('\nIt looks like BigQuery may have run out of memory. '
                      'Try lowering --num-samples or using --large-query-mode.')
      raise RuntimeError(error_msg)
  return json.loads(stdout)


def _GenerateBigQueryCommand(project, parameters):
  """Generate a BigQuery commandline.

  Does not contain the actual query, as that is passed in via stdin.

  Args:
    project: A string containing the billing project to use for BigQuery.
    parameters: A dict specifying parameters to substitute in the query in
        the format {type: {key: value}}. For example, the dict:
        {'INT64': {'num_builds': 5}}
        would result in --parameter=num_builds:INT64:5 being passed to BigQuery.

  Returns:
    A list containing the BigQuery commandline, suitable to be passed to a
    method from the subprocess module.
  """
  cmd = [
      'bq',
      'query',
      '--max_rows=%d' % MAX_ROWS,
      '--format=json',
      '--project_id=%s' % project,
      '--use_legacy_sql=false',
  ]

  for parameter_type, parameter_pairs in parameters.iteritems():
    for k, v in parameter_pairs.iteritems():
      cmd.append('--parameter=%s:%s:%s' % (k, parameter_type, v))
  return cmd


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
