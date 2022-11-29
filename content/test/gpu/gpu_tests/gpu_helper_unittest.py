#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from gpu_tests import gpu_helper


class ReplaceTagsUnittest(unittest.TestCase):
  def testSubstringReplacement(self):
    tags = ['some_tag', 'some-nvidia-corporation', 'another_tag']
    self.assertEqual(gpu_helper.ReplaceTags(tags),
                     ['some_tag', 'some-nvidia', 'another_tag'])

  def testRegexReplacement(self):
    tags = [
        'some_tag',
        'google-Vulkan-1.3.0-(SwiftShader-Device-(LLVM-10.0.0)-(0x0000C0DE))',
        'another_tag'
    ]
    self.assertEqual(gpu_helper.ReplaceTags(tags),
                     ['some_tag', 'google-vulkan', 'another_tag'])


if __name__ == '__main__':
  unittest.main(verbosity=2)
