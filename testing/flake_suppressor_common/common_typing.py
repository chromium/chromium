# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for shared/commonly used type hinting."""

from collections import namedtuple
from enum import Enum
from typing import Any, Dict, List, Tuple

TagTupleType = Tuple[str, ...]
# TODO(crbug.com/1358735): Remove this and update both GPU and Web test
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

# Sample:
# {
#   'test_suite': {
#     'test_name': {
#       ('typ', 'tags', 'as', 'tuple'): [ (status, url), (status, url) ],
#     },
#   },
# }
ResultTupleType = namedtuple('ResultTupleType', ['status', 'build_url'])
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
  ABORT = "ABORT"
  CRASH = "CRASH"
  FAIL = "FAIL"
