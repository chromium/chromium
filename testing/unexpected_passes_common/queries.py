# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying the ResultDB BigQuery tables."""

from __future__ import print_function

import json
import logging
import math
import multiprocessing.pool
import os
import subprocess
import threading
import time

import six

from typ import expectations_parser
from unexpected_passes_common import builders as builders_module
from unexpected_passes_common import data_types
from unexpected_passes_common import expectations
from unexpected_passes_common import multiprocessing_utils

DEFAULT_NUM_SAMPLES = 100
MAX_ROWS = (2**31) - 1
MAX_QUERY_TRIES = 3
# The target number of results/rows per query. Higher values = longer individual
# query times and higher chances to run out of memory in BigQuery. Lower
# values = more parallelization overhead and more issues with rate limit errors.
TARGET_RESULTS_PER_QUERY = 20000
# Used to prevent us from triggering too many queries simultaneously and causing
# a bunch of rate limit errors. Anything below 1.5 seemed to result in enough
# rate limit errors to cause problems. Raising above that for safety.
QUERY_DELAY = 2

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
        AND exported.realm = "chromium:{builder_type}"
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
            WHERE key = "typ_tag") as typ_tags,
          ARRAY(
            SELECT value
            FROM tr.tags
            WHERE key = "raw_typ_expectation") as typ_expectations
      )) as test_results,
      FROM `luci-resultdb.chromium.gpu_{builder_type}_test_results` tr
      WHERE
        status != "SKIP"
        AND exported.realm = "chromium:{builder_type}"
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
        {suite_filter_clause}
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


class BigQueryQuerier(object):
  """Class to handle all BigQuery queries for a script invocation."""

  def __init__(self, suite, project, num_samples, large_query_mode):
    """
    Args:
      suite: A string containing the name of the suite that is being queried
          for.
      project: A string containing the billing project to use for BigQuery.
      num_samples: An integer containing the number of builds to pull results
          from.
      large_query_mode: A boolean indicating whether large query mode should be
          used. In this mode, an initial, smaller query is made and its results
          are used to perform additional filtering on a second, larger query in
          BigQuery. This works around hitting a hard memory limit when running
          the ORDER BY clause.
    """
    self._suite = suite
    self._project = project
    self._num_samples = num_samples or DEFAULT_NUM_SAMPLES
    self._large_query_mode = large_query_mode
    self._check_webgl_version = None
    self._webgl_version_tag = None

    assert self._num_samples > 0

    # WebGL 1 and 2 tests are technically the same suite, but have different
    # expectation files. This leads to us getting both WebGL 1 and 2 results
    # when we only have expectations for one of them, which causes all the
    # results from the other to be reported as not having a matching
    # expectation.
    # TODO(crbug.com/1140283): Remove this once WebGL expectations are merged
    # and there's no need to differentiate them.
    if 'webgl_conformance' in self._suite:
      webgl_version = self._suite[-1]
      self._suite = 'webgl_conformance'
      self._webgl_version_tag = 'webgl-version-%s' % webgl_version
      self._check_webgl_version =\
          lambda tags: self._webgl_version_tag in tags
    else:
      self._check_webgl_version = lambda tags: True

    # Most test names are |suite|_integration_test, but there are several that
    # are not reported that way in typ, and by extension ResultDB, so adjust
    # that here.
    self._suite = TELEMETRY_SUITE_TO_RDB_SUITE_EXCEPTION_MAP.get(
        self._suite, self._suite + '_integration_test')

  def FillExpectationMapForCiBuilders(self, expectation_map, builders):
    """Fills |expectation_map| for CI builders.

    See _FillExpectationMapForBuilders() for more information.
    """
    logging.info('Filling test expectation map with CI results')
    return self._FillExpectationMapForBuilders(expectation_map, builders, 'ci')

  def FillExpectationMapForTryBuilders(self, expectation_map, builders):
    """Fills |expectation_map| for try builders.

    See _FillExpectationMapForBuilders() for more information.
    """
    logging.info('Filling test expectation map with try results')
    return self._FillExpectationMapForBuilders(expectation_map, builders, 'try')

  def _FillExpectationMapForBuilders(self, expectation_map, builders,
                                     builder_type):
    """Fills |expectation_map| with results from |builders|.

    Args:
      expectation_map: A data_types.TestExpectationMap. Will be modified
          in-place.
      builders: A list of strings containing the names of builders to query.
      builder_type: A string containing the type of builder to query, either
          "ci" or "try".

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
    assert isinstance(expectation_map, data_types.TestExpectationMap)

    # Spin up a separate process for each query/add step. This is wasteful in
    # the sense that we'll have a bunch of idle processes once faster steps
    # start finishing, but ensures that we start slow queries early and avoids
    # the overhead of passing large amounts of data between processes. See
    # crbug.com/1182459 for more information on performance considerations.
    process_pool = multiprocessing_utils.GetProcessPool(nodes=len(builders))

    args = [(b, builder_type, expectation_map) for b in builders]

    results = process_pool.map(self._QueryAddCombined, args)

    tmp_expectation_map = data_types.TestExpectationMap()
    all_unmatched_results = {}

    for (unmatched_results, prefixed_builder_name, merge_map) in results:
      expectations.MergeExpectationMaps(tmp_expectation_map, merge_map,
                                        expectation_map)
      if unmatched_results:
        all_unmatched_results[prefixed_builder_name] = unmatched_results

    expectation_map.clear()
    expectation_map.update(tmp_expectation_map)

    return all_unmatched_results

  def _QueryAddCombined(self, inputs):
    """Combines the query and add steps for use in a process pool.

    Args:
      inputs: An iterable of inputs for QueryBuilder() and
          expectations.AddResultListToMap(). Should be in the order:
          builder builder_type expectation_map

    Returns:
      The output of expectations.AddResultListToMap().
    """
    builder, builder_type, expectation_map = inputs
    results = self.QueryBuilder(builder, builder_type)

    prefixed_builder_name = '%s:%s' % (builder_type, builder)
    unmatched_results = expectations.AddResultListToMap(expectation_map,
                                                        prefixed_builder_name,
                                                        results)

    return unmatched_results, prefixed_builder_name, expectation_map

  def QueryBuilder(self, builder, builder_type):
    """Queries ResultDB for results from |builder|.

    Args:
      builder: A string containing the name of the builder to query.
      builder_type: A string containing the type of builder to query, either
          "ci" or "try".

    Returns:
      The results returned by the query converted into a list of
      data_types.Resultobjects.
    """

    test_filter = self._GetTestFilterForBuilder(builder, builder_type)
    if not test_filter:
      # No affected tests on this builder, so early return.
      return []

    # Query for the test data from the builder, splitting the query if we run
    # into the BigQuery hard memory limit. Even if we keep failing, this will
    # eventually stop due to getting a QuerySplitError when we can't split the
    # query any further.
    query_results = None
    while query_results is None:
      try:
        queries = []
        for tfc in test_filter.GetClauses():
          query = GPU_BQ_QUERY_TEMPLATE.format(builder_type=builder_type,
                                               test_filter_clause=tfc,
                                               suite=self._suite)
          queries.append(query)

        query_results = self._RunBigQueryCommandsForJsonOutput(
            queries, {
                '': {
                    'builder_name': builder
                },
                'INT64': {
                    'num_builds': self._num_samples
                }
            })
      except MemoryLimitError:
        logging.warning(
            'Query to builder %s hit BigQuery hard memory limit, trying again '
            'with more query splitting.', builder)
        test_filter.SplitFilter()

    results = []
    if not query_results:
      # Don't bother logging if we know this is a fake CI builder.
      if not (builder_type == 'ci'
              and builder in builders_module.FAKE_CI_BUILDERS):
        logging.warning(
            'Did not get results for "%s", but this may be because its '
            'results do not apply to any expectations for this suite.', builder)
      return results

    for r in query_results:
      if not self._check_webgl_version(r['typ_tags']):
        continue
      build_id = _StripPrefixFromBuildId(r['id'])
      test_name = _StripPrefixFromTestId(r['test_id'])
      actual_result = _ConvertActualResultToExpectationFileFormat(r['status'])
      tags = r['typ_tags']
      step = r['step_name']
      results.append(
          data_types.Result(test_name, tags, actual_result, step, build_id))
    logging.debug('Got %d results for %s builder %s', len(results),
                  builder_type, builder)
    return results

  def _GetTestFilterForBuilder(self, builder, builder_type):
    """Returns a _BaseQueryTestFilter instance to only include relevant tests.

    Args:
      builder: A string containing the name of the builder to query.
      builder_type: A string containing the type of builder to query, either
          "ci" or "try".

    Returns:
      None if the query returned no results. Otherwise, some instance of a
      _BaseQueryTestFilter.
    """

    if not self._large_query_mode:
      # Look for all tests that match the given suite.
      return _FixedQueryTestFilter("""\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.%s\.")""" % self._suite)

    query = TEST_FILTER_QUERY_TEMPLATE.format(
        builder_type=builder_type,
        suite=self._suite,
        suite_filter_clause=self._GetSuiteFilterClause())
    query_results = self._RunBigQueryCommandsForJsonOutput(
        query, {'': {
            'builder_name': builder
        }})
    test_ids = ['"%s"' % r['test_id'] for r in query_results]

    if not test_ids:
      return None

    # Only consider specific test cases that were found to have active
    # expectations in the above query. Also perform any initial query splitting.
    target_num_ids = TARGET_RESULTS_PER_QUERY / self._num_samples
    return _SplitQueryTestFilter(test_ids, target_num_ids)

  def _GetSuiteFilterClause(self):
    """Returns a SQL clause to only include relevant suites.

    Meant for cases where suites are differentiated by typ tag rather than
    reported suite name, e.g. WebGL 1 vs. 2 conformance.

    Returns:
      A string containing a valid SQL clause. Will be an empty string if no
      filtering is possible/necessary.
    """
    if not self._webgl_version_tag:
      return ''

    return 'AND "%s" IN UNNEST(typ_tags)' % self._webgl_version_tag

  def _RunBigQueryCommandsForJsonOutput(self, queries, parameters):
    """Runs the given BigQuery queries and returns their outputs as JSON.

    Args:
      queries: A list of strings containing valid BigQuery queries to run or a
          single string containing a query.
      parameters: A dict specifying parameters to substitute in the query in
          the format {type: {key: value}}. For example, the dict:
          {'INT64': {'num_builds': 5}}
          would result in --parameter=num_builds:INT64:5 being passed to
          BigQuery.

    Returns:
      The combined results of |queries| in JSON.
    """
    if isinstance(queries, str):
      queries = [queries]
    assert isinstance(queries, list)

    processes = set()
    processes_lock = threading.Lock()

    def run_cmd_in_thread(inputs):
      cmd, query = inputs
      with open(os.devnull, 'w') as devnull:
        processes_lock.acquire()
        # Starting many queries at once causes us to hit rate limits much more
        # frequently, so stagger query starts to help avoid that.
        time.sleep(QUERY_DELAY)
        p = subprocess.Popen(cmd,
                             stdout=subprocess.PIPE,
                             stderr=devnull,
                             stdin=subprocess.PIPE)
        processes.add(p)
        processes_lock.release()

        # We pass in the query via stdin instead of including it on the
        # commandline because we can run into command length issues in large
        # query mode.
        stdout, _ = p.communicate(query)
        if p.returncode:
          # When running many queries in parallel, it's possible to hit the
          # rate limit for the account if we're unlucky, so try again if we do.
          if 'Exceeded rate limits' in stdout:
            raise RateLimitError()
          error_msg = 'Error running command %s. stdout: %s' % (cmd, stdout)
          if 'memory' in stdout:
            raise MemoryLimitError(error_msg)
          raise RuntimeError(error_msg)
        return stdout

    def run_cmd(cmd, tries):
      if tries >= MAX_QUERY_TRIES:
        raise RuntimeError('Query failed too many times, aborting')

      # We use a thread pool with a thread for each query/process instead of
      # just creating the processes due to guidance from the Python docs:
      # https://docs.python.org/3/library/subprocess.html#subprocess.Popen.stderr
      # We need to write to stdin to pass the query in, but using
      # stdout/stderr/stdin directly is discouraged due to the potential for
      # deadlocks. The suggested method (using .communicate()) blocks, so we
      # need the thread pool to maintain parallelism.
      pool = multiprocessing.pool.ThreadPool(len(queries))

      def cleanup():
        pool.terminate()
        for p in processes:
          try:
            p.terminate()
          except OSError:
            # We can fail to terminate if the process is already finished, so
            # ignore such failures.
            pass
        processes.clear()

      args = [(cmd, q) for q in queries]
      try:
        return pool.map(run_cmd_in_thread, args)
      except RateLimitError:
        logging.warning('Query hit rate limit, retrying')
        cleanup()
        return run_cmd(cmd, tries + 1)
      finally:
        cleanup()

    bq_cmd = _GenerateBigQueryCommand(self._project, parameters)
    stdouts = run_cmd(bq_cmd, 0)
    combined_json = []
    for result in [json.loads(s) for s in stdouts]:
      for row in result:
        combined_json.append(row)
    return combined_json


class _BaseQueryTestFilter(object):
  """Abstract base class for test filters."""

  def SplitFilter(self):
    """Splits the test filter into more clauses/queries."""
    raise NotImplementedError('SplitFilter must be overridden in a child class')

  def GetClauses(self):
    """Gets string representations of the test filters.

    Returns:
      A list of strings, each string being a valid SQL clause that applies a
      portion of the test filter to a query.
    """
    raise NotImplementedError('GetClauses must be overridden in a child class')


class _FixedQueryTestFilter(_BaseQueryTestFilter):
  """Concrete test filter that cannot be split."""

  def __init__(self, test_filter):
    """
    Args:
      test_filter: A string containing the test filter SQL clause to use.
    """
    self._test_filter = test_filter

  def SplitFilter(self):
    raise QuerySplitError('Tried to split a query without any test IDs to use, '
                          'use --large-query-mode')

  def GetClauses(self):
    return [self._test_filter]


class _SplitQueryTestFilter(_BaseQueryTestFilter):
  """Concrete test filter that can be split to a desired size."""

  def __init__(self, test_ids, target_num_samples):
    """
    Args:
      test_ids: A list of strings containing the test IDs to use in the test
          test filter.
      target_num_samples: The target/max number of samples to get from each
          query that uses clauses from this test filter.
    """
    self._test_id_lists = []
    self._target_num_samples = target_num_samples
    self._clauses = []
    self._PerformInitialSplit(test_ids)

  def _PerformInitialSplit(self, test_ids):
    """Evenly splits |test_ids| into lists that are  ~|_target_num_samples| long

    Only to be called from the constructor.

    Args:
      test_ids: A list of test IDs to split and assign to the _test_id_lists
          member.
    """
    assert isinstance(test_ids[0], six.string_types)

    num_lists = int(math.ceil(float(len(test_ids)) / self._target_num_samples))
    list_size = int(math.ceil(float(len(test_ids)) / num_lists))

    split_lists = []
    start = 0
    for _ in range(num_lists):
      end = min(len(test_ids), start + list_size)
      split_lists.append(test_ids[start:end])
      start = end
    self._test_id_lists = split_lists
    self._GenerateClauses()

  def _GenerateClauses(self):
    test_filter_clauses = []
    for id_list in self._test_id_lists:
      clause = 'AND test_id IN UNNEST([%s])' % ', '.join(id_list)
      test_filter_clauses.append(clause)
    self._clauses = test_filter_clauses

  def SplitFilter(self):
    def _SplitListInHalf(l):
      assert len(l) > 1
      front = l[:len(l) // 2]
      back = l[len(l) // 2:]
      return front, back

    tmp_test_id_lists = []
    for til in self._test_id_lists:
      if len(til) <= 1:
        raise QuerySplitError(
            'Cannot split query any further, try lowering --num-samples')
      front, back = _SplitListInHalf(til)
      tmp_test_id_lists.append(front)
      tmp_test_id_lists.append(back)
    self._test_id_lists = tmp_test_id_lists
    self._GenerateClauses()

  def GetClauses(self):
    return self._clauses


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

  for parameter_type, parameter_pairs in parameters.items():
    for k, v in parameter_pairs.items():
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


class RateLimitError(Exception):
  """Exception raised when BigQuery hits a rate limit error."""
  pass


class MemoryLimitError(Exception):
  """Exception raised when BigQuery hits its hard memory limit."""
  pass


class QuerySplitError(Exception):
  """Exception raised when a query cannot be split any further."""
  pass
