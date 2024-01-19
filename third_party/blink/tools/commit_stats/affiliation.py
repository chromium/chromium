#!/usr/bin/env vpython3
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from datetime import datetime

PYJSON5_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                           'third_party', 'pyjson5', 'src')
sys.path.append(PYJSON5_DIR)
import json5

current_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
CONTRIBUTORS_FILENAME = current_dir + "/affiliations.json5"


def get_contributor_data(user_id):
    with open(CONTRIBUTORS_FILENAME) as contributors_as_json5:
        contributors = json5.loads(contributors_as_json5.read())
    return contributors.get(user_id, None)


def get_affiliation_at_date(user_id, commit_date):
    contributor = get_contributor_data(user_id)
    if not contributor:
        return None
    for affiliation in contributor["affiliations"]:
        if get_date_from_string(commit_date) >= get_date_from_string(
                affiliation["start"]):
            return affiliation["domain"]
    return None


def get_date_from_string(date_string):
    return datetime.strptime(date_string, "%Y-%m-%d")


def main():
    args = sys.argv
    if "-h" in args:
        print(args[0] + "<chromium user id> <commit-date>")
    user_id = args[1]
    commit_date = args[2]
    print(get_affiliation_at_date(user_id, commit_date))


if __name__ == "__main__":
    main()
