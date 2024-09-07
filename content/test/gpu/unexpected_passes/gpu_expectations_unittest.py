#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import datetime
from typing import Dict
import unittest
from unittest import mock

from unexpected_passes import gpu_expectations
from unexpected_passes_common import data_types

# pylint: disable=protected-access


class CreateTestExpectationMapUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = gpu_expectations.GpuExpectations()

    self._expectation_content: Dict[str, str] = {}
    self._content_patcher = mock.patch.object(
        self.instance, '_GetNonRecentExpectationContent')
    self._content_mock = self._content_patcher.start()
    self.addCleanup(self._content_patcher.stop)

    def SideEffect(filepath, _):
      return self._expectation_content[filepath]

    self._content_mock.side_effect = SideEffect

  def testSlowExpectationsDropped(self) -> None:
    """Tests that slow expectations get dropped from the generated map."""
    filename = '/tmp/foo'
    self._expectation_content[filename] = """\
# tags: [ win linux ]
# tags: [ nvidia intel ]
# results: [ Failure Slow ]

[ win intel ] foo/test [ Failure ]
[ win nvidia ] foo/test [ Slow ]
[ linux nvidia ] foo/test [ Slow ]
[ linux intel ] foo/test [ Failure ]
"""
    expectation_map = self.instance.CreateTestExpectationMap(
        filename, None, datetime.timedelta(days=0))
    # The Slow expectations should be omitted.
    expected_expectation_map = {
        filename: {
            data_types.Expectation('foo/test', ['win', 'intel'], ['Failure']):
            {},
            data_types.Expectation('foo/test', ['linux', 'intel'], ['Failure']):
            {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)


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

  def test15InchMacbookPro2019(self) -> None:
    """Tests that 15" Macbook Pro 2019 tags are properly consolidated."""
    tags = frozenset([
        'mac', 'amd', 'amd-0x67ef', 'release', 'intel', 'intel-0x3e9b',
        'intel-gen-9'
    ])
    consolidated_tags = self.expectations._ConsolidateKnownOverlappingTags(tags)
    self.assertEqual(consolidated_tags, {'mac', 'amd', 'amd-0x67ef', 'release'})

  def test16InchMacbookPro2019(self) -> None:
    """Tests that 16" Macbook Pro 2019 tags are properly consolidated."""
    tags = frozenset([
        'mac', 'amd', 'amd-0x7340', 'release', 'intel', 'intel-0x3e9b',
        'intel-gen-9'
    ])
    consolidated_tags = self.expectations._ConsolidateKnownOverlappingTags(tags)
    self.assertEqual(consolidated_tags, {'mac', 'amd', 'amd-0x7340', 'release'})

  def testSpecificMacVersion(self) -> None:
    """Tests that specific Mac versions can be used for IDing dual GPUs."""
    tags = frozenset([
        'angle-metal', 'amd-0x67ef', 'sonoma', 'intel-gen-9', 'amd',
        'passthrough'
    ])
    consolidated_tags = self.expectations._ConsolidateKnownOverlappingTags(tags)
    self.assertEqual(
        consolidated_tags,
        {'angle-metal', 'amd-0x67ef', 'sonoma', 'amd', 'passthrough'})


if __name__ == '__main__':
  unittest.main(verbosity=2)
