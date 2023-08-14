# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from blinkpy.common.system.executive import Executive, ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.checkout.git import Git


# These tests could likely be run on Windows if we first used Git.find_executable_name.
@unittest.skipIf(sys.platform == 'win32', 'fails on Windows')
class GitTestWithRealFilesystemAndExecutive(unittest.TestCase):
    def setUp(self):
        self.executive = Executive()
        self.filesystem = FileSystem()

        self.original_cwd = self.filesystem.getcwd()

        # Set up fresh git repository with one commit.
        self.untracking_checkout_path = self._mkdtemp(
            suffix='-git_unittest_untracking')
        try:
            self._run(['git', 'init', self.untracking_checkout_path])
        except ScriptError:
            # Skip the test if git is not installed on the system
            raise self.skipTest("git init failed. Skipping the test")

        self._chdir(self.untracking_checkout_path)
        # Explicitly create the default branch instead of relying on
        # init.defaultBranch. We don't use the new --initial-branch flag with
        # `git init` to keep the tests compatible with older versions of git.
        self._run(['git', 'checkout', '-b', 'main'])
        self._set_user_config()
        self._write_text_file('foo_file', 'foo')
        self._run(['git', 'add', 'foo_file'])
        self._run(['git', 'commit', '-am', 'dummy commit'])
        self.untracking_git = Git(cwd=self.untracking_checkout_path,
                                  filesystem=self.filesystem,
                                  executive=self.executive)

        # Then set up a second git repo that tracks the first one.
        self.tracking_git_checkout_path = self._mkdtemp(
            suffix='-git_unittest_tracking')
        self._run([
            'git', 'clone', '--quiet', self.untracking_checkout_path,
            self.tracking_git_checkout_path
        ])
        self._chdir(self.tracking_git_checkout_path)
        self._set_user_config()
        self.tracking_git = Git(cwd=self.tracking_git_checkout_path,
                                filesystem=self.filesystem,
                                executive=self.executive)

    def tearDown(self):
        self._chdir(self.original_cwd)
        self._run(['rm', '-rf', self.tracking_git_checkout_path])
        self._run(['rm', '-rf', self.untracking_checkout_path])

    def _set_user_config(self):
        self._run(['git', 'config', '--local', 'user.name', 'Fake'])
        self._run(
            ['git', 'config', '--local', 'user.email', 'fake@example.com'])

    def _chdir(self, path):
        self.filesystem.chdir(path)

    def _mkdir(self, path):
        assert not self.filesystem.exists(path)
        self.filesystem.maybe_make_directory(path)

    def _mkdtemp(self, **kwargs):
        return str(self.filesystem.mkdtemp(**kwargs))

    def _run(self, *args, **kwargs):
        return self.executive.run_command(*args, **kwargs)

    def _write_text_file(self, path, contents):
        self.filesystem.write_text_file(path, contents)

    def test_add_list(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._run(['ls', 'added_dir'])
        self._run(['pwd'])
        self._run(['cat', 'added_dir/added_file'])
        git.add_list(['added_dir/added_file'])
        self.assertIn('added_dir/added_file', git.added_files())

    def test_delete_recursively(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        git.add_list(['added_dir/added_file'])
        self.assertIn('added_dir/added_file', git.added_files())
        git.delete_list(['added_dir/added_file'])
        self.assertNotIn('added_dir', git.added_files())

    def test_delete_recursively_or_not(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._write_text_file('added_dir/another_added_file', 'more new stuff')
        git.add_list(['added_dir/added_file', 'added_dir/another_added_file'])
        self.assertIn('added_dir/added_file', git.added_files())
        self.assertIn('added_dir/another_added_file', git.added_files())
        git.delete_list(['added_dir/added_file'])
        self.assertIn('added_dir/another_added_file', git.added_files())

    def test_exists(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._chdir(git.checkout_root)
        self.assertFalse(git.exists('foo.txt'))
        self._write_text_file('foo.txt', 'some stuff')
        self.assertFalse(git.exists('foo.txt'))
        git.add_list(['foo.txt'])
        git.commit_locally_with_message('adding foo')
        self.assertTrue(git.exists('foo.txt'))
        git.delete_list(['foo.txt'])
        git.commit_locally_with_message('deleting foo')
        self.assertFalse(git.exists('foo.txt'))

    def test_show_blob(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._chdir(git.checkout_root)
        self.filesystem.write_binary_file('foo.txt',
                                          b'some stuff, possibly binary \xff')
        git.add_list(['foo.txt'])
        git.commit_locally_with_message('adding foo')
        self.assertEqual(git.show_blob('foo.txt', ref='HEAD'),
                         b'some stuff, possibly binary \xff')

    def test_move(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._write_text_file('added_file', 'new stuff')
        git.add_list(['added_file'])
        git.move('added_file', 'moved_file')
        self.assertIn('moved_file', git.added_files())

    def test_move_recursive(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_git
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._write_text_file('added_dir/another_added_file', 'more new stuff')
        git.add_list(['added_dir'])
        git.move('added_dir', 'moved_dir')
        self.assertIn('moved_dir/added_file', git.added_files())
        self.assertIn('moved_dir/another_added_file', git.added_files())

    def test_remote_branch_ref(self):
        # This tests a protected method. pylint: disable=protected-access
        self.assertEqual(self.tracking_git._remote_branch_ref(),
                         'refs/remotes/origin/main')
        self._chdir(self.untracking_checkout_path)
        self.assertRaises(ScriptError, self.untracking_git._remote_branch_ref)

    def test_create_patch(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_git
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        git.commit_locally_with_message('message')
        patch = git.create_patch()
        self.assertNotRegexpMatches(patch, b'Subversion Revision:')

    def test_patches_have_filenames_with_prefixes(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_git
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        git.commit_locally_with_message('message')

        # Even if diff.noprefix is enabled, create_patch() produces diffs with prefixes.
        self._run(['git', 'config', 'diff.noprefix', 'true'])
        patch = git.create_patch()
        self.assertRegexpMatches(
            patch, b'^diff --git a/test_file_commit1 b/test_file_commit1')

    def test_rename_files(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_git
        git.move('foo_file', 'bar_file')
        git.commit_locally_with_message('message')

        patch = git.create_patch(changed_files=git.changed_files())
        self.assertTrue(b'rename from' in patch)

    def test_commit_position_from_git_log(self):
        # This tests a protected method. pylint: disable=protected-access
        git_log = """
commit 624c3081c0
Author: foobarbaz1 <foobarbaz1@chromium.org>
Date:   Mon Sep 28 19:10:30 2015 -0700

    Test foo bar baz qux 123.

    BUG=000000

    Review URL: https://codereview.chromium.org/999999999

    Cr-Commit-Position: refs/heads/main@{#1234567}
"""
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_git
        self.assertEqual(git._commit_position_from_git_log(git_log), 1234567)


class GitTestWithMock(unittest.TestCase):
    def make_git(self):
        git = Git(cwd='.',
                  executive=MockExecutive(),
                  filesystem=MockFileSystem())
        return git

    def test_unstaged_files(self):
        git = self.make_git()
        status_lines = [
            ' M d/modified.txt',
            ' D d/deleted.txt',
            '?? d/untracked.txt',
            '?? a',
            'D  d/deleted.txt',
            'M  d/modified-staged.txt',
            'A  d/added-staged.txt',
        ]
        git.run = lambda args: '\x00'.join(status_lines) + '\x00'
        self.assertEqual(
            git.unstaged_changes(), {
                'd/modified.txt': 'M',
                'd/deleted.txt': 'D',
                'd/untracked.txt': '?',
                'a': '?',
            })

    def test_uncommitted_changes(self):
        git = self.make_git()
        status_lines = [
            ' M d/modified.txt',
            ' D d/deleted.txt',
            '?? d/untracked.txt',
            '?? a',
            'D  d/deleted.txt',
            'M  d/modified-staged.txt',
            'A  d/added-staged.txt',
            'AM d/added-then-modified.txt',
            'MM d/modified-then-modified.txt',
        ]
        git.run = lambda args: '\x00'.join(status_lines) + '\x00'
        self.assertEqual(git.uncommitted_changes(), [
            'd/modified.txt',
            'd/deleted.txt',
            'd/untracked.txt',
            'a',
            'd/deleted.txt',
            'd/modified-staged.txt',
            'd/added-staged.txt',
            'd/added-then-modified.txt',
            'd/modified-then-modified.txt',
        ])
