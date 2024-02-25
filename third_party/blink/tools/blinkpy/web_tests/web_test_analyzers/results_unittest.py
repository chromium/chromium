# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.web_test_analyzers import data_types
from blinkpy.web_tests.web_test_analyzers import results
from flake_suppressor_common import tag_utils as common_tag_utils
from flake_suppressor_common import unittest_utils as uu


class BaseResultsUnittest(unittest.TestCase):
    def setUp(self) -> None:
        common_tag_utils.SetTagUtilsImplementation(uu.UnitTestTagUtils)
        self._result_processor = results.ResultProcessor()


class AggregateResultsUnittest(BaseResultsUnittest):
    def testBasic(self) -> None:
        """Basic functionality test."""
        query_results = [
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-1111',
                'typ_tags': ['win'],
                'image_diff_max_difference': '10',
                'image_diff_total_pixels': '40',
            },
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-2222',
                'typ_tags': ['win'],
                'image_diff_max_difference': '15',
                'image_diff_total_pixels': '42',
            },
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-3333',
                'typ_tags': ['linux'],
                'image_diff_max_difference': '10',
                'image_diff_total_pixels': '40',
            },
            {
                'name': ('conformance/textures/misc/texture-npot-video.html'),
                'id': 'build-4444',
                'typ_tags': ['win'],
                'image_diff_max_difference': '10',
                'image_diff_total_pixels': '40',
            },
        ]
        expected_output = {
            'conformance/textures/misc/video-rotation.html': {
                tuple(['win']): [(10, 40, 'http://ci.chromium.org/b/1111'),
                                 (15, 42, 'http://ci.chromium.org/b/2222')],
                tuple(['linux']): [(10, 40, 'http://ci.chromium.org/b/3333')],
            },
            'conformance/textures/misc/texture-npot-video.html': {
                tuple(['win']): [(10, 40, 'http://ci.chromium.org/b/4444')],
            },
        }
        self.assertEqual(
            self._result_processor.aggregate_results(query_results),
            expected_output)


class ConvertJsonResultsToResultObjectsUnittest(BaseResultsUnittest):
    def testBasic(self) -> None:
        """Basic functionality test."""
        r = [
            {
                'name': 'conformance/textures/misc/video-rotation.html',
                'id': 'build-1111',
                'typ_tags': ['win', 'x86'],
                'image_diff_max_difference': '10',
                'image_diff_total_pixels': '40',
            },
            {
                'name': 'conformance/textures/misc/video-rotation.html',
                'id': 'build-1111',
                'typ_tags': ['win', 'x86'],
                'image_diff_max_difference': '12',
                'image_diff_total_pixels': '45',
            },
        ]
        expected_results = [
            data_types.Result('conformance/textures/misc/video-rotation.html',
                              ('win', 'x86'), (10, 40), '1111'),
            data_types.Result('conformance/textures/misc/video-rotation.html',
                              ('win', 'x86'), (12, 45), '1111'),
        ]

        self.assertEqual(
            self._result_processor._convert_json_results_to_result_objects(r),
            expected_results)


class AggregateSlownessResultsUnittest(BaseResultsUnittest):
    def testBasic(self) -> None:
        """Basic functionality test."""
        query_results = [
            {
                'test_name': 'test1',
                'builder': 'builder1',
                'slow_count': '1',
                'non_slow_count': '40',
                'avg_duration': '1.2',
                'timeout_count': '0',
                'timeout': '6',
            },
            {
                'test_name': 'test2',
                'builder': 'builder2',
                'slow_count': '10',
                'non_slow_count': '4',
                'avg_duration': '5.2',
                'timeout_count': '3',
                'timeout': '6',
            },
            {
                'test_name': 'test3',
                'builder': 'builder1',
                'slow_count': '0',
                'non_slow_count': '100',
                'avg_duration': '0.2',
                'timeout_count': '0',
                'timeout': '6',
            },
            {
                'test_name': 'test1',
                'builder': 'builder2',
                'slow_count': '100',
                'non_slow_count': '0',
                'avg_duration': '5.9',
                'timeout_count': '20',
                'timeout': '6',
            },
        ]
        expected_output = {
            'test1': [('builder1', 1, 40, 1.2, 0, 6),
                      ('builder2', 100, 0, 5.9, 20, 6)],
            'test2': [('builder2', 10, 4, 5.2, 3, 6)],
            'test3': [('builder1', 0, 100, 0.2, 0, 6)],
        }
        self.assertEqual(
            self._result_processor.aggregate_test_slowness_results(
                query_results), expected_output)
