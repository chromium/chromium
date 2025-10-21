#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

from automatic_ownership import (
    get_existing_owners,
    is_high_level_owner,
    extract_commits_informations,
    determine_owners_from_zscore
)
from commit import Commit
from test_data import FAKE_GIT_LOG
from gitutils import split_log_into_commits


class AutomaticOwnershipTest(unittest.TestCase):

    def setUp(self):
        """Creates a temporary directory with mock OWNERS files."""
        self.temp_dir = tempfile.mkdtemp()
        self.ios_dir = os.path.join(self.temp_dir, 'ios')
        self.browser_dir = os.path.join(self.ios_dir, 'chrome', 'browser')
        os.makedirs(self.browser_dir)

        with open(os.path.join(self.ios_dir, 'OWNERS'), 'w') as f:
            f.write('set noparent\n')
            f.write('user_x@chromium.org\n')
            f.write('per-file BUILD.gn=*\n')
            f.write('per-file *.mm=user_ios_dev@chromium.org\n')

        with open(os.path.join(self.browser_dir, 'OWNERS'), 'w') as f:
            f.write('user_y@chromium.org\n')
            f.write('# This is a comment\n')
            f.write('user_b@chromium.org\n')
            f.write('per-file *.cc=user_z@chromium.org\n')
            f.write(
                'per-file *service*=file://ios/chrome/browser/service/OWNERS\n'
            )

    def tearDown(self):
        """Removes the temporary directory."""
        shutil.rmtree(self.temp_dir)

    def test_get_existing_owners(self):
        """Tests that OWNERS files are correctly parsed."""
        owners_map = get_existing_owners(self.temp_dir)
        # Test paths should be relative to self.temp_dir
        ios_rel_path = os.path.relpath(self.ios_dir, self.temp_dir)
        browser_rel_path = os.path.relpath(self.browser_dir, self.temp_dir)
        self.assertIn('user_x', owners_map[ios_rel_path])
        self.assertIn('user_y', owners_map[browser_rel_path])
        self.assertIn('user_b', owners_map[browser_rel_path])
        self.assertEqual(len(owners_map[ios_rel_path]), 1)
        self.assertEqual(len(owners_map[browser_rel_path]), 2)

    def test_is_high_level_owner(self):
        """Tests the high-level owner check."""
        owners_map = get_existing_owners(self.temp_dir)
        browser_rel_path = os.path.relpath(self.browser_dir, self.temp_dir)
        ios_rel_path = os.path.relpath(self.ios_dir, self.temp_dir)
        self.assertTrue(
            is_high_level_owner('user_x', browser_rel_path, owners_map))
        self.assertTrue(
            is_high_level_owner('user_y', browser_rel_path, owners_map))
        self.assertFalse(
            is_high_level_owner('user_a', browser_rel_path, owners_map))
        self.assertTrue(is_high_level_owner('user_x', ios_rel_path, owners_map))
        self.assertFalse(
            is_high_level_owner('user_y', ios_rel_path, owners_map))
        self.assertFalse(
            is_high_level_owner('user_ios_dev', ios_rel_path, owners_map))

    def test_extract_commits_informations_with_owner_filter(self):
        """Tests that high-level owners are excluded from review stats."""
        owners_map = get_existing_owners(self.temp_dir)
        commits = split_log_into_commits(FAKE_GIT_LOG)
        stats = extract_commits_informations(commits,
                                             owners_map,
                                             owner_exclusion=True,
                                             quiet=True)

        # In FAKE_GIT_LOG, user_b reviews 8 commits in
        # 'ios/chrome/browser/feature'. Since user_b is an owner of
        # 'ios/chrome/browser', their reviews should be ignored.
        # The path from the commit log is relative.
        feature_path = 'ios/chrome/browser/feature'
        feature_stats = stats.get(feature_path)
        self.assertIsNotNone(feature_stats)

        # user_b should not have any review stats in this directory.
        user_b_stats = feature_stats['individual_stats'].get('user_b')
        if user_b_stats:
            self.assertEqual(user_b_stats['review_count'], 0)

        # Verify that other reviewers (who are not high-level owners) are
        # still counted.
        user_c_stats = feature_stats['individual_stats'].get('user_c')
        self.assertEqual(user_c_stats['review_count'], 1)

    def test_extract_commits_informations_without_owner_filter(self):
        """Tests that the owner exclusion can be disabled."""
        owners_map = get_existing_owners(self.temp_dir)
        commits = split_log_into_commits(FAKE_GIT_LOG)
        stats = extract_commits_informations(commits,
                                             owners_map,
                                             owner_exclusion=False,
                                             quiet=True)

        # In FAKE_GIT_LOG, user_b reviews 8 commits in
        # 'ios/chrome/browser/feature'. With exclusion disabled, their
        # reviews should be counted.
        feature_path = 'ios/chrome/browser/feature'
        feature_stats = stats.get(feature_path)
        self.assertIsNotNone(feature_stats)

        # user_b's reviews should now be counted.
        user_b_stats = feature_stats['individual_stats'].get('user_b')
        self.assertIsNotNone(user_b_stats)
        self.assertEqual(user_b_stats['review_count'], 7)

        # Verify that other reviewers are still counted correctly.
        user_c_stats = feature_stats['individual_stats'].get('user_c')
        self.assertEqual(user_c_stats['review_count'], 1)

if __name__ == '__main__':
    unittest.main()