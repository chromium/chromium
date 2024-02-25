#!/usr/bin/env vpython3
#
# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import os
import sys
from datetime import datetime
from collections import defaultdict

PYJSON5_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                           'third_party', 'pyjson5', 'src')
sys.path.append(PYJSON5_DIR)
import json5

# Set to True to break all unknown orgs out using their domain name
SPLIT_OTHERS = False

# For getting specific directories / repositories
FIXED_DIRS = None
#FIXED_DIRS = [["./third_party/blink", "blink"]]

# Paths to necessary files
topdir = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                        capture_output=True,
                        text=True).stdout.strip()
scriptdir = os.path.dirname(os.path.abspath(__file__))
org_list_file = os.path.join(scriptdir, 'org-list.txt')
git_dirs_file = os.path.join(scriptdir, 'git-dirs.txt')
contributors_file = os.path.join(scriptdir, 'affiliations.json5')

# Bot address substrings to exclude.
botpatterns = [
    "chrome-metrics-team+robot@google.com", "chrome-release-bot@chromium.org",
    "gserviceaccount.com", "chrome-admin@google.com",
    "v8-autoroll@chromium.org", "deps-roller@chromium.org",
    "autoroller@chromium.org"
]

# Read org-list.txt and create a domain to org mapping
domain_to_org = {}
with open(org_list_file, 'r') as file:
    for line in file:
        line = line.strip()
        if line:
            parts = line.split(',')
            domain_to_org[parts[1].strip()] = parts[0].strip()

# Read git-dirs.txt and create a list of directories
repos = []
if FIXED_DIRS:
    repos = FIXED_DIRS
else:
    with open(git_dirs_file, 'r') as file:
        for line in file:
            parts = line.strip().split(',')
            repos.append(parts)

contributors = {}
with open(contributors_file, 'r') as f:
    contributors = json5.load(f)


# Given contributor email address and commit date, return a domain for any
# override in affiliations.json5, or None otherwise. Can also be the special
# tokens 'Individual' or 'Undisclosed'
def get_affiliation_override(email, date):
    contributor = contributors.get(email)
    if contributor:
        for affiliation in contributor['affiliations']:
            if datetime.strptime(date, '%Y-%m-%d') >= datetime.strptime(
                    affiliation['start'], '%Y-%m-%d'):
                return affiliation['domain']
    return None


# Function to get commit distribution
def get_commit_distribution(start_date, end_date):
    commit_counts = {}
    dirs_processed = set()

    for [repo_dir, repo_name] in repos:
        if repo_name not in commit_counts:
            commit_counts[repo_name] = defaultdict(int)
        os.chdir(os.path.join(topdir, repo_dir))

        git_base = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                                  capture_output=True,
                                  text=True).stdout.strip()
        if git_base in dirs_processed:
            raise Exception("Duplicate repository directory: " + repo_dir)
        dirs_processed.add(git_base)

        runcmd = [
            'git', 'log', '--after={}'.format(start_date),
            '--before={}'.format(end_date), '--pretty=format:%ae;%as'
        ]
        if FIXED_DIRS:
            # Specifying a directory (even the root one) makes log take about 5x
            # longer! So do it only when we know we're using non-repo dirs.
            runcmd.append('.')
        log_output = subprocess.run(runcmd, capture_output=True,
                                    text=True).stdout

        for line in log_output.splitlines():
            email, date = line.split(';')
            domain = email.split('@')[1] if '@' in email else email
            domain = domain.lower()
            # Skip any emails which have one of the bot patterns as a substring
            if any(botpat in email for botpat in botpatterns):
                continue
            affiliation = ""
            override = get_affiliation_override(email, date)
            if override:
                if override == 'Individual':
                    affiliation = 'Individuals'
                elif override == 'Undisclosed':
                    affiliation = override
                else:
                    domain = override
            if not affiliation:
                if domain in domain_to_org:
                    affiliation = domain_to_org[domain]
                else:
                    affiliation = domain if SPLIT_OTHERS else 'Others'
            commit_counts[repo_name][affiliation] += 1

    return commit_counts


# MM-DD to start the annual analysis on. Can be changed to a recent date.
DATE_SUFFIX = "01-01"

# Run distribution for specified years
print("Year,Repo,Org,Commit count")
for start_year in range(2015, 2025):
    start_date = f'{start_year}-{DATE_SUFFIX}'
    end_date = f'{start_year+1}-{DATE_SUFFIX}'
    commit_counts = get_commit_distribution(start_date, end_date)
    for repo, counts in commit_counts.items():
        for org, count in sorted(counts.items(),
                                 key=lambda x: x[1],
                                 reverse=True):
            print(f"{start_year},{repo},{org},{count}")
