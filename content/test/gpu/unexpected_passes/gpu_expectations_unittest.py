#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import unittest

from unexpected_passes import gpu_expectations

# pylint: disable=protected-access


class GetExpectationFilepathsUnittest(unittest.TestCase):
  def testGetExpectationFilepathsFindsSomething(self) -> None:
    """Tests that the _GetExpectationFilepaths finds something in the dir."""
    expectations = gpu_expectations.GpuExpectations()
    self.assertTrue(len(expectations.GetExpectationFilepaths()) > 0)


class ConsolidateKnownOverlappingTagsUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.expectations = gpu_expectations.GpuExpectations()

  def testNoOp(self) -> None:
    """Tests that consolidation is a no-op if there is no overlap."""
    tags = frozenset(['mac', 'amd', 'amd-0x6821', 'release'])
    consolidated_tags = self.expectations._ConsolidateKnownOverlappingTags(tags)
    self.assertEqual(consolidated_tags, tags)

  def testMacbookPro2015(self) -> None:
    """Tests that Macbook Pro 2015 tags are properly consolidated."""
    tags = frozenset(
        ['mac', 'amd', 'amd-0x6821', 'release', 'intel', 'intel-0xd26'])
    consolidated_tags = self.expectations._ConsolidateKnownOverlappingTags(tags)
    self.assertEqual(consolidated_tags, {'mac', 'amd', 'amd-0x6821', 'release'})


if __name__ == '__main__':
  unittest.main(verbosity=2)
