#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import subprocess


def split_log_into_commits(log_output: str) -> list[str]:
    """Splits a raw git log output string into a list of individual commits.

    Args:
        log_output: The raw string output from a `git log` command.

    Returns:
        A list of strings, where each string is the raw text of a single
        commit. It filters out any empty strings that result from the split.
    """
    # The log is split by 'commit', and the first element is often empty.
    commits = log_output.split('commit')
    return [commit.strip() for commit in commits if commit.strip()]


def get_commits_in_folder_in_period(path: str,
                                    date_range: tuple[datetime]) -> list[str]:
    """Executes `git log` to retrieve commit descriptions for a given path.

    Args:
        path: The directory or file path to get the log for.
        date_range: A tuple containing the start and end datetime objects.

    Returns:
        A list of strings, where each string is the raw text of a single
        commit description. Returns None if the git command fails.
    """
    try:
        begin, end = date_range
        begin = begin.strftime('%Y-%m-%d')
        end = end.strftime('%Y-%m-%d')
        print('Get commits from ' + begin + ' to ' + end)
        git_log_command = ('git log --stat=500 --after=' + begin +
                           ' --before=' + end + ' -- ' + path).split()
        log_output = subprocess.check_output(
            git_log_command,
            stderr=subprocess.STDOUT).decode("utf-8")
        return split_log_into_commits(log_output)
    except subprocess.CalledProcessError as e:
        print("Exception on process, rc=", e.returncode, "output=", e.output)
        return None


def get_blame_for_file(file: str, date_filter: datetime) -> list[str]:
    """Executes `git blame` to retrieve line-by-line author information.

    Args:
        file: The path to the file to run blame on.
        date_filter: A datetime object used as the --after filter for blame.

    Returns:
        A list of strings, where each string is a line of the `git blame`
        output. Returns None if the git command fails.
    """
    try:
        date_filter = date_filter.strftime('%Y-%m-%d')
        gitCmd_fullLog = ('git --no-pager blame -e -f --date=short --after=' +
                          date_filter + ' -- ' + file).split()
        return subprocess.check_output(
            gitCmd_fullLog,
            stderr=subprocess.STDOUT).decode('utf-8').split('\n')
    except subprocess.CalledProcessError as e:
        print("Exception on process, rc=", e.returncode, "output=", e.output)
        return None
