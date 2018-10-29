# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive, mock_git_commands
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c.common import DEFAULT_WPT_COMMITTER_EMAIL, DEFAULT_WPT_COMMITTER_NAME
from blinkpy.w3c.local_wpt import LocalWPT


class LocalWPTTest(unittest.TestCase):

    def test_fetch_when_wpt_dir_exists(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/tmp/wpt': ''
        })

        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'fetch', 'origin'],
            ['git', 'reset', '--hard', 'origin/master'],
        ])

    def test_fetch_when_wpt_dir_does_not_exist(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/web-platform-tests/wpt.git', '/tmp/wpt'],
            ['git', 'config', 'user.name', DEFAULT_WPT_COMMITTER_NAME],
            ['git', 'config', 'user.email', DEFAULT_WPT_COMMITTER_EMAIL],
        ])

    def test_constructor(self):
        host = MockHost()
        LocalWPT(host, 'token')
        self.assertEqual(len(host.executive.calls), 0)

    def test_run(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.run(['echo', 'rutabaga'])
        self.assertEqual(host.executive.calls, [['echo', 'rutabaga']])

    def test_create_branch_with_patch(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        local_wpt.create_branch_with_patch('chromium-export-decafbad', 'message', 'patch', 'author <author@author.com>')
        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/web-platform-tests/wpt.git', '/tmp/wpt'],
            ['git', 'config', 'user.name', DEFAULT_WPT_COMMITTER_NAME],
            ['git', 'config', 'user.email', DEFAULT_WPT_COMMITTER_EMAIL],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-D', 'chromium-export-decafbad'],
            ['git', 'checkout', '-b', 'chromium-export-decafbad'],
            ['git', 'apply', '-'],
            ['git', 'add', '.'],
            ['git', 'commit', '--author', 'author <author@author.com>', '-am', 'message'],
            ['git', 'push', 'origin', 'chromium-export-decafbad']])

    def test_test_patch_success(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'apply': '',
            'add': '',
            'diff': 'non-trivial patch',
            'reset': '',
            'clean': '',
            'checkout': '',
        }, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (True, ''))

    def test_test_patch_empty_diff(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'apply': '',
            'add': '',
            'diff': '',
            'reset': '',
            'clean': '',
            'checkout': '',
        }, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (False, ''))

    def test_test_patch_error(self):
        def _run_fn(args):
            if args[0] == 'git' and args[1] == 'apply':
                raise ScriptError('MOCK failed applying patch')
            return ''

        host = MockHost()
        host.executive = MockExecutive(run_command_fn=_run_fn)
        local_wpt = LocalWPT(host, 'token')

        self.assertEqual(local_wpt.test_patch('dummy patch'), (False, 'MOCK failed applying patch'))

    def test_commits_in_range(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'rev-list': '34ab6c3f5aee8bf05207b674edbcb6affb179545 Fix presubmit errors\n'
                        '8c596b820634a623dfd7a2b0f36007ce2f7a0c9f test\n'
        }, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertTrue(local_wpt.commits_in_range('HEAD~2', 'HEAD'))
        self.assertEqual(host.executive.calls, [['git', 'rev-list', '--pretty=oneline', 'HEAD~2..HEAD']])

    def test_is_commit_affecting_directory(self):
        host = MockHost()
        # return_exit_code=True is passed to run() in the method under test,
        # so the mock return value should be exit code instead of output.
        host.executive = mock_git_commands({'diff-tree': 1}, strict=True)
        local_wpt = LocalWPT(host, 'token')

        self.assertTrue(local_wpt.is_commit_affecting_directory('HEAD', 'css/'))
        self.assertEqual(host.executive.calls, [['git', 'diff-tree', '--quiet', '--no-commit-id', 'HEAD', '--', 'css/']])

    def test_seek_change_id(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')

        local_wpt.seek_change_id('Ifake-change-id')
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--grep', '^Change-Id: Ifake-change-id']])

    def test_seek_commit_position(self):
        host = MockHost()
        local_wpt = LocalWPT(host, 'token')

        local_wpt.seek_commit_position('refs/heads/master@{12345}')
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--grep', '^Cr-Commit-Position: refs/heads/master@{12345}']])
