# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' expectations module."""

import os

from unexpected_passes_common import expectations

WEB_TEST_ROOT_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                 'web_tests'))

MAIN_EXPECTATION_FILE = os.path.join(WEB_TEST_ROOT_DIR, 'TestExpectations')

TOP_LEVEL_EXPECTATION_FILES = {
    'ASANExpectations',
    'LeakExpectations',
    'MSANExpectations',
    # NeverFixTests omitted since they're never expected to be
    # unsuppressed.
    'SlowTests',
    'W3CImportExpectations',
    'WPTOverrideExpectations',
    'WebDriverExpectations',
    'WebGPUExpectations',
}


class WebTestExpectations(expectations.Expectations):
    def __init__(self):
        super(WebTestExpectations, self).__init__()
        self._expectation_filepaths = None
        self._expectation_file_tag_header = None
        self._flag_specific_expectation_files = None

    def GetExpectationFilepaths(self):
        # We don't use the Port classes' expectations_files() and
        # extra_expectations_files() since they do not actually return all
        # expectation files. For example, it only returns the one flag-specific
        # expectation file that matches the port's flag-specific config.
        if self._expectation_filepaths is None:
            self._expectation_filepaths = []
            for ef in self._GetTopLevelExpectationFiles():
                self._expectation_filepaths.append(
                    os.path.join(WEB_TEST_ROOT_DIR, ef))
            flag_directory = os.path.join(WEB_TEST_ROOT_DIR,
                                          'FlagExpectations')
            for ef in self._GetFlagSpecificExpectationFiles():
                self._expectation_filepaths.append(
                    os.path.join(flag_directory, ef))

        return self._expectation_filepaths

    def _GetTopLevelExpectationFiles(self):
        return TOP_LEVEL_EXPECTATION_FILES

    def _GetFlagSpecificExpectationFiles(self):
        if self._flag_specific_expectation_files is None:
            self._flag_specific_expectation_files = set()
            flag_directory = os.path.join(WEB_TEST_ROOT_DIR,
                                          'FlagExpectations')
            for ef in os.listdir(flag_directory):
                if ef != 'README.txt':
                    self._flag_specific_expectation_files.add(ef)
        return self._flag_specific_expectation_files

    def _GetExpectationFileTagHeader(self):
        if self._expectation_file_tag_header is None:
            # Copy all the comments and blank lines at the top of the file,
            # which constitutes the header.
            self._expectation_file_tag_header = ''
            with open(MAIN_EXPECTATION_FILE) as f:
                contents = f.read()
            for line in contents.splitlines(True):
                line = line.lstrip()
                if line and not line.startswith('#'):
                    break
                self._expectation_file_tag_header += line
        return self._expectation_file_tag_header
