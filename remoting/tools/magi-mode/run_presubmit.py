#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script to run MAGI presubmit checks locally."""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PRESUBMIT import CheckChangeOnUpload


class MockFile:

    def __init__(self, absolute_path, repo_root):
        self._absolute_path = absolute_path
        self._repo_root = repo_root

    def LocalPath(self):
        return os.path.relpath(self._absolute_path, self._repo_root)

    def AbsoluteLocalPath(self):
        return self._absolute_path

    def Action(self):
        return 'M'


class MockChange:

    def __init__(self, repo_root):
        self._repo_root = repo_root

    def RepositoryRoot(self):
        return self._repo_root


class MockInputApi:

    def __init__(self, files, repo_root):
        self._files = [MockFile(f, repo_root) for f in files]
        self.change = MockChange(repo_root)
        self.os_path = os.path
        self.re = re

    def AffectedFiles(self, file_filter=None, include_deletes=False):
        if file_filter:
            return [f for f in self._files if file_filter(f)]
        return self._files

    def ReadFile(self, affected_file):
        with open(affected_file.AbsoluteLocalPath(), 'r',
                  encoding='utf-8') as f:
            return f.read()

    def PresubmitLocalPath(self):
        return os.path.dirname(os.path.abspath(__file__))

    def FilterSourceFile(self,
                         affected_file,
                         files_to_check=None,
                         files_to_skip=None):
        files_to_check = files_to_check or []
        files_to_skip = files_to_skip or []
        path = affected_file.LocalPath()
        for regex in files_to_skip:
            if re.match(regex, path):
                return False
        for regex in files_to_check:
            if re.match(regex, path):
                return True
        return len(files_to_check) == 0


class MockOutputApi:

    class PresubmitError:

        def __init__(self, message, *args, **kwargs):
            self.message = message

        def __str__(self):
            return f"ERROR: {self.message}"

    class PresubmitPromptWarning:

        def __init__(self, message, *args, **kwargs):
            self.message = message

        def __str__(self):
            return f"WARNING: {self.message}"

    class PresubmitNotifyResult:

        def __init__(self, message, *args, **kwargs):
            self.message = message

        def __str__(self):
            return f"NOTIFY: {self.message}"

    class PresubmitPromptOrNotify:

        def __init__(self, message, *args, **kwargs):
            self.message = message

        def __str__(self):
            return f"PROMPT/NOTIFY: {self.message}"


def main():
    # Collect all files in the current directory and tests directory
    files = []
    magi_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(magi_dir, '../../..'))

    for root, dirs, filenames in os.walk(magi_dir):
        for filename in filenames:
            if filename.endswith(('.py', '.md', '.json')):
                files.append(os.path.abspath(os.path.join(root, filename)))

    input_api = MockInputApi(files, repo_root)
    output_api = MockOutputApi()

    print(f"Running checks on {len(files)} files...")
    results = CheckChangeOnUpload(input_api, output_api)

    errors = [
        r for r in results if isinstance(r, MockOutputApi.PresubmitError)
    ]
    warnings = [
        r for r in results
        if isinstance(r, MockOutputApi.PresubmitPromptWarning)
    ]

    print(f"Found {len(errors)} errors and {len(warnings)} warnings.")
    for r in results:
        print(r)

    if errors:
        sys.exit(1)


if __name__ == '__main__':
    main()
