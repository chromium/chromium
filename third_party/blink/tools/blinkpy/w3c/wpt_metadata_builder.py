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
import fnmatch
import logging
import os
import re

from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models.typ_types import ResultType
from collections import defaultdict

_log = logging.getLogger(__name__)

# Define some status bitmasks for combinations of statuses a test could exhibit.
# The test has a harness error in its baseline file.
HARNESS_ERROR = 1
# The test has at least one failing subtest in its baseline file.
SUBTEST_FAIL = 1 << 1  # 2
# The test should be skipped
SKIP_TEST = 1 << 2  # 4
# The test passes - this typically appears alongside another status indicating
# a flaky test.
TEST_PASS = 1 << 3  # 8
# The test fails
TEST_FAIL = 1 << 4  # 16
# The test times out
TEST_TIMEOUT = 1 << 5  # 32
# The test crashes
TEST_CRASH = 1 << 6  # 64
# The test failed a precondition assertion
TEST_PRECONDITION_FAILED = 1 << 7  # 128


class WPTMetadataBuilder(object):
    def __init__(self, expectations, port):
        """
        Args:
            expectations: a blinkpy.web_tests.models.test_expectations.TestExpectations object
            port: a blinkpy.web_tests.port.Port object
        """
        self.expectations = expectations
        self.port = port
        # TODO(lpz): Use self.fs everywhere in this class and add tests
        self.fs = FileSystem()
        self.wpt_manifest = self.port.wpt_manifest("external/wpt")
        self.metadata_output_dir = ""
        self.checked_in_metadata_dir = ""
        self.process_baselines = True
        self.handle_annotations = True

    def run(self, args=None):
        """Main entry point to parse flags and execute the script."""
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            "--metadata-output-dir",
            help="The directory to output the metadata files into.")
        parser.add_argument(
            "--checked-in-metadata-dir",
            help="Root directory of any checked-in WPT metadata files to use. "
            "If set, these files will take precedence over legacy expectations "
            "and baselines when both exist for a test.")
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='More verbose logging.')
        parser.add_argument(
            "--process-baselines",
            action="store_true",
            default=True,
            dest="process_baselines",
            help="Whether to translate baseline (-expected.txt) files into WPT "
            "metadata files. This translation is lossy and results in any "
            "subtest being accepted by wptrunner.")
        parser.add_argument("--no-process-baselines",
                            action="store_false",
                            dest="process_baselines")
        parser.add_argument(
            "--handle-annotations",
            action="store_true",
            default=True,
            dest="handle_annotations",
            help="Whether to handle annotations in expectations files. These "
            "are trailing comments that give additional details for how "
            "to translate an expectation into WPT metadata.")
        parser.add_argument("--no-handle-annotations",
                            action="store_false",
                            dest="handle_annotations")
        args = parser.parse_args(args)

        log_level = logging.DEBUG if args.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        self.metadata_output_dir = args.metadata_output_dir
        self.checked_in_metadata_dir = args.checked_in_metadata_dir
        self.process_baselines = args.process_baselines
        self.handle_annotations = args.handle_annotations
        self._build_metadata_and_write()

        return 0

    @staticmethod
    def status_bitmap_to_string(test_status_bitmap):
        statuses = []
        result = ""
        if test_status_bitmap & SUBTEST_FAIL:
            result += "  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n"

        if test_status_bitmap & HARNESS_ERROR:
            statuses.append("ERROR")
        if test_status_bitmap & TEST_PASS:
            # We need both PASS and OK. Reftests will PASS while testharness
            # tests are OK.
            statuses.append("PASS")
            statuses.append("OK")
        if test_status_bitmap & TEST_FAIL:
            # We need both FAIL and ERROR. Reftests will FAIL while testharness
            # tests have ERRORs.
            statuses.append("FAIL")
            statuses.append("ERROR")
        if test_status_bitmap & TEST_TIMEOUT:
            statuses.append("TIMEOUT")
        if test_status_bitmap & TEST_CRASH:
            statuses.append("CRASH")
        if test_status_bitmap & TEST_PRECONDITION_FAILED:
            statuses.append("PRECONDITION_FAILED")

        if statuses:
            result += "  expected: [%s]\n" % ", ".join(statuses)
        return result

    def _build_metadata_and_write(self):
        """Build the metadata files and write them to disk."""
        if os.path.exists(self.metadata_output_dir):
            _log.debug("Output dir exists, deleting: %s",
                       self.metadata_output_dir)
            import shutil
            shutil.rmtree(self.metadata_output_dir)

        tests_for_metadata = self.get_tests_needing_metadata()
        _log.info("Found %d tests requiring metadata", len(tests_for_metadata))
        for test_name, test_status_bitmap in tests_for_metadata.items():
            filename, file_contents = self.get_metadata_filename_and_contents(
                test_name, test_status_bitmap)
            if not filename or not file_contents:
                continue
            self._write_to_file(filename, file_contents)

        if self.checked_in_metadata_dir and os.path.exists(
                self.checked_in_metadata_dir):
            _log.info("Copying checked-in WPT metadata on top of translated "
                      "files.")
            self._copy_checked_in_metadata()
        else:
            _log.warning("Not using checked-in WPT metadata, path is empty or "
                         "does not exist: %s" % self.checked_in_metadata_dir)

        # Finally, output a stamp file with the same name as the output
        # directory. The stamp file is empty, it's only used for its mtime.
        # This makes the GN build system happy (see crbug.com/995112).
        with open(self.metadata_output_dir + ".stamp", "w"):
            pass

    def _copy_checked_in_metadata(self):
        """Copies checked-in metadata files to the metadata output directory."""
        for filename in self.fs.files_under(self.checked_in_metadata_dir):
            # We match any .ini files in the path. This will find .ini files
            # other than just metadata (such as tox.ini), but that is ok
            # since wptrunner will just ignore those.
            if not fnmatch.fnmatch(filename, "*.ini"):
                continue

            # Found a checked-in .ini file. Copy it to the metadata output
            # directory in the same sub-path as where it is checked in.
            # So /checked/in/a/b/c.ini goes to /metadata/out/a/b/c.ini
            output_path = filename.replace(self.checked_in_metadata_dir,
                                           self.metadata_output_dir)
            if not self.fs.exists(self.fs.dirname(output_path)):
                self.fs.maybe_make_directory(self.fs.dirname(output_path))
            _log.debug("Copying %s to %s" % (filename, output_path))
            self.fs.copyfile(filename, output_path)

    def _write_to_file(self, filename, file_contents):
        # Write the contents to the file name
        if not os.path.exists(os.path.dirname(filename)):
            os.makedirs(os.path.dirname(filename))
        # Note that we append to the metadata file in order to allow multiple
        # tests to be present in the same .ini file (ie: for multi-global tests)
        with open(filename, "a") as metadata_file:
            metadata_file.write(file_contents)

    def get_tests_needing_metadata(self):
        """Determines which tests need metadata files.

        This function loops over the tests to be run and checks whether each test
        has an expectation (eg: in TestExpectations) and/or a baseline (ie:
        test-name-expected.txt). The existence of those things will determine
        the information that will be emitted into the tests's metadata file.

        Returns:
            A dict. The key is the string test name and the value is an integer
            bitmap of statuses for the test.
        """
        tests_needing_metadata = defaultdict(int)
        for test_name in self.port.tests(paths=["external/wpt"]):
            # First check for expectations. If a test is skipped then we do not
            # look for more statuses
            expectation_line = self.expectations.get_expectations(test_name)
            self._handle_test_with_expectation(test_name, expectation_line,
                                               tests_needing_metadata)
            if self._test_was_skipped(test_name, tests_needing_metadata):
                # Do not consider other statuses if a test is skipped
                continue

            # Check if the test has a baseline
            if self.process_baselines:
                test_baseline = self.port.expected_text(test_name)
                if not test_baseline:
                    continue
                self._handle_test_with_baseline(test_name, test_baseline,
                                                tests_needing_metadata)
        return tests_needing_metadata

    def _handle_test_with_expectation(self, test_name, expectation_line,
                                      status_dict):
        """Handles a single test expectation and updates |status_dict|."""
        test_statuses = expectation_line.results
        annotations = expectation_line.trailing_comments
        if ResultType.Skip in test_statuses:
            # Skips are handled alone, so don't look at any other statuses
            status_dict[test_name] |= SKIP_TEST
            return

        # Guard against the only test_status being Pass (without any
        # annotations), we don't want to create metadata for such a test.
        if (len(test_statuses) == 1 and ResultType.Pass in test_statuses
                and not annotations):
            return

        status_bitmap = 0
        if ResultType.Pass in test_statuses:
            status_bitmap |= TEST_PASS
        if ResultType.Failure in test_statuses:
            status_bitmap |= TEST_FAIL
        if ResultType.Timeout in test_statuses:
            status_bitmap |= TEST_TIMEOUT
        if ResultType.Crash in test_statuses:
            status_bitmap |= TEST_CRASH
        if self.handle_annotations and annotations:
            if "wpt_subtest_failure" in annotations:
                status_bitmap |= SUBTEST_FAIL
            if "wpt_precondition_failed" in annotations:
                status_bitmap |= TEST_PRECONDITION_FAILED
        # Update status bitmap for this test
        status_dict[test_name] |= status_bitmap

    def _test_was_skipped(self, test_name, status_dict):
        """Returns whether |test_name| is marked as skipped in |status_dict|."""
        return test_name in status_dict and (
            status_dict[test_name] & SKIP_TEST)

    def _handle_test_with_baseline(self, test_name, test_baseline,
                                   status_dict):
        """Handles a single test baseline and updates |status_dict|."""
        status_bitmap = 0
        if re.search(r"^(FAIL|NOTRUN|TIMEOUT)", test_baseline, re.MULTILINE):
            status_bitmap |= SUBTEST_FAIL
        if re.search(r"^Harness Error\.", test_baseline, re.MULTILINE):
            status_bitmap |= HARNESS_ERROR
        if status_bitmap > 0:
            status_dict[test_name] |= status_bitmap
        else:
            # Treat this as an error because we don't want it to happen.
            # Either the non-FAIL statuses need to be handled here, or the
            # baseline is all PASS which should just be deleted.
            _log.error("Test %s has a non-FAIL baseline" % test_name)

    def _metadata_filename_from_test_file(self, wpt_test_file):
        """Returns the filename of the metadata (.ini) file for the test.

        Args:
            wpt_test_file: The file on disk that the specified test lives in.
                For multi-global tests this is usually a ".js" file.

        Returns:
            The fully-qualified string path of the metadata file for this test.
        """
        assert "?" not in wpt_test_file
        test_file_parts = wpt_test_file.split("/")
        return os.path.join(self.metadata_output_dir,
                            *test_file_parts) + ".ini"

    def _metadata_inline_test_name_from_test_name(self, wpt_test_name):
        """Returns the test name to use *inside* of a metadata file.

        The inline name inside the metadata file is the logical name of the
        test without any subdirectories.
        For multi-global tests this means that it must have the specific scope
        of the test (eg: worker, window, etc). This name must also include any
        variants that are set.

        Args:
            wpt_test_name: The fully-qualified test name which contains all
                subdirectories as well as scope (for multi-globals), and
                variants.

        Returns:
            The string test name inside of the metadata file.
        """
        # To generate the inline test name we basically want to strip away the
        # subdirectories from the test name, being careful not to accidentally
        # clobber the variant.
        variant_split = wpt_test_name.split("?")
        test_path = variant_split[0]
        test_name_part = test_path.split("/")[-1]
        variant = "?" + variant_split[1] if len(variant_split) == 2 else ""
        return test_name_part + variant

    def get_metadata_filename_and_contents(self,
                                           chromium_test_name,
                                           test_status_bitmap=0):
        """Determines the metadata filename and contents for the specified test.

        The metadata filename is derived from the test name but will differ if
        the expectation is for a single test or for a directory of tests. The
        contents of the metadata file will also differ for those two cases.

        Args:
            chromium_test_name: A Chromium test name from the expectation file,
                which starts with `external/wpt`.
            test_status_bitmap: An integer containing additional data about the
                status, such as enumerating flaky statuses, or whether a test has
                a combination of harness error and subtest failure.

        Returns:
            A pair of strings, the first is the path to the metadata file and
            the second is the contents to write to that file. Or None if the
            test does not need a metadata file.
        """
        # Ignore expectations for non-WPT tests
        if (not chromium_test_name
                or not chromium_test_name.startswith('external/wpt')):
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
            # of the test in the file as well.
            test_file_path = self.wpt_manifest.file_path_for_test_url(
                wpt_test_name)
            if not test_file_path:
                _log.info("Could not find file for test %s, skipping" %
                          wpt_test_name)
                return None, None

            metadata_filename = self._metadata_filename_from_test_file(
                test_file_path)
            _log.debug("Creating a test ini file %s with status_bitmap %s",
                       metadata_filename, test_status_bitmap)
            inline_test_name = self._metadata_inline_test_name_from_test_name(
                wpt_test_name)
            metadata_file_contents = self._get_test_failed_string(
                inline_test_name, test_status_bitmap)

        return metadata_filename, metadata_file_contents

    def _get_dir_disabled_string(self):
        return "disabled: wpt_metadata_builder.py\n"

    def _get_test_disabled_string(self, test_name):
        return "[%s]\n  disabled: wpt_metadata_builder.py\n" % test_name

    def _get_test_failed_string(self, inline_test_name, test_status_bitmap):
        # The contents of the metadata file is two lines:
        # 1. the inline name of the WPT test pathinside square brackets. This
        #    name contains the test scope (for multi-globals) and variants.
        # 2. an indented line with the test status and reason
        result = "[%s]\n" % inline_test_name

        # A skipped test is a little special in that it doesn't happen along with
        # any other status. So we compare directly against SKIP_TEST and also
        # return right away.
        if test_status_bitmap == SKIP_TEST:
            result += "  disabled: wpt_metadata_builder.py\n"
            return result

        # Other test statuses can exist together. But ensure we have at least one.
        expected_string = self.status_bitmap_to_string(test_status_bitmap)
        if expected_string:
            result += expected_string
        return result
