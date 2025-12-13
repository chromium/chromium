#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from filters import (avoid_file, avoid_directory, avoid_commit, avoid_username,
                       avoid_owner_line)


class FiltersTest(unittest.TestCase):

    def test_avoid_file(self):
        """Tests the avoid_file function."""
        self.assertFalse(avoid_file('file.h'),
                         ".h files should not be avoided.")
        self.assertFalse(avoid_file('file.mm'),
                         ".mm files should not be avoided.")
        self.assertFalse(avoid_file('file.cc'),
                         ".cc files should not be avoided.")
        self.assertFalse(avoid_file('file.swift'),
                         ".swift files should not be avoided.")
        self.assertFalse(avoid_file('file.ts'),
                         ".ts files should not be avoided.")
        self.assertFalse(avoid_file('file_unittest.mm'),
                         "Unittest files should not be avoided.")
        self.assertTrue(avoid_file('file.txt'),
                        "Non-source files should be avoided.")
        self.assertTrue(avoid_file('OWNERS'),
                        "OWNERS files should be avoided.")
        self.assertTrue(avoid_file('file.gn'), ".gn files should be avoided.")
        self.assertTrue(avoid_file('file.json'),
                        ".json files should be avoided.")

    def test_avoid_directory(self):
        """Tests the avoid_directory function."""
        self.assertTrue(avoid_directory('ios/chrome/browser/resources/'),
                        "Resource directories should be avoided.")
        self.assertTrue(avoid_directory('ios/chrome/test/'),
                        "Test directories should be avoided.")
        self.assertTrue(avoid_directory('ios/third_party/something/'),
                        "Third-party directories should be avoided.")
        self.assertFalse(avoid_directory('ios/chrome/browser/ui/'),
                         "Source directories should not be avoided.")

    def test_avoid_commit(self):
        """Tests the avoid_commit function."""
        self.assertTrue(avoid_commit('Revert "A change"'),
                        "Revert commits should be avoided.")
        self.assertTrue(avoid_commit('Author: bot\n\n[gardener] A change'),
                        "Gardener commits should be avoided.")
        self.assertTrue(avoid_commit('Author: user\n\n25 files changed,'),
                        "Commits with >20 files should be avoided.")
        self.assertTrue(avoid_commit('No author line here'),
                        "Commits without an author should be avoided.")
        self.assertFalse(
            avoid_commit('Author: user\n\nA normal commit\n5 files changed,'),
            "Normal commits should not be avoided.")

    def test_avoid_username(self):
        """Tests the avoid_username function."""
        self.assertTrue(avoid_username('chromium-autoroll'),
                        "Bot usernames should be avoided.")
        self.assertFalse(avoid_username('testuser'),
                         "Human usernames should not be avoided.")

    def test_avoid_owner_line(self):
        """Tests the avoid_owner_line function."""
        self.assertTrue(avoid_owner_line('# This is a comment'),
                        "Comment lines should be filtered.")
        self.assertTrue(avoid_owner_line(''),
                        "Empty lines should be filtered.")
        self.assertTrue(avoid_owner_line('  '),
                        "Whitespace-only lines should be filtered.")
        self.assertTrue(avoid_owner_line('set noparent'),
                        "'set noparent' lines should be filtered.")
        self.assertTrue(
            avoid_owner_line('per-file *.cc=user@chromium.org'),
            "'per-file' lines should be filtered.")
        self.assertFalse(avoid_owner_line('user@chromium.org'),
                         "Valid owner lines should not be filtered.")
        self.assertFalse(avoid_owner_line('  user@chromium.org  '),
                         "Valid owner lines should not be filtered.")


if __name__ == '__main__':
    unittest.main()
