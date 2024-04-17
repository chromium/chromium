# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List, Optional

from blinkpy.common.host import Host
from blinkpy.common.system.executive import ScriptError
from blinkpy.w3c.common import is_file_exportable


class ChromiumCommit:

    def __init__(self,
                 host: Host,
                 sha: Optional[str] = None,
                 position: Optional[str] = None):
        """Initializes a ChomiumCommit object, given a sha or commit position.

        Args:
            host: A Host object.
            sha: A Chromium commit SHA hash.
            position: A commit position footer string of the form:
                    'Cr-Commit-Position: refs/heads/main@{#431915}'
                or just the commit position string:
                    'refs/heads/main@{#431915}'
        """
        self.project_config = host.project_config
        self.test_root = self.project_config.test_root
        self._git = host.git(path=self.project_config.project_root)

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

    def __str__(self) -> str:
        return '{} "{}"'.format(self.short_sha, self.subject())

    @property
    def short_sha(self) -> str:
        return self.sha[0:10]

    def num_behind_main(self) -> int:
        """Returns the number of commits this commit is behind origin/main.

        It is inclusive of this commit and of the latest commit.
        """
        return len(
            self._git.run([
                'rev-list',
                f'{self.sha}..origin/{self.host.project_config.gerrit_branch}',
            ]).splitlines())

    def position_to_sha(self, commit_position: str) -> str:
        return self._git.run(['crrev-parse', commit_position]).strip()

    def sha_to_position(self, sha: str) -> str:
        try:
            return self._git.run(['footers', '--position', sha]).strip()
        except ScriptError as e:
            # Commits from Gerrit CLs that have not yet been committed in
            # Chromium do not have a commit position.
            if 'Unable to infer commit position from footers' in e.message:
                return 'no-commit-position-yet'
            else:
                raise

    def subject(self) -> str:
        return self._git.run(['show', '--format=%s', '--no-patch',
                              self.sha]).strip()

    def body(self) -> str:
        return self._git.run(['show', '--format=%b', '--no-patch', self.sha])

    def author(self) -> str:
        return self._git.run(
            ['show', '--format=%aN <%aE>', '--no-patch', self.sha]).strip()

    def message(self) -> str:
        """Returns a string with a commit's subject and body."""
        return self._git.run(['show', '--format=%B', '--no-patch', self.sha])

    def change_id(self) -> str:
        """Returns the Change-Id footer if it is present."""
        return self._git.run(['footers', '--key', 'Change-Id',
                              self.sha]).strip()

    def filtered_changed_files(self) -> List[str]:
        """Returns a list of modified exportable files."""
        changed_files = self._git.run([
            'diff-tree',
            '--name-only',
            '--no-commit-id',
            '-r',
            self.sha,
            '--',
            self.test_root,
        ]).splitlines()
        return [
            f for f in changed_files
            if is_file_exportable(f, self.project_config)
        ]

    def format_patch(self) -> str:
        """Makes a patch with only exportable changes."""
        filtered_files = self.filtered_changed_files()
        if not filtered_files:
            return ''
        # Disable rename detection, which may allow a chained CL with renames
        # to export too early (https://crbug.com/40242850#comment8).
        return self._git.run([
            'format-patch',
            '-1',
            '--no-renames',
            '--stdout',
            self.sha,
            '--',
            *filtered_files,
        ])

    def url(self) -> str:
        """Returns a URL to view more information about this commit."""
        return f'https://chromium.googlesource.com/{self.project_config.gerrit_project}/+/{self.short_sha}'
