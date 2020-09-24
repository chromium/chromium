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
import urllib2

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
        self._sink_ctx = sink_ctx
        self._sink_url = (
            'http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
            self._sink_ctx['address'])

    def _send(self, data):
        req = urllib2.Request(
            url=self._sink_url,
            data=json.dumps(data),
            headers={
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'Authorization':
                'ResultSink %s' % self._sink_ctx['auth_token'],
            },
        )
        return urllib2.urlopen(req)

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
        Returns:
            A dict of artifacts, where the key is the artifact ID and
            the value is a dict with the absolute file path.
        """
        ret = {}
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

        return ret

    def sink(self, expected, result):
        """Reports the test result to ResultSink.

        Args:
            expected: True if the test was expected to fail and actually failed.
                False, otherwise.
            result: The TestResult object to report.
        Exceptions:
            urllib2.URLError, if there was a network connection error.
            urllib2.HTTPError, if ResultSink responded an error for the request.
        """
        # The structure and member definitions of this dict can be found at
        # https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/resultdb/proto/sink/v1/test_result.proto
        r = {
            'artifacts': self._artifacts(result),
            'duration': '%ss' % result.total_run_time,
            # device failures are never expected.
            'expected': not result.device_failed and expected,
            'status': self._status(result),
            # TODO(crbug/1093659): web_tests report TestResult with the start
            # time.
            # 'startTime': result.start_time
            'tags': self._tags(result),
            'testId': result.test_name,

            # testLocation is where the test is defined. It is used to find
            # the associated component/team/os information in flakiness and
            # disabled-test dashboards.
            'testLocation': {
                'fileName':
                '//third_party/blink/web_tests/' + result.test_name,
            },
        }
        self._send({'testResults': [r]})
