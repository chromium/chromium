#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from flake_suppressor import tag_utils


class RemoveMostIgnoredTagsUnittest(unittest.TestCase):
  def testBasic(self):
    tags = ['win', 'win-laptop', 'webgl-version-1']
    filtered_tags = tag_utils.RemoveMostIgnoredTags(tags)
    self.assertEqual(filtered_tags, ['webgl-version-1', 'win'])


class RemoveTemporarilyKeptIgnoredTagsUnittest(unittest.TestCase):
  def testBasic(self):
    # win-laptop shouldn't technically be here since it would have been removed
    # by RemoveMostIgnoredTags(), but since this is *only* supposed to remove
    # temporarily kept ignored tags, include it to test that.
    tags = ['win', 'win-laptop', 'webgl-version-1', 'amd']
    filtered_tags = tag_utils.RemoveTemporarilyKeptIgnoredTags(tags)
    self.assertEqual(filtered_tags, ['amd', 'win', 'win-laptop'])


if __name__ == '__main__':
  unittest.main(verbosity=2)
