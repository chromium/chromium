# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for working with BigQuery results."""

import collections
import os

from flake_suppressor import data_types
from flake_suppressor import expectations
from flake_suppressor import tag_utils

from typ import expectations_parser


def AggregateResults(results):
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
  results = _ConvertJsonResultsToResultObjects(results)
  results = _FilterOutSuppressedResults(results)
  aggregated_results = {}
  for r in results:
    build_url = 'http://ci.chromium.org/b/%s' % r.build_id

    build_url_list = aggregated_results.setdefault(r.suite, {}).setdefault(
        r.test, {}).setdefault(r.tags, [])
    build_url_list.append(build_url)
  return aggregated_results


def _ConvertJsonResultsToResultObjects(results):
  """Converts JSON BigQuery results to data_types.Result objects.

  Args:
    results: Parsed JSON results from a BigQuery query

  Returns:
    The contents of |results| as a list of data_types.Result objects.
  """
  object_results = []
  for r in results:
    suite, test_name = GetTestSuiteAndNameFromResultDbName(r['name'])
    build_id = r['id'].split('-')[-1]
    typ_tags = tuple(tag_utils.RemoveMostIgnoredTags(r['typ_tags']))
    object_results.append(
        data_types.Result(suite, test_name, typ_tags, build_id))
  return object_results


def _FilterOutSuppressedResults(results):
  """Filters out results that have already been suppressed in the repo.

  Args:
    results: A list of data_types.Result objects.

  Returns:
    |results| with any already-suppressed failures removed.
  """
  # Get all the expectations.
  origin_expectation_contents = (
      expectations.GetExpectationFilesFromLocalCheckout())
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
    expectation_filename = expectations.GetExpectationFileForSuite(
        r.suite, r.tags)
    expectation_filename = os.path.basename(expectation_filename)
    should_keep = True
    for e in origin_expectations[expectation_filename]:
      if e.AppliesToResult(r):
        should_keep = False
        break
    if should_keep:
      kept_results.append(r)

  return kept_results


def GetTestSuiteAndNameFromResultDbName(result_db_name):
  _, suite, __, test_name = result_db_name.split('.', 3)
  return suite, test_name
