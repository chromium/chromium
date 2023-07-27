# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' expectations module."""

from __future__ import print_function

import collections
import logging
import os
from typing import FrozenSet, List, Optional, Set

import validate_tag_consistency

from gpu_tests import gpu_integration_test

from unexpected_passes_common import data_types
from unexpected_passes_common import expectations

EXPECTATIONS_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', 'gpu_tests',
                 'test_expectations'))


class GpuExpectations(expectations.Expectations):
  def __init__(self):
    super().__init__()
    self._known_tags: Optional[Set[str]] = None
    self._expectation_files: Optional[List[str]] = None

  def CreateTestExpectationMap(self, *args,
                               **kwargs) -> data_types.TestExpectationMap:
    expectation_map = super().CreateTestExpectationMap(*args, **kwargs)
    # We currently don't support handling Slow expectations, so drop them
    # immediately so they can't be accidentally removed.
    expectations_to_drop = collections.defaultdict(list)
    for expectation_file, expectation_dict in expectation_map.items():
      for expectation in expectation_dict:
        if 'Slow' in expectation.expected_results:
          expectations_to_drop[expectation_file].append(expectation)
    for expectation_file, expectation_list in expectations_to_drop.items():
      for expectation in expectation_list:
        logging.info(
            'Dropping expectation "%s" from %s since it includes a "Slow" '
            'expected result', expectation.AsExpectationFileString(),
            expectation_file)
        del expectation_map[expectation_file][expectation]
    return expectation_map

  def GetExpectationFilepaths(self) -> List[str]:
    if self._expectation_files is None:
      self._expectation_files = []
      name_mapping = gpu_integration_test.GenerateTestNameMapping()
      for suite_class in name_mapping.values():
        self._expectation_files.extend(suite_class.ExpectationsFiles())
    return self._expectation_files

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
