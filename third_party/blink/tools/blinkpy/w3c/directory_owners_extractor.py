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


class DirectoryOwnersExtractor(object):
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

    def find_owners_file(self, start_path):
        """Finds the first enclosing OWNERS file for a given path.

        Starting from the given path, walks up the directory tree until the
        first OWNERS file is found or web_tests/external is reached.

        Args:
            start_path: A relative path from the root of the repository, or an
                absolute path. The path can be a file or a directory.

        Returns:
            The absolute path to the first OWNERS file found; None if not found
            or if start_path is outside of web_tests/external.
        """
        abs_start_path = (start_path if self.filesystem.isabs(start_path) else
                          self.finder.path_from_chromium_base(start_path))
        directory = (abs_start_path if self.filesystem.isdir(abs_start_path)
                     else self.filesystem.dirname(abs_start_path))
        external_root = self.finder.path_from_web_tests('external')
        if not directory.startswith(external_root):
            return None
        # Stop at web_tests, which is the parent of external_root.
        while directory != self.finder.web_tests_dir():
            owners_file = self.filesystem.join(directory, 'OWNERS')
            if self.filesystem.isfile(
                    self.finder.path_from_chromium_base(owners_file)):
                return owners_file
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

    def extract_component(self, metadata_file):
        """Extracts the component from an DIR_METADATA file.

        Args:
            metadata_file: An absolute path to an DIR_METADATA file.

        Returns:
            A string, or None if not found.
        """
        dir_metadata = self._read_dir_metadata(metadata_file)
        if dir_metadata and dir_metadata.component:
            return dir_metadata.component
        return None

    def is_wpt_notify_enabled(self, metadata_file):
        """Checks if the DIR_METADATA file enables WPT-NOTIFY.

        Args:
            metadata_file: An absolute path to an DIR_METADATA file.

        Returns:
            A boolean.
        """
        dir_metadata = self._read_dir_metadata(metadata_file)
        return dir_metadata and dir_metadata.should_notify

    @memoized
    def _read_text_file(self, path):
        return self.filesystem.read_text_file(path)

    @memoized
    def _read_dir_metadata(self, path):
        """Read the content from a path.

        Args:
            path: An absolute path.

        Returns:
            A WPTDirMetadata object, or None if not found.
        """
        print('_read_dir_metadata %s' % path)
        dir_path = self.filesystem.dirname(path)

        # dirmd starts with an absolute directory path, `dir_path`, traverses all
        # parent directories and stops at `root_path` to find the first available DIR_METADATA
        # file. `root_path` is the web_tests directory.
        json_data = self.executive.run_command([
            self.finder.path_from_depot_tools_base('dirmd'),
            'read',
            '-form', 'sparse',
            dir_path,
        ])
        try:
            data = json.loads(json_data)
        except ValueError:
            return None

        # Paths in the dirmd output are relative to the repo root.
        repo_root = self.finder.path_from_chromium_base()
        relative_path = self.filesystem.relpath(dir_path, repo_root)
        return WPTDirMetadata(data, relative_path)


class WPTDirMetadata(object):
    def __init__(self, data, path):
        """Constructor for WPTDirMetadata.

        Args:
            data: The output of `dirmd` in _read_dir_metadata; e.g.
            {
                "dirs":{
                    "tools/binary_size/libsupersize/testdata/mock_source_directory/base":{
                        "monorail":{
                            "project":"chromium",
                            "component":"Blink>Internal"
                        },
                        "teamEmail":"team@chromium.org",
                        "os":"LINUX",
                        "wpt":{
                            "notify":"YES"
                        }
                    }
                }
            }

            path: The relative directory path of the DIR_METADATA to the web_tests directory;
                see `relative_path` in _read_dir_metadata.
        """
        self._data = data
        self._path = path

    def _get_content(self):
        return self._data['dirs'][self._path]

    def _is_empty(self):
        return len(self._get_content()) == 0

    @property
    def team_email(self):
        if self._is_empty():
            return None
        # Only returns a single email.
        return self._get_content()['teamEmail']

    @property
    def component(self):
        if self._is_empty():
            return None
        return self._get_content()['monorail']['component']

    @property
    def should_notify(self):
        if self._is_empty():
            return None

        notify = self._get_content().get('wpt', {}).get('notify')
        # The value of `notify` is one of ['TRINARY_UNSPECIFIED', 'YES', 'NO'].
        # Assume that users opt out by default; return True only when notify is 'YES'.
        return notify == 'YES'
