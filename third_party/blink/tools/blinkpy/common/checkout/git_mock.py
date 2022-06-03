# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.executive_mock import MockExecutive


class MockGit(object):

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
        self.added_paths.update(set(destination_paths))
        if return_exit_code:
            return 0

    def has_working_directory_changes(self, pathspec=None):
        return False

    def current_branch(self):
        return 'mock-branch-name'

    def exists(self, path):
        # TestRealMain.test_real_main (and several other rebaseline tests) are sensitive to this return value.
        # We should make those tests more robust, but for now we just return True always (since no test needs otherwise).
        return True

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
        self._local_commits.append([message])

    def local_commits(self):
        """Returns the internal recording of commits made via |commit_locally_with_message|.

        This is a testing convenience method; commits are formatted as:
          [ message, commit_all_working_directory_changes, author ].
        """
        return self._local_commits

    def delete(self, path):
        return self.delete_list([path])

    def delete_list(self, paths):
        if not self._filesystem:
            return
        for path in paths:
            if self._filesystem.exists(path):
                self._filesystem.remove(path)

    def move(self, origin, destination):
        if self._filesystem:
            self._filesystem.move(
                self.absolute_path(origin), self.absolute_path(destination))

    def changed_files(self, diff_filter='ADM'):
        return []

    def unstaged_changes(self):
        return {}
