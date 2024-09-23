# Copyright (c) 2009, 2010, 2011 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import enum
import logging
import re
import os
from typing import List, Mapping, NamedTuple, Optional, Union

from blinkpy.common.memoized import memoized
from blinkpy.common.system.executive import Executive, ScriptError
from blinkpy.common.system.filesystem import FileSystem

_log = logging.getLogger(__name__)


class CommitRange(NamedTuple):
    start: str
    end: str

    def __str__(self) -> str:
        return f'{self.start}...{self.end}'


class FileStatusType(enum.Flag):
    ADD = enum.auto()
    COPY = enum.auto()
    DELETE = enum.auto()
    MODIFY = enum.auto()
    RENAME = enum.auto()

    def __str__(self) -> str:
        return ''.join(status.name[0] for status in FileStatusType
                       if status & self)

    @classmethod
    def parse_diff_filter(cls, pattern: str) -> 'FileStatusType':
        """Parse a parameter to `git diff --diff-filter` [0].

        [0]: https://git-scm.com/docs/git-diff#Documentation/git-diff.txt---diff-filterACDMRTUXB82308203
        """
        status_by_symbol = {member.name[0]: member for member in cls}
        status = FileStatusType(0)
        for symbol in pattern:
            status |= status_by_symbol[symbol]
        return status


class FileStatus(NamedTuple):
    status_type: FileStatusType
    # Source path for copied and renamed files. Ignored for other files.
    source: Optional[str] = None


class Git:
    # Unless otherwise specified, methods are expected to return paths relative
    # to self.checkout_root.

    # Git doesn't appear to document error codes, but seems to return
    # 1 or 128, mostly.
    ERROR_FILE_IS_MISSING = 128
    DEFAULT_DIFF_FILTER = (FileStatusType.ADD | FileStatusType.DELETE
                           | FileStatusType.MODIFY)

    def __init__(self,
                 cwd=None,
                 executive=None,
                 filesystem=None,
                 platform=None):
        self._executive = executive or Executive()
        self._filesystem = filesystem or FileSystem()
        self._executable_name = self.find_executable_name(
            self._executive, platform)

        self.cwd = cwd or self._filesystem.abspath(self._filesystem.getcwd())
        if not self.in_working_directory(self.cwd):
            module_directory = self._filesystem.abspath(
                self._filesystem.dirname(
                    self._filesystem.path_to_module(self.__module__)))
            _log.info(
                'The current directory (%s) is not in a git repo, trying directory %s.',
                cwd, module_directory)
            if self.in_working_directory(module_directory):
                self.cwd = module_directory
            _log.error('Failed to find Git repo for %s or %s', cwd,
                       module_directory)

        self.checkout_root = self.find_checkout_root(self.cwd)

    @staticmethod
    def find_executable_name(executive, platform):
        """Finds the git executable name which may be different on Windows.

        The Win port uses the depot_tools package, which contains a number
        of development tools, including Python and git. Instead of using a
        real git executable, depot_tools indirects via a batch file, called
        "git.bat". This batch file is used because it allows depot_tools to
        auto-update the real git executable, which is contained in a
        subdirectory.

        FIXME: This is a hack and should be resolved in a different way if
        possible.
        """
        if not platform or not platform.is_win():
            return 'git'
        try:
            executive.run_command(['git', 'help'], debug_logging=False)
            return 'git'
        except OSError:
            _log.debug('Using "git.bat" as git executable.')
            return 'git.bat'

    def run(self,
            command_args,
            cwd=None,
            stdin=None,
            decode_output=True,
            return_exit_code=False):
        """Invokes git with the given args."""
        full_command_args = [self._executable_name] + command_args
        cwd = cwd or self.checkout_root
        return self._executive.run_command(full_command_args,
                                           cwd=cwd,
                                           input=stdin,
                                           return_exit_code=return_exit_code,
                                           decode_output=decode_output,
                                           debug_logging=False)

    def absolute_path(self, repository_relative_path):
        """Converts repository-relative paths to absolute paths."""
        return self._filesystem.join(self.checkout_root,
                                     repository_relative_path)

    def in_working_directory(self, path):
        return self._executive.run_command(
            [self._executable_name, 'rev-parse', '--is-inside-work-tree'],
            cwd=path,
            error_handler=Executive.ignore_error,
            debug_logging=False).rstrip() == 'true'

    def find_checkout_root(self, path):
        """Returns the absolute path to the root of the repository."""
        if os.getcwd().startswith('/google/cog/cloud'):
            return os.getcwd()
        return self.run(['rev-parse', '--show-toplevel'], cwd=path).strip()

    @classmethod
    def read_git_config(cls, key, cwd=None, executive=None):
        # FIXME: This should probably use cwd=self.checkout_root.
        # Pass --get-all for cases where the config has multiple values
        # Pass the cwd if provided so that we can handle the case of running
        # blink_tool.py outside of the working directory.
        # FIXME: This should use an Executive.
        executive = executive or Executive()
        return executive.run_command(
            [cls.executable_name, 'config', '--get-all', key],
            error_handler=Executive.ignore_error,
            cwd=cwd).rstrip('\n')

    def has_working_directory_changes(self, pathspec=None):
        """Checks whether there are uncommitted changes."""
        command = ['diff', 'HEAD', '--no-renames', '--name-only']
        if pathspec:
            command.extend(['--', pathspec])
        output = self.run(command)
        if output != '':
            _log.error('Has working directory changes:\n%s', output)
            return True
        return False

    def uncommitted_changes(self):
        """List files with uncommitted changes, including untracked files."""
        return [path for _, _, path in self._working_changes()]

    def unstaged_changes(self):
        """Lists files with unstaged changes, including untracked files.

        Returns a dict mapping modified file paths (relative to checkout root)
        to one-character codes identifying the change, e.g. 'M' for modified,
        'D' for deleted, '?' for untracked.
        """
        return {
            path: unstaged_status
            for _, unstaged_status, path in self._working_changes()
            if unstaged_status
        }

    def _working_changes(self):
        # `git status -z` is a version of `git status -s`, that's recommended
        # for machine parsing. Lines are terminated with NUL rather than LF.
        change_lines = self.run(
            ['status', '-z', '--no-renames',
             '--untracked-files=all']).rstrip('\x00')
        if not change_lines:
            return
        for line in change_lines.split('\x00'):
            assert len(line) >= 4, 'Unexpected change line format %s' % line
            path = line[3:]
            yield line[0].strip(), line[1].strip(), path

    def add_list(self, paths: List[str], return_exit_code: bool = False):
        return self._run_chunked(['add'],
                                 paths,
                                 return_exit_code=return_exit_code)

    def delete_list(self, paths: List[str], ignore_unmatch: bool = False):
        command = ['rm', '-f']
        if ignore_unmatch:
            command.append('--ignore-unmatch')
        return self._run_chunked(command, paths)

    def _run_chunked(self,
                     command: List[str],
                     paths: List[str],
                     chunk_size: int = 128,
                     **run_kwargs):
        """Safely run `git` operations on an arbitrary number of paths.

        This helper transparently avoids command line length limitations on
        Windows by splitting paths across multiple `git` invocations. This only
        works for commands that can operate on a variable number of paths.

        Arguments:
            command: The non-path arguments after `git` but before the paths.
            paths: The paths to operate on.
            chunk_size: The maximum number of paths to operate on at a time. The
                default was picked heuristically.

        Returns:
            The first truthy value returned by a `run` command. This is usually
            stdout or a nonzero exit code.
        """
        rv = 0
        for chunk_start in range(0, len(paths), chunk_size):
            chunk = paths[chunk_start:chunk_start + chunk_size]
            rv = rv or self.run(command + chunk, **run_kwargs)
        return rv

    def move(self, origin, destination):
        return self.run(['mv', '-f', origin, destination])

    def exists(self, path: str) -> bool:
        try:
            self.show_blob(path, ref='HEAD')
        except ScriptError as error:
            return error.exit_code != self.ERROR_FILE_IS_MISSING
        return True

    def show_blob(self, path: str, ref: Optional[str] = None) -> bytes:
        ref = ref or self._merge_base()
        return self.run(['show', f'{ref}:{path}'], decode_output=False)

    def _branch_from_ref(self, ref):
        return ref.replace('refs/heads/', '')

    def current_branch(self):
        """Returns the name of the current branch, or empty string if HEAD is detached."""
        ref = self.run(['rev-parse', '--symbolic-full-name', 'HEAD']).strip()
        if ref == 'HEAD':
            # HEAD is detached; return an empty string.
            return ''
        return self._branch_from_ref(ref)

    def current_revision(self):
        """Return the commit hash of HEAD."""
        return self.run(['rev-parse', 'HEAD']).strip()

    def new_branch(self, name: str, stack: bool = True):
        """Create and switch to a new branch.

        Arguments:
            stack: If true, track the current branch (if it exists). Otherwise,
                track tip-of-tree (origin/main).
        """
        if stack and self.current_branch():
            self.run(['new-branch', '--upstream-current', name])
        else:
            self.run(['new-branch', name])

    def _upstream_branch(self):
        current_branch = self.current_branch()
        return self._branch_from_ref(
            self.read_git_config('branch.%s.merge' % current_branch,
                                 cwd=self.checkout_root,
                                 executive=self._executive).strip())

    def _merge_base(self, git_commit=None):
        if git_commit:
            # Rewrite UPSTREAM to the upstream branch
            if 'UPSTREAM' in git_commit:
                upstream = self._upstream_branch()
                if not upstream:
                    raise ScriptError(
                        message='No upstream/tracking branch set.')
                git_commit = git_commit.replace('UPSTREAM', upstream)

            # Special-case <refname>.. to include working copy changes, e.g., 'HEAD....' shows only the diffs from HEAD.
            if git_commit.endswith('....'):
                return git_commit[:-4]

            if '..' not in git_commit:
                git_commit = git_commit + '^..' + git_commit
            return git_commit

        return self._remote_merge_base()

    def changed_files(
        self,
        commits: Union[None, str, CommitRange] = None,
        diff_filter: Union[str, FileStatusType] = DEFAULT_DIFF_FILTER,
        path: Optional[str] = None,
        rename_threshold: Optional[float] = None,
    ) -> Mapping[str, FileStatus]:
        if isinstance(commits, CommitRange):
            commit_arg = str(commits)
        else:
            commit_arg = self._merge_base(commits)
        status_command = [
            'diff',
            '-r',
            '-z',
            '--name-status',
            '--no-ext-diff',
            '--full-index',
            f'--diff-filter={diff_filter}',
            commit_arg,
        ]
        if rename_threshold is None:
            status_command.append('--no-renames')
        else:
            status_command.append(f'--find-renames={100 * rename_threshold}%')
        if path:
            status_command.append(path)

        file_statuses = {}
        raw_output = self.run(status_command)
        if not raw_output:
            return file_statuses
        values = iter(raw_output.rstrip('\x00').split('\x00'))
        while (status_type := next(values, None)) is not None:
            status_type = FileStatusType.parse_diff_filter(status_type[0])
            affected_file = next(values)
            if status_type in FileStatusType.COPY | FileStatusType.RENAME:
                file_statuses[next(values)] = FileStatus(
                    status_type, affected_file)
            else:
                file_statuses[affected_file] = FileStatus(status_type)
        return file_statuses

    def added_files(self):
        return self._run_status_and_extract_filenames(self.status_command(),
                                                      self._status_regexp('A'))

    def deleted_files(self):
        return self._run_status_and_extract_filenames(self.status_command(),
                                                      self._status_regexp('D'))

    def _run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        # We run with cwd=self.checkout_root so that returned-paths are root-relative.
        for line in self.run(status_command,
                             cwd=self.checkout_root).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    def status_command(self):
        # git status returns non-zero when there are changes, so we use git diff name --name-status HEAD instead.
        # No file contents printed, thus utf-8 autodecoding in self.run is fine.
        return ['diff', '--name-status', '--no-renames', 'HEAD']

    def _status_regexp(self, expected_types):
        return '^(?P<status>[%s])\t(?P<filename>.+)$' % expected_types

    def display_name(self):
        return 'git'

    def most_recent_log_matching(self,
                                 grep_str: str,
                                 path: Optional[str] = None,
                                 commits: Union[None, str, CommitRange] = None,
                                 format_pattern: Optional[str] = None) -> str:
        """Find and return the most recent commit message matching a pattern.

        Arguments:
            grep_str: A grep-style regular expression.
            path: A path that matching commits should modify.
            commits: A revision range to search, where:
              * `None` searches the full history up to `HEAD` (inclusive).
              * `str` searches the history up to that revision (inclusive).
              * `CommitRange` searches between the explicit start (exclusive)
                and end (inclusive) revisions.
            format_pattern: How `git log` should format the message, if found.
        """
        # We use '--grep=' + foo rather than '--grep', foo because
        # git 1.7.0.4 (and earlier) didn't support the separate arg.
        command = [
            'log',
            '-1',
            f'--grep={grep_str}',
            '--date=iso',
        ]
        if format_pattern:
            command.append(f'--format={format_pattern}')
        if commits:
            command.append(str(commits))
        if path:
            command.extend(['--', path])
        return self.run(command)

    def _commit_position_from_git_log(self, git_log):
        match = re.search(
            r"^\s*Cr-Commit-Position:.*@\{#(?P<commit_position>\d+)\}",
            git_log, re.MULTILINE)
        if not match:
            return ''
        return int(match.group('commit_position'))

    def commit_position(self, path):
        """Returns the latest chromium commit position found in the checkout."""
        git_log = self.most_recent_log_matching('Cr-Commit-Position:', path)
        return self._commit_position_from_git_log(git_log)

    def create_patch(self, git_commit=None, changed_files=None):
        """Returns a byte array (str) representing the patch file.

        Patch files are effectively binary since they may contain
        files of multiple different encodings.
        """
        command = [
            'diff',
            '--binary',
            '--no-color',
            '--no-ext-diff',
            '--full-index',
            '-M',
            '--src-prefix=a/',
            '--dst-prefix=b/',
        ]
        command += [self._merge_base(git_commit), '--']
        if changed_files:
            command += changed_files
        return self.run(command, decode_output=False, cwd=self.checkout_root)

    @memoized
    def commit_position_from_git_commit(self, git_commit):
        git_log = self.git_commit_detail(git_commit)
        return self._commit_position_from_git_log(git_log)

    def _branch_ref_exists(self, branch_ref):
        return self.run(['show-ref', '--quiet', '--verify', branch_ref],
                        return_exit_code=True) == 0

    def _remote_merge_base(self):
        return self.run(['merge-base',
                         self._remote_branch_ref(), 'HEAD']).strip()

    def _remote_branch_ref(self):
        # Use references so that we can avoid collisions, e.g. we don't want to operate on refs/heads/trunk if it exists.
        remote_main_ref = 'refs/remotes/origin/main'
        if self._branch_ref_exists(remote_main_ref):
            return remote_main_ref
        error_msg = "Can't find a branch to diff against. %s does not exist" % remote_main_ref
        raise ScriptError(message=error_msg)

    def commit_locally_with_message(self, message):
        command = ['commit', '--all', '-F', '-']
        self.run(command, stdin=message)

    def latest_git_commit(self):
        return self.run(['log', '-1', '--format=%H']).strip()

    def git_commits_since(self, commit):
        return self.run(
            ['log', commit + '..master', '--format=%H', '--reverse']).split()

    def git_commit_detail(self, commit, format=None):  # pylint: disable=redefined-builtin
        args = ['log', '-1', commit]
        if format:
            args.append('--format=' + format)
        return self.run(args)
