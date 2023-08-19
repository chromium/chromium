# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for tag-related helper functions."""

from typing import Iterable

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import tag_utils

WEB_TESTS_TAGS_TO_IGNORE = set(['x86_64', 'x86', 'arm', 'arm64'])


class WebTestsTagUtils(tag_utils.BaseTagUtils):
    def RemoveIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
        tags = list(set(tags) - WEB_TESTS_TAGS_TO_IGNORE)
        tags.sort()
        return tuple(tags)
