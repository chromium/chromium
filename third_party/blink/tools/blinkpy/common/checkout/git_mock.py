# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from typing import Mapping, NamedTuple, Optional, Union

from blinkpy.common.checkout.git import (
    CommitRange,
    FileStatus,
    FileStatusType,
    Git,
)
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
        self._current_branch = 'mock-branch-name'
        self.tracking_branch = 'origin/main'
        self._branch_positions = {}

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
        file_paths = []
        for path in destination_paths:
            if self._filesystem.isfile(path):
                file_paths.append(path)
            else:
                file_paths.extend(self._filesystem.files_under(path))
        for path in file_paths:
            self._staging[path] = self._filesystem.read_binary_file(path)
        self.added_paths.update(set(destination_paths))
        if return_exit_code:
            return 0

    def has_working_directory_changes(self, pathspec=None):
        if not self._local_commits:
            return False
        assert not pathspec, "fake doesn't support pathspec currently"
        return self._local_commits[-1].tree != self._filesystem.files

    def current_branch(self):
        return self._current_branch

    def new_branch(self, name: str, stack: bool = True):
        assert stack, 'fake can only support stacking currently'
        assert name not in {self._current_branch, *self._branch_positions}
        last = len(self._local_commits) - 1
        self._branch_positions[self._current_branch] = last
        self.tracking_branch = self._current_branch
        self._current_branch = name

    def exists(self, path):
        return True

    def show_blob(self, path: str, ref: Optional[str] = None) -> bytes:
        commit = self._local_commits[self._get_commit_position(ref)]
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

    def most_recent_log_matching(self,
                                 grep_str: str,
                                 path: Optional[str] = None,
                                 commits: Union[None, str, CommitRange] = None,
                                 format_pattern: Optional[str] = None) -> str:
        start, end = 0, len(self._local_commits)
        if isinstance(commits, str):
            end = self._get_commit_position(commits) + 1
        elif isinstance(commits, CommitRange):
            # Exclude the start, include the end.
            start = self._get_commit_position(commits.start) + 1
            end = self._get_commit_position(commits.end) + 1

        for position in reversed(range(start, end)):
            commit = self._local_commits[position]
            if re.search(grep_str, commit.message):
                # See https://git-scm.com/docs/pretty-formats for the complete
                # list.
                format_specifiers = {
                    # The mock SHA-1 commit hash is simply the position as hex.
                    'H': hex(position)[2:].zfill(40),
                    's': commit.message.splitlines()[0],
                }
                return re.sub(
                    '%(?P<specifier>[a-zA-Z])',
                    lambda match: format_specifiers[match['specifier']],
                    format_pattern) + '\n'
        return ''

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

    def changed_files(
        self,
        commits: Union[None, str, CommitRange] = None,
        diff_filter: Union[str, FileStatusType] = Git.DEFAULT_DIFF_FILTER,
        path: Optional[str] = None,
        rename_threshold: Optional[float] = None,
    ) -> Mapping[str, FileStatus]:
        if not self._local_commits:
            return []
        if isinstance(commits, CommitRange):
            start_pos = self._get_commit_position(commits.start)
            end_pos = self._get_commit_position(commits.end)
            files_before = self._local_commits[start_pos].tree
            files_after = self._local_commits[end_pos].tree
        else:
            # Pretend this branch is tracking the first commit.
            files_before = self._local_commits[0].tree
            files_after = self._filesystem.files

        if isinstance(diff_filter, str):
            diff_filter = FileStatusType.parse_diff_filter(diff_filter)

        changed_files = {}
        for path in sorted(set(files_before) | set(files_after)):
            before, after = files_before.get(path), files_after.get(path)
            if before == after:
                continue
            elif before is None:
                status = FileStatusType.ADD
            elif after is None:
                status = FileStatusType.DELETE
            else:
                status = FileStatusType.MODIFY
            if status & diff_filter:
                path_from_checkout_root = self._filesystem.relpath(
                    path, self.checkout_root)
                changed_files[path_from_checkout_root] = FileStatus(status)
        return changed_files

    def _get_commit_position(self, ref: str) -> int:
        if ref == '@{u}':
            return self._branch_positions[self.tracking_branch]
        match = re.fullmatch(
            r'(?P<base>HEAD|[\da-fA-F]{40})(~(?P<offset>\d+))?', ref)
        if not match:
            raise NotImplementedError(
                'only the `(HEAD|<sha1>)(~<n>)?` syntax is supported')
        if match['base'] == 'HEAD':
            base_position = len(self._local_commits) - 1
        else:
            base_position = int(match['base'], 16)
        offset_from_base = int(match['offset'] or 0)
        return base_position - offset_from_base

    def unstaged_changes(self):
        return {}

    def uncommitted_changes(self):
        return []
