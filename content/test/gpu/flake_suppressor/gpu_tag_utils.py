# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for tag-related helper functions."""

from typing import Iterable

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import tag_utils

from gpu_tests import gpu_integration_test


class GpuTagUtils(tag_utils.BaseTagUtils):
  def RemoveIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
    ignored_tags = set(gpu_integration_test.GpuIntegrationTest.IgnoredTags())
    tags = set(tags)
    tags -= ignored_tags
    tags = list(tags)
    tags.sort()
    return tuple(tags)
