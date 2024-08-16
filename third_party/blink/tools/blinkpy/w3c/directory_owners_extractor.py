# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A limited finder & parser for Chromium OWNERS and DIR_METADATA files.

This module is intended to be used within web_tests/external and is
informative only. For authoritative uses, please rely on `git cl owners`.
For example, it does not support directives other than email addresses.
"""

import collections
import json
import re
from typing import NamedTuple, Optional

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

# Format of OWNERS files can be found at //src/third_party/depot_tools/owners.py
# In our use case (under external/wpt), we only process the first enclosing
# OWNERS file for any given path (i.e. always assuming "set noparent"), and we
# ignore "per-file:" lines, "file:" directives, etc.
#
# For DIR_METADATA files, we rely on the dirmd tool from depot_tools to parse
# them into a JSON blob.

# Recognizes 'X@Y' email addresses. Very simplistic. (from owners.py)
BASIC_EMAIL_REGEXP = r'^[\w\-\+\%\.]+\@[\w\-\+\%\.]+$'


class WPTDirMetadata(NamedTuple):
    team_email: Optional[str] = None
    should_notify: bool = False
    buganizer_public_component: Optional[str] = None


class DirectoryOwnersExtractor:
    def __init__(self, host):
        self.filesystem = host.filesystem
        self.finder = PathFinder(self.filesystem)
        self.executive = host.executive
        self.owner_map = None

    def list_owners(self, changed_files):
        """Looks up the owners for the given set of changed files.

        Args:
            changed_files: A list of file paths relative to the repository root.

        Returns:
            A dict mapping tuples of owner email addresses to lists of
            owned directories (paths relative to the root of web tests).
        """
        email_map = collections.defaultdict(set)
        external_root_owners = self.finder.path_from_web_tests(
            'external', 'OWNERS')
        for relpath in changed_files:
            # Try to find the first *non-empty* OWNERS file.
            absolute_path = self.finder.path_from_chromium_base(relpath)
            owners = None
            owners_file = self.find_owners_file(absolute_path)
            while owners_file:
                owners = self.extract_owners(owners_file)
                if owners:
                    break
                # Found an empty OWNERS file. Try again from the parent directory.
                absolute_path = self.filesystem.dirname(
                    self.filesystem.dirname(owners_file))
                owners_file = self.find_owners_file(absolute_path)
            # Skip web_tests/external/OWNERS.
            if not owners or owners_file == external_root_owners:
                continue

            owned_directory = self.filesystem.dirname(owners_file)
            owned_directory_relpath = self.filesystem.relpath(
                owned_directory, self.finder.web_tests_dir())
            email_map[tuple(owners)].add(owned_directory_relpath)
        return {
            owners: sorted(owned_directories)
            for owners, owned_directories in email_map.items()
        }

    def find_owners_file(self, start_path: str) -> Optional[str]:
        return self._find_first_file(start_path, 'OWNERS')

    def find_dir_metadata_file(self, start_path: str) -> Optional[str]:
        return self._find_first_file(start_path, 'DIR_METADATA')

    def _find_first_file(self, start_path: str,
                         filename: str) -> Optional[str]:
        """Find the first enclosing file for a given path.

        Starting from the given path, walk up the directory tree until the first
        file with the given name is found or web_tests is reached.

        Args:
            start_path: A relative path from the root of the repository, or an
                absolute path. The path can be a file or a directory.
            filename: File to look for in each candidate directory.

        Returns:
            The absolute path to the first file, if found; None otherwise.
        """
        abs_start_path = (start_path if self.filesystem.isabs(start_path) else
                          self.finder.path_from_chromium_base(start_path))
        directory = (self.filesystem.normpath(abs_start_path)
                     if self.filesystem.isdir(abs_start_path) else
                     self.filesystem.dirname(abs_start_path))
        if not directory.startswith(self.finder.web_tests_dir()):
            return None
        while directory != self.finder.web_tests_dir():
            maybe_file = self.filesystem.join(directory, filename)
            if self.filesystem.isfile(
                    self.finder.path_from_chromium_base(maybe_file)):
                return maybe_file
            directory = self.filesystem.dirname(directory)
        return None

    def extract_owners(self, owners_file):
        """Extracts owners from an OWNERS file.

        Args:
            owners_file: An absolute path to an OWNERS file.

        Returns:
            A list of valid owners (email addresses).
        """
        contents = self._read_text_file(owners_file)
        email_regexp = re.compile(BASIC_EMAIL_REGEXP)
        addresses = []
        for line in contents.splitlines():
            line = line.strip()
            if email_regexp.match(line):
                addresses.append(line)
        return addresses

    @memoized
    def _read_text_file(self, path):
        return self.filesystem.read_text_file(path)

    @memoized
    def read_dir_metadata(self, path: str) -> Optional[WPTDirMetadata]:
        """Read the `DIR_METADATA` of a directory.

        The output of `dirmd` in JSON format looks like:
            {
                "dirs": {
                    "tools/binary_size/libsupersize/testdata": {
                        "teamEmail": "team@chromium.org",
                        "os": "LINUX",
                        "wpt": {
                            "notify": "YES"
                        },
                        "buganizerPublic": {
                            "componentId": "12345"
                        }
                    }
                }
            }

        Arguments:
            path: An absolute path to the directory, *not* the `DIR_METADATA`
                file itself.

        Returns:
            A WPTDirMetadata object, or None if not found.
        """
        # dirmd starts with an absolute directory path, `dir_path`, traverses
        # all parent directories and stops at `root_path` to find the first
        # available DIR_METADATA file. `root_path` is the web_tests directory.
        json_data = self.executive.run_command([
            self.finder.path_from_depot_tools_base('dirmd'),
            'read',
            '-form',
            'sparse',
            path,
        ])
        # Paths in the dirmd output are relative to the repo root.
        repo_root = self.finder.path_from_chromium_base()
        relative_path = self.filesystem.relpath(path, repo_root)
        try:
            data = json.loads(json_data)['dirs'][relative_path]
        except (ValueError, KeyError):
            return None

        # The value of `notify` is one of ['TRINARY_UNSPECIFIED', 'YES', 'NO'].
        # Assume that users opt in by default; return False only when notify is
        # 'NO'.
        return WPTDirMetadata(
            data.get('teamEmail'),
            data.get('wpt', {}).get('notify') != 'NO',
            data.get('buganizerPublic', {}).get('componentId'))
