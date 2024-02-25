# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for working with BigQuery results."""

import collections
import datetime
import os
from collections import defaultdict
from typing import List, Tuple

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import data_types
from flake_suppressor_common import expectations
from flake_suppressor_common import tag_utils

from typ import expectations_parser


class ResultProcessor():
  def __init__(self, expectations_processor: expectations.ExpectationProcessor):
    self._expectations_processor = expectations_processor

  def AggregateResults(self,
                       results: ct.QueryJsonType) -> ct.AggregatedResultsType:
    """Aggregates BigQuery results.

    Also filters out any results that have already been suppressed.

    Args:
      results: Parsed JSON results from a BigQuery query.

    Returns:
      A map in the following format:
      {
        'test_suite': {
          'test_name': {
            'typ_tags_as_tuple': [ 'list', 'of', 'urls' ],
          },
        },
      }
    """
    results = self._ConvertJsonResultsToResultObjects(results)
    results = self._FilterOutSuppressedResults(results)
    aggregated_results = {}
    for r in results:
      build_url = 'http://ci.chromium.org/b/%s' % r.build_id

      build_url_list = aggregated_results.setdefault(r.suite, {}).setdefault(
          r.test, {}).setdefault(r.tags, [])
      build_url_list.append(build_url)
    return aggregated_results

  def AggregateTestStatusResults(
      self, results: ct.QueryJsonType) -> ct.AggregatedStatusResultsType:
    """Aggregates BigQuery results.

    Also filters out any results that have already been suppressed.

    Args:
      results: Parsed JSON results from a BigQuery query.

    Returns:
      A map in the following format:
      {
        'test_suite': {
          'test_name': {
            ('typ', 'tags', 'as', 'tuple'):
            [ (status, url, date, is_slow, typ_expectations),
              (status, url, date, is_slow, typ_expectations) ],
          },
        },
      }
    """
    results = self._ConvertJsonResultsToResultObjects(results)
    results = self._FilterOutSuppressedResults(results)
    aggregated_results = defaultdict(
        lambda: defaultdict(lambda: defaultdict(list)))
    for r in results:
      build_url = 'http://ci.chromium.org/b/%s' % r.build_id
      aggregated_results[r.suite][r.test][r.tags].append(
          ct.ResultTupleType(r.status, build_url, r.date, r.is_slow,
                             r.typ_expectations))
    return aggregated_results

  def _ConvertJsonResultsToResultObjects(self, results: ct.QueryJsonType
                                         ) -> List[data_types.Result]:
    """Converts JSON BigQuery results to data_types.Result objects.

    Args:
      results: Parsed JSON results from a BigQuery query

    Returns:
      The contents of |results| as a list of data_types.Result objects.
    """
    object_results = []
    for r in results:
      suite, test_name = self.GetTestSuiteAndNameFromResultDbName(r['name'])
      build_id = r['id'].split('-')[-1]
      typ_tags = tuple(tag_utils.TagUtils.RemoveIgnoredTags(r['typ_tags']))
      status = None
      date = None
      is_slow = None
      typ_expectations = None
      if 'status' in r:
        status = r['status']
      if 'date' in r:
        date = datetime.date.fromisoformat(r['date'])
      if 'is_slow' in r:
        is_slow = r['is_slow']
      if 'typ_expectations' in r:
        typ_expectations = r['typ_expectations']
      object_results.append(
          data_types.Result(suite, test_name, typ_tags, build_id, status, date,
                            is_slow, typ_expectations))
    return object_results

  def _FilterOutSuppressedResults(self, results: List[data_types.Result]
                                  ) -> List[data_types.Result]:
    """Filters out results that have already been suppressed in the repo.

    Args:
      results: A list of data_types.Result objects.

    Returns:
      |results| with any already-suppressed failures removed.
    """
    # Get all the expectations.
    origin_expectation_contents = (
        self._expectations_processor.GetLocalCheckoutExpectationFileContents())
    origin_expectations = collections.defaultdict(list)
    for filename, contents in origin_expectation_contents.items():
      list_parser = expectations_parser.TaggedTestListParser(contents)
      for e in list_parser.expectations:
        expectation = data_types.Expectation(e.test, e.tags, e.raw_results,
                                             e.reason)
        origin_expectations[filename].append(expectation)

    # Discard any results that already have a matching expectation.
    kept_results = []
    for r in results:
      expectation_filename = (
          self._expectations_processor.GetExpectationFileForSuite(
              r.suite, r.tags))
      expectation_filename = os.path.basename(expectation_filename)
      should_keep = True
      for e in origin_expectations[expectation_filename]:
        if e.AppliesToResult(r):
          should_keep = False
          break
      if should_keep:
        kept_results.append(r)

    return kept_results

  def GetTestSuiteAndNameFromResultDbName(self, result_db_name: str
                                          ) -> Tuple[str, str]:
    raise NotImplementedError
