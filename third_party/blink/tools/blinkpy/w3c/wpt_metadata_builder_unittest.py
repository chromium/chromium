# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import json
import os
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_metadata_builder import (
    WPTMetadataBuilder,
    HARNESS_ERROR,
    SKIP_TEST,
    SUBTEST_FAIL,
    TEST_FAIL,
    TEST_PASS,
    TEST_TIMEOUT,
    TEST_PRECONDITION_FAILED,
    USE_CHECKED_IN_METADATA,
)


def _append_to_expectation_dict(exp_dict,
                                exp_file,
                                test_path,
                                test_statuses,
                                is_default_pass=False,
                                trailing_comments=""):
    """Appends expectation lines to an expectation dict.

    Allows creating multi-layered expectations spread across several files.

    Args:
        ordered_dict: an OrderedDict to append to
        exp_file: str, name of the expectation file (eg: NeverFixTests, FooTests)
        test_path: str, the path to set expectations for
        test_status: str, the statuses of the test
        is_default_pass: bool, is_default_pass flag for the new expectation
        trailing_comments: str, comments at the end of the expectation line.
    """
    if exp_file not in exp_dict:
        exp_dict[exp_file] = "# results: [ Pass Failure Timeout Crash Skip ]\n"
    if is_default_pass:
        return
    exp_dict[exp_file] += ("%s [ %s ]%s\n" %
                           (test_path, test_statuses, trailing_comments))


def _make_expectation_with_dict(port, expectation_dict):
    """Creates an expectation object from an expectation dict.

    Args:
        port: the port to run against
        expectation_dict: an OrderedDict containing expectation files and their
            contents

    Returns:
        An expectation object with the contents of the dict.
    """
    return TestExpectations(port, expectations_dict=expectation_dict)


def _make_expectation(port,
                      test_path,
                      test_statuses,
                      is_default_pass=False,
                      trailing_comments=""):
    """Creates an expectation object for a single test or directory.

    Args:
        port: the port to run against
        test_path: str, the path to set expectations for
        test_status: str, the statuses of the test
        is_default_pass: bool, is_default_pass flag for the new expectation
        trailing_comments: str, comments at the end of the expectation line.

    Returns:
        An expectation object with the given test and statuses.
    """
    expectation_dict = OrderedDict()
    _append_to_expectation_dict(expectation_dict, "expectations", test_path,
                                test_statuses, is_default_pass, trailing_comments)
    return _make_expectation_with_dict(port, expectation_dict)


class WPTMetadataBuilderTest(unittest.TestCase):
    def setUp(self):
        self.num = 2
        self.host = MockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.port = self.host.port_factory.get()

        # Write a dummy manifest file, describing what tests exist.
        self.host.filesystem.write_text_file(
            self.port.web_tests_dir() + '/external/' + BASE_MANIFEST_NAME,
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['reftest-ref.html', '==']], {}],
                        ]
                    },
                    'testharness': {
                        'test.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}]
                        ],
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
                        'dir': {
                            'zzzz.html': [
                                '03ada7aa0d4d43811652fc679a00a41b9653013d',
                                [None, {}]
                            ],
                            'multiglob.https.any.js': [
                                'd6498c3e388e0c637830fa080cca78b0ab0e5305',
                                ['dir/multiglob.https.any.window.html', {}],
                                ['dir/multiglob.https.any.worker.html', {}],
                            ],
                        },
                    },
                    'manual': {
                        'x-manual.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            [None, {}]
                        ],
                    },
                },
            }))

    def test_non_wpt_test(self):
        """A non-WPT test should not get any metadata."""
        test_name = "some/other/test.html"
        expectations = _make_expectation(self.port, test_name, "Skip")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SKIP_TEST)
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_without_manifest_entry(self):
        """A WPT test that is not in the manifest should not get a baseline."""
        test_name = "external/wpt/test-not-in-manifest.html"
        expectations = _make_expectation(self.port, test_name, "Skip")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SKIP_TEST)
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_not_skipped(self):
        """A WPT test that is not skipped should not get a SKIP metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "Timeout")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_names = metadata_builder.get_tests_needing_metadata()
        # The test will appear in the result but won't have a SKIP status
        found = False
        for name_item, status_item in test_names.items():
            if name_item == test_name:
                found = True
                self.assertNotEqual(SKIP_TEST, status_item)
        self.assertTrue(found)

    def test_same_metadata_file_for_variants(self):
        """Variants of a test all go in the same metadata file."""
        test_name1 = "external/wpt/variant.html?foo=bar/abc"
        test_name2 = "external/wpt/variant.html?foo=baz"
        expectation_dict = OrderedDict()
        _append_to_expectation_dict(expectation_dict, "TestExpectations",
                                    test_name1, "Failure")
        _append_to_expectation_dict(expectation_dict, "TestExpectations",
                                    test_name2, "Timeout")
        expectations = _make_expectation_with_dict(self.port, expectation_dict)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        metadata_builder.metadata_output_dir = "out"
        metadata_builder.fs = MockFileSystem()
        metadata_builder._build_metadata_and_write()

        # Both the tests go into the same metadata file, named without any
        # variants.
        metadata_file = os.path.join("out", "variant.html.ini")
        with metadata_builder.fs.open_text_file_for_reading(
                metadata_file) as f:
            data = f.read()
        # Inside the metadata file, we have separate entries for each variant
        self.assertEqual(
            "[variant.html?foo=bar/abc]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n"
            "  expected: [FAIL, ERROR]\n"
            "[variant.html?foo=baz]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n"
            "  expected: [TIMEOUT]\n",
            data)

    def test_parse_baseline_all_pass(self):
        """A WPT test with an all-pass baseline doesn't get metadata."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nPASS another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertFalse(test_and_status_dict)

    def test_parse_baseline_subtest_fail(self):
        """Test parsing a baseline with a failing subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nFAIL another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_notrun(self):
        """Test parsing a baseline with a notrun subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nNOTRUN another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_timeout(self):
        """Test parsing a baseline with a timeout subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nTIMEOUT some subtest\nPASS another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_harness_error(self):
        """Test parsing a baseline with a harness error."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename, "This is a test\nHarness Error. some stuff\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(HARNESS_ERROR, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_fail_and_harness_error(self):
        """Test parsing a baseline with a harness error AND a subtest fail."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nHarness Error. some stuff\nPASS some subtest\nFAIL another subtest\n"
        )
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL | HARNESS_ERROR,
                         test_and_status_dict[test_name])

    def test_metadata_for_flaky_test(self):
        """A WPT test that is flaky has multiple statuses in metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "Pass Failure")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, TEST_PASS | TEST_FAIL)
        self.assertEqual("test.html.ini", filename)
        # The PASS and FAIL expectations fan out to also include OK and ERROR
        # to support reftest/testharness test differences.
        self.assertEqual(
            "[test.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [PASS, OK, FAIL, ERROR]\n",
            contents)

    def test_metadata_for_skipped_test(self):
        """A skipped WPT test should get a test-specific metadata file."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "Skip")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SKIP_TEST)
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  disabled: wpt_metadata_builder.py\n",
                         contents)

    def test_metadata_for_skipped_test_with_variants(self):
        """A skipped WPT tests with variants should get a test-specific metadata file."""
        test_name = "external/wpt/variant.html?foo=bar/abc"
        expectations = _make_expectation(self.port, test_name, "Skip")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SKIP_TEST)
        # The metadata file name should not include variants
        self.assertEqual("variant.html.ini", filename)
        # ..but the contents of the file should include variants in the test name
        self.assertEqual(
            "[variant.html?foo=bar/abc]\n  disabled: wpt_metadata_builder.py\n",
            contents)

    def test_metadata_for_skipped_directory(self):
        """A skipped WPT directory should get a dir-wide metadata file."""
        test_dir = "external/wpt/test_dir/"
        expectations = _make_expectation(self.port, test_dir, "Skip")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_dir, SKIP_TEST)
        self.assertEqual(os.path.join("test_dir", "__dir__.ini"), filename)
        self.assertEqual("disabled: wpt_metadata_builder.py\n", contents)

    def test_metadata_for_wpt_test_with_fail_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SUBTEST_FAIL)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_metadata_for_wpt_test_with_harness_error_baseline(self):
        """A WPT test with a baseline file containing a harness error gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, HARNESS_ERROR)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [ERROR]\n",
            contents)

    def test_metadata_for_wpt_test_with_harness_error_and_subtest_fail_baseline(
            self):
        """A WPT test with a baseline file containing a harness error and subtest failure gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SUBTEST_FAIL | HARNESS_ERROR)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [ERROR]\n",
            contents)

    def test_metadata_for_wpt_multiglobal_test_with_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/multiglob.https.any.window.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, SUBTEST_FAIL)
        # The metadata filename matches the test *filename*, not the test name,
        # which in this case is the js file from the manifest.
        self.assertEqual(
            os.path.join("dir", "multiglob.https.any.js.ini"), filename)
        # The metadata contents contains the *test name*
        self.assertEqual(
            "[multiglob.https.any.window.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_metadata_for_precondition_failed(self):
        """A WPT ttest hat fails a precondition."""
        test_name = "external/wpt/test.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, TEST_PRECONDITION_FAILED)
        self.assertEqual("test.html.ini", filename)
        # The PASS and FAIL expectations fan out to also include OK and ERROR
        # to support reftest/testharness test differences.
        self.assertEqual(
            "[test.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [PRECONDITION_FAILED]\n",
            contents)

    def test_metadata_for_use_checked_in_metadata_annotation(self):
        """A WPT test annotated to use checked-in metadata."""
        test_name = "external/wpt/test.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, USE_CHECKED_IN_METADATA)
        # If the test is using the metadata that is checked-in, then there is no
        # work to be done by the metadata builder.
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_parse_subtest_failure_annotation(self):
        """Check that we parse the wpt_subtest_failure annotation correctly."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(
            self.port,
            test_name,
            "Pass",
            trailing_comments=" # wpt_subtest_failure")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_PASS | SUBTEST_FAIL,
                         test_and_status_dict[test_name])

    def test_parse_precondition_failure_annotation(self):
        """Check that we parse the wpt_precondition_failed annotation correctly."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(
            self.port,
            test_name,
            "Pass",
            trailing_comments=" # wpt_precondition_failed")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_PASS | TEST_PRECONDITION_FAILED,
                         test_and_status_dict[test_name])

    def test_metadata_filename_from_test_file(self):
        """Check that we get the correct metadata filename in various cases."""
        expectations = TestExpectations(self.port)
        mb = WPTMetadataBuilder(expectations, self.port)
        self.assertEqual("test.html.ini",
                         mb._metadata_filename_from_test_file("test.html"))
        test_file = os.path.join("dir", "multiglob.https.any.js")
        self.assertEqual(test_file + ".ini",
                         mb._metadata_filename_from_test_file(test_file))
        with self.assertRaises(AssertionError):
            mb._metadata_filename_from_test_file("test.html?variant=abc")

    def test_inline_test_name_from_test_name(self):
        """Check that we get the correct inline test name in various cases."""
        expectations = TestExpectations(self.port)
        mb = WPTMetadataBuilder(expectations, self.port)
        self.assertEqual(
            "test.html",
            mb._metadata_inline_test_name_from_test_name("test.html"))
        self.assertEqual(
            "test.html",
            mb._metadata_inline_test_name_from_test_name("dir/test.html"))
        self.assertEqual(
            "test.html?variant=abc",
            mb._metadata_inline_test_name_from_test_name(
                "dir/test.html?variant=abc"))
        self.assertEqual(
            "test.html?variant=abc/def",
            mb._metadata_inline_test_name_from_test_name(
                "dir/test.html?variant=abc/def"))
        self.assertEqual(
            "test.worker.html",
            mb._metadata_inline_test_name_from_test_name("test.worker.html"))
        self.assertEqual(
            "test.worker.html?variant=abc",
            mb._metadata_inline_test_name_from_test_name(
                "dir/test.worker.html?variant=abc"))

    def test_copy_checked_in_metadata(self):
        # Ensure that ini metadata files are copied from the checked-in dir to
        # the output dir as expected.
        expectations = TestExpectations(self.port)
        mb = WPTMetadataBuilder(expectations, self.port)
        # Set the metadata builder to use mock filesystem populated with some
        # test data
        mb.checked_in_metadata_dir = "src"
        mb.metadata_output_dir = "out"
        mock_checked_in_files = {
            "src/a/b/c.html": b"",
            "src/a/b/c.html.ini": b"",
            "src/a/d/e.html": b"",
            "src/a/d/e.html.ini": b"checked-in",
            "src/a/tox.ini": b"",

            # Put one duplicate file in the output directory to simulate a test
            # with both legacy expectations and checked-in metadata
            "out/a/d/e.html.ini": b"legacy",
        }
        mb.fs = MockFileSystem(files=mock_checked_in_files)

        # Ensure that the duplicate file starts out with the legacy content.
        duplicate_ini_file = "out/a/d/e.html.ini"
        self.assertEqual("legacy", mb.fs.read_text_file(duplicate_ini_file))

        mb._copy_checked_in_metadata()

        # Ensure only the ini files are copied, not the tests
        self.assertEqual(3, len(mb.fs.written_files))
        self.assertTrue("out/a/b/c.html.ini" in mb.fs.written_files)
        self.assertTrue("out/a/d/e.html.ini" in mb.fs.written_files)
        self.assertTrue("out/a/tox.ini" in mb.fs.written_files)

        # Also ensure that the content of the duplicate file was overwritten
        # with the checked-in contents.
        self.assertEqual("checked-in",
                         mb.fs.read_text_file(duplicate_ini_file))

    def test_baseline_and_expectation(self):
        """A test has a failing baseline and a timeout expectation."""
        test_name = "external/wpt/test.html"
        # Create a baseline with a failing subtest, and a TIMEOUT expectation
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nFAIL some subtest\nPASS another subtest\n")
        expectations = _make_expectation(self.port, test_name, "Timeout")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_TIMEOUT, test_and_status_dict[test_name])

    def test_expectations_across_files(self):
        """Check the inheritance order of expectations across several files."""
        test_name = "external/wpt/test.html"
        expectation_dict = OrderedDict()
        _append_to_expectation_dict(expectation_dict, "TestExpectations",
                                    test_name, "Failure")
        _append_to_expectation_dict(expectation_dict,
                                    "WPTOverrideExpectations", test_name,
                                    "Timeout")
        expectations = _make_expectation_with_dict(self.port, expectation_dict)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        # The test statuses from the two files are combined
        self.assertEqual(TEST_FAIL | TEST_TIMEOUT,
                         test_and_status_dict[test_name])

    def test_overriding_skip_expectation_no_annotation(self):
        """Check how a skipped test (in NeverFix) interacts with an override.

        In this case, only the SKIP status is translated since skipped tests are
        not combined with any other statuses. This is because wpt metadata
        requires a special 'disabled' keyword to skip tests, it's not just
        another status in the expected status list like in Chromium.
        """
        test_name = "external/wpt/test.html"
        expectation_dict = OrderedDict()
        _append_to_expectation_dict(expectation_dict, "NeverFixTests",
                                    test_name, "Skip")
        _append_to_expectation_dict(expectation_dict,
                                    "WPTOverrideExpectations", test_name,
                                    "Failure")
        expectations = _make_expectation_with_dict(self.port, expectation_dict)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        # Skip statuses always live alone. The override status is ignored
        # because it does not have an annotation to use checked-in metadata.
        self.assertEqual(SKIP_TEST, test_and_status_dict[test_name])

    def test_overriding_skip_expectation_with_annotation(self):
        """A skipped tests is overridden by an annotation.

        In this case, the SKIP status gets ignored because the status line in
        the override file has the 'wpt_use_checked_in_metadata' annotation. This
        forces us to prioritize the checked-in metadata over whatever is in
        any of the expectation files.
        """
        test_name = "external/wpt/test.html"
        expectation_dict = OrderedDict()
        _append_to_expectation_dict(expectation_dict, "NeverFixTests",
                                    test_name, "Skip")
        _append_to_expectation_dict(
            expectation_dict,
            "WPTOverrideExpectations",
            test_name,
            "Pass",
            trailing_comments=" # wpt_use_checked_in_metadata")
        expectations = _make_expectation_with_dict(self.port, expectation_dict)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        # Skip statuses always live alone. The override status is ignored
        # because it does not have an annotation to use checked-in metadata.
        self.assertEqual(USE_CHECKED_IN_METADATA,
                         test_and_status_dict[test_name])

    def test_expectation_overwrites_checked_in_metadata(self):
        """Test an entry in an expectation overwriting checked-in metadata.

        When an expectation has no annotation to use checked-in metadata then
        the expectation will overwrite any checked-in metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "Timeout")
        mb = WPTMetadataBuilder(expectations, self.port)
        # Set the metadata builder to use mock filesystem populated with some
        # test data
        mb.checked_in_metadata_dir = "src"
        mb.metadata_output_dir = "out"
        mock_checked_in_files = {
            "src/external/wpt/test.html": "",
            "src/external/wpt/test.html.ini": "checked-in metadata",
        }
        mb.fs = MockFileSystem(files=mock_checked_in_files)

        mb._build_metadata_and_write()
        # Ensure that the data written to the metadata file is the translated
        # status, not the checked-in contents.
        resulting_ini_file = os.path.join("out", "test.html.ini")
        with mb.fs.open_text_file_for_reading(resulting_ini_file) as f:
            data = f.read()
        self.assertEqual(
            "[test.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [TIMEOUT]\n",
            data)

    def test_use_subtest_results_flag_with_expectation(self):
        """Test that the --use-subtest-results flag updates metadata correctly.

        The --use-subtest-results flag should result in the
        blink_expect_any_subtest_status tag not being applied to metadata for
        any tests."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "Failure")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        metadata_builder.use_subtest_results = True
        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, TEST_FAIL)
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  expected: [FAIL, ERROR]\n", contents)

    def test_use_subtest_results_flag_with_baseline_and_timeout(self):
        """If a test has both baseline and a non-default-pass expectation, do not
        derive expectation from the baseline"""
        # Create a baseline with a failing subtest, and a TIMEOUT expectation
        test_name = "external/wpt/test.html"
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nFAIL some subtest\nPASS another subtest\n")

        expectations = _make_expectation(self.port, test_name, "Timeout")

        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        metadata_builder.use_subtest_results = True

        test_and_status_dict = metadata_builder.get_tests_needing_metadata()

        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_TIMEOUT, test_and_status_dict[test_name])

        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, test_and_status_dict[test_name])
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  expected: [TIMEOUT]\n",
                         contents)

    def test_use_subtest_results_flag_with_baseline_and_default_pass(self):
        """A test may have a failing baseline because there are subtest failures.
        When the wptrunner see's the failing subtest it will return failure
        for the test since we are not setting expectations for the subtest in
        the metadata. However the expected result will be set to FAIL in the
        JSON results and the CI will stay green."""
        # Create a baseline with a failing subtest, and a default pass expectation
        test_name = "external/wpt/test.html"
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nFAIL some subtest\nPASS another subtest\n")
        expectations = _make_expectation(self.port, test_name,
                                         "Pass", is_default_pass=True)

        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        metadata_builder.use_subtest_results = True

        test_and_status_dict = metadata_builder.get_tests_needing_metadata()

        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, test_and_status_dict[test_name])
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  expected: [FAIL, ERROR]\n",
                         contents)

    def test_without_use_subtest_results_flag_with_baseline(self):
        """A test has a failing baseline."""
        test_name = "external/wpt/test.html"
        # Create a baseline with a failing subtest, and a TIMEOUT expectation
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nFAIL some subtest\nPASS another subtest\n")

        expectations = _make_expectation(self.port, test_name, "Timeout")

        metadata_builder = WPTMetadataBuilder(expectations, self.port)

        test_and_status_dict = metadata_builder.get_tests_needing_metadata()

        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_TIMEOUT,
                         test_and_status_dict[test_name])

        filename, contents = metadata_builder.get_metadata_filename_and_contents(
            test_name, test_and_status_dict[test_name])
        self.assertEqual("test.html.ini", filename)
        self.assertEqual(("[test.html]\n  blink_expect_any_subtest_status: "
                          "True # wpt_metadata_builder.py\n  "
                          "expected: [TIMEOUT]\n"), contents)
