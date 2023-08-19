#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from flake_suppressor import gpu_tag_utils as tag_utils


class RemoveIgnoredTagsUnittest(unittest.TestCase):
  def testBasic(self) -> None:
    tags = ['win', 'win-laptop']
    filtered_tags = tag_utils.GpuTagUtils().RemoveIgnoredTags(tags)
    self.assertEqual(filtered_tags, ('win', ))


if __name__ == '__main__':
  unittest.main(verbosity=2)
