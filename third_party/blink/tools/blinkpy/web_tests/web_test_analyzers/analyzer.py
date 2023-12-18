# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for all matching analyzers."""

from typing import List

from blinkpy.web_tests.web_test_analyzers import data_types as dt

MAX_BUILDER_NUM = 5
AVG_DURATION_THRESHOLD = 0.75


class ImageMatchingAnalyzer:
    """Abstract base class for all analyzers."""
    def run_analyzer(
            self,
            test_results: dt.TestToTypTagsType) -> dt.TestAnalysisResultType:
        """Gets the analysis result of the input test's results.

        Returns:
          A test analysis result representing the analyzer suggested matching
          information for the input image comparison test's result.
        """
        raise NotImplementedError()

    def description(self) -> str:
        """Returns a description string representation of the analyzer."""
        raise NotImplementedError()


class FuzzyMatchingAnalyzer(ImageMatchingAnalyzer):
    def __init__(self,
                 fuzzy_match_image_diff_num_threshold: int = 3,
                 fuzzy_match_distinct_diff_num_threshold: int = 3):
        """Class for the fuzzy matching analyzer for web tests.

        Args:
          fuzzy_match_image_diff_num_threshold: An int denoting the number of
              image test result at least to get the suggested fuzzy match range.
          fuzzy_match_distinct_diff_num_threshold: An int denoting the number of
              different image diff result at least to calculate the suggested
              fuzzy match range.
        """
        super().__init__()
        assert fuzzy_match_image_diff_num_threshold >= 0
        assert fuzzy_match_distinct_diff_num_threshold >= 0
        self._image_diff_num_threshold = fuzzy_match_image_diff_num_threshold
        self._distinct_diff_num_threshold = fuzzy_match_distinct_diff_num_threshold

    def run_analyzer(
            self,
            test_results: dt.TestToTypTagsType) -> dt.TestAnalysisResultType:
        """Analyze the input image comparison test result for a fuzzy match
        range suggestion.

        Args:
          test_results: Parsed test results of a test with image diff data.

        Returns:
          A test analysis result representing the analyzer suggested matching
          information for the input image comparison test's result.
        """
        # Check the image num threshold
        total_image_diff_num = 0
        image_diff_counts = {}
        for image_diff_list in test_results.values():
            total_image_diff_num += len(image_diff_list)
            for image_diff in image_diff_list:
                key = dt.ImageDiffTagTupleType(image_diff.color_difference,
                                               image_diff.pixel_difference, "")
                image_diff_counts[key] = image_diff_counts.get(key, 0) + 1

        # Total image diff number does not meet the threshold, return no result.
        if total_image_diff_num < self._image_diff_num_threshold:
            return dt.TestAnalysisResultType(
                False, 'Total image diff number is less than %d, no result' %
                self._image_diff_num_threshold)

        # Total distinct image diff number does not reach the threshold, return all
        # these image diff as suggested matching.
        if len(image_diff_counts) < self._distinct_diff_num_threshold:
            result = 'Total test number that have image diff is '
            result += '%d. ' % total_image_diff_num
            result += 'Total distinct image diff results is less '
            result += 'than %d. Suggested ' % self._distinct_diff_num_threshold
            result += 'check these image diff result (color_difference, '
            result += 'pixel_difference) individually to fix the issue:'
            for image_diff, image_diff_number in image_diff_counts.items():
                result = result + ' (%d, %d) with total test number %d' % (
                    image_diff.color_difference, image_diff.pixel_difference,
                    image_diff_number)
            return dt.TestAnalysisResultType(True, result)

        # Calculate the suggested fuzzy match number.
        color_diff_list = []
        pixel_diff_list = []
        for image_diff, count in image_diff_counts.items():
            color_diff_list += [image_diff.color_difference] * count
            pixel_diff_list += [image_diff.pixel_difference] * count

        result = 'Total test number that have image diff is '
        result += '%d. ' % total_image_diff_num
        result += 'The list of fuzzy match range suggested for this test: '
        result += '\nFor color difference:\n'
        result += self._calculate_data_percentile(color_diff_list)
        result += '\nFor pixel difference:\n'
        result += self._calculate_data_percentile(pixel_diff_list)
        return dt.TestAnalysisResultType(True, result)

    def _calculate_data_percentile(self, data_list: List[int]) -> str:
        """Calculate the input data list for the percentile result.

        Args:
          data_list: List of int to calculate.

        Returns:
          A string of percentile information.
        """

        if len(data_list) > 0:
            data_list.sort()
            color_diff_length = len(data_list) - 1
            result = '%d to cover 50 percentile, ' % data_list[int(
                color_diff_length * 0.5)]
            result += '%d to cover 75 ' % data_list[int(
                color_diff_length * 0.75)]
            result += 'percentile, %d to ' % data_list[int(
                color_diff_length * 0.9)]
            result += 'cover 90 percentile, %d to cover 95' % data_list[int(
                color_diff_length * 0.95)]
            result += ' percentile, %d to cover all.' % data_list[
                color_diff_length]
            return result

        return "No data."

    def description(self) -> str:
        des = ('Fuzzy match analyzer with image_diff_num_threshold of %d and ' %
               self._image_diff_num_threshold)
        des += ('distinct_diff_num_threshold of %d' %
                self._distinct_diff_num_threshold)
        return des


class SlowTestAnalyzer:
    def __init__(self,
                 slow_result_ratio_threshold: float = 0.9,
                 timeout_result_threshold: int = 0):
        """Class for the slow test analyzer for web tests.

        Args: (Both thresholds must be hit for a test to be considered slow)
          slow_result_ratio_threshold: A float denoting what fraction
            of results need to be slow for a test to be considered slow.
          timeout_result_threshold: An int denoting the number of timeout
            results necessary for a test to be considered slow.
        """
        assert slow_result_ratio_threshold >= 0
        assert slow_result_ratio_threshold <= 1
        assert timeout_result_threshold >= 0
        self._slow_result_ratio_threshold = slow_result_ratio_threshold
        self._timeout_result_threshold = timeout_result_threshold

    def run_analyzer(
        self, test_results: List[dt.TestSlownessTupleType]
    ) -> dt.TestAnalysisResultType:
        """Analyze the input test slowness data to show a statistics and
        suggestion.

        Args:
          test_results: list of slowness data per builder.

        Returns:
          A test analysis result representing the analyzer's statistics and
            suggestion for the slow test.
        """
        # Filter slowness data per builder
        slow_tests = []
        for result in test_results:
            if (result.timeout_count >= self._timeout_result_threshold
                    and result.avg_duration >=
                    result.timeout * AVG_DURATION_THRESHOLD):
                total_non_timeout_count = result.slow_count + result.non_slow_count
                slow_ratio = (result.slow_count / total_non_timeout_count
                              if total_non_timeout_count else 0)
                if slow_ratio >= self._slow_result_ratio_threshold:
                    slow_tests.append(
                        dt.TestSlownessData(result.builder, result.slow_count,
                                            slow_ratio, result.timeout_count,
                                            result.avg_duration))

        # No slowness data meet the threshold, return no result.
        if not slow_tests:
            return dt.TestAnalysisResultType(
                False, 'Test does not meet threshold in all builders.')

        sorted(slow_tests, key=lambda x: x.slow_ratio)
        count = min(MAX_BUILDER_NUM, len(slow_tests))
        result = 'Test is slow in the below list of builders:\n'
        for i in range(count):
            test = slow_tests[i]
            result += '%s : timeout count: %d, ' % (test.builder,
                                                    test.timeout_count)
            result += 'slow count: %d, ' % test.slow_count
            result += 'slow ratio: %.2f, ' % test.slow_ratio
            result += 'avg duration: %.2f\n' % test.avg_duration
        return dt.TestAnalysisResultType(True, result)

    def description(self) -> str:
        des = (f'Slow test analyzer with slow_result_ration_threshold of '
               f'{self._slow_result_ratio_threshold} and '
               f'timeout_result_threshold of {self._timeout_result_threshold}')
        return des
