# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import os
import tempfile
from typing import Any
import unittest
import unittest.mock as mock
import urllib.error

from blinkpy.web_tests.flake_suppressor import web_tests_expectations

from pyfakefs import fake_filesystem_unittest  # pylint:disable=import-error


class GetExpectationFileForSuiteUnittest(unittest.TestCase):
    def setUp(self) -> None:
        self.expectations = (
            web_tests_expectations.WebTestsExpectationProcessor())

    def testRegularExpectationFile(self) -> None:
        """Tests that a regular expectation file is found properly."""
        expected_filepath = os.path.join(
            web_tests_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
            'TestExpectations')
        actual_filepath = self.expectations.GetExpectationFileForSuite(
            'pixel_integration_test', tuple(['arm-64']))
        self.assertEqual(actual_filepath, expected_filepath)


class GetOriginExpectationFileContentsUnittest(unittest.TestCase):
    class FakeRequestResult():
        def __init__(self):
            self.text = ''

        def read(self) -> str:
            return self.text

    def setUp(self) -> None:
        self.expectations = (
            web_tests_expectations.WebTestsExpectationProcessor())
        self._get_patcher = mock.patch(
            'flake_suppressor_common.expectations.urllib.request.urlopen')
        self._get_mock = self._get_patcher.start()
        self.addCleanup(self._get_patcher.stop)

    def testBasic(self) -> None:
        """Tests basic functionality along the happy path."""

        def SideEffect(
                url: str
        ) -> GetOriginExpectationFileContentsUnittest.FakeRequestResult:
            request_result = (
                GetOriginExpectationFileContentsUnittest.FakeRequestResult())
            text = ''
            if url.endswith('web_tests?format=TEXT'):
                text = """\
mode type hash TestExpectations
mode type hash bar_tests.txt"""
            elif url.endswith('TestExpectations?format=TEXT'):
                text = 'TestExpectations content'
            elif url.endswith('bar_tests.txt?format=TEXT'):
                text = 'bar_tests.txt content'
            else:
                self.fail('Given unhandled URL %s' % url)
            request_result.text = base64.b64encode(text.encode('utf-8'))
            return request_result

        self._get_mock.side_effect = SideEffect
        test_exp = os.path.join(
            web_tests_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY,
            'TestExpectations')
        expected_contents = {
            test_exp: 'TestExpectations content',
        }
        self.assertEqual(self.expectations.GetOriginExpectationFileContents(),
                         expected_contents)
        self.assertEqual(self._get_mock.call_count, 2)

    def testNonOkStatusCodesSurfaced(self) -> None:
        """Tests that getting a non-200 status code back results in a failure."""

        def SideEffect(_: Any) -> None:
            raise urllib.error.HTTPError('url', 404, 'No exist :(', {}, None)

        self._get_mock.side_effect = SideEffect
        with self.assertRaises(urllib.error.HTTPError):
            self.expectations.GetOriginExpectationFileContents()


class GetLocalCheckoutExpectationFileContentsUnittest(
        fake_filesystem_unittest.TestCase):
    def setUp(self) -> None:
        self.expectations = (
            web_tests_expectations.WebTestsExpectationProcessor())
        self.setUpPyfakefs()

    def testBasic(self) -> None:
        """Tests basic functionality."""
        os.makedirs(web_tests_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
        with open(
                os.path.join(
                    web_tests_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                    'TestExpectations'), 'w') as outfile:
            outfile.write('foo.txt contents')
        with open(
                os.path.join(
                    web_tests_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                    'bar.txt'), 'w') as outfile:
            outfile.write('bar.txt contents')
        test_exp = os.path.join(
            web_tests_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY,
            'TestExpectations')
        expected_contents = {
            test_exp: 'foo.txt contents',
        }
        self.assertEqual(
            self.expectations.GetLocalCheckoutExpectationFileContents(),
            expected_contents)


class FilterToMostSpecificTagTypeUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self) -> None:
        self._expectations = (
            web_tests_expectations.WebTestsExpectationProcessor())
        self.setUpPyfakefs()
        with tempfile.NamedTemporaryFile(delete=False) as tf:
            self.expectation_file = tf.name

    def testMultiline(self) -> None:
        """Tests that functionality works when tags cover multiple lines."""
        expectation_file_contents = """\
# tags: [ tag1_least_specific
#         tag1_middle_specific
#         tag1_most_specific ]
# tags: [ tag2_least_specific
#         tag2_middle_specific tag2_most_specific ]"""
        with open(self.expectation_file, 'w') as outfile:
            outfile.write(expectation_file_contents)

        tags = ('tag1_least_specific', 'tag1_middle_specific',
                'tag1_most_specific', 'tag2_middle_specific',
                'tag2_least_specific')
        filtered_tags = self._expectations.FilterToMostSpecificTypTags(
            tags, self.expectation_file)
        self.assertEqual(filtered_tags,
                         ('tag1_most_specific', 'tag2_middle_specific'))
