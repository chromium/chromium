# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' expectations module."""

from __future__ import print_function

import os
from typing import FrozenSet, List, Set

import validate_tag_consistency

from unexpected_passes_common import expectations

EXPECTATIONS_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', 'gpu_tests',
                 'test_expectations'))


class GpuExpectations(expectations.Expectations):
  def __init__(self):
    super().__init__()
    self._known_tags = None

  def GetExpectationFilepaths(self) -> List[str]:
    filepaths = []
    for f in os.listdir(EXPECTATIONS_DIR):
      if f.endswith('_expectations.txt'):
        filepaths.append(os.path.join(EXPECTATIONS_DIR, f))
    return filepaths

  def _GetExpectationFileTagHeader(self, _: str) -> str:
    return validate_tag_consistency.TAG_HEADER

  def _GetKnownTags(self) -> Set[str]:
    if self._known_tags is None:
      list_parser = self.ParseTaggedTestListContent(
          self._GetExpectationFileTagHeader(''))
      self._known_tags = set()
      for ts in list_parser.tag_sets:
        self._known_tags |= ts
    return self._known_tags

  def _ConsolidateKnownOverlappingTags(self, typ_tags: FrozenSet[str]
                                       ) -> FrozenSet[str]:
    typ_tags = set(typ_tags)
    # 2015 Macbook Pros w/ dual GPUs.
    if {'amd-0x6821', 'intel-0xd26'} <= typ_tags:
      typ_tags -= {'intel', 'intel-0xd26'}
    return frozenset(typ_tags)
