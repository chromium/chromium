# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying the ResultDB BigQuery tables."""

import logging
import time
from typing import Collection, Dict, Generator, Iterable, List, Optional, Tuple

from google.cloud import bigquery
from google.cloud import bigquery_storage
import pandas

from typ import expectations_parser
from typ import json_results
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import expectations

DEFAULT_NUM_SAMPLES = 100

# Subquery for getting all try builds that were used for CL submission. 30 days
# is chosen because the ResultDB tables we pull data from only keep data around
# for 30 days.
PARTITIONED_SUBMITTED_BUILDS_TEMPLATE = """\
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

QueryResult = pandas.Series


class BigQueryQuerier:
  """Class to handle all BigQuery queries for a script invocation."""

  def __init__(self, suite: Optional[str], project: str, num_samples: int,
               keep_unmatched_results: bool):
    """
    Args:
      suite: A string containing the name of the suite that is being queried
          for. Can be None if there is no differentiation between different
          suites.
      project: A string containing the billing project to use for BigQuery.
      num_samples: An integer containing the number of builds to pull results
          from.
      keep_unmatched_results: Whether to store and return unmatched results
          for debugging purposes.
    """
    self._suite = suite
    self._project = project
    self._num_samples = num_samples or DEFAULT_NUM_SAMPLES
    self._keep_unmatched_results = keep_unmatched_results

    assert self._num_samples > 0

  def FillExpectationMapForBuilders(
      self, expectation_map: data_types.TestExpectationMap,
      builders: Collection[data_types.BuilderEntry]
  ) -> Dict[str, data_types.ResultListType]:
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
    start_time = time.time()
    logging.debug('Starting to fill expectation map for %d builders',
                  len(builders))
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

    internal_statuses = set()
    for b in builders:
      internal_statuses.add(b.is_internal_builder)

    matched_builders = set()
    all_unmatched_results = {}
    for internal in internal_statuses:
      for builder_name, results, expectation_files in (
          self.GetBuilderGroupedQueryResults(builder_type, internal)):
        matching_builder = None
        for b in builders:
          if b.name == builder_name and b.is_internal_builder == internal:
            matching_builder = b
            break

        if not matching_builder:
          logging.warning(
              'Did not find a matching builder for name %s and '
              'internal status %s. This is normal if the builder '
              'is no longer running tests (e.g. it was '
              'experimental).', builder_name, internal)
          continue

        if matching_builder in matched_builders:
          raise RuntimeError(
              f'Got query result batches matched to builder '
              f'{matching_builder} twice - this is indicative of a malformed '
              f'query returning results that are not sorted by builder')
        matched_builders.add(matching_builder)

        prefixed_builder_name = '%s/%s:%s' % (matching_builder.project,
                                              matching_builder.builder_type,
                                              matching_builder.name)
        unmatched_results = expectation_map.AddResultList(
            prefixed_builder_name, results, expectation_files)
        if self._keep_unmatched_results:
          if unmatched_results:
            all_unmatched_results[prefixed_builder_name] = unmatched_results
        else:
          logging.info('Dropping %d unmatched results', len(unmatched_results))

    logging.debug('Filling expectation map took %f', time.time() - start_time)
    return all_unmatched_results

  def GetBuilderGroupedQueryResults(
      self, builder_type: str, is_internal: bool
  ) -> Generator[Tuple[str, data_types.ResultListType, Optional[List[str]]],
                 None, None]:
    """Generates results for all relevant builders grouped by builder name.

    Args:
      builder_type: Whether the builders are CI or try builders.
      is_internal: Whether the builders are internal.

    Yields:
      A tuple (builder_name, results). |builder_name| is a string specifying the
      builder that |results| came from. |results| is a data_types.ResultListType
      containing all the results for |builder_name|.
    """
    if builder_type == constants.BuilderTypes.CI:
      if is_internal:
        query = self._GetInternalCiQuery()
      else:
        query = self._GetPublicCiQuery()
    elif builder_type == constants.BuilderTypes.TRY:
      if is_internal:
        query = self._GetInternalTryQuery()
      else:
        query = self._GetPublicTryQuery()
    else:
      raise RuntimeError(f'Unknown builder type {builder_type}')

    current_builder = None
    rows_for_builder = []
    for row in self._GetSeriesForQuery(query):
      if current_builder is None:
        current_builder = row.builder_name
      if row.builder_name != current_builder:
        results_for_builder, expectation_files = self._ProcessRowsForBuilder(
            rows_for_builder)
        # The processing should have cleared out all the stored rows.
        assert not rows_for_builder
        yield current_builder, results_for_builder, expectation_files
        current_builder = row.builder_name
      rows_for_builder.append(row)

    if current_builder is None:
      logging.warning(
          'Did not get any results for builder type %s and internal status %s. '
          'Depending on where tests are run and how frequently trybots are '
          'used for submission, this may be benign.', builder_type, is_internal)

    if current_builder is not None and rows_for_builder:
      results_for_builder, expectation_files = self._ProcessRowsForBuilder(
          rows_for_builder)
      assert not rows_for_builder
      yield current_builder, results_for_builder, expectation_files

  def _GetSeriesForQuery(self,
                         query: str) -> Generator[pandas.Series, None, None]:
    """Generates results for |query|.

    Args:
      query: A string containing the BigQuery query to run.

    Yields:
      A pandas.Series object for each row returned by the query. Columns can be
      accessed directly as attributes.
    """
    client = bigquery.Client(project=self._project)
    job = client.query(query)
    row_iterator = job.result()
    # Using a Dataframe iterator instead of directly using |row_iterator| allows
    # us to use the BigQuery Storage API, which results in ~10x faster query
    # result retrieval at the cost of a few more dependencies.
    dataframe_iterator = row_iterator.to_dataframe_iterable(
        bigquery_storage.BigQueryReadClient())
    for df in dataframe_iterator:
      for _, row in df.iterrows():
        yield row

  def _GetPublicCiQuery(self) -> str:
    """Returns the BigQuery query for public CI builder results."""
    raise NotImplementedError()

  def _GetInternalCiQuery(self) -> str:
    """Returns the BigQuery query for internal CI builder results."""
    raise NotImplementedError()

  def _GetPublicTryQuery(self) -> str:
    """Returns the BigQuery query for public try builder results."""
    raise NotImplementedError()

  def _GetInternalTryQuery(self) -> str:
    """Returns the BigQuery query for internal try builder results."""
    raise NotImplementedError()

  def _ProcessRowsForBuilder(
      self, rows: List[QueryResult]
  ) -> Tuple[data_types.ResultListType, Optional[List[str]]]:
    """Processes rows from a query into data_types.Result representations.

    Args:
      rows: A list of rows from a BigQuery query.

    Returns:
      A tuple (results, expectation_files). |results| is a list of
      data_types.Result objects. |expectation_files| is the list of expectation
      files that are used by the tests in |results|, but can be None to specify
      that all expectation files should be considered.
    """
    # It's possible that a builder runs multiple versions of a test with
    # different expectation files for each version. So, find a result for each
    # unique step and get the expectation files from all of them.
    results_for_each_step = {}
    for r in rows:
      step_name = r.step_name
      if step_name not in results_for_each_step:
        results_for_each_step[step_name] = r

    expectation_files = set()
    for r in results_for_each_step.values():
      # None is a special value indicating "use all expectation files", so
      # handle that.
      ef = self._GetRelevantExpectationFilesForQueryResult(r)
      if ef is None:
        expectation_files = None
        break
      expectation_files |= set(ef)
    if expectation_files is not None:
      expectation_files = list(expectation_files)

    # The query result list is potentially very large, so reduce the list as we
    # iterate over it instead of using a standard for/in so that we don't
    # temporarily end up with a ~2x increase in memory.
    results = []
    while rows:
      r = rows.pop()
      if self._ShouldSkipOverResult(r):
        continue
      results.append(self._ConvertBigQueryRowToResultObject(r))

    return results, expectation_files

  def _ConvertBigQueryRowToResultObject(self,
                                        row: QueryResult) -> data_types.Result:
    """Converts a single BigQuery result row to a data_types.Result.

    Args:
      row: A single row from BigQuery.

    Returns:
      A data_types.Result object containing the information from |row|.
    """
    build_id = _StripPrefixFromBuildId(row.id)
    test_name = self._StripPrefixFromTestId(row.test_id)
    actual_result = _ConvertActualResultToExpectationFileFormat(row.status)
    tags = expectations.GetInstance().FilterToKnownTags(row.typ_tags)
    step = row.step_name
    return data_types.Result(test_name, tags, actual_result, step, build_id)

  def _GetRelevantExpectationFilesForQueryResult(
      self, query_result: QueryResult) -> Optional[Iterable[str]]:
    """Gets the relevant expectation file names for a given query result.

    Args:
      query_result: An object representing a row/result from a query. Columns
          can be accessed via .column_name.

    Returns:
      An iterable of strings containing expectation file names that are
      relevant to |query_result|, or None if all expectation files should be
      considered relevant.
    """
    raise NotImplementedError()

  def _ShouldSkipOverResult(self, result: QueryResult) -> bool:
    """Whether |result| should be ignored and skipped over.

    Args:
      result: A dict containing a single BigQuery result row.

    Returns:
      True if the result should be skipped over/ignored, otherwise False.
    """
    del result
    return False

  def _StripPrefixFromTestId(self, test_id: str) -> str:
    """Strips the prefix from a test ID, leaving only the test case name.

    Args:
      test_id: A string containing a full ResultDB test ID, e.g.
          ninja://target/directory.suite.class.test_case

    Returns:
      A string containing the test cases name extracted from |test_id|.
    """
    raise NotImplementedError()


def _StripPrefixFromBuildId(build_id: str) -> str:
  # Build IDs provided by ResultDB are prefixed with "build-"
  split_id = build_id.split('-')
  assert len(split_id) == 2
  return split_id[-1]


def _ConvertActualResultToExpectationFileFormat(actual_result: str) -> str:
  # Web tests use ResultDB's ABORT value for both test timeouts and device
  # failures, but Abort is not defined in typ. So, map it to timeout now.
  if actual_result == 'ABORT':
    actual_result = json_results.ResultType.Timeout
  # The result reported to ResultDB is in the format PASS/FAIL, while the
  # expected results in an expectation file are in the format Pass/Failure.
  return expectations_parser.RESULT_TAGS[actual_result]
