# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying the ResultDB BigQuery tables."""

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
from typ import json_results
from unexpected_passes_common import builders as builders_module
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import multiprocessing_utils

DEFAULT_NUM_SAMPLES = 100
MAX_ROWS = (2**31) - 1
MAX_QUERY_TRIES = 3
# Used to prevent us from triggering too many queries simultaneously and causing
# a bunch of rate limit errors. Anything below 1.5 seemed to result in enough
# rate limit errors to cause problems. Raising above that for safety.
QUERY_DELAY = 2
# The target number of results/rows per query when running in large query mode.
# Higher values = longer individual query times and higher chances of running
# out of memory in BigQuery. Lower values = more parallelization overhead and
# more issues with rate limit errors.
TARGET_RESULTS_PER_QUERY = 20000

# Subquery for getting all try builds that were used for CL submission. 30 days
# is chosen because the ResultDB tables we pull data from only keep data around
# for 30 days.
SUBMITTED_BUILDS_TEMPLATE = """\
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.{project_view}.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)"""

# pylint: disable=super-with-arguments,useless-object-inheritance


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

    assert self._num_samples > 0

  def FillExpectationMapForBuilders(self, expectation_map, builders):
    """Fills |expectation_map| with results from |builders|.

    Args:
      expectation_map: A data_types.TestExpectationMap. Will be modified
          in-place.
      builders: An iterable of data_types.BuilderEntry containing the builders
          to query.

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
    # Ensure that all the builders are of the same type since we make some
    # assumptions about that later on.
    assert builders
    builder_type = None
    for b in builders:
      if builder_type is None:
        builder_type = b.builder_type
      else:
        assert b.builder_type == builder_type

    # Filter out any builders that we can easily determine do not currently
    # produce data we care about.
    builders = self._FilterOutInactiveBuilders(builders, builder_type)

    # Spin up a separate process for each query/add step. This is wasteful in
    # the sense that we'll have a bunch of idle processes once faster steps
    # start finishing, but ensures that we start slow queries early and avoids
    # the overhead of passing large amounts of data between processes. See
    # crbug.com/1182459 for more information on performance considerations.
    process_pool = multiprocessing_utils.GetProcessPool(nodes=len(builders))

    args = [(b, expectation_map) for b in builders]

    results = process_pool.map(self._QueryAddCombined, args)

    tmp_expectation_map = data_types.TestExpectationMap()
    all_unmatched_results = {}

    for (unmatched_results, prefixed_builder_name, merge_map) in results:
      tmp_expectation_map.Merge(merge_map, expectation_map)
      if unmatched_results:
        all_unmatched_results[prefixed_builder_name] = unmatched_results

    expectation_map.clear()
    expectation_map.update(tmp_expectation_map)

    return all_unmatched_results

  def _FilterOutInactiveBuilders(self, builders, builder_type):
    """Filters out any builders that are not producing data.

    This helps save time on querying, as querying for the builder names is cheap
    while querying for individual results from a builder is expensive. Filtering
    out inactive builders lets us preemptively remove builders that we know we
    won't get any data from, and thus don't need to waste time querying.

    Args:
      builders: An iterable of data_types.BuilderEntry containing the builders
          to query.
      builder_type: A string containing the type of builder to query, either
          "ci" or "try".

    Returns:
      A copy of |builders| with any inactive builders removed.
    """
    include_internal_builders = any(b.is_internal_builder for b in builders)
    query = self._GetActiveBuilderQuery(
        builder_type, include_internal_builders).encode('utf-8')
    cmd = GenerateBigQueryCommand(self._project, {}, batch=False)
    with open(os.devnull, 'w') as devnull:
      p = subprocess.Popen(cmd,
                           stdout=subprocess.PIPE,
                           stderr=devnull,
                           stdin=subprocess.PIPE)
    stdout, _ = p.communicate(query)
    if not isinstance(stdout, six.string_types):
      stdout = stdout.decode('utf-8')
    results = json.loads(stdout)

    # We filter from an initial list instead of directly using the returned
    # builders since there are cases where they aren't equivalent, such as for
    # GPU tests if a particular builder doesn't run a particular suite. This
    # could be encapsulated in the query, but this would cause the query to take
    # longer. Since generating the initial list locally is basically
    # instantenous and we're optimizing for runtime, filtering is the better
    # option.
    active_builders = {r['builder_name'] for r in results}
    filtered_builders = [b for b in builders if b.name in active_builders]
    return filtered_builders

  def _QueryAddCombined(self, inputs):
    """Combines the query and add steps for use in a process pool.

    Args:
      inputs: An iterable of inputs for QueryBuilder() and
          data_types.TestExpectationMap.AddResultList(). Should be in the order:
          builder expectation_map

    Returns:
      The output of data_types.TestExpectationMap.AddResultList().
    """
    builder, expectation_map = inputs
    results, expectation_files = self.QueryBuilder(builder)

    prefixed_builder_name = '%s/%s:%s' % (builder.project, builder.builder_type,
                                          builder.name)
    unmatched_results = expectation_map.AddResultList(prefixed_builder_name,
                                                      results,
                                                      expectation_files)

    return unmatched_results, prefixed_builder_name, expectation_map

  def QueryBuilder(self, builder):
    """Queries ResultDB for results from |builder|.

    Args:
      builder: A data_types.BuilderEntry containing the builder to query.

    Returns:
      A tuple (results, expectation_files). |results| is the results returned by
      the query converted into a list of data_types.Result objects.
      |expectation_files| is a set of strings denoting which expectation files
      are relevant to |results|, or None if all should be used.
    """

    query_generator = self._GetQueryGeneratorForBuilder(builder)
    if not query_generator:
      # No affected tests on this builder, so early return.
      return [], None

    # Query for the test data from the builder, splitting the query if we run
    # into the BigQuery hard memory limit. Even if we keep failing, this will
    # eventually stop due to getting a QuerySplitError when we can't split the
    # query any further.
    query_results = None
    while query_results is None:
      try:
        query_results = self._RunBigQueryCommandsForJsonOutput(
            query_generator.GetQueries(), {
                '': {
                    'builder_name': builder.name
                },
                'INT64': {
                    'num_builds': self._num_samples
                }
            })
      except MemoryLimitError:
        logging.warning(
            'Query to builder %s hit BigQuery hard memory limit, trying again '
            'with more query splitting.', builder.name)
        query_generator.SplitQuery()

    results = []
    if not query_results:
      # Don't bother logging if we know this is a fake CI builder.
      if not (builder.builder_type == constants.BuilderTypes.CI
              and builder in builders_module.GetInstance().GetFakeCiBuilders()):
        logging.warning(
            'Did not get results for "%s", but this may be because its '
            'results do not apply to any expectations for this suite.',
            builder.name)
      return results, None

    # It's possible that a builder runs multiple versions of a test with
    # different expectation files for each version. So, find a result for each
    # unique step and get the expectation files from all of them.
    results_for_each_step = {}
    for qr in query_results:
      step_name = qr['step_name']
      if step_name not in results_for_each_step:
        results_for_each_step[step_name] = qr

    expectation_files = []
    for qr in results_for_each_step.values():
      # None is a special value indicating "use all expectation files", so
      # handle that.
      ef = self._GetRelevantExpectationFilesForQueryResult(qr)
      if ef is None:
        expectation_files = None
        break
      expectation_files.extend(ef)
    if expectation_files is not None:
      expectation_files = list(set(expectation_files))

    for r in query_results:
      if self._ShouldSkipOverResult(r):
        continue
      results.append(self._ConvertJsonResultToResultObject(r))
    logging.debug('Got %d results for %s builder %s', len(results),
                  builder.builder_type, builder.name)
    return results, expectation_files

  def _ConvertJsonResultToResultObject(self, json_result):
    """Converts a single BigQuery JSON result to a data_types.Result.

    Args:
      json_result: A single row/result from BigQuery in JSON format.

    Returns:
      A data_types.Result object containing the information from |json_result|.
    """
    build_id = _StripPrefixFromBuildId(json_result['id'])
    test_name = self._StripPrefixFromTestId(json_result['test_id'])
    actual_result = _ConvertActualResultToExpectationFileFormat(
        json_result['status'])
    tags = json_result['typ_tags']
    step = json_result['step_name']
    return data_types.Result(test_name, tags, actual_result, step, build_id)

  def _GetRelevantExpectationFilesForQueryResult(self, query_result):
    """Gets the relevant expectation file names for a given query result.

    Args:
      query_result: A dict containing single row/result from a BigQuery query.

    Returns:
      An iterable of strings containing expectation file names that are
      relevant to |query_result|, or None if all expectation files should be
      considered relevant.
    """
    raise NotImplementedError()

  def _ShouldSkipOverResult(self, result):
    """Whether |result| should be ignored and skipped over.

    Args:
      result: A dict containing a single BigQuery result row.

    Returns:
      True if the result should be skipped over/ignored, otherwise False.
    """
    del result
    return False

  def _GetQueryGeneratorForBuilder(self, builder):
    """Returns a _BaseQueryGenerator instance to only include relevant tests.

    Args:
      builder: A data_types.BuilderEntry containing the builder to query.

    Returns:
      None if the query returned no results. Otherwise, some instance of a
      _BaseQueryGenerator.
    """
    raise NotImplementedError()

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
      query = query.encode('utf-8')
      with open(os.devnull, 'w') as devnull:
        with processes_lock:
          # Starting many queries at once causes us to hit rate limits much more
          # frequently, so stagger query starts to help avoid that.
          time.sleep(QUERY_DELAY)
          p = subprocess.Popen(cmd,
                               stdout=subprocess.PIPE,
                               stderr=devnull,
                               stdin=subprocess.PIPE)
          processes.add(p)

        # We pass in the query via stdin instead of including it on the
        # commandline because we can run into command length issues in large
        # query mode.
        stdout, _ = p.communicate(query)
        if not isinstance(stdout, six.string_types):
          stdout = stdout.decode('utf-8')
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

    bq_cmd = GenerateBigQueryCommand(self._project, parameters)
    stdouts = run_cmd(bq_cmd, 0)
    combined_json = []
    for result in [json.loads(s) for s in stdouts]:
      for row in result:
        combined_json.append(row)
    return combined_json

  def _StripPrefixFromTestId(self, test_id):
    """Strips the prefix from a test ID, leaving only the test case name.

    Args:
      test_id: A string containing a full ResultDB test ID, e.g.
          ninja://target/directory.suite.class.test_case

    Returns:
      A string containing the test cases name extracted from |test_id|.
    """
    raise NotImplementedError()

  def _GetActiveBuilderQuery(self, builder_type, include_internal_builders):
    """Gets the SQL query for determining which builders actually produce data.

    Args:
      builder_type: A string containing the type of builders to query, either
          "ci" or "try".
      include_internal_builders: A boolean indicating whether internal builders
          should be included in the data that the query will access.

    Returns:
      A string containing a SQL query that will get all the names of all
      relevant builders that are active/producing data.
    """
    raise NotImplementedError()


class _BaseQueryGenerator(object):
  """Abstract base class for query generators."""

  def __init__(self, builder):
    self._builder = builder

  def SplitQuery(self):
    """Splits the query into more clauses/queries."""
    raise NotImplementedError('SplitQuery must be overridden in a child class')

  def GetClauses(self):
    """Gets string representations of the test filters.

    Returns:
      A list of strings, each string being a valid SQL clause that applies a
      portion of the test filter to a query.
    """
    raise NotImplementedError('GetClauses must be overridden in a child class')

  def GetQueries(self):
    """Gets string representations of the queries to run.

    Returns:
      A list of strings, each string being a valid SQL query that queries a
      portion of the tests of interest.
    """
    raise NotImplementedError('GetQueries must be overridden in a child class')


# pylint: disable=abstract-method
class FixedQueryGenerator(_BaseQueryGenerator):
  """Concrete test filter that cannot be split."""

  def __init__(self, builder, test_filter):
    """
    Args:
      test_filter: A string containing the test filter SQL clause to use.
    """
    super(FixedQueryGenerator, self).__init__(builder)
    self._test_filter = test_filter

  def SplitQuery(self):
    raise QuerySplitError('Tried to split a query without any test IDs to use, '
                          'use --large-query-mode')

  def GetClauses(self):
    return [self._test_filter]
# pylint: enable=abstract-method


# pylint: disable=abstract-method
class SplitQueryGenerator(_BaseQueryGenerator):
  """Concrete test filter that can be split to a desired size."""

  def __init__(self, builder, test_ids, target_num_samples):
    """
    Args:
      test_ids: A list of strings containing the test IDs to use in the test
          test filter.
      target_num_samples: The target/max number of samples to get from each
          query that uses clauses from this test filter.
    """
    super(SplitQueryGenerator, self).__init__(builder)
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

  def SplitQuery(self):
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
# pylint: enable=abstract-method


def GenerateBigQueryCommand(project, parameters, batch=True):
  """Generate a BigQuery commandline.

  Does not contain the actual query, as that is passed in via stdin.

  Args:
    project: A string containing the billing project to use for BigQuery.
    parameters: A dict specifying parameters to substitute in the query in
        the format {type: {key: value}}. For example, the dict:
        {'INT64': {'num_builds': 5}}
        would result in --parameter=num_builds:INT64:5 being passed to BigQuery.
    batch: Whether to run the query in batch mode or not. Batching adds some
        random amount of overhead since it means the query has to wait for idle
        resources, but also allows for much better parallelism.

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

  if batch:
    cmd.append('--batch')

  for parameter_type, parameter_pairs in parameters.items():
    for k, v in parameter_pairs.items():
      cmd.append('--parameter=%s:%s:%s' % (k, parameter_type, v))
  return cmd


def _StripPrefixFromBuildId(build_id):
  # Build IDs provided by ResultDB are prefixed with "build-"
  split_id = build_id.split('-')
  assert len(split_id) == 2
  return split_id[-1]


def _ConvertActualResultToExpectationFileFormat(actual_result):
  # Web tests use ResultDB's ABORT value for both test timeouts and device
  # failures, but Abort is not defined in typ. So, map it to timeout now.
  if actual_result == 'ABORT':
    actual_result = json_results.ResultType.Timeout
  # The result reported to ResultDB is in the format PASS/FAIL, while the
  # expected results in an expectation file are in the format Pass/Failure.
  return expectations_parser.RESULT_TAGS[actual_result]


class RateLimitError(Exception):
  """Exception raised when BigQuery hits a rate limit error."""


class MemoryLimitError(Exception):
  """Exception raised when BigQuery hits its hard memory limit."""


class QuerySplitError(Exception):
  """Exception raised when a query cannot be split any further."""
