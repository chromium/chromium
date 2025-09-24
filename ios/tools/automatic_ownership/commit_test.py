#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from commit import Commit
from datetime import datetime
from test_data import FAKE_GIT_LOG
from gitutils import split_log_into_commits


class CommitTest(unittest.TestCase):

    def setUp(self):
        """Sets up a new Commit object for each test."""
        # Pass an empty string for initialization, as we are testing
        # analyse_line directly.
        self.commit = Commit('')

    def test_analyse_line_author(self):
        """Tests that an author line is correctly parsed."""
        line = 'Author: Test User <test.user@chromium.org>'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.author, 'test.user')

    def test_analyse_line_reviewer(self):
        """Tests that a reviewer line is correctly parsed."""
        line = '    Reviewed-by: Reviewer One <reviewer.one@chromium.org>'
        self.commit.analyse_line(line)
        self.assertIn('reviewer.one', self.commit.reviewers)

    def test_analyse_line_multiple_reviewers(self):
        """Tests that multiple reviewer lines are correctly parsed."""
        lines = [
            '    Reviewed-by: Reviewer One <reviewer.one@chromium.org>',
            '    Reviewed-by: Reviewer Two <reviewer.two@chromium.org>'
        ]
        for line in lines:
            self.commit.analyse_line(line)
        self.assertIn('reviewer.one', self.commit.reviewers)
        self.assertIn('reviewer.two', self.commit.reviewers)
        self.assertEqual(len(self.commit.reviewers), 2)

    def test_analyse_line_date(self):
        """Tests that a date line is correctly parsed."""
        # This is a sample line from `git log` output.
        line = 'Date:   Tue Sep 2 15:30:00 2025 -0700'
        self.commit.analyse_line(line)
        expected_date = datetime(2025, 9, 2, 15, 30, 0)
        self.assertEqual(self.commit.date, expected_date)

    def test_analyse_line_file_change(self):
        """Tests that a file change line is correctly parsed."""
        line = ' ios/chrome/browser/ui/some_file.mm | 10 +++++-----'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 10)
        self.assertIn('ios/chrome/browser/ui', self.commit.files_stats)
        self.assertEqual(self.commit.files_stats['ios/chrome/browser/ui'], 10)

    def test_analyse_line_file_change_with_ellipsis(self):
        """Tests parsing a file change line with an ellipsis prefix."""
        line = ' .../browser/ui/another_file.h | 5 +++++'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 5)
        self.assertIn('.../browser/ui', self.commit.files_stats)
        self.assertEqual(self.commit.files_stats['.../browser/ui'], 5)

    def test_analyse_line_file_change_unittest_file_is_not_ignored(self):
        """Tests that file changes in unittest files are not ignored."""
        line = ' ios/chrome/browser/ui/some_file_unittest.mm | 10 +++++-----'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 10)
        self.assertIn('ios/chrome/browser/ui', self.commit.files_stats)

    def test_analyse_line_irrelevant_line(self):
        """Tests that an irrelevant line does not alter the commit data."""
        line = 'This is a commit message body.'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.author, '')
        self.assertEqual(len(self.commit.reviewers), 0)
        self.assertEqual(self.commit.date, datetime.min)
        self.assertEqual(self.commit.total_change, 0)
        self.assertEqual(len(self.commit.files_stats), 0)

    def test_analyse_line_file_change_long_extension(self):
        """Tests that a file with a long extension is parsed correctly."""
        line = ' ios/chrome/browser/ui/some_file.swift | 7 +++++--'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 7)
        self.assertIn('ios/chrome/browser/ui', self.commit.files_stats)
        self.assertEqual(self.commit.files_stats['ios/chrome/browser/ui'], 7)

    def test_analyse_line_binary_file_change_ignored(self):
        """Tests that changes to binary files are ignored."""
        line = ' ios/chrome/browser/ui/icon.png | Bin 1024 -> 2048 bytes'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 0)
        self.assertEqual(len(self.commit.files_stats), 0)

    def test_determine_modified_folder_finds_common_parent(self):
        """Tests that the nearest parent is chosen for distributed changes."""
        commit_description = """commit 12345
Author: Test User <test.user@chromium.org>
Date:   Tue Sep 2 15:30:00 2025 -0700

    A sample commit.

 ios/chrome/browser/feature1/file.mm | 10 +++++-----
 ios/chrome/browser/feature2/file.mm | 10 +++++-----
"""
        commit = Commit(commit_description)
        # The common parent 'ios/chrome/browser' has 20 changes (100%)
        # and should be selected.
        self.assertEqual(commit.modified_path, 'ios/chrome/browser')

    def test_analyse_line_file_change_with_realistic_spacing(self):
        """Tests a realistic git log line with varied spacing."""
        line = ' ios/chrome/browser/ui/file1.mm    |   10 +++++-----'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 10)
        self.assertIn('ios/chrome/browser/ui', self.commit.files_stats)

    def test_extend_paths_aggregation(self):
        """Tests that extend_paths correctly aggregates changes up the tree."""
        self.commit.files_stats = {
            'ios/feature_a/ui': 10,
            'ios/feature_a/data': 5,
            'ios/feature_b': 20
        }
        extended = self.commit.extend_paths()

        self.assertEqual(extended.get('ios/feature_a/ui'), 10)
        self.assertEqual(extended.get('ios/feature_a/data'), 5)
        self.assertEqual(extended.get('ios/feature_b'), 20)
        self.assertEqual(extended.get('ios/feature_a'), 15)
        self.assertEqual(extended.get('ios'), 35)
        self.assertIsNone(extended.get(''))  # Root should not be present.

    def test_analyse_line_path_with_test_substring_is_not_ignored(self):
        """Tests that a path with 'test' as a substring is not ignored."""
        # The old logic would incorrectly ignore this path.
        line = ' ios/chrome/browser/attestation/file.mm | 4 ++--'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 4)
        self.assertIn('ios/chrome/browser/attestation',
                      self.commit.files_stats)

    def test_analyse_line_path_in_test_dir_is_ignored(self):
        """Tests that a path in a directory named 'test' is ignored."""
        line = ' ios/chrome/browser/ui/test/file.mm | 10 +++++-----'
        self.commit.analyse_line(line)
        self.assertEqual(self.commit.total_change, 0)
        self.assertEqual(len(self.commit.files_stats), 0)

    def test_analyse_line_skipping_tests_disabled(self):
        """Tests that test paths are not ignored if skipping is disabled."""
        commit = Commit('', skip_tests=False)
        line = ' ios/chrome/browser/ui/test/file.mm | 10 +++++-----'
        commit.analyse_line(line)
        self.assertEqual(commit.total_change, 10)
        self.assertIn('ios/chrome/browser/ui/test', commit.files_stats)

    def test_analyse_line_unindented_reviewer_is_parsed(self):
        """Tests that a reviewer line with no indentation is parsed."""
        line = 'Reviewed-by: Reviewer One <reviewer.one@chromium.org>'
        self.commit.analyse_line(line)
        self.assertIn('reviewer.one', self.commit.reviewers)

    def test_commit_parsing_with_fake_git_log(self):
        """Tests the Commit class with a realistic commit message."""
        # The first commit from our test corpus.
        commits = split_log_into_commits(FAKE_GIT_LOG)
        self.assertGreater(len(commits), 0, "FAKE_GIT_LOG was not split")
        commit_text = commits[0]

        commit = Commit(commit_text)
        self.assertEqual(commit.author, 'user_a')
        self.assertIn('user_b', commit.reviewers)
        self.assertEqual(commit.total_change, 10)
        self.assertIn('ios/chrome/browser/feature', commit.files_stats)


if __name__ == '__main__':
    unittest.main()
