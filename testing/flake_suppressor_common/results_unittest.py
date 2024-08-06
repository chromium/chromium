#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import datetime
import os
import unittest
import unittest.mock as mock

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import data_types
from flake_suppressor_common import tag_utils as common_tag_utils
from flake_suppressor_common import unittest_utils as uu

GENERIC_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# results: [ Failure ]
crbug.com/1111 [ win ] foo_test [ Failure ]
"""

GPU_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# tags: [ amd nvidia ]
# results: [ Failure ]
crbug.com/1111 [ win nvidia ] conformance/textures/misc/video-rotation.html [ Failure ]
"""


class BaseResultsUnittest(unittest.TestCase):
  def setUp(self) -> None:
    common_tag_utils.SetTagUtilsImplementation(uu.UnitTestTagUtils)
    expectations_processor = uu.UnitTestExpectationProcessor()
    self._results = uu.UnitTestResultProcessor(expectations_processor)
    self._local_patcher = mock.patch(
        'flake_suppressor_common.results.expectations.'
        'ExpectationProcessor.GetLocalCheckoutExpectationFileContents')
    self._local_mock = self._local_patcher.start()
    self._local_mock.return_value = {}
    self.addCleanup(self._local_patcher.stop)
    self._expectation_file_patcher = mock.patch.object(
        uu.UnitTestExpectationProcessor, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self.addCleanup(self._expectation_file_patcher.stop)


class AggregateResultsUnittest(BaseResultsUnittest):
  def testBasic(self) -> None:
    """Basic functionality test."""
    query_results = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['win', 'nvidia'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-3333',
            'typ_tags': ['win', 'amd'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/texture-npot-video.html'),
            'id':
            'build-4444',
            'typ_tags': ['win', 'nvidia'],
        },
        {
            'name': ('gpu_tests.pixel_integration_test.PixelIntegrationTest.'
                     'Pixel_CSS3DBlueBox'),
            'id':
            'build-5555',
            'typ_tags': ['win', 'nvidia'],
        },
    ]
    expected_output = {
        'webgl_conformance_integration_test': {
            'conformance/textures/misc/video-rotation.html': {
                ('nvidia', 'win'): [
                    'http://ci.chromium.org/b/1111',
                    'http://ci.chromium.org/b/2222',
                ],
                ('amd', 'win'): ['http://ci.chromium.org/b/3333'],
            },
            'conformance/textures/misc/texture-npot-video.html': {
                ('nvidia', 'win'): ['http://ci.chromium.org/b/4444'],
            },
        },
        'pixel_integration_test': {
            'Pixel_CSS3DBlueBox': {
                ('nvidia', 'win'): ['http://ci.chromium.org/b/5555'],
            },
        },
    }
    self.assertEqual(self._results.AggregateResults(query_results),
                     expected_output)


class AggregateTestStatusResultsUnittest(BaseResultsUnittest):
  def testBasic(self) -> None:
    """Basic functionality test."""
    query_results = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
            'status':
            ct.ResultStatus.FAIL,
            'date':
            '2023-03-01',
            'is_slow':
            False,
            'typ_expectations': ['Pass'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['win', 'nvidia'],
            'status':
            ct.ResultStatus.CRASH,
            'date':
            '2023-03-02',
            'is_slow':
            False,
            'typ_expectations': ['Pass'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-3333',
            'typ_tags': ['win', 'amd'],
            'status':
            ct.ResultStatus.FAIL,
            'date':
            '2023-03-03',
            'is_slow':
            True,
            'typ_expectations': ['Pass', 'Slow'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/texture-npot-video.html'),
            'id':
            'build-4444',
            'typ_tags': ['win', 'nvidia'],
            'status':
            ct.ResultStatus.FAIL,
            'date':
            '2023-03-04',
            'is_slow':
            True,
            'typ_expectations': ['Pass', 'Slow'],
        },
        {
            'name': ('gpu_tests.pixel_integration_test.PixelIntegrationTest.'
                     'Pixel_CSS3DBlueBox'),
            'id':
            'build-5555',
            'typ_tags': ['win', 'nvidia'],
            'status':
            ct.ResultStatus.FAIL,
            'date':
            '2023-03-05',
            'is_slow':
            False,
            'typ_expectations': ['Pass'],
        },
    ]
    expected_output = {
        'webgl_conformance_integration_test': {
            'conformance/textures/misc/video-rotation.html': {
                ('nvidia', 'win'): [
                    (ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/1111',
                     datetime.date.fromisoformat('2023-03-01'), False, ['Pass'
                                                                        ]),
                    (ct.ResultStatus.CRASH, 'http://ci.chromium.org/b/2222',
                     datetime.date.fromisoformat('2023-03-02'), False, ['Pass'
                                                                        ]),
                ],
                ('amd', 'win'): [
                    (ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/3333',
                     datetime.date.fromisoformat('2023-03-03'), True,
                     ['Pass', 'Slow']),
                ],
            },
            'conformance/textures/misc/texture-npot-video.html': {
                ('nvidia', 'win'):
                [(ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/4444',
                  datetime.date.fromisoformat('2023-03-04'), True,
                  ['Pass', 'Slow'])],
            },
        },
        'pixel_integration_test': {
            'Pixel_CSS3DBlueBox': {
                ('nvidia', 'win'):
                [(ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/5555',
                  datetime.date.fromisoformat('2023-03-05'), False, ['Pass'])],
            },
        },
    }
    self.assertEqual(self._results.AggregateTestStatusResults(query_results),
                     expected_output)


class ConvertJsonResultsToResultObjectsUnittest(BaseResultsUnittest):
  def testBasic(self) -> None:
    """Basic functionality test."""
    r = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['nvidia', 'win'],
        },
    ]
    expected_results = [
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '1111'),
        data_types.Result(
            'webgl_conformance_integration_test',
            'conformance/textures/misc/video-rotation.html',
            ('nvidia', 'win'),
            '2222',
        ),
    ]
    self.assertEqual(self._results._ConvertJsonResultsToResultObjects(r),
                     expected_results)

  def testOnQueryResultWithOptionalAttributes(self) -> None:
    """Functionality test on query result with optional attributes."""
    r = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
            'status':
            ct.ResultStatus.FAIL,
            'date':
            '2023-03-01',
            'is_slow':
            False,
            'typ_expectations': ['Pass'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['nvidia', 'win'],
            'status':
            ct.ResultStatus.CRASH,
            'date':
            '2023-03-02',
            'is_slow':
            True,
            'typ_expectations': ['Pass', 'Slow'],
        },
    ]
    expected_results = [
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '1111', ct.ResultStatus.FAIL,
                          datetime.date.fromisoformat('2023-03-01'), False,
                          ['Pass']),
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '2222', ct.ResultStatus.CRASH,
                          datetime.date.fromisoformat('2023-03-02'), True,
                          ['Pass', 'Slow']),
    ]
    self.assertEqual(self._results._ConvertJsonResultsToResultObjects(r),
                     expected_results)


class FilterOutSuppressedResultsUnittest(BaseResultsUnittest):
  def testNoSuppressedResults(self) -> None:
    """Tests functionality when no expectations apply to the given results."""
    self._local_mock.return_value = {
        'foo_expectations.txt': GENERIC_EXPECTATION_FILE_CONTENTS,
    }
    r = [
        data_types.Result('foo_integration_test', 'foo_test', tuple(['linux']),
                          'id'),
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
        data_types.Result('bar_integration_test', 'foo_test', tuple(['win']),
                          'id')
    ]

    self.assertEqual(self._results._FilterOutSuppressedResults(r), r)

  def testSuppressedResults(self) -> None:
    """Tests functionality when expectations apply to the given results."""
    self._local_mock.return_value = {
        'foo_expectations.txt': GENERIC_EXPECTATION_FILE_CONTENTS,
    }
    self._expectation_file_mock.return_value = os.path.join(
        uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY, 'foo_expectations.txt')

    r = [
        data_types.Result('foo_integration_test', 'foo_test', ('win', 'nvidia'),
                          'id'),
        data_types.Result('foo_integration_test', 'foo_test', tuple(['win']),
                          'id'),
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
    ]

    expected_filtered_results = [
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
    ]

    self.assertEqual(self._results._FilterOutSuppressedResults(r),
                     expected_filtered_results)


if __name__ == '__main__':
  unittest.main(verbosity=2)
