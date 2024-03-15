# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from typing import Mapping, NamedTuple, Optional, Union

from blinkpy.common.checkout.git import CommitRange
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem


class MockCommit(NamedTuple):
    message: str
    tree: Mapping[str, bytes]


class MockGit:

    # Arguments are listed below, even if they're unused, in order to match
    # the Git class. pylint: disable=unused-argument

    def __init__(self,
                 cwd=None,
                 filesystem=None,
                 executive=None,
                 platform=None):
        self.checkout_root = '/mock-checkout'
        self.cwd = cwd or self.checkout_root
        self.added_paths = set()
        self._filesystem = filesystem or MockFileSystem()
        self._staging = dict(self._filesystem.files)
        self._executive = executive or MockExecutive()
        self._executable_name = 'git'
        self._local_commits = []

    def run(self,
            command_args,
            cwd=None,
            stdin=None,
            decode_output=True,
            return_exit_code=False):
        full_command_args = [self._executable_name] + command_args
        cwd = cwd or self.checkout_root
        return self._executive.run_command(
            full_command_args,
            cwd=cwd,
            input=stdin,
            return_exit_code=return_exit_code,
            decode_output=decode_output)

    def add(self, destination_path, return_exit_code=False):
        self.add_list([destination_path], return_exit_code)

    def add_list(self, destination_paths, return_exit_code=False):
        for path in destination_paths:
            self._staging[path] = self._filesystem.read_binary_file(path)
        self.added_paths.update(set(destination_paths))
        if return_exit_code:
            return 0

    def has_working_directory_changes(self, pathspec=None):
        return False

    def current_branch(self):
        return 'mock-branch-name'

    def exists(self, path):
        return True

    def show_blob(self, path: str, ref: Optional[str] = None) -> bytes:
        commit = self._get_commit(ref)
        try:
            return commit.tree[self.absolute_path(path)]
        except KeyError:
            raise ScriptError

    def absolute_path(self, *comps):
        return self._filesystem.join(self.checkout_root, *comps)

    def commit_position(self, path):
        return 5678

    def commit_position_from_git_commit(self, git_commit):
        if git_commit == '6469e754a1':
            return 1234
        if git_commit == '624c3081c0':
            return 5678
        if git_commit == '624caaaaaa':
            return 10000
        return None

    def commit_locally_with_message(self, message):
        self._local_commits.append(MockCommit(message, dict(self._staging)))

    def local_commits(self):
        """Returns the internal recording of commits made via |commit_locally_with_message|.

        This is a testing convenience method; commits are formatted as:
          [ message, commit_all_working_directory_changes, author ].
        """
        return [[commit.message] for commit in self._local_commits]

    def delete(self, path):
        return self.delete_list([path])

    def delete_list(self, paths, ignore_unmatch: bool = False):
        if not self._filesystem:
            return
        for path in paths:
            self._staging.pop(path, None)
            if self._filesystem.exists(path):
                self._filesystem.remove(path)

    def move(self, origin, destination):
        if self._filesystem:
            self._filesystem.move(
                self.absolute_path(origin), self.absolute_path(destination))

    def changed_files(self,
                      commits: Union[None, str, CommitRange] = None,
                      diff_filter: str = 'ADM',
                      path: Optional[str] = None):
        if not self._local_commits:
            return []
        if isinstance(commits, CommitRange):
            files_before = self._get_commit(commits.start).tree
            files_after = self._get_commit(commits.end).tree
        else:
            # Pretend this branch is tracking the first commit.
            files_before = self._local_commits[0].tree
            files_after = self._filesystem.files

        changed_files = []
        for path in sorted(set(files_before) | set(files_after)):
            before, after = files_before.get(path), files_after.get(path)
            added = 'A' in diff_filter and before is None and after is not None
            deleted = ('D' in diff_filter and before is not None
                       and after is None)
            modified = 'M' in diff_filter and before != after
            if added or deleted or modified:
                changed_files.append(
                    self._filesystem.relpath(path, self.checkout_root))
        return changed_files

    def _get_commit(self, ref: str) -> MockCommit:
        match = re.fullmatch(r'HEAD(~(?P<back>\d+))?', ref)
        if not match:
            raise NotImplementedError(
                'only the HEAD~<n> syntax is supported for now')
        back = int(match.group('back') or 0)
        return self._local_commits[-1 - back]

    def unstaged_changes(self):
        return {}

    def uncommitted_changes(self):
        return []
