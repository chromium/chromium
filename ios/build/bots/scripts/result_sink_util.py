# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import base64
import cgi
import json
import logging
import os
import requests

LOGGER = logging.getLogger(__name__)
# Max summaryHtml length (4 KiB) from
# https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto;drc=ca12b9f52b27f064b0fa47c39baa3b011ffa5790;l=96
MAX_REPORT_LEN = 4 * 1024
# VALID_STATUSES is a list of valid status values for test_result['status'].
# The full list can be obtained at
# https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto;drc=ca12b9f52b27f064b0fa47c39baa3b011ffa5790;l=151-174
VALID_STATUSES = {"PASS", "FAIL", "CRASH", "ABORT", "SKIP"}


def compose_test_result(test_id, status, expected, test_log=None, tags=None):
  """Composes the test_result dict item to be posted to result sink.

  Args:
    test_id: (str) A unique identifier of the test in LUCI context.
    status: (str) Status of the test. Must be one in |VALID_STATUSES|.
    expected: (bool) Whether the status is expected.
    test_log: (str) Log of the test. Optional.
    tags: (list) List of tags. Each item in list should be a length 2 tuple of
        string as ("key", "value"). Optional.

  Returns:
    A dict of test results with input information, confirming to
      https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/test_result.proto
  """
  assert status in VALID_STATUSES, (
      '%s is not a valid status (one in %s) for ResultSink.' %
      (status, VALID_STATUSES))

  for tag in tags or []:
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
      } for (key, value) in (tags or [])]
  }

  if test_log:
    summary = '<pre>%s</pre>' % cgi.escape(test_log)
    summary_trunc = ''

    if len(summary) > MAX_REPORT_LEN:
      summary_trunc = (
          summary[:MAX_REPORT_LEN - 45] +
          '...Full output in "Test Log" Artifact.</pre>')

    test_result['summaryHtml'] = summary_trunc or summary
    if summary_trunc:
      test_result['artifacts'] = {
          'Test Log': {
              'contents': base64.b64encode(test_log)
          },
      }

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

    logging.getLogger("requests").setLevel(logging.WARNING)

  def close(self):
    """Closes the connection to result sink server."""
    if not self.sink:
      return
    LOGGER.info('Closing connection with result sink server.')
    # Reset to default logging level of test runner scripts.
    logging.getLogger("requests").setLevel(logging.DEBUG)
    self._session.close()

  def post(self, test_result):
    """Posts single test result to server.

    Args:
        test_result: (dict) Confirming to protocol defined in
          https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/test_result.proto
    """
    if not self.sink:
      return

    res = self._session.post(
        url=self.url,
        headers=self.headers,
        data=json.dumps({'testResults': [test_result]}),
    )
    res.raise_for_status()
