# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import unittest
import unittest.mock as mock

from blinkpy.web_tests.flake_suppressor import web_tests_expectations
from blinkpy.web_tests.flake_suppressor import web_tests_results
from blinkpy.web_tests.flake_suppressor import web_tests_tag_utils as tag_utils

from flake_suppressor_common import data_types
from flake_suppressor_common import tag_utils as common_tag_utils

GENERIC_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# results: [ Failure ]
crbug.com/1111 [ win ] foo_test [ Failure ]
"""

TEST_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# results: [ Failure ]
crbug.com/1111 [ win ] conformance/textures/misc/video-rotation.html [ Failure ]
"""


class WebTestsResultsUnittest(unittest.TestCase):
    def setUp(self) -> None:
        common_tag_utils.SetTagUtilsImplementation(tag_utils.WebTestsTagUtils)
        expectations_processor = (
            web_tests_expectations.WebTestsExpectationProcessor())
        self._results = web_tests_results.WebTestsResultProcessor(
            expectations_processor)
        self._local_patcher = mock.patch(
            'flake_suppressor_common.results.expectations.'
            'ExpectationProcessor.GetLocalCheckoutExpectationFileContents')
        self._local_mock = self._local_patcher.start()
        self._local_mock.return_value = {}
        self.addCleanup(self._local_patcher.stop)


class AggregateResultsUnittest(WebTestsResultsUnittest):
    def testWithFiltering(self) -> None:
        """Tests that results are properly filtered out."""
        self._local_mock.return_value = {
            'TestExpectations': TEST_EXPECTATION_FILE_CONTENTS,
        }
        query_results = [
            # Expected to be removed.
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-1111',
                'typ_tags': ['win'],
            },
            # Expected to be removed.
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-2222',
                'typ_tags': ['win'],
            },
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-3333',
                'typ_tags': ['linux'],
            },
            {
                'name': ('conformance/textures/misc/texture-npot-video.html'),
                'id': 'build-4444',
                'typ_tags': ['win'],
            },
        ]

        expected_output = {
            '': {
                'conformance/textures/misc/video-rotation.html': {
                    tuple(['linux']): ['http://ci.chromium.org/b/3333'],
                },
                'conformance/textures/misc/texture-npot-video.html': {
                    tuple(['win']): ['http://ci.chromium.org/b/4444'],
                },
            },
        }
        self.assertEqual(self._results.AggregateResults(query_results),
                         expected_output)


class ConvertJsonResultsToResultObjectsUnittest(WebTestsResultsUnittest):
    def testDuplicateResults(self) -> None:
        """Tests that duplicate results are not merged."""
        r = [
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-1111',
                'typ_tags': ['win', 'x86'],
            },
            {
                'name': ('conformance/textures/misc/video-rotation.html'),
                'id': 'build-1111',
                'typ_tags': ['win', 'x86'],
            },
        ]
        expected_results = [
            data_types.Result('',
                              'conformance/textures/misc/video-rotation.html',
                              tuple(['win']), '1111'),
            data_types.Result('',
                              'conformance/textures/misc/video-rotation.html',
                              tuple(['win']), '1111'),
        ]

        self.assertEqual(self._results._ConvertJsonResultsToResultObjects(r),
                         expected_results)
