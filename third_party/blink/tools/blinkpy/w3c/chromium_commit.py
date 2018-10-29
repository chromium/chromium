# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.system.executive import ScriptError
from blinkpy.w3c.chromium_finder import absolute_chromium_dir, absolute_chromium_wpt_dir
from blinkpy.w3c.common import is_file_exportable


class ChromiumCommit(object):

    def __init__(self, host, sha=None, position=None):
        """Initializes a ChomiumCommit object, given a sha or commit position.

        Args:
            host: A Host object.
            sha: A Chromium commit SHA hash.
            position: A commit position footer string of the form:
                    'Cr-Commit-Position: refs/heads/master@{#431915}'
                or just the commit position string:
                    'refs/heads/master@{#431915}'
        """
        self.host = host
        self.absolute_chromium_dir = absolute_chromium_dir(host)
        self.absolute_chromium_wpt_dir = absolute_chromium_wpt_dir(host)

        assert sha or position, 'requires sha or position'
        assert not (sha and position), 'cannot accept both sha and position'

        if position:
            if position.startswith('Cr-Commit-Position: '):
                position = position[len('Cr-Commit-Position: '):]

            sha = self.position_to_sha(position)
        else:
            position = self.sha_to_position(sha)

        assert len(sha) == 40, 'Expected SHA-1 hash, got {}'.format(sha)
        assert sha and position, 'ChromiumCommit should have sha and position after __init__'
        self.sha = sha
        self.position = position

    def __str__(self):
        return '{} "{}"'.format(self.short_sha, self.subject())

    @property
    def short_sha(self):
        return self.sha[0:10]

    def num_behind_master(self):
        """Returns the number of commits this commit is behind origin/master.

        It is inclusive of this commit and of the latest commit.
        """
        return len(self.host.executive.run_command([
            'git', 'rev-list', '{}..origin/master'.format(self.sha)
        ], cwd=self.absolute_chromium_dir).splitlines())

    def position_to_sha(self, commit_position):
        return self.host.executive.run_command([
            'git', 'crrev-parse', commit_position
        ], cwd=self.absolute_chromium_dir).strip()

    def sha_to_position(self, sha):
        try:
            return self.host.executive.run_command([
                'git', 'footers', '--position', sha
            ], cwd=self.absolute_chromium_dir).strip()
        except ScriptError as e:
            # Commits from Gerrit CLs that have not yet been committed in
            # Chromium do not have a commit position.
            if 'Unable to infer commit position from footers' in e.message:
                return 'no-commit-position-yet'
            else:
                raise

    def subject(self):
        return self.host.executive.run_command([
            'git', 'show', '--format=%s', '--no-patch', self.sha
        ], cwd=self.absolute_chromium_dir).strip()

    def body(self):
        return self.host.executive.run_command([
            'git', 'show', '--format=%b', '--no-patch', self.sha
        ], cwd=self.absolute_chromium_dir)

    def author(self):
        return self.host.executive.run_command([
            'git', 'show', '--format=%aN <%aE>', '--no-patch', self.sha
        ], cwd=self.absolute_chromium_dir).strip()

    def message(self):
        """Returns a string with a commit's subject and body."""
        return self.host.executive.run_command([
            'git', 'show', '--format=%B', '--no-patch', self.sha
        ], cwd=self.absolute_chromium_dir)

    def change_id(self):
        """Returns the Change-Id footer if it is present."""
        return self.host.executive.run_command([
            'git', 'footers', '--key', 'Change-Id', self.sha
        ], cwd=self.absolute_chromium_dir).strip()

    def filtered_changed_files(self):
        """Returns a list of modified exportable files."""
        changed_files = self.host.executive.run_command([
            'git', 'diff-tree', '--name-only', '--no-commit-id', '-r', self.sha,
            '--', self.absolute_chromium_wpt_dir
        ], cwd=self.absolute_chromium_dir).splitlines()
        return [f for f in changed_files if is_file_exportable(f)]

    def format_patch(self):
        """Makes a patch with only exportable changes."""
        filtered_files = self.filtered_changed_files()

        if not filtered_files:
            return ''

        return self.host.executive.run_command([
            'git', 'format-patch', '-1', '--stdout', self.sha, '--'
        ] + filtered_files, cwd=self.absolute_chromium_dir)

    def url(self):
        """Returns a URL to view more information about this commit."""
        return 'https://chromium.googlesource.com/chromium/src/+/%s' % self.short_sha
