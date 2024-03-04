# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for all data type definitions."""

from typing import Any, Dict, List, Tuple
from collections import namedtuple

TypTagTupleType = Tuple[str, ...]
# Image diff data, first item is color difference,
# second item is pixel difference, third is build url, example: [2, 30, 'url']
ImageDiffTagTupleType = namedtuple(
    'ImageDiffTagTupleType',
    ['color_difference', 'pixel_difference', 'build_url'])

# Sample:
# {
#   'test_name': {
#     ('typ', 'tags', 'as', 'tuple'): [ (0, 200, 'url_1'), (3, 400, 'url_2'),],
#   },
# }
TestToTypTagsType = Dict[TypTagTupleType, List[ImageDiffTagTupleType]]
AggregatedResultsType = Dict[str, TestToTypTagsType]

# Test analysis result data, first item is bool if the tests is analyzed
# second is analysis result string, example: [true, 'result is fool']
TestAnalysisResultType = namedtuple('TestAnalysisResultType',
                                    ['is_analyzed', 'analysis_result'])

# Test slowness data, example: ['builder1', 5, 100, 2.1, 10, 6]
# Sample:
# {
#   'test_name': [ ('builder1', 5, 100, 2.1, 1, 6),
#                  ('builder2', 10, 5, 5.1, 20, 6), ]
# }
TestSlownessTupleType = namedtuple('TestSlownessTupleType', [
    'builder', 'slow_count', 'non_slow_count', 'avg_duration', 'timeout_count',
    'timeout'
])
AggregatedSlownessResultsType = Dict[str, List[TestSlownessTupleType]]

TestSlownessData = namedtuple(
    'TestSlownessData',
    ['builder', 'slow_count', 'slow_ratio', 'timeout_count', 'avg_duration'])

BUGANIZER = 'Buganizer'
FUZZY_DIFF_ANALYZER = 'fuzzy_diff_analyzer'
SLOW_TEST_ANALYZER = 'slow_test_analyzer'

class Result:
    """Container for an image diff test result.

    Contains all the relevant information we get back from BigQuery for a result
    for the purposes of the fuzzy diff analyzer.
    """
    def __init__(self, test: str, typ_tags: TypTagTupleType,
                 image_diff_tag: Tuple[int, int], build_id: str):
        """Class for store an image diff web tests data.

        Args:
          test: The test name.
          typ_tags: A tuple of typ tags such as ('linux', 'x86')
          image_diff_tag: A tuple containing image diff data, first is color
          difference, second is the pixel difference.
          build_id: The build id for this test.
        """
        assert isinstance(typ_tags, tuple), \
          'Typ tags must be in tuple form to be hashable'
        assert isinstance(image_diff_tag, tuple), \
          'Image diff tag must be in tuple form to be hashable'
        assert len(image_diff_tag) == 2, 'Image diff tag must be 2 length'
        # Results should not have any globs.
        assert '*' not in test
        self.test = test
        self.typ_tags = typ_tags
        self.image_diff_tag = image_diff_tag
        self.build_id = build_id

    def __eq__(self, other: Any) -> bool:
        return (isinstance(other, Result) and self.test == other.test
                and self.typ_tags == other.typ_tags
                and self.image_diff_tag == other.image_diff_tag
                and self.build_id == other.build_id)

    def __hash__(self) -> int:
        return hash(
            (self.test, self.typ_tags, self.image_diff_tag, self.build_id))
