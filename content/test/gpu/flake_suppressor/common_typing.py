# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for shared/commonly used type hinting."""

import typing

TagTupleType = typing.Tuple[str, ...]

# Sample:
# {
#   'test_suite': {
#     'test_name': {
#       ('typ', 'tags', 'as', 'tuple'): [ 'list', 'of', 'urls' ],
#     },
#   },
# }
TagsToUrlsType = typing.Dict[TagTupleType, typing.List[str]]
TestToTagsType = typing.Dict[str, TagsToUrlsType]
AggregatedResultsType = typing.Dict[str, TestToTagsType]

# Sample:
# {
#   typ_tags (tuple): {
#     test_name (str): result_count (int)
#   }
# }
TestToResultCountType = typing.Dict[str, int]
ResultCountType = typing.Dict[TagTupleType, TestToResultCountType]

SingleQueryResultType = typing.Dict[str, typing.Any]
QueryJsonType = typing.List[SingleQueryResultType]
