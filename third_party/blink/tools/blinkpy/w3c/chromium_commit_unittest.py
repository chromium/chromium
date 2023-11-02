# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WPT_TESTS
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive, mock_git_commands
from blinkpy.w3c.chromium_commit import ChromiumCommit


class ChromiumCommitTest(unittest.TestCase):
    def test_validates_sha(self):
        with self.assertRaises(AssertionError):
            ChromiumCommit(MockHost(), sha='rutabaga')

    def test_derives_sha_from_position(self):
        host = MockHost()
        host.executive = MockExecutive(
            output='c881563d734a86f7d9cd57ac509653a61c45c240')
        pos = 'Cr-Commit-Position: refs/heads/main@{#789}'
        chromium_commit = ChromiumCommit(host, position=pos)

        self.assertEqual(chromium_commit.position, 'refs/heads/main@{#789}')
        self.assertEqual(chromium_commit.sha,
                         'c881563d734a86f7d9cd57ac509653a61c45c240')

    def test_derives_position_from_sha(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'footers':
            'refs/heads/main@{#789}'
        })
        chromium_commit = ChromiumCommit(
            host, sha='c881563d734a86f7d9cd57ac509653a61c45c240')

        self.assertEqual(chromium_commit.position, 'refs/heads/main@{#789}')
        self.assertEqual(chromium_commit.sha,
                         'c881563d734a86f7d9cd57ac509653a61c45c240')

    def test_when_commit_has_no_position(self):
        host = MockHost()

        def run_command(_):
            raise ScriptError(
                'Unable to infer commit position from footers rutabaga')

        host.executive = MockExecutive(run_command_fn=run_command)
        chromium_commit = ChromiumCommit(
            host, sha='c881563d734a86f7d9cd57ac509653a61c45c240')

        self.assertEqual(chromium_commit.position, 'no-commit-position-yet')
        self.assertEqual(chromium_commit.sha,
                         'c881563d734a86f7d9cd57ac509653a61c45c240')

    def test_filtered_changed_files_skips_special_files(self):
        host = MockHost()

        fake_files = ['file1', 'MANIFEST.json', 'file3', 'OWNERS']
        qualified_fake_files = [RELATIVE_WPT_TESTS + f for f in fake_files]

        host.executive = mock_git_commands({
            'diff-tree':
            '\n'.join(qualified_fake_files),
            'crrev-parse':
            'c881563d734a86f7d9cd57ac509653a61c45c240',
        })

        position_footer = 'Cr-Commit-Position: refs/heads/main@{#789}'
        chromium_commit = ChromiumCommit(host, position=position_footer)

        files = chromium_commit.filtered_changed_files()

        expected_files = ['file1', 'file3']
        qualified_expected_files = [
            RELATIVE_WPT_TESTS + f for f in expected_files
        ]

        self.assertEqual(files, qualified_expected_files)

    def test_short_sha(self):
        chromium_commit = ChromiumCommit(
            MockHost(), sha='c881563d734a86f7d9cd57ac509653a61c45c240')
        self.assertEqual(chromium_commit.short_sha, 'c881563d73')

    def test_url(self):
        chromium_commit = ChromiumCommit(
            MockHost(), sha='c881563d734a86f7d9cd57ac509653a61c45c240')
        self.assertEqual(
            chromium_commit.url(),
            'https://chromium.googlesource.com/chromium/src/+/c881563d73')
