# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.system.executive import ScriptError

from blinkpy.w3c.common import is_file_exportable


class ChromiumCommit(object):
    def __init__(self, host, sha=None, position=None):
        """Initializes a ChomiumCommit object, given a sha or commit position.

        Args:
            host: A Host object.
            sha: A Chromium commit SHA hash.
            position: A commit position footer string of the form:
                    'Cr-Commit-Position: refs/heads/main@{#431915}'
                or just the commit position string:
                    'refs/heads/main@{#431915}'
        """
        self.host = host
        self.project_config = host.project_config

        self.project_root = self.project_config.project_root
        self.test_root = self.project_config.test_root

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

    def num_behind_main(self):
        """Returns the number of commits this commit is behind origin/main.

        It is inclusive of this commit and of the latest commit.
        """
        return len(
            self.host.executive.run_command([
                'git', 'rev-list',
                f'{self.sha}..origin/{self.host.project_config.gerrit_branch}'
            ],
                                            cwd=self.project_root).splitlines(
                                            ))

    def position_to_sha(self, commit_position):
        return self.host.executive.run_command(
            ['git', 'crrev-parse', commit_position],
            cwd=self.project_root).strip()

    def sha_to_position(self, sha):
        try:
            return self.host.executive.run_command(
                ['git', 'footers', '--position', sha],
                cwd=self.project_root).strip()
        except ScriptError as e:
            # Commits from Gerrit CLs that have not yet been committed in
            # Chromium do not have a commit position.
            if 'Unable to infer commit position from footers' in e.message:
                return 'no-commit-position-yet'
            else:
                raise

    def subject(self):
        return self.host.executive.run_command(
            ['git', 'show', '--format=%s', '--no-patch', self.sha],
            cwd=self.project_root).strip()

    def body(self):
        return self.host.executive.run_command(
            ['git', 'show', '--format=%b', '--no-patch', self.sha],
            cwd=self.project_root)

    def author(self):
        return self.host.executive.run_command(
            ['git', 'show', '--format=%aN <%aE>', '--no-patch', self.sha],
            cwd=self.project_root).strip()

    def message(self):
        """Returns a string with a commit's subject and body."""
        return self.host.executive.run_command(
            ['git', 'show', '--format=%B', '--no-patch', self.sha],
            cwd=self.project_root)

    def change_id(self):
        """Returns the Change-Id footer if it is present."""
        return self.host.executive.run_command(
            ['git', 'footers', '--key', 'Change-Id', self.sha],
            cwd=self.project_root).strip()

    def filtered_changed_files(self):
        """Returns a list of modified exportable files."""
        changed_files = self.host.executive.run_command(
            [
                'git', 'diff-tree', '--name-only', '--no-commit-id', '-r',
                self.sha, '--', self.test_root
            ],
            cwd=self.project_root).splitlines()
        return [f for f in changed_files if is_file_exportable(f, self.project_config)]

    def format_patch(self):
        """Makes a patch with only exportable changes."""
        filtered_files = self.filtered_changed_files()

        if not filtered_files:
            return ''

        return self.host.executive.run_command(
            ['git', 'format-patch', '-1', '--stdout', self.sha, '--'] +
            filtered_files,
            cwd=self.project_root)

    def url(self):
        """Returns a URL to view more information about this commit."""
        return f'https://chromium.googlesource.com/{self.project_config.gerrit_project}/+/{self.short_sha}'
