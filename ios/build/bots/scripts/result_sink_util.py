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
sys.path.extend([
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')),
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/'))
])
import measures
import exception_recorder

from google.protobuf import json_format
from google.protobuf import any_pb2

LOGGER = logging.getLogger(__name__)
# VALID_STATUSES is a list of valid status values for test_result['status'].
# The full list can be obtained at
# https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto;drc=ca12b9f52b27f064b0fa47c39baa3b011ffa5790;l=151-174
VALID_STATUSES = {"PASS", "FAIL", "CRASH", "ABORT", "SKIP"}

EXTENDED_PROPERTIES_KEY = 'extended_properties'

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

  def post_extended_properties(self):
    """Posts extended properties to server with retry.
    """
    if not self.sink:
      return
    try_count = 0
    try_count_max = 2
    while try_count < try_count_max:
      try_count += 1
      try:
        self._post_extended_properties()
        break
      except Exception as e:
        logging.error("Got error %s when uploading extended properties.", e)
        if try_count < try_count_max:
          # Upload can fail due to record size being too big. In this case,
          # report just the upload failure.
          exception_recorder.clear()
          measures.clear()
          exception_recorder.register(e)
        else:
          # Swallow the exception if the upload fails again and hit the max
          # try so that it won't fail the test task (and it shouldn't).
          logging.error("Hit max retry. Skip uploading extended properties.")

  def _post_extended_properties(self):
    """Posts extended properties to server.

    Assumes self.sink has been initialized.

    Packages exception_occurrences_pb2 and test_script_metrics_pb2 and sends an
    UpdateInvocation post request to result sink.
    """
    invocation = {EXTENDED_PROPERTIES_KEY: {}}
    paths = []

    if exception_recorder.size() > 0:
      invocation[EXTENDED_PROPERTIES_KEY][
          exception_recorder.EXCEPTION_OCCURRENCES_KEY] = \
            exception_recorder.to_dict()
      paths.append('%s.%s' % (EXTENDED_PROPERTIES_KEY,
                              exception_recorder.EXCEPTION_OCCURRENCES_KEY))

    if measures.size() > 0:
      invocation[EXTENDED_PROPERTIES_KEY][measures.TEST_SCRIPT_METRICS_KEY] = \
        measures.to_dict()
      paths.append('%s.%s' %
                   (EXTENDED_PROPERTIES_KEY, measures.TEST_SCRIPT_METRICS_KEY))

    req = {'invocation': invocation, 'update_mask': {'paths': paths}}

    inv_data = json.dumps(req, sort_keys=True)

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
