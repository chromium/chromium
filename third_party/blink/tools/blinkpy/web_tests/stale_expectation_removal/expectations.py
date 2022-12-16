# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' expectations module."""

import os
from typing import List, Set

from blinkpy.web_tests.stale_expectation_removal import constants

from unexpected_passes_common import expectations

MAIN_EXPECTATION_FILE = os.path.join(constants.WEB_TEST_ROOT_DIR,
                                     'TestExpectations')

TOP_LEVEL_EXPECTATION_FILES = {
    'ASANExpectations',
    'LeakExpectations',
    'MSANExpectations',
    # NeverFixTests omitted since they're never expected to be
    # unsuppressed.
    'SlowTests',
    'TestExpectations',
    'W3CImportExpectations',
    # WebDriver and WPTOverride omitted since we do not get ResultDB data for
    # those suites. WebGPU omitted since that suite is currently not supported
    # by this script.
}


class WebTestExpectations(expectations.Expectations):
    def __init__(self):
        super(WebTestExpectations, self).__init__()
        self._expectation_filepaths = None
        self._expectation_file_tag_headers = {}
        self._flag_specific_expectation_files = None
        self._known_tags = None

    def GetExpectationFilepaths(self) -> List[str]:
        # We don't use the Port classes' expectations_files() and
        # extra_expectations_files() since they do not actually return all
        # expectation files. For example, it only returns the one flag-specific
        # expectation file that matches the port's flag-specific config.
        if self._expectation_filepaths is None:
            self._expectation_filepaths = []
            for ef in self._GetTopLevelExpectationFiles():
                self._expectation_filepaths.append(
                    os.path.join(constants.WEB_TEST_ROOT_DIR, ef))
            flag_directory = os.path.join(constants.WEB_TEST_ROOT_DIR,
                                          'FlagExpectations')
            for ef in self._GetFlagSpecificExpectationFiles():
                self._expectation_filepaths.append(
                    os.path.join(flag_directory, ef))

        return self._expectation_filepaths

    def _GetTopLevelExpectationFiles(self) -> Set[str]:
        return TOP_LEVEL_EXPECTATION_FILES

    def _GetFlagSpecificExpectationFiles(self) -> Set[str]:
        if self._flag_specific_expectation_files is None:
            self._flag_specific_expectation_files = set()
            flag_directory = os.path.join(constants.WEB_TEST_ROOT_DIR,
                                          'FlagExpectations')
            for ef in os.listdir(flag_directory):
                if ef != 'README.txt':
                    self._flag_specific_expectation_files.add(ef)
        return self._flag_specific_expectation_files

    def _GetExpectationFileTagHeader(self, expectation_file: str) -> str:
        if expectation_file not in self._expectation_file_tag_headers:
            # Copy all the comments and blank lines at the top of the file,
            # which constitutes the header.
            header = ''
            with open(expectation_file) as f:
                contents = f.read()
            for line in contents.splitlines(True):
                line = line.lstrip()
                if line and not line.startswith('#'):
                    break
                header += line
            self._expectation_file_tag_headers[expectation_file] = header
        return self._expectation_file_tag_headers[expectation_file]

    def _GetKnownTags(self) -> Set[str]:
        if self._known_tags is None:
            self._known_tags = set()
            for f in self.GetExpectationFilepaths():
                list_parser = self.ParseTaggedTestListContent(
                    self._GetExpectationFileTagHeader(f))
                for ts in list_parser.tag_sets:
                    self._known_tags |= ts
            self._known_tags = {t.lower() for t in self._known_tags}
        return self._known_tags
