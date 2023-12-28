# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.web_test_analyzers import analyzer
from blinkpy.web_tests.web_test_analyzers import data_types as dt


class FuzzyMatchingAnalyzerTest(unittest.TestCase):
    def test_run_analyzer_in_image_diff_num_threshold(self) -> None:
        fuzzy_match_analyzer = analyzer.FuzzyMatchingAnalyzer(
            fuzzy_match_image_diff_num_threshold=4,
            fuzzy_match_distinct_diff_num_threshold=5)

        test_data = \
        {
            tuple(['win']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/1111'),
                dt.ImageDiffTagTupleType(
                    15, 42, 'http://ci.chromium.org/b/2222')],
            tuple(['linux']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/3333')],
        }
        actual_result = fuzzy_match_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Total image diff number is less than 4, no result'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_distinct_diff_num_threshold(self) -> None:
        fuzzy_match_analyzer = analyzer.FuzzyMatchingAnalyzer(
            fuzzy_match_image_diff_num_threshold=3,
            fuzzy_match_distinct_diff_num_threshold=4)

        test_data = \
        {
            tuple(['win']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/1111'),
                dt.ImageDiffTagTupleType(
                    15, 42, 'http://ci.chromium.org/b/2222')],
            tuple(['linux']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/3333')],
        }
        actual_result = fuzzy_match_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Total test number that have image diff is 3. ' \
                          'Total distinct image diff results is less than 4. ' \
                          'Suggested check these image diff result ' \
                          '(color_difference, pixel_difference) ' \
                          'individually to fix the issue: (10, 40) ' \
                          'with total test number 2 ' \
                          '(15, 42) with total test number 1'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_fuzzy_match_range_in_different_platform(
            self) -> None:
        fuzzy_match_analyzer = analyzer.FuzzyMatchingAnalyzer(
            fuzzy_match_image_diff_num_threshold=3,
            fuzzy_match_distinct_diff_num_threshold=3)

        test_data = \
        {
            tuple(['win']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/1111'),
                dt.ImageDiffTagTupleType(
                    15, 42, 'http://ci.chromium.org/b/2222'),
                dt.ImageDiffTagTupleType(
                    16, 46, 'http://ci.chromium.org/b/4444'),
                dt.ImageDiffTagTupleType(
                    303, 8000, 'http://ci.chromium.org/b/5555')],
            tuple(['linux']): [
                dt.ImageDiffTagTupleType(
                    10, 40, 'http://ci.chromium.org/b/3333')],
        }
        actual_result = fuzzy_match_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Total test number that have image diff is 5. ' \
                          'The list of fuzzy match range suggested for this ' \
                          'test: \nFor color difference:\n15 to cover 50 percentile, 16 ' \
                          'to cover 75 percentile, 16 to cover 90 percentile, ' \
                          '16 to cover 95 percentile, 303 to cover all.\nFor ' \
                          'pixel difference:\n42 to cover 50 percentile, 46 to cover ' \
                          '75 percentile, 46 to cover 90 percentile, 46 to cover ' \
                          '95 percentile, 8000 to cover all.'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_fuzzy_match_range_with_large_even_number_tests(
            self) -> None:
        fuzzy_match_analyzer = analyzer.FuzzyMatchingAnalyzer(
            fuzzy_match_image_diff_num_threshold=3,
            fuzzy_match_distinct_diff_num_threshold=3)
        image_diffs = []
        for i in range(1, 101):
            image_diffs.append(dt.ImageDiffTagTupleType(i, 100 + i, ''))
        test_data = \
        {
            tuple(['win']): image_diffs,
        }
        actual_result = fuzzy_match_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Total test number that have image diff is 100. ' \
                          'The list of fuzzy match range suggested for this ' \
                          'test: \nFor color difference:\n50 to cover 50 percentile, 75 ' \
                          'to cover 75 percentile, 90 to cover 90 percentile, ' \
                          '95 to cover 95 percentile, 100 to cover all.\nFor ' \
                          'pixel difference:\n150 to cover 50 percentile, 175 to cover ' \
                          '75 percentile, 190 to cover 90 percentile, 195 to cover ' \
                          '95 percentile, 200 to cover all.'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_fuzzy_match_range_with_large_odd_number_tests(
            self) -> None:
        fuzzy_match_analyzer = analyzer.FuzzyMatchingAnalyzer(
            fuzzy_match_image_diff_num_threshold=3,
            fuzzy_match_distinct_diff_num_threshold=3)
        image_diffs = []
        for i in range(1, 102):
            image_diffs.append(dt.ImageDiffTagTupleType(i, 100 + i, ''))
        test_data = \
        {
            tuple(['win']): image_diffs,
        }
        actual_result = fuzzy_match_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Total test number that have image diff is 101. ' \
                          'The list of fuzzy match range suggested for this ' \
                          'test: \nFor color difference:\n51 to cover 50 percentile, 76 ' \
                          'to cover 75 percentile, 91 to cover 90 percentile, ' \
                          '96 to cover 95 percentile, 101 to cover all.\nFor ' \
                          'pixel difference:\n151 to cover 50 percentile, 176 to cover ' \
                          '75 percentile, 191 to cover 90 percentile, 196 to cover ' \
                          '95 percentile, 201 to cover all.'
        self.assertEqual(actual_result, expected_result)

    def test_invalid_args(self) -> None:
        # Test negative number.
        with self.assertRaises(AssertionError):
            analyzer.FuzzyMatchingAnalyzer(
                fuzzy_match_image_diff_num_threshold=-1,
                fuzzy_match_distinct_diff_num_threshold=0)
        # Test negative number.
        with self.assertRaises(AssertionError):
            analyzer.FuzzyMatchingAnalyzer(
                fuzzy_match_image_diff_num_threshold=0,
                fuzzy_match_distinct_diff_num_threshold=-1)


class SlowTestAnalyzerTest(unittest.TestCase):
    def test_run_analyzer_in_bad_slow_ratio_threshold(self) -> None:
        slow_test_analyzer = analyzer.SlowTestAnalyzer(0.95, 0)

        test_data = \
        [
            dt.TestSlownessTupleType('builder1', 1, 30, 1.1, 2, 6),
            dt.TestSlownessTupleType('builder2', 30, 5, 3.1, 5, 6),
        ]
        actual_result = slow_test_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Test does not meet threshold in all builders.'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_good_slow_ratio_threshold(self) -> None:
        slow_test_analyzer = analyzer.SlowTestAnalyzer(0.9, 0)

        test_data = \
        [
            dt.TestSlownessTupleType('builder1', 1, 30, 1.1, 2, 6),
            dt.TestSlownessTupleType('builder2', 30, 3, 4.9, 5, 6),
        ]
        actual_result = slow_test_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Test is slow in the below list of builders:\n' \
                          'builder2 : timeout count: 5, slow count: 30, ' \
                          'slow ratio: 0.91, avg duration: 4.90\n'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_bad_timeout_count_threshold(self) -> None:
        slow_test_analyzer = analyzer.SlowTestAnalyzer(0.9, 10)

        test_data = \
        [
            dt.TestSlownessTupleType('builder1', 20, 1, 5.1, 2, 6),
            dt.TestSlownessTupleType('builder2', 30, 1, 3.1, 5, 6),
        ]
        actual_result = slow_test_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Test does not meet threshold in all builders.'
        self.assertEqual(actual_result, expected_result)

    def test_run_analyzer_in_good_timeout_count_threshold(self) -> None:
        slow_test_analyzer = analyzer.SlowTestAnalyzer(0.9, 4)

        test_data = \
        [
            dt.TestSlownessTupleType('builder1', 20, 1, 3.1, 6, 6),
            dt.TestSlownessTupleType('builder2', 30, 1, 5.1, 5, 6),
        ]
        actual_result = slow_test_analyzer.run_analyzer(
            test_data).analysis_result
        expected_result = 'Test is slow in the below list of builders:\n' \
                          'builder2 : timeout count: 5, slow count: 30, ' \
                          'slow ratio: 0.97, avg duration: 5.10\n'
        self.assertEqual(actual_result, expected_result)

    def test_invalid_args(self) -> None:
        # Test negative number.
        with self.assertRaises(AssertionError):
            analyzer.SlowTestAnalyzer(slow_result_ratio_threshold=-1,
                                      timeout_result_threshold=0)
        # Test number greater than 1.
        with self.assertRaises(AssertionError):
            analyzer.SlowTestAnalyzer(slow_result_ratio_threshold=1.1,
                                      timeout_result_threshold=0)
        # Test negative number.
        with self.assertRaises(AssertionError):
            analyzer.SlowTestAnalyzer(slow_result_ratio_threshold=0,
                                      timeout_result_threshold=-1)
