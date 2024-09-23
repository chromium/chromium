# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for shared/commonly used type hinting."""

import datetime
from enum import Enum
from typing import Any, Dict, List, Tuple, NamedTuple

TagTupleType = Tuple[str, ...]
# TODO(crbug.com/40237087): Remove this and update both GPU and Web test
# suppressor with status support.
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

# Sample of AggregatedStatusResultsType:
# {
#   'test_suite': {
#     'test_name': {
#       ('typ', 'tags', 'as', 'tuple'):
#       [ (status, url, date, is_slow, typ_expectations),
#         (status, url, date, is_slow, typ_expectations) ],
#     },
#   },
# }
class ResultTupleType(NamedTuple):
  status: str
  build_url: str
  date: datetime.date
  is_slow: bool
  typ_expectations: List[str]


TagsToResultType = Dict[TagTupleType, List[ResultTupleType]]
TestStatusToTagsType = Dict[str, TagsToResultType]
AggregatedStatusResultsType = Dict[str, TestStatusToTagsType]

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


class ResultStatus(str, Enum):
  ABORT = 'ABORT'
  CRASH = 'CRASH'
  FAIL = 'FAIL'
