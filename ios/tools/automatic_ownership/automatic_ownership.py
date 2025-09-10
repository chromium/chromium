#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Automatic Ownership Calculator.

This script analyzes the git history of a specified directory (by default,
'ios/') to automatically determine potential code owners for each
sub-directory.

Usage:
    python3 automatic_ownership.py [path]

    [path]: Optional. The root directory to start the analysis from.
            Defaults to 'ios'.

The script works in two main phases:
1. Data Collection: It fetches the last two years of git history and git blame
   information for the specified path. This is done in parallel for efficiency.
2. Analysis: It walks through each subdirectory and applies one of two
   algorithms to determine ownership:
    a) Z-Score Analysis: For directories with a rich commit history (more than
       5 commits), it calculates a weighted score for each author/reviewer and
       identifies statistical outliers as owners.
    b) Git Blame Fallback: For directories with sparse history, it falls back
       to analyzing `git blame` output to find the authors who have written the
       most lines of code.

The final output is a CSV file named `final_algo.csv` containing the suggested
owners for each directory.
"""

import datetime
import json
import math
import os
import sys

from concurrent.futures import ProcessPoolExecutor, as_completed
from commit import Commit
from filters import avoid_directory, avoid_file, avoid_username
from gitutils import get_commits_in_folder_in_period, get_blame_for_file

MONTH = 30
YEAR = 12 * MONTH
TWO_YEARS = 2 * YEAR


def get_dates_range() -> list[tuple[datetime.date, datetime.date]]:
    """Generates a list of monthly date ranges spanning the last two years.

    Returns:
        A list of tuples, where each tuple contains the start and end date
        for a one-month period.
    """
    dates_range = []
    date_start = datetime.date.today()
    date_end = date_start - datetime.timedelta(TWO_YEARS)
    while date_start > date_end:
        end = date_start
        begin = date_start - datetime.timedelta(MONTH)
        date_start = begin
        dates_range.append((begin, end))
    return dates_range


def progress_indicator(future) -> None:
    """Simple progress indicator callback function for multi-process calls."""
    print('.', end='', flush=True)


def get_all_commits_of_folder(path: str) -> list[str]:
    """Retrieves all raw commit logs for a folder over the last two years.

    This function parallelizes the git log calls by splitting the time period
    into monthly chunks.

    Args:
        path: The directory to retrieve commit logs for.

    Returns:
        A list of raw commit description strings.
    """
    commits = []
    executor = ProcessPoolExecutor()
    # Dispatch tasks into the process pool and create a list of futures.
    futures = [
        executor.submit(get_commits_in_folder_in_period, path, dates)
        for dates in get_dates_range()
    ]
    # Register the progress indicator callback.
    for future in futures:
        future.add_done_callback(progress_indicator)
    # Iterate over all submitted tasks and get results as they are available.
    for future in as_completed(futures):
        # Get the result for the next completed task.
        result = future.result()  # blocks
        if result:
            commits += result
    # Shutdown the process pool.
    executor.shutdown(wait=True)  # blocks
    return commits


def extract_commits_informations(commits: list[str]) -> dict:
    """Parses raw commit logs and aggregates statistics by folder.

    Args:
        commits: A list of raw commit description strings.

    Returns:
        A dictionary where keys are folder paths and values are dictionaries
        containing aggregated commit/review stats for that folder.
    """
    allStatsPerFolder = {}
    commit_to_analyse_count = len(commits)
    print('Getting logs done. Total number of commits to analyse: ',
          str(commit_to_analyse_count))

    for commit_description in commits:
        commit_to_analyse_count -= 1
        analysed_commit = Commit(commit_description)
        (author, reviewers, changes, path, date,
         commit_hash) = analysed_commit.all_informations()
        if len(changes) == 0 or not path:
            continue
        print(('Save commit ' + commit_hash + ' from ' + author + ' in\t' +
               path),
              end='',
              flush=True)
        if not path in allStatsPerFolder:
            allStatsPerFolder[path] = dict(total_commit=0,
                                           total_review=0,
                                           individual_stats={},
                                           final_score={},
                                           last_update=datetime.datetime.min)
        if not author in allStatsPerFolder[path]['individual_stats']:
            allStatsPerFolder[path]['individual_stats'][author] = dict(
                commit_count=0, review_count=0)

        allStatsPerFolder[path]['last_update'] = max(
            allStatsPerFolder[path]['last_update'], date)
        allStatsPerFolder[path]['total_commit'] += 1
        allStatsPerFolder[path]['total_review'] += len(reviewers)
        allStatsPerFolder[path]['individual_stats'][author][
            'commit_count'] += 1
        for reviewer in reviewers:
            if not reviewer in allStatsPerFolder[path]['individual_stats']:
                allStatsPerFolder[path]['individual_stats'][reviewer] = dict(
                    commit_count=0, review_count=0)
            allStatsPerFolder[path]['individual_stats'][reviewer][
                'review_count'] += 1
        print('\t\t\tDONE, commits left: ',
              commit_to_analyse_count,
              flush=True)
    return allStatsPerFolder


def get_all_git_blame_informations_for_folder(
        file_paths: list[str], date_filter: datetime) -> list[str]:
    """Retrieves all `git blame` output for a list of files in parallel.

    Args:
        file_paths: A list of file paths to run `git blame` on.
        date_filter: The date to use for the `--after` flag in git blame.

    Returns:
        A list of strings, where each string is one line of blame output.
    """
    lines = []
    print('[Git blame] ' + os.path.dirname(file_paths[0]), end='', flush=True)
    executor = ProcessPoolExecutor(max_workers=6)
    # Dispatch tasks into the process pool and create a list of futures.
    futures = [
        executor.submit(get_blame_for_file, file, date_filter)
        for file in file_paths
    ]
    # Register the progress indicator callback.
    for future in futures:
        future.add_done_callback(progress_indicator)
    # Iterate over all submitted tasks and get results as they are available.
    for future in as_completed(futures):
        # Get the result for the next completed task.
        result = future.result()  # blocks
        if result:
            lines += result
    # Shutdown the process pool.
    executor.shutdown(wait=True)  # blocks
    return lines


def extract_blame_informations(lines: list[str]) -> tuple[dict, int]:
    """Parses raw `git blame` output to count lines per author.

    Args:
        lines: A list of strings from the output of `git blame`.

    Returns:
        A tuple containing:
        - A dictionary mapping usernames to their line counts.
        - The total number of lines analyzed.
    """
    stats = {}
    linesCount = 0
    for line in lines:
        info = line.split()
        if info:
            if len(info) > 5:
                change = info[5]
                # Skip comment lines.
                if change.startswith('#') or change.startswith('//'):
                    continue

                username = info[2]
                username = username.split('@')[0][2:]

                if avoid_username(username):
                    continue

                if not username in stats:
                    stats[username] = 0
                stats[username] += 1
                linesCount += 1
    return stats, linesCount


def determine_owners_from_git_blame_informations(
        stats: dict, lines_count: int) -> list[str]:
    """Determines owners from aggregated blame stats.

    An author is considered an owner if they have written more than 10% of the
    lines in the analyzed files.

    Args:
        stats: A dictionary mapping usernames to their line counts.
        lines_count: The total number of lines analyzed.

    Returns:
        A list of usernames identified as owners.
    """
    result = {}
    owners = []
    for username in stats:
        result[username] = (stats[username] * 100) / lines_count
        if result[username] > 10:
            owners.append(username)
    return owners


def determine_owners_from_git_blame(root: str, files: list[str],
                                    last_update: datetime) -> list[str]:
    """High-level function to determine owners using the git blame strategy.

    Args:
        root: The root directory of the files.
        files: A list of filenames within the root directory.
        last_update: The last update time for the directory, used for filtering.

    Returns:
        A list of usernames identified as owners.
    """
    file_paths = []
    for file in files:
        if avoid_file(file):
            continue
        file_paths.append(os.path.join(root, file))

    if not file_paths:
        return []

    date_filter = last_update - datetime.timedelta(TWO_YEARS)
    lines = get_all_git_blame_informations_for_folder(file_paths, date_filter)
    stats, lines_count = extract_blame_informations(lines)
    return determine_owners_from_git_blame_informations(stats, lines_count)


def determine_owners_from_zscore(stats: dict) -> list[str]:
    """Determines owners from commit stats using Z-Score analysis.

    This method calculates a weighted score (60% commit, 40% review) for each
    contributor in a directory. It then uses the Z-score to identify
    statistical outliers who are significantly more active than the average.

    Args:
        stats: A dictionary of commit/review statistics for a directory.

    Returns:
        A list of usernames identified as owners.
    """
    owners = []
    individual_score = {}
    threshold = 1

    total_commit_count = stats['total_commit']
    total_review_count = stats['total_review']
    individual_stats = stats['individual_stats']

    # Calculate a weighted score for each contributor.
    for username in individual_stats:
        normalized_commit_count = (
            individual_stats[username]['commit_count'] /
            total_commit_count) if total_commit_count > 0 else 0
        normalized_review_count = (
            individual_stats[username]['review_count'] /
            total_review_count) if total_review_count > 0 else 0
        individual_score[username] = (0.6 * normalized_commit_count) + (
            0.4 * normalized_review_count)

    if len(individual_score) > 0:
        # Compute Z-Score to find statistical outliers.
        data = individual_score.values()
        mean_data = sum(data) / len(data)
        std_dev = math.sqrt(
            sum([(x - mean_data)**2 for x in data]) / len(data))
        for username in individual_score:
            zscore = (individual_score[username] -
                      mean_data) / std_dev if std_dev > 0 else 0
            if zscore >= threshold:
                owners.append(username)

    return owners


if __name__ == '__main__':
    # TODO: Use argparse for options
    root_folder = 'ios'
    if len(sys.argv) > 1:
        root_folder = sys.argv[1]

    # Phase 1: Data Collection
    commits = get_all_commits_of_folder(root_folder)
    stats_per_folder = extract_commits_informations(commits)

    # Phase 2: Analysis and Ownership Calculation
    steps = len(stats_per_folder)
    step_count = 0
    for root, dirs, files in os.walk(root_folder):
        if avoid_directory(root):
            continue
        if not root in stats_per_folder:
            with open('final_algo.csv', 'a') as file:
                file.write(root + '\n')
            continue

        step_count += 1
        print(str(step_count) + '/' + str(steps) + '\t', end='', flush=True)

        # Decide which algorithm to use based on commit history.
        if stats_per_folder[root]['total_commit'] > 5:
            owners = determine_owners_from_zscore(stats_per_folder[root])
            print('[Z-Score] ' + root + '\tRESULT: ' + str(owners))
        else:
            owners = determine_owners_from_git_blame(
                root, files, stats_per_folder[root]['last_update'])
            print('[Blame] ' + root + '\tRESULT: ' + str(owners))

        # Write results to the output CSV.
        with open('final_algo.csv', 'a') as file:
            file.write(root + ', ' +
                       str(stats_per_folder[root]['last_update']))
            for owner in owners:
                file.write(',' + owner)
            file.write('\n')
