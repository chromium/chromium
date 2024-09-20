#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import mock
import os
import requests
import sys
import unittest

import result_sink_util
import test_runner

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.append(
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')))
import measures
import exception_recorder

from google.protobuf import json_format
from google.protobuf import any_pb2


SINK_ADDRESS = 'sink/address'
SINK_POST_URL = 'http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' % SINK_ADDRESS
UPATE_POST_URL = 'http://%s/prpc/luci.resultsink.v1.Sink/UpdateInvocation' % SINK_ADDRESS
AUTH_TOKEN = 'some_sink_token'
LUCI_CONTEXT_FILE_DATA = """
{
  "result_sink": {
    "address": "%s",
    "auth_token": "%s"
  }
}
""" % (SINK_ADDRESS, AUTH_TOKEN)
HEADERS = {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'Authorization': 'ResultSink %s' % AUTH_TOKEN
}
CRASH_TEST_LOG = """
Exception Reason:
App crashed and disconnected.

Recovery Suggestion:
"""


class UnitTest(unittest.TestCase):

  def test_compose_test_result(self):
    """Tests compose_test_result function."""
    # Test a test result without log_path.
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'PASS', True)
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)
    short_log = 'Some logs.'
    # Tests a test result with log_path.
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'PASS',
        True,
        test_log=short_log,
        duration=1233,
        file_artifacts={'name': '/path/to/name'})
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(short_log.encode('utf-8')).decode('utf-8')
            },
            'name': {
                'filePath': '/path/to/name'
            },
        },
        'duration': '1.233000000s',
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)


  def test_parsing_crash_message(self):
    """Tests parsing crash message from test log and setting it as the
    failure reason"""
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'FAIL', False, test_log=CRASH_TEST_LOG)
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'FAIL',
        'expected': False,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'tags': [],
        'failureReason': {
            'primaryErrorMessage': 'App crashed and disconnected.'
        },
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(CRASH_TEST_LOG.encode('utf-8')
                                    ).decode('utf-8')
            },
        },
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    self.assertEqual(test_result, expected)

  def test_long_test_log(self):
    """Tests long test log is reported as expected."""
    len_32_str = 'This is a string in length of 32'
    self.assertEqual(len(len_32_str), 32)
    len_4128_str = (4 * 32 + 1) * len_32_str
    self.assertEqual(len(len_4128_str), 4128)

    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': '<text-artifact artifact-id="Test Log" />',
        'artifacts': {
            'Test Log': {
                'contents':
                    base64.b64encode(len_4128_str.encode('utf-8')
                                    ).decode('utf-8')
            },
        },
        'tags': [],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething', 'PASS', True, test_log=len_4128_str)
    self.assertEqual(test_result, expected)

  def test_compose_test_result_assertions(self):
    """Tests invalid status is rejected"""
    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'SOME_INVALID_STATUS', True)

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=('a', 'b'))

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething',
          'PASS',
          True,
          tags=[('a', 'b', 'c'), ('d', 'e')])

    with self.assertRaises(AssertionError):
      test_result = result_sink_util._compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=[('a', 'b'), ('c', 3)])

  def test_composed_with_tags(self):
    """Tests tags is in correct format."""
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'SKIP',
        True,
        tags=[('disabled_test', 'true')])
    self.assertEqual(test_result, expected)

  def test_composed_with_location(self):
    """Tests with test locations"""
    test_loc = {'repo': 'https://test', 'fileName': '//test.cc'}
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': test_loc,
        },
    }
    test_result = result_sink_util._compose_test_result(
        'TestCase/testSomething',
        'SKIP',
        True,
        test_loc=test_loc,
        tags=[('disabled_test', 'true')])
    self.assertEqual(test_result, expected)

  @mock.patch.object(requests.Session, 'post')
  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  def test_post_test_result(self, mock_open_file, mock_session_post):
    test_result = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }],
        'testMetadata': {
            'name': 'TestCase/testSomething',
            'location': None,
        },
    }
    client = result_sink_util.ResultSinkClient()

    client._post_test_result(test_result)
    mock_session_post.assert_called_with(
        url=SINK_POST_URL,
        headers=HEADERS,
        data=json.dumps({'testResults': [test_result]}))

  @mock.patch.object(requests.Session, 'post')
  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  @mock.patch('exception_recorder._record_time')
  def test_post_extended_properties(self, _, mock_open_file, mock_session_post):
    test_exception = test_runner.XcodeVersionNotFoundError("15abcd")
    exception_recorder.register(test_exception)

    count = measures.count('test_count')
    count.record()
    count.record()

    inv_data = json.dumps(
        {
            "invocation": {
                "extended_properties": {
                    "exception_occurrences": {
                        "@type": "type.googleapis.com/build.util.lib.proto.ExceptionOccurrences",
                        "datapoints": [
                            {
                                "name": "test_runner.XcodeVersionNotFoundError",
                                "stacktrace": [
                                    f"test_runner.XcodeVersionNotFoundError: Xcode version not found: 15abcd\n"
                                ]
                            }
                        ]
                    },
                    "test_script_metrics": {
                        "@type": "type.googleapis.com/build.util.lib.proto.TestScriptMetrics",
                        "metrics": [
                            {
                                "name": "test_count",
                                "value": 2.0
                            }
                        ]
                    }
                }
            },
            "update_mask": {
                "paths": [
                    "extended_properties.exception_occurrences",
                    "extended_properties.test_script_metrics"
                ]
            }
        },
        sort_keys=True)

    client = result_sink_util.ResultSinkClient()
    client.post_extended_properties()
    mock_session_post.assert_called_with(
        url=UPATE_POST_URL, headers=HEADERS, data=inv_data)

  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  @mock.patch(
      'result_sink_util.ResultSinkClient._post_extended_properties',
      side_effect=Exception())
  def test_post_extended_properties_retries(self, mock_post_ext_props, _):
    count = measures.count('test_count')
    count.record()

    client = result_sink_util.ResultSinkClient()
    client.post_extended_properties()

    self.assertEqual(mock_post_ext_props.call_count, 2)

  @mock.patch.object(requests.Session, 'close')
  @mock.patch.object(requests.Session, 'post')
  @mock.patch('%s.open' % 'result_sink_util',
              mock.mock_open(read_data=LUCI_CONTEXT_FILE_DATA))
  @mock.patch('os.environ.get', return_value='filename')
  def test_close(self, mock_open_file, mock_session_post, mock_session_close):

    client = result_sink_util.ResultSinkClient()

    client._post_test_result({'some': 'result'})
    mock_session_post.assert_called()

    client.close()
    mock_session_close.assert_called()

  def test_post(self):
    client = result_sink_util.ResultSinkClient()
    client.sink = 'Make sink not None so _compose_test_result will be called'
    client._post_test_result = mock.MagicMock()

    client.post(
        'testname',
        'PASS',
        True,
        test_log='some_log',
        tags=[('tag key', 'tag value')])
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result(
            'testname',
            'PASS',
            True,
            test_log='some_log',
            tags=[('tag key', 'tag value')]))

    client.post('testname', 'PASS', True, test_log='some_log')
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result(
            'testname', 'PASS', True, test_log='some_log'))

    client.post('testname', 'PASS', True)
    client._post_test_result.assert_called_with(
        result_sink_util._compose_test_result('testname', 'PASS', True))


if __name__ == '__main__':
  unittest.main()
