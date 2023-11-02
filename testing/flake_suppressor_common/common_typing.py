# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for shared/commonly used type hinting."""

from typing import Any, Dict, List, Tuple

TagTupleType = Tuple[str, ...]

# Sample:
# {
#   'test_suite': {
#     'test_name': {
#       ('typ', 'tags', 'as', 'tuple'): [ 'list', 'of', 'urls' ],
#     },
#   },
# }
TagsToUrlsType = Dict[TagTupleType, List[str]]
TestToTagsType = Dict[str, TagsToUrlsType]
AggregatedResultsType = Dict[str, TestToTagsType]

# Sample:
# {
#   typ_tags (tuple): {
#     test_name (str): result_count (int)
#   }
# }
TestToResultCountType = Dict[str, int]
ResultCountType = Dict[TagTupleType, TestToResultCountType]

SingleQueryResultType = Dict[str, Any]
QueryJsonType = List[SingleQueryResultType]
