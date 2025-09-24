#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import os
from collections import defaultdict

from datetime import datetime
from filters import avoid_commit

# This regex is for file change details that look like the following:
#  ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.mm    | 4 ++--
# or like the following in case that stat=500 is not enough:
# .../browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.mm    | 2 +
FILE_CHANGE_REGEX = r"""
    ^ \s*                        # Start of the line with one or more spaces.
    (ios|\.{3})                  # Root folder ('ios' or '...').
    \/                           # Path separator.
    (\w+\/)*                     # Zero or more directory segments.
    (\w+                         # Filename.
      (\.\w+)?                   # Optional extension.
    )
    \s* \| .* \s*                # Pipe separator, e.g., " | 10 ".
    [0-9]+                       # Number of changed lines.
    \s [+-]+                     # +/- indicators.
    $                            # End of the line.
"""


class Commit:
    """A class to represent a single Git commit.

    Parses a raw commit description string to extract key information such as
    author, reviewers, date, changed files, and the primary modified folder.
    """

    def __init__(self,
                 commit_description: str,
                 skip_tests: bool = True) -> None:
        """Initializes a Commit object from a raw commit commit_description.

        Args:
            commit_description: A string containing the full text of a Git
                commit, including hash, author, date, and file statistics.
            skip_tests: If True, changes in directories named 'test' will be
                ignored.
        """
        self.author = ''
        self.reviewers = []
        self.files_stats = defaultdict(int)
        self.date = datetime.min
        self.modified_path = ''
        self.total_change = 0
        self.hash = ''
        self.skip_tests = skip_tests
        if avoid_commit(commit_description):
            return

        lines = commit_description.split('\n')
        self.hash = lines[0]
        for line in lines[1:]:
            self.analyse_line(line)

        if self.total_change == 0:
            return
        self.determine_modified_folder()

    def all_informations(self):
        """Returns all extracted commit information.

        Returns:
            A tuple containing the author, a list of reviewers, a dictionary of
            file stats, the primary modified path, the commit date, and the
            commit hash.
        """
        return (self.author, self.reviewers, self.files_stats,
                self.modified_path, self.date, self.hash)

    def extend_paths(self) -> list[dict[str:int]]:
        """Expands file paths to include all parent directories.

        Aggregates the line changes from individual files into their parent
        directories, providing a view of changes at every level of the
        directory tree.

        Returns:
            A dictionary where keys are directory paths and values are the
            sum of line changes within that directory and its subdirectories,
            sorted by path depth.
        """
        all_paths = defaultdict(int, self.files_stats)
        for path in self.files_stats:
            dirname = os.path.dirname(path)
            while (dirname):
                all_paths[dirname] += self.files_stats[path]
                dirname = os.path.dirname(dirname)

        # Sort the dictionary by path length.
        result = dict(
            reversed(
                sorted(all_paths.items(), key=lambda x: len(x[0].split('/')))))
        return result

    def determine_modified_folder(self):
        """Identifies the primary folder modified in the commit.

        Sets the `modified_path` instance variable to the path that contains
        more than 50% of the total line changes for the commit.
        """
        extanded_files_stats = self.extend_paths()
        for file in extanded_files_stats:
            stat = extanded_files_stats[file] * 100 / self.total_change
            if stat > 50:
                self.modified_path = file
                return

    def extract_username_from_line(self, line: str) -> str:
        """Extracts a username from a commit metadata line.

        Args:
            line: A string from the commit description, e.g., "Author: ..."
                or "Reviewed-by: ...".

        Returns:
            The extracted username (the part of the email before the '@').
        """
        lineDetail = line.split()
        email = lineDetail[-1][1:-1]
        username = email.split('@')[0]
        return username

    def analyse_line(self, line: str) -> None:
        """Parses a single line of a commit description.

        Updates the instance variables (author, reviewers, date, file_stats)
        based on the content of the line. Skips lines indicating changes to
        binary files.

        Args:
            line: A single line from the commit description text.
        """
        if line.startswith('Author:'):
            self.author = self.extract_username_from_line(line)
            return
        if 'Reviewed-by:' in line:
            username = self.extract_username_from_line(line)
            self.reviewers.append(username)
            return
        if line.startswith('Date:'):
            self.date = datetime.strptime(' '.join(line.split()[1:-1]),
                                          '%a %b %d %H:%M:%S %Y')
            return
        if re.match(FILE_CHANGE_REGEX, line, re.VERBOSE):
            path = line.split()[0]
            if self.skip_tests and 'test' in path.split(os.path.sep):
                return
            change_count = int(line.split()[-2])
            self.total_change += change_count
            directory = os.path.dirname(path)
            self.files_stats[directory] += change_count
