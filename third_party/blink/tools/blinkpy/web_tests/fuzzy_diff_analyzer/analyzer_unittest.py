# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.fuzzy_diff_analyzer import analyzer
from blinkpy.web_tests.fuzzy_diff_analyzer import data_types as dt


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
        actual_result = fuzzy_match_analyzer.run_analyzer(test_data)
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
        actual_result = fuzzy_match_analyzer.run_analyzer(test_data)
        expected_result = 'Total image diff number is 3. ' \
                          'Total distinct image diff number is less than 4. ' \
                          'Suggested make all following image diff ' \
                          '(color_difference, pixel_difference) to match actual ' \
                          'image result: (10, 40) with total 2 (15, 42) with total 1'
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
        actual_result = fuzzy_match_analyzer.run_analyzer(test_data)
        expected_result = 'Total image diff number is 5. The list of fuzzy match range suggested for this ' \
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
        actual_result = fuzzy_match_analyzer.run_analyzer(test_data)
        expected_result = 'Total image diff number is 100. The list of fuzzy match range suggested for this ' \
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
        actual_result = fuzzy_match_analyzer.run_analyzer(test_data)
        expected_result = 'Total image diff number is 101. The list of fuzzy match range suggested for this ' \
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
