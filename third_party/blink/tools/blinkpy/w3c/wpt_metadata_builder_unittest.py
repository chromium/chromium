# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import json
import os
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_metadata_builder import WPTMetadataBuilder


def _make_expectation(port, test_path, test_statuses, test_names=[]):
    """Creates an expectation object for a single test or directory.

    Args:
        port: the port to run against
        test_path: the path to set expectations for
        test_status: the statuses of the test
        test_names: a set of tests under the 'test_path'. Should be non-empty
            when the 'test_path' is a directory instead of a single test

    Returns:
        An expectation object with the given test and statuses.
    """
    expectation_dict = OrderedDict()
    expectation_dict["expectations"] = "Bug(test) %s [ %s ]" % (test_path, test_statuses)

    # When test_path is a dir, we expect test_names to be provided.
    is_dir = test_path.endswith('/')
    assert is_dir == bool(test_names)
    if not is_dir:
        test_names = [test_path]

    return TestExpectations(port, tests=test_names, expectations_dict=expectation_dict)


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
                            ['reftest.html', [['reftest-ref.html', '==']], {}]
                        ]
                    },
                    'testharness': {
                        'test.html': [['test.html', {}]],
                        'variant.html': [['variant.html?foo=bar', {}],
                                         ['variant.html?foo=baz', {}]],
                        'dir/zzzz.html': [['dir/zzzz.html', {}]],
                        'dir/multiglob.https.any.js': [
                            ['dir/multiglob.https.any.window.html', {}],
                            ['dir/multiglob.https.any.worker.html', {}],
                        ],
                    },
                    'manual': {
                        'x-manual.html': [['x-manual.html', {}]],
                    },
                },
            }))

    def test_skipped_test(self):
        """A skipped WPT test should get a test-specific metadata file."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'SKIP')
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  disabled: wpt_metadata_builder.py\n", contents)

    def test_skipped_test_with_variants(self):
        """A skipped WPT tests with variants should get a test-specific metadata file."""
        test_name = "external/wpt/variant.html?foo=bar"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'SKIP')
        # The metadata file name should not include variants
        self.assertEqual("variant.html.ini", filename)
        # ..but the contents of the file should include variants in the test name
        self.assertEqual("[variant.html?foo=bar]\n  disabled: wpt_metadata_builder.py\n", contents)

    def test_skipped_directory(self):
        """A skipped WPT directory should get a dir-wide metadata file."""
        test_dir = "external/wpt/test_dir/"
        test_name = "external/wpt/test_dir/test.html"
        expectations = _make_expectation(self.port, test_dir, "SKIP", test_names=[test_name])
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_dir, 'SKIP')
        self.assertEqual(os.path.join("test_dir", "__dir__.ini"), filename)
        self.assertEqual("disabled: wpt_metadata_builder.py\n", contents)

    def test_non_wpt_test(self):
        """A non-WPT test should not get any metadata."""
        test_name = "some/other/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'SKIP')
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_without_manifest_entry(self):
        """A WPT test that is not in the manifest should not get a baseline."""
        test_name = "external/wpt/test-not-in-manifest.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'SKIP')
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_not_skipped(self):
        """A WPT test that is not skipped should not get a SKIP metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "TIMEOUT")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_names = metadata_builder.get_test_names_to_skip()
        self.assertFalse(test_names)

    def test_wpt_test_with_fail_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = "external/wpt/dir/zzzz-expected.txt"
        self.host.filesystem.write_text_file(
            os.path.join(self.port.web_tests_dir(), baseline_filename),
            "This is a test\nPASS some subtest\nFAIL some failing subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'FAIL')
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_wpt_test_with_notrun_baseline(self):
        """A WPT test with a baseline file containing notrun test gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = "external/wpt/dir/zzzz-expected.txt"
        self.host.filesystem.write_text_file(
            os.path.join(self.port.web_tests_dir(), baseline_filename),
            "This is a test\nPASS some subtest\nNOTRUN some not run subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'FAIL')
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_wpt_test_with_timeout_baseline(self):
        """A WPT test with a baseline file containing timeouts gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = "external/wpt/dir/zzzz-expected.txt"
        self.host.filesystem.write_text_file(
            os.path.join(self.port.web_tests_dir(), baseline_filename),
            "This is a test\nPASS some subtest\nTIMEOUT some timeout subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'FAIL')
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_wpt_test_with_passing_baseline(self):
        """A WPT test with an all-pass baseline doesn't get metadata."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/test/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = "external/wpt/test/zzzz-expected.txt"
        self.host.filesystem.write_text_file(
            os.path.join(self.port.web_tests_dir(), baseline_filename),
            "This is a test\nPASS some subtest\nPASS another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_names = metadata_builder.get_test_names_to_fail()
        self.assertFalse(test_names)

    def test_wpt_multiglobal_test_with_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/multiglob.https.any.window.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = "external/wpt/dir/multiglob.https.any.window-expected.txt"
        self.host.filesystem.write_text_file(
            os.path.join(self.port.web_tests_dir(), baseline_filename),
            "This is a test\nPASS some subtest\nFAIL some failing subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, 'FAIL')
        # The metadata filename matches the test *filename*, not the test name,
        # which in this case is the js file from the manifest.
        self.assertEqual(os.path.join("dir", "multiglob.https.any.js.ini"), filename)
        # The metadata contents contains the *test name*
        self.assertEqual(
            "[multiglob.https.any.window.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)
