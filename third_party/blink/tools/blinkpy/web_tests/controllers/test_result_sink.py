# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""TestResultSink uploads test results and artifacts to ResultDB via ResultSink.

ResultSink is a micro service that simplifies integration between ResultDB
and domain-specific test frameworks. It runs a given test framework and uploads
all the generated test results and artifacts to ResultDB in a progressive way.
- APIs: https://godoc.org/go.chromium.org/luci/resultdb/proto/sink/v1

TestResultSink implements methods for uploading test results and artifacts
via ResultSink, and is activated only if LUCI_CONTEXT is present with ResultSink
section.
"""

import json
import logging
import requests

from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


# A map from the enum values of typ.ResultType to ResultSink.Status.
# The enum values of ResultSink.Status can be found at
# https://godoc.org/go.chromium.org/luci/resultdb/proto/sink/v1#pkg-variables.
_result_type_to_sink_status = {
    ResultType.Pass:
    'PASS',
    ResultType.Failure:
    'FAIL',
    # timeout is just a special case of a reason to abort a test result.
    ResultType.Timeout:
    'ABORT',
    # 'Aborted' is a web_tests-specific type given on TestResults with a device
    # failure.
    'Aborted':
    'ABORT',
    ResultType.Crash:
    'CRASH',
    ResultType.Skip:
    'SKIP',
}


class TestResultSinkClosed(Exception):
    """Raises if sink() is called over a closed TestResultSink instance."""


def CreateTestResultSink(port):
    """Creates TestResultSink, if result_sink is present in LUCI_CONTEXT.

    Args:
        port: A blinkpy.web_tests.port.Port object
    Returns:
        TestResultSink object if result_sink section is present in LUCI_CONTEXT.
            None, otherwise.
    """
    luci_ctx_path = port.host.environ.get('LUCI_CONTEXT')
    if luci_ctx_path is None:
        return None

    with port.host.filesystem.open_text_file_for_reading(luci_ctx_path) as f:
        sink_ctx = json.load(f).get('result_sink')
        if sink_ctx is None:
            return None

    return TestResultSink(port, sink_ctx)


class TestResultSink(object):
    """A class for uploading test results and artifacts via ResultSink."""

    def __init__(self, port, sink_ctx):
        self._port = port
        self.is_closed = False
        self._sink_ctx = sink_ctx
        self._url = (
            'http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
            self._sink_ctx['address'])
        self._session = requests.Session()
        sink_headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            'Authorization': 'ResultSink %s' % self._sink_ctx['auth_token'],
        }
        self._session.headers.update(sink_headers)

    def _send(self, data):
        self._session.post(self._url, data=json.dumps(data)).raise_for_status()

    def _status(self, result):
        """Returns the TestStatus enum value corresponding to the result type.

        Args:
            result: The TestResult object to find the status of.
        Returns:
            The corresponding enum value.
        """
        status = _result_type_to_sink_status.get(
            'Aborted' if result.device_failed else result.type)

        assert status is not None, 'unsupported result.type %r' % result.type
        return status

    def _tags(self, result):
        """Returns a list of tags that should be added into a given test result.

        Args:
            result: The TestResult object to generate Tags for.
        Returns:
            A list of {'key': 'tag-name', 'value': 'tag-value'} dicts.
        """
        # the message structure of the dict can be found at
        # https://chromium.googlesource.com/infra/luci/luci-go/+/master/resultdb/proto/type/common.proto#56
        pair = lambda k, v: {'key': k, 'value': v}
        return [
            pair('test_name', result.test_name),
            pair('web_tests_device_failed', str(result.device_failed)),
            pair('web_tests_result_type', result.type),
        ]

    def _artifacts(self, result):
        """Returns a dict of artifacts with the absolute file paths.

        Args:
            result: The TestResult object to look for the artifacts of.
            summaries: A list of strings to be included in the summary html.
        Returns:
            A list of artifact HTML tags to be added into the summary html
            A dict of artifacts, where the key is the artifact ID and
            the value is a dict with the absolute file path.
        """
        ret = {}
        summaries = []
        base_dir = self._port.results_directory()
        for name, paths in result.artifacts.artifacts.iteritems():
            for p in paths:
                art_id = name
                i = 1
                while art_id in ret:
                    art_id = '%s-%d' % (name, i)
                    i += 1

                ret[art_id] = {
                    'filePath': self._port.host.filesystem.join(base_dir, p),
                }
                # Web tests generate the same artifact names for text-diff(s)
                # and image diff(s).
                # - {actual,expected}_text, {text,pretty_text}_diff
                # - {actual,expected}_image, {image,pretty_image}_diff
                # - reference_file_{mismatch,match}
                #
                # Milo recognizes the names and auto generates a summary html
                # to render them with <text-diff-artifact> or
                # <img-diff-artifact>.
                #
                # command, stderr and crash_log are artifact names that are
                # not included in the auto-generated summary. This uses
                # <text-artifact> to render them in the summary_html section
                # of each test.
                if name in ['command', 'stderr', 'crash_log']:
                    summaries.append(
                        '<h3>%s</h3>'
                        '<p><text-artifact artifact-id="%s" /></p>' %
                        (art_id, art_id))

        return summaries, ret

    def sink(self, expected, result):
        """Reports the test result to ResultSink.

        Args:
            expected: True if the test was expected to fail and actually failed.
                False, otherwise.
            result: The TestResult object to report.
        Exceptions:
            requests.exceptions.ConnectionError, if there was a network
              connection error.
            requests.exceptions.HTTPError, if ResultSink responded an error
              for the request.
            ResultSinkClosed, if sink.close() was called prior to sink().
        """
        if self.is_closed:
            raise TestResultSinkClosed('sink() cannot be called after close()')

        # The structure and member definitions of this dict can be found at
        # https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/resultdb/proto/sink/v1/test_result.proto
        loc_file_name = '//%s%s' % (RELATIVE_WEB_TESTS, result.test_name)
        summaries, artifacts = self._artifacts(result)
        r = {
            'artifacts': artifacts,
            'duration': '%ss' % result.total_run_time,
            # device failures are never expected.
            'expected': not result.device_failed and expected,
            'status': self._status(result),
            # TODO(crbug/1093659): web_tests report TestResult with the start
            # time.
            # 'startTime': result.start_time
            'tags': self._tags(result),
            'testId': result.test_name,
            'testMetadata': {
                'name': result.test_name,

                # location is where the test is defined. It is used to find
                # the associated component/team/os information in flakiness
                # and disabled-test dashboards.
                'location': {
                    'repo': 'https://chromium.googlesource.com/chromium/src',
                    'fileName': loc_file_name,
                    # skip: 'line'
                },
            },
        }
        if summaries:
            r['summaryHtml'] = '\n'.join(summaries)

        self._send({'testResults': [r]})

    def close(self):
        """Close closes all the active connections to SinkServer.

        The sink object is no longer usable after being closed.
        """
        if not self.is_closed:
            self.is_closed = True
            self._session.close()
