#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Data Collector for Automatic Ownership.

This script gathers git commit history and OWNERS file information, which can
be used as a cache for the main automatic_ownership.py script.
"""

import argparse
import datetime
import json
import os
import sys

from concurrent.futures import ProcessPoolExecutor, as_completed
from filters import avoid_owner_line
from gitutils import get_commits_in_folder_in_period

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


def get_existing_owners(root_directory: str) -> dict[str, set[str]]:
    """Walks a directory to find all OWNERS files and parse them.

    Args:
        root_directory: The directory to start the search from.

    Returns:
        A dictionary mapping directory paths to a set of owner usernames.
    """
    owners_map = {}
    for root, _, files in os.walk(root_directory):
        if 'OWNERS' in files:
            owners_path = os.path.join(root, 'OWNERS')
            with open(owners_path, 'r') as f:
                owners = set()
                for line in f:
                    if avoid_owner_line(line):
                        continue
                    # Extract username from email format.
                    if '@' in line:
                        owners.add(line.split('@')[0])
                if owners:
                    owners_map[os.path.relpath(
                        root, root_directory)] = owners
    return owners_map


def progress_indicator(future) -> None:
    """Simple progress indicator callback function for multi-process calls."""
    print('.', end='', flush=True)


def get_all_commits_of_folder(path: str, quiet: bool = False) -> str:
    """Retrieves all raw commit logs for a folder over the last two years.

    This function parallelizes the git log calls by splitting the time period
    into monthly chunks.

    Args:
        path: The directory to retrieve commit logs for.
        quiet: If True, suppresses progress indicators.

    Returns:
        A single raw string containing all commit descriptions.
    """
    raw_logs = []
    executor = ProcessPoolExecutor()
    # Dispatch tasks into the process pool and create a list of futures.
    futures = [
        executor.submit(get_commits_in_folder_in_period, path, dates)
        for dates in get_dates_range()
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
            raw_logs.append(result)
    # Shutdown the process pool.
    executor.shutdown(wait=True)  # blocks
    return "".join(raw_logs)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Data Collector for Automatic Ownership.')
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
        '--commits-output-file',
        default='commits.log',
        help="The path to the output file for commit logs. "
        "Defaults to 'commits.log'.")
    parser.add_argument(
        '--owners-output-file',
        default='owners.json',
        help="The path to the output file for the OWNERS map. "
        "Defaults to 'owners.json'.")
    args = parser.parse_args()

    root_folder = args.root_directory
    commits_output_file = args.commits_output_file
    owners_output_file = args.owners_output_file
    quiet_mode = args.quiet

    # 1. Collect and save OWNERS data.
    owners_map = get_existing_owners(root_folder)
    # Convert sets to lists for JSON serialization.
    serializable_owners_map = {
        k: list(v)
        for k, v in owners_map.items()
    }
    with open(owners_output_file, 'w') as f:
        json.dump(serializable_owners_map, f, indent=4)
    if not quiet_mode:
        print(f"OWNERS map saved to {owners_output_file}")

    # 2. Collect and save commit logs.
    commit_log = get_all_commits_of_folder(root_folder, quiet=quiet_mode)
    with open(commits_output_file, 'w') as f:
        f.write(commit_log)
    if not quiet_mode:
        print(f"Commit logs saved to {commits_output_file}")
