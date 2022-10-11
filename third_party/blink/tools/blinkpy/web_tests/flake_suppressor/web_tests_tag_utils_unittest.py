#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.flake_suppressor import web_tests_tag_utils as tag_utils


class RemoveMostIgnoredTagsUnittest(unittest.TestCase):
    def testBasic(self) -> None:
        tags = ['win', 'x86', 'release']
        filtered_tags = tag_utils.WebTestsTagUtils().RemoveMostIgnoredTags(
            tags)
        self.assertEqual(filtered_tags, ('release', 'win'))


class RemoveTemporarilyKeptIgnoredTagsUnittest(unittest.TestCase):
    def testBasic(self) -> None:
        tags = ['win', 'x86_64', 'mac11']
        # RemoveTemporarilyKeptIgnoredTags is always called after
        # RemoveMostIgnoredTags. For web tests RemoveTemporarilyKeptIgnoredTags
        # returns input tag set.
        filtered_tags = (tag_utils.WebTestsTagUtils().
                         RemoveTemporarilyKeptIgnoredTags(tags))
        self.assertEqual(filtered_tags, ('win', 'x86_64', 'mac11'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
