#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts the zipfile from the cipd instance and commits it to the repo."""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import zipfile

_THIRD_PARTY_ANDROIDX_PATH = pathlib.Path(__file__).resolve().parents[0]
_SRC_PATH = _THIRD_PARTY_ANDROIDX_PATH.parents[1]
_COMMITED_DIR_PATH = _THIRD_PARTY_ANDROIDX_PATH / 'committed'
_TO_COMMIT_ZIP_NAME = 'to_commit.zip'


def _HasChanges(repo):
    output = subprocess.check_output(
        ['git', '-C', repo, 'status', '--porcelain=v1'])
    return bool(output)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--cipd-package-path',
                        default=_THIRD_PARTY_ANDROIDX_PATH / 'cipd',
                        help='Path to androidx\'s cipd package path.')
    options = parser.parse_args()

    cipd_package_path = pathlib.Path(options.cipd_package_path)
    to_commit_zip_path = cipd_package_path / _TO_COMMIT_ZIP_NAME
    if not to_commit_zip_path.exists():
        print(f'No zipfile found at {to_commit_zip_path}', file=sys.stderr)
        print('Doing nothing', file=sys.stderr)
        return

    if os.path.exists(_COMMITED_DIR_PATH):
        # Delete original contents.
        shutil.rmtree(_COMMITED_DIR_PATH)
    os.makedirs(_COMMITED_DIR_PATH)
    # Replace with the contents of the zip.
    with zipfile.ZipFile(to_commit_zip_path) as z:
        z.extractall(_COMMITED_DIR_PATH)

    if not _HasChanges(_SRC_PATH):
        print(
            "No changes found after extracting zip. Did you run this script before?"
        )
        return

    git_add_cmd = [
        'git', '-C', _SRC_PATH, 'add',
        os.path.relpath(_COMMITED_DIR_PATH, _SRC_PATH)
    ]
    subprocess.check_call(git_add_cmd)


if __name__ == '__main__':
    main()
