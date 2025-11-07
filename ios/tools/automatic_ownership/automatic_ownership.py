#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Automatic Ownership Calculator.

This script analyzes pre-collected git history and OWNERS data to automatically
determine potential code owners for each sub-directory.

Usage:
    python3 automatic_ownership.py [path]

    [path]: Optional. The root directory to start the analysis from.
            Defaults to 'ios'.
"""

import argparse
import datetime
import json
import math
import os
import sys

from concurrent.futures import ProcessPoolExecutor, as_completed
from commit import Commit
from filters import avoid_directory, avoid_file, avoid_username
from gitutils import get_blame_for_file, split_log_into_commits

MONTH = 30
YEAR = 12 * MONTH
TWO_YEARS = 2 * YEAR


def is_high_level_owner(reviewer: str, path: str,
                        owners_map: dict[str, set[str]]) -> bool:
    """Checks if a reviewer is an owner of the given path or any parent path.

    Args:
        reviewer: The username of the reviewer.
        path: The file path of the change being reviewed.
        owners_map: The map of existing owners.

    Returns:
        True if the reviewer is an owner of the path or a parent directory.
    """
    current_path = path
    while current_path:
        if current_path in owners_map and reviewer in owners_map[current_path]:
            return True
        parent_path = os.path.dirname(current_path)
        if parent_path == current_path:
            break
        current_path = parent_path
    return False


def progress_indicator(future) -> None:
    """Simple progress indicator callback function for multi-process calls."""
    print('.', end='', flush=True)


def extract_commits_informations(commits: list[str],
                                 owners_map: dict[str, set[str]],
                                 owner_exclusion: bool,
                                 quiet: bool = False) -> dict:
    """Parses raw commit logs and aggregates statistics by folder.

    For each commit, its statistics are aggregated into the commit's directory
    and all of its parent directories.

    Args:
        commits: A list of raw commit description strings.
        owners_map: A dictionary mapping directories to their owners.
        owner_exclusion: If True, exclude high-level owners from review stats.
        quiet: If True, suppresses progress indicators.

    Returns:
        A dictionary where keys are folder paths and values are dictionaries
        containing aggregated commit/review stats for that folder.
    """
    all_stats_per_dir = {}
    commit_to_analyse_count = len(commits)
    if not quiet:
        print('Getting logs done. Total number of commits to analyse: ',
              str(commit_to_analyse_count))

    for commit_description in commits:
        commit_to_analyse_count -= 1
        analysed_commit = Commit(commit_description)
        (author, reviewers, changes, path,
         date,
         commit_hash) = analysed_commit.all_informations()
        if len(changes) == 0 or not path:
            continue
        if not quiet:
            print(('Save commit ' + commit_hash + ' from ' + author + ' in\t' +
                   path),
                  end='',
                  flush=True)

        current_path = path
        while current_path:
            if not current_path in all_stats_per_dir:
                all_stats_per_dir[current_path] = dict(
                    total_commit=0,
                    total_review=0,
                    individual_stats={},
                    final_score={},
                    last_update=datetime.datetime.min)
            if not author in all_stats_per_dir[current_path][
                    'individual_stats']:
                all_stats_per_dir[current_path]['individual_stats'][
                    author] = dict(commit_count=0, review_count=0)

            all_stats_per_dir[current_path]['last_update'] = max(
                all_stats_per_dir[current_path]['last_update'], date)
            all_stats_per_dir[current_path]['total_commit'] += 1
            all_stats_per_dir[current_path]['individual_stats'][author][
                'commit_count'] += 1
            for reviewer in reviewers:
                if owner_exclusion and is_high_level_owner(
                        reviewer, path, owners_map):
                    continue
                all_stats_per_dir[current_path]['total_review'] += 1
                if not reviewer in all_stats_per_dir[current_path][
                        'individual_stats']:
                    all_stats_per_dir[current_path][
                        'individual_stats'][reviewer] = dict(commit_count=0,
                                                             review_count=0)
                all_stats_per_dir[current_path]['individual_stats'][
                    reviewer]['review_count'] += 1

            parent_path = os.path.dirname(current_path)
            if parent_path == current_path:
                break
            current_path = parent_path

        if not quiet:
            print('\t\t\tDONE, commits left: ',
                  commit_to_analyse_count,
                  flush=True)
    return all_stats_per_dir



def get_all_git_blame_informations_for_folder(
        file_paths: list[str],
        date_filter: datetime,
        quiet: bool = False) -> list[str]:
    """Retrieves all `git blame` output for a list of files in parallel.

    Args:
        file_paths: A list of file paths to run `git blame` on.
        date_filter: The date to use for the `--after` flag in git blame.
        quiet: If True, suppresses progress indicators.

    Returns:
        A list of strings, where each string is one line of blame output.
    """
    lines = []
    if not quiet:
        print('[Git blame] ' + os.path.dirname(file_paths[0]),
              end='',
              flush=True)
    executor = ProcessPoolExecutor(max_workers=6)
    # Dispatch tasks into the process pool and create a list of futures.
    futures = [
        executor.submit(get_blame_for_file, file, date_filter)
        for file in file_paths
    ]
    if not quiet:
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
                                    last_update: datetime,
                                    quiet: bool = False) -> list[str]:
    """High-level function to determine owners using the git blame strategy.

    Args:
        root: The root directory of the files.
        files: A list of filenames within the root directory.
        last_update: The last update time for the directory, used for filtering.
        quiet: If True, suppresses progress indicators.

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
    lines = get_all_git_blame_informations_for_folder(file_paths, date_filter,
                                                      quiet)
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
            total_review_count) if total_commit_count > 0 else 0
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
    parser = argparse.ArgumentParser(
        description='Automatic Ownership Calculator.')
    parser.add_argument(
        '-q',
        '--quiet',
        action='store_true',
        help='Enable quiet mode, suppresses progress indicators.')
    parser.add_argument(
        '--root-directory',
        default='ios',
        help="The root directory to start the analysis from. Default: 'ios'.")
    parser.add_argument(
        '--commits-input-file',
        default='commits.log',
        help="The path to the input file for commit logs. "
        "Defaults to 'commits.log'.")
    parser.add_argument(
        '--owners-input-file',
        default='owners.json',
        help="The path to the input file for the OWNERS map. "
        "Defaults to 'owners.json'.")
    parser.add_argument(
        '--output-file',
        default='final_algo.csv',
        help="The path to the output CSV file. Defaults to 'final_algo.csv'.")
    parser.add_argument(
        '--disable-owner-exclusion',
        action='store_true',
        help='Disable exclusion of higher-level owners from review stats.')
    parser.add_argument(
        '--commit-threshold',
        type=int,
        default=5,
        help='The minimum number of commits required to use the Z-score method.'
        ' Defaults to 5.')
    parser.add_argument(
        '--use-blame-for-low-commit-dirs',
        action='store_true',
        help='If enabled, use the git blame method for directories with commit '
        'counts below the threshold. Defaults to False.')
    args = parser.parse_args()

    root_folder = args.root_directory
    commits_input_file = args.commits_input_file
    owners_input_file = args.owners_input_file
    output_file = args.output_file
    quiet_mode = args.quiet
    owner_exclusion = not args.disable_owner_exclusion

    # Load pre-collected data.
    with open(owners_input_file, 'r') as f:
        owners_map = json.load(f)
    # Convert lists back to sets.
    owners_map = {k: set(v) for k, v in owners_map.items()}

    with open(commits_input_file, 'r') as f:
        commit_log = f.read()
    commits = split_log_into_commits(commit_log)

    # Phase 1: Data Analysis
    stats_per_folder = extract_commits_informations(commits,
                                                    owners_map,
                                                    owner_exclusion,
                                                    quiet=quiet_mode)

    # Clear output file before starting
    with open(output_file, 'w') as f:
        pass

    # Phase 2: Ownership Calculation
    steps = len(stats_per_folder)
    step_count = 0
    for root, dirs, files in os.walk(root_folder):
        if avoid_directory(root):
            continue
        if not root in stats_per_folder:
            with open(output_file, 'a') as file:
                file.write(root + '\n')
            continue

        step_count += 1
        if not quiet_mode:
            print(
                str(step_count) + '/' + str(steps) + '\t',
                end='',
                flush=True)

        owners = []
        # Decide which algorithm to use based on commit history.
        if stats_per_folder[root]['total_commit'] >= args.commit_threshold:
            owners = determine_owners_from_zscore(stats_per_folder[root])
            if not quiet_mode:
                print('[Z-Score] ' + root + '\tRESULT: ' + str(owners))
        elif args.use_blame_for_low_commit_dirs:
            owners = determine_owners_from_git_blame(
                root,
                files,
                stats_per_folder[root]['last_update'],
                quiet=quiet_mode)
            if not quiet_mode:
                print('[Blame] ' + root + '\tRESULT: ' + str(owners))

        # Write results to the output CSV.
        with open(output_file, 'a') as file:
            file.write(root + ', ' +
                       str(stats_per_folder[root]['last_update']))
            for owner in owners:
                file.write(',' + owner)
            file.write('\n')