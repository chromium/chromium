# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.flake_suppressor import web_tests_tag_utils as tag_utils


class RemoveIgnoredTagsUnittest(unittest.TestCase):
    def testBasic(self) -> None:
        tags = ['win', 'x86', 'release']
        filtered_tags = tag_utils.WebTestsTagUtils().RemoveIgnoredTags(tags)
        self.assertEqual(filtered_tags, ('release', 'win'))
