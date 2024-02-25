# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for working with BigQuery results."""

from collections import defaultdict
from typing import List, Tuple

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import tag_utils
from blinkpy.web_tests.web_test_analyzers import data_types as dt


class ResultProcessor:
    def aggregate_results(
            self, results: ct.QueryJsonType) -> dt.AggregatedResultsType:
        """Aggregates BigQuery results for all image comparison tests.

        Args:
          results: Parsed JSON test results from a BigQuery query.

        Returns:
          A map in the following format:
          {
            'test_name': {
              'typ_tags_as_tuple': [ (0, 200, 'url_1'), (3, 400, 'url_2'),],
            },
          }
        """
        results = self._convert_json_results_to_result_objects(results)
        aggregated_results = defaultdict(lambda: defaultdict(list))
        for r in results:
            build_url = 'http://ci.chromium.org/b/%s' % r.build_id
            aggregated_results[r.test][r.typ_tags].append(
                dt.ImageDiffTagTupleType(r.image_diff_tag[0],
                                         r.image_diff_tag[1], build_url))
        return aggregated_results

    def _convert_json_results_to_result_objects(
            self, results: ct.QueryJsonType) -> List[dt.Result]:
        """Converts JSON BigQuery results to data_types.Result objects.

        Args:
          results: Parsed JSON results from a BigQuery query

        Returns:
          The contents of |results| as a list of data_types.Result objects.
        """
        object_results = []
        for r in results:
            build_id = r['id'].split('-')[-1]
            typ_tags = tuple(
                tag_utils.TagUtils.RemoveIgnoredTags(r['typ_tags']))
            image_diff_tag = (int(r['image_diff_max_difference']),
                              int(r['image_diff_total_pixels']))
            object_results.append(
                dt.Result(r['name'], typ_tags, image_diff_tag, build_id))
        return object_results

    def aggregate_test_slowness_results(
            self,
            results: ct.QueryJsonType) -> dt.AggregatedSlownessResultsType:
        """Aggregates BigQuery results for test slowness data.

        Args:
          results: Parsed JSON test results from a BigQuery query.

        Returns:
          A map in the following format:
          {
            'test_name': {
              'typ_tags_as_tuple': [ ('builder1', 5, 100, 2.1, 1),
                                     ('builder2', 10, 5, 5.1, 20), ]
            },
          }
        """
        aggregated_results = defaultdict(lambda: [])
        for r in results:
            avg_duration = float(r['avg_duration']) if r['avg_duration'] else 0
            slowness_data = dt.TestSlownessTupleType(r['builder'],
                                                     int(r['slow_count']),
                                                     int(r['non_slow_count']),
                                                     avg_duration,
                                                     int(r['timeout_count']),
                                                     float(r['timeout']))
            aggregated_results[r['test_name']].append(slowness_data)
        return aggregated_results
