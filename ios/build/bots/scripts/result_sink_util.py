# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import base64
import cgi
import json
import logging
import os
import requests
import sys
import traceback

import constants

# import protos for exceptions reporting
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.append(
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')))
import exception_occurrences_pb2

from google.protobuf import json_format
from google.protobuf import any_pb2

LOGGER = logging.getLogger(__name__)
# VALID_STATUSES is a list of valid status values for test_result['status'].
# The full list can be obtained at
# https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto;drc=ca12b9f52b27f064b0fa47c39baa3b011ffa5790;l=151-174
VALID_STATUSES = {"PASS", "FAIL", "CRASH", "ABORT", "SKIP"}


def format_exception_stacktrace(e: Exception):
  exception_trace = traceback.format_exception(type(e), e, e.__traceback__)
  return exception_trace


def _compose_test_result(test_id,
                         status,
                         expected,
                         duration=None,
                         test_log=None,
                         test_loc=None,
                         tags=None,
                         file_artifacts=None):
  """Composes the test_result dict item to be posted to result sink.

  Args:
    test_id: (str) A unique identifier of the test in LUCI context.
    status: (str) Status of the test. Must be one in |VALID_STATUSES|.
    duration: (int) Test duration in milliseconds or None if unknown.
    expected: (bool) Whether the status is expected.
    test_log: (str) Log of the test. Optional.
    tags: (list) List of tags. Each item in list should be a length 2 tuple of
        string as ("key", "value"). Optional.
    test_loc: (dict): Test location metadata as described in
        https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_metadata.proto;l=32;drc=37488404d1c8aa8fccca8caae4809ece08828bae
    file_artifacts: (dict) IDs to abs paths mapping of existing files to
        report as artifact.

  Returns:
    A dict of test results with input information, confirming to
      https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/test_result.proto
  """
  tags = tags or []
  file_artifacts = file_artifacts or {}

  assert status in VALID_STATUSES, (
      '%s is not a valid status (one in %s) for ResultSink.' %
      (status, VALID_STATUSES))

  for tag in tags:
    assert len(tag) == 2, 'Items in tags should be length 2 tuples of strings'
    assert isinstance(tag[0], str) and isinstance(
        tag[1], str), ('Items in'
                       'tags should be length 2 tuples of strings')

  test_result = {
      'testId': test_id,
      'status': status,
      'expected': expected,
      'tags': [{
          'key': key,
          'value': value
      } for (key, value) in tags],
      'testMetadata': {
          'name': test_id,
          'location': test_loc,
      }
  }

  test_result['artifacts'] = {
      name: {
          'filePath': file_artifacts[name]
      } for name in file_artifacts
  }
  if test_log:
    message = ''
    if sys.version_info.major < 3:
      message = base64.b64encode(test_log)
    else:
      # Python3 b64encode takes and returns bytes. The result must be
      # serializable in order for the eventual json.dumps to succeed
      message = base64.b64encode(test_log.encode('utf-8')).decode('utf-8')
    test_result['summaryHtml'] = '<text-artifact artifact-id="Test Log" />'
    if constants.CRASH_MESSAGE in test_log:
      test_result['failureReason'] = {
          'primaryErrorMessage': constants.CRASH_MESSAGE
      }
    test_result['artifacts'].update({
        'Test Log': {
            'contents': message
        },
    })
  if not test_result['artifacts']:
    test_result.pop('artifacts')

  if duration:
    test_result['duration'] = '%.9fs' % (duration / 1000.0)

  return test_result


def _compose_exception_occurrence(exception):
  """Composes the exception_occurrence item to be posted to result sink.

  Args:
    exception: (Exception) the exception to be posted to result sink.

  Returns:
    exception_occurrence: Conforming to protos defined in
          //build/util/lib/proto/exception_occurrences.proto
  """

  occurrence = exception_occurrences_pb2.ExceptionOccurrence(
      name=type(exception).__name__,
      stacktrace=format_exception_stacktrace(exception),
  )
  occurrence.occurred_time.GetCurrentTime()
  return occurrence


class ExceptionResults:
  results = []  # Static variable to hold exception results

  @staticmethod
  def add_result(exception):
    ExceptionResults.results.append(exception)


class ResultSinkClient(object):
  """Stores constants and handles posting to ResultSink."""

  def __init__(self):
    """Initiates and stores constants to class."""
    self.sink = None
    luci_context_file = os.environ.get('LUCI_CONTEXT')
    if not luci_context_file:
      logging.warning('LUCI_CONTEXT not found in environment. ResultDB'
                      ' integration disabled.')
      return

    with open(luci_context_file) as f:
      self.sink = json.load(f).get('result_sink')
      if not self.sink:
        logging.warning('ResultSink constants not found in LUCI context.'
                        ' ResultDB integration disabled.')
        return

      self.url = ('http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
                  self.sink['address'])
      self.headers = {
          'Content-Type': 'application/json',
          'Accept': 'application/json',
          'Authorization': 'ResultSink %s' % self.sink['auth_token'],
      }
      self._session = requests.Session()

      # Ensure session is closed at exit.
      atexit.register(self.close)

    logging.getLogger("urllib3.connectionpool").setLevel(logging.WARNING)

  def close(self):
    """Closes the connection to result sink server."""
    if not self.sink:
      return
    LOGGER.info('Closing connection with result sink server.')
    # Reset to default logging level of test runner scripts.
    logging.getLogger("urllib3.connectionpool").setLevel(logging.DEBUG)
    self._session.close()

  def post(self, test_id, status, expected, **kwargs):
    """Composes and posts a test and status to result sink.

    Args:
      test_id: (str) A unique identifier of the test in LUCI context.
      status: (str) Status of the test. Must be one in |VALID_STATUSES|.
      expected: (bool) Whether the status is expected.
      **kwargs: Optional keyword args. Namely:
        duration: (int) Test duration in milliseconds or None if unknown.
        test_log: (str) Log of the test. Optional.
        tags: (list) List of tags. Each item in list should be a length 2 tuple
          of string as ("key", "value"). Optional.
        file_artifacts: (dict) IDs to abs paths mapping of existing files to
          report as artifact.
    """
    if not self.sink:
      return
    self._post_test_result(
        _compose_test_result(test_id, status, expected, **kwargs))

  def post_exceptions(self, exceptions):
    """Composes and posts exception result to server.

    Args:
      exception: [Exception] list of exceptions to be posted to result sink.
    """
    if not self.sink:
      return
    exception_occurrences = []
    for exception in exceptions:
      exception_occurrences.append(_compose_exception_occurrence(exception))
    self._post_exceptions(exception_occurrences)

  def _post_test_result(self, test_result):
    """Posts single test result to server.

    This method assumes |self.sink| is not None.

    Args:
        test_result: (dict) Confirming to protocol defined in
          https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/test_result.proto
    """
    res = self._session.post(
        url=self.url,
        headers=self.headers,
        data=json.dumps({'testResults': [test_result]}),
    )
    res.raise_for_status()

  def _post_exceptions(self, exception_occurrences):
    """Posts exception result to server.

    This method assumes |self.sink| is not None.

    Args:
        exception_occurrences: list of exception_occurrences,
          conforming to protos defined in
          //build/util/lib/proto/exception_occurrences.proto
    """

    occurrences = exception_occurrences_pb2.ExceptionOccurrences()
    occurrences.datapoints.extend(exception_occurrences)
    any_msg = any_pb2.Any()
    any_msg.Pack(occurrences)
    inv_data = json.dumps(
        {
            'invocation': {
                'extended_properties': {
                    'exception_occurrences':
                        json_format.MessageToDict(
                            any_msg, preserving_proto_field_name=True)
                }
            },
            'update_mask': {
                'paths': ['extended_properties.exception_occurrences'],
            }
        },
        sort_keys=True)

    LOGGER.info(inv_data)

    updateInvo_url = (
        'http://%s/prpc/luci.resultsink.v1.Sink/UpdateInvocation' %
        self.sink['address'])
    res = self._session.post(
        url=updateInvo_url,
        headers=self.headers,
        data=inv_data,
    )
    res.raise_for_status()
