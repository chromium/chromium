# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Converts Chromium Test Expectations into WPT Metadata ini files.

This script loads TestExpectations for any WPT test and creates the metadata
files corresponding to the expectation. This script runs as a BUILD action rule.
The output is then bundled into the WPT isolate package to be shipped to bots
running the WPT test suite.
"""

import argparse
import logging
import os
import re

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models import test_expectations

_log = logging.getLogger(__name__)


class WPTMetadataBuilder(object):
    def __init__(self, expectations, port):
        """
        Args:
            expectations: a blinkpy.web_tests.models.test_expectations.TestExpectations object
            port: a blinkpy.web_tests.port.Port object
        """
        self.expectations = expectations
        self.port = port
        self.wpt_manifest = self.port.wpt_manifest("external/wpt")
        self.metadata_output_dir = ""

    def run(self, args=None):
        """Main entry point to parse flags and execute the script."""
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--metadata-output-dir",
                            help="The directory to output the metadata files into.")
        parser.add_argument('-v', '--verbose', action='store_true', help='More verbose logging.')
        args = parser.parse_args(args)

        log_level = logging.DEBUG if args.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        self.metadata_output_dir = args.metadata_output_dir
        self._build_metadata_and_write()

        return 0

    def _build_metadata_and_write(self):
        """Build the metadata files and write them to disk."""
        if os.path.exists(self.metadata_output_dir):
            _log.debug("Output dir exists, deleting: %s",
                       self.metadata_output_dir)
            import shutil
            shutil.rmtree(self.metadata_output_dir)

        failing_baseline_tests = self.get_test_names_to_fail()
        _log.info("Found %d tests with failing baselines", len(failing_baseline_tests))
        for test_name in failing_baseline_tests:
            filename, file_contents = self.get_metadata_filename_and_contents(test_name, 'FAIL')
            if not filename or not file_contents:
                continue
            self._write_to_file(filename, file_contents)

        skipped_tests = self.get_test_names_to_skip()
        _log.info("Found %d tests with skip expectations", len(skipped_tests))
        for test_name in skipped_tests:
            if test_name in failing_baseline_tests:
                _log.error("Test %s has a baseline but is also skipped" % test_name)
            filename, file_contents = self.get_metadata_filename_and_contents(test_name, 'SKIP')
            if not filename or not file_contents:
                continue
            self._write_to_file(filename, file_contents)

        # Finally, output a stamp file with the same name as the output
        # directory. The stamp file is empty, it's only used for its mtime.
        # This makes the GN build system happy (see crbug.com/995112).
        with open(self.metadata_output_dir + ".stamp", "w"):
            pass

    def _write_to_file(self, filename, file_contents):
        # Write the contents to the file name
        if not os.path.exists(os.path.dirname(filename)):
            os.makedirs(os.path.dirname(filename))
        # Note that we append to the metadata file in order to allow multiple
        # tests to be present in the same .ini file (ie: for multi-global tests)
        with open(filename, "a") as metadata_file:
            metadata_file.write(file_contents)

    def get_test_names_to_skip(self):
        """Determines which tests in the expectation file need metadata.

        Returns:
            A list of test names that need metadata.
        """
        return self.expectations.get_tests_with_result_type(
            test_expectations.SKIP)

    def get_test_names_to_fail(self):
        """Determines which tests should be expected to fail.

        This is currently tests with baseline files containing failing subtests.
        Failing subtests are those with statuses in (FAIL, NOTRUN, TIMEOUT).

        Returns:
            A list of test names that need metadata.
        """
        all_tests = self.port.tests(paths=['external/wpt'])
        failing_baseline_tests = []
        for test in all_tests:
            test_baseline = self.port.expected_text(test)
            if not test_baseline:
                continue
            if re.search("^(FAIL|NOTRUN|TIMEOUT)", test_baseline, re.MULTILINE):
                failing_baseline_tests.append(test)
            else:
                # Treat this as an error because we don't want it to happen.
                # Either the non-FAIL statuses need to be handled here, or the
                # baseline is all PASS which should just be deleted.
                _log.error("Test %s has a non-FAIL baseline" % test)
        return failing_baseline_tests

    def get_metadata_filename_and_contents(self, chromium_test_name, test_status):
        """Determines the metadata filename and contents for the specified test.

        The metadata filename is derived from the test name but will differ if
        the expectation is for a single test or for a directory of tests. The
        contents of the metadata file will also differ for those two cases.

        Args:
            chromium_test_name: A Chromium test name from the expectation file,
                which starts with `external/wpt`.
            test_status: The expected status of this test. Possible values:
                'SKIP' - skip this test (or directory).
                'FAIL' - the test is expected to fail, not applicable to dirs.

        Returns:
            A pair of strings, the first is the path to the metadata file and
            the second is the contents to write to that file. Or None if the
            test does not need a metadata file.
        """
        assert test_status in ('SKIP', 'FAIL')

        # Ignore expectations for non-WPT tests
        if not chromium_test_name or not chromium_test_name.startswith('external/wpt'):
            return None, None

        # Split the test name by directory. We omit the first 2 entries because
        # they are 'external' and 'wpt' and these don't exist in the WPT's test
        # names.
        wpt_test_name_parts = chromium_test_name.split("/")[2:]
        # The WPT test name differs from the Chromium test name in that the WPT
        # name omits `external/wpt`.
        wpt_test_name = "/".join(wpt_test_name_parts)

        # Check if this is a test file or a test directory
        is_test_dir = chromium_test_name.endswith("/")
        metadata_filename = None
        metadata_file_contents = None
        if is_test_dir:
            # A test directory gets one metadata file called __dir__.ini and all
            # tests in that dir are skipped.
            metadata_filename = os.path.join(self.metadata_output_dir,
                                             *wpt_test_name_parts)
            metadata_filename = os.path.join(metadata_filename, "__dir__.ini")
            _log.debug("Creating a dir-wide ini file %s", metadata_filename)

            metadata_file_contents = self._get_dir_disabled_string()
        else:
            # For individual tests, we create one file per test, with the name
            # of the test in the file as well. This name can contain variants.
            test_file_path = self.wpt_manifest.file_path_for_test_url(wpt_test_name)
            if not test_file_path:
                _log.info("Could not find file for test %s, skipping" % wpt_test_name)
                return None, None
            test_file_parts = test_file_path.split("/")

            # Append `.ini` to the test filename to indicate it's the metadata
            # file. The `test_filename` can differ from the `wpt_test_name` for
            # multi-global tests.
            test_file_parts[-1] += ".ini"
            metadata_filename = os.path.join(self.metadata_output_dir,
                                             *test_file_parts)
            _log.debug("Creating a test ini file %s with status %s",
                       metadata_filename, test_status)

            # The contents of the metadata file is two lines:
            # 1. the last part of the WPT test path (ie the filename) inside
            #    square brackets - this could differ from the metadata filename.
            # 2. an indented line with the test status and reason
            wpt_test_file_name_part = wpt_test_name_parts[-1]
            if test_status == 'SKIP':
                metadata_file_contents = self._get_test_disabled_string(wpt_test_file_name_part)
            elif test_status == 'FAIL':
                metadata_file_contents = self._get_test_failed_string(wpt_test_file_name_part)

        return metadata_filename, metadata_file_contents

    def _get_dir_disabled_string(self):
        return "disabled: wpt_metadata_builder.py\n"

    def _get_test_disabled_string(self, test_name):
        return "[%s]\n  disabled: wpt_metadata_builder.py\n" % test_name

    def _get_test_failed_string(self, test_name):
        return "[%s]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n" % test_name
