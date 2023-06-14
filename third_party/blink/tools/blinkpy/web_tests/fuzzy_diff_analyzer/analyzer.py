# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for all matching analyzers."""

from typing import List

from blinkpy.web_tests.fuzzy_diff_analyzer import data_types as dt


class ImageMatchingAnalyzer:
    """Abstract base class for all analyzers."""
    def run_analyzer(self, test_results: dt.TestToTypTagsType) -> str:
        """Gets the analysis result of the input test's results.

        Returns:
          A string representing the analyzer suggested matching information for
          the input image comparison test's result.
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

    def run_analyzer(self, test_results: dt.TestToTypTagsType) -> str:
        """Analyze the input image comparison test result for a fuzzy match
        range suggestion.

        Args:
          test_results: Parsed test results of a test with image diff data.

        Returns:
          A string of fuzzy match range suggestion
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
            return ('Total image diff number is less than %d, no result' %
                    self._image_diff_num_threshold)

        # Total distinct image diff number does not reach the threshold, return all
        # these image diff as suggested matching.
        if len(image_diff_counts) < self._distinct_diff_num_threshold:
            result = 'Total image diff number is %d. ' % total_image_diff_num
            result += 'Total distinct image diff number is less '
            result += 'than %d. Suggested ' % self._distinct_diff_num_threshold
            result += 'make all following image diff (color_difference, '
            result += 'pixel_difference) to match actual image result:'
            for image_diff, image_diff_number in image_diff_counts.items():
                result = result + ' (%d, %d) with total %d' % (
                    image_diff.color_difference, image_diff.pixel_difference,
                    image_diff_number)
            return result

        # Calculate the suggested fuzzy match number.
        color_diff_list = []
        pixel_diff_list = []
        for image_diff, count in image_diff_counts.items():
            color_diff_list += [image_diff.color_difference] * count
            pixel_diff_list += [image_diff.pixel_difference] * count

        result = 'Total image diff number is %d. ' % total_image_diff_num
        result += 'The list of fuzzy match range suggested for this test: '
        result += '\nFor color difference:\n'
        result += self._calculate_data_percentile(color_diff_list)
        result += '\nFor pixel difference:\n'
        result += self._calculate_data_percentile(pixel_diff_list)
        return result

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
