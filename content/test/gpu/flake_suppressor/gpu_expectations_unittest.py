#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
from email.message import EmailMessage
import os
from typing import Any
import unittest
import unittest.mock as mock
import urllib.error

from flake_suppressor import gpu_expectations

from pyfakefs import fake_filesystem_unittest  # pylint:disable=import-error


class GetExpectationFileForSuiteUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.expectations = gpu_expectations.GpuExpectationProcessor()

  def testRegularExpectationFile(self) -> None:
    """Tests that a regular expectation file is found properly."""
    expected_filepath = os.path.join(
        gpu_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'pixel_expectations.txt')
    actual_filepath = self.expectations.GetExpectationFileForSuite(
        'pixel_integration_test', tuple())
    self.assertEqual(actual_filepath, expected_filepath)

  def testOverrideExpectationFile(self) -> None:
    """Tests that an overridden expectation file is found properly."""
    expected_filepath = os.path.join(
        gpu_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'info_collection_expectations.txt')
    actual_filepath = self.expectations.GetExpectationFileForSuite(
        'info_collection_test', tuple())
    self.assertEqual(actual_filepath, expected_filepath)


class GetOriginExpectationFileContentsUnittest(unittest.TestCase):
  class FakeRequestResult():
    def __init__(self):
      self.text = ''

    def read(self) -> str:
      return self.text

  def setUp(self) -> None:
    self.expectations = gpu_expectations.GpuExpectationProcessor()
    self._get_patcher = mock.patch(
        'flake_suppressor_common.expectations.urllib.request.urlopen')
    self._get_mock = self._get_patcher.start()
    self.addCleanup(self._get_patcher.stop)

  def testBasic(self) -> None:
    """Tests basic functionality along the happy path."""

    def SideEffect(
        url: str) -> GetOriginExpectationFileContentsUnittest.FakeRequestResult:
      request_result = (
          GetOriginExpectationFileContentsUnittest.FakeRequestResult())
      text = ''
      if url.endswith('test_expectations?format=TEXT'):
        text = """\
mode type hash foo_tests.txt
mode type hash bar_tests.txt"""
      elif url.endswith('foo_tests.txt?format=TEXT'):
        text = 'foo_tests.txt content'
      elif url.endswith('bar_tests.txt?format=TEXT'):
        text = 'bar_tests.txt content'
      else:
        self.fail('Given unhandled URL %s' % url)
      request_result.text = base64.b64encode(text.encode('utf-8'))
      return request_result

    self._get_mock.side_effect = SideEffect

    foo_tests_txt = (os.path.join(
        gpu_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY, 'foo_tests.txt'))
    bar_tests_txt = (os.path.join(
        gpu_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY, 'bar_tests.txt'))
    expected_contents = {
        foo_tests_txt: 'foo_tests.txt content',
        bar_tests_txt: 'bar_tests.txt content',
    }
    self.assertEqual(self.expectations.GetOriginExpectationFileContents(),
                     expected_contents)
    self.assertEqual(self._get_mock.call_count, 3)

  def testNonOkStatusCodesSurfaced(self) -> None:
    """Tests that getting a non-200 status code back results in a failure."""

    def SideEffect(_: Any) -> None:
      raise urllib.error.HTTPError('url', 404, 'No exist :(', EmailMessage(),
                                   None)

    self._get_mock.side_effect = SideEffect
    with self.assertRaises(urllib.error.HTTPError):
      self.expectations.GetOriginExpectationFileContents()


class GetLocalCheckoutExpectationFileContentsUnittest(
    fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.expectations = gpu_expectations.GpuExpectationProcessor()
    self.setUpPyfakefs()

  def testBasic(self) -> None:
    """Tests basic functionality."""
    os.makedirs(gpu_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
    with open(
        os.path.join(gpu_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                     'foo.txt'), 'w') as outfile:
      outfile.write('foo.txt contents')
    with open(
        os.path.join(gpu_expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                     'bar.txt'), 'w') as outfile:
      outfile.write('bar.txt contents')
    foo_txt = os.path.join(gpu_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY,
                           'foo.txt')
    bar_txt = os.path.join(gpu_expectations.RELATIVE_EXPECTATION_FILE_DIRECTORY,
                           'bar.txt')
    expected_contents = {
        foo_txt: 'foo.txt contents',
        bar_txt: 'bar.txt contents',
    }

    self.assertEqual(
        self.expectations.GetLocalCheckoutExpectationFileContents(),
        expected_contents)


if __name__ == '__main__':
  unittest.main(verbosity=2)
