# Copyright 2020 The Chromium Authors
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
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.typ_types import ResultType

logging.getLogger("urllib3").setLevel(logging.WARNING)
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

    def _tags(self, result, expectations):
        """Returns a list of tags that should be added into a given test result.

        Args:
            result: The TestResult object to generate Tags for.
            expectations: A test_expectations.TestExpectations object to pull
                expectation data from.
        Returns:
            A list of {'key': 'tag-name', 'value': 'tag-value'} dicts.
        """
        # the message structure of the dict can be found at
        # https://chromium.googlesource.com/infra/luci/luci-go/+/master/resultdb/proto/type/common.proto#56
        pair = lambda k, v: {'key': k, 'value': v}

        # According to //third_party/blink/web_tests/SlowTests, a test is
        # considered slow if it is slower than ~30% of its timeout since test
        # times can vary by up to 3x.
        portion_of_timeout = result.total_run_time / (self._port.timeout_ms() /
                                                      1000)
        test_was_slow = portion_of_timeout > 0.3

        tags = [
            pair('test_name', result.test_name),
            pair('web_tests_device_failed', str(result.device_failed)),
            pair('web_tests_result_type', result.type),
            pair('web_tests_flag_specific_config_name',
                 self._port.flag_specific_config_name() or ''),
            pair('web_tests_base_timeout',
                 str(int(self._port.timeout_ms() / 1000))),
            pair('web_tests_test_was_slow', json.dumps(test_was_slow)),
        ]

        # The hash allows `rebaseline-cl` to determine whether baselines are
        # equal without needing to download the files.
        if result.actual_image_hash:
            tags.append(
                pair(test_failures.FailureImage.ACTUAL_HASH_RDB_TAG,
                     result.actual_image_hash))

        if (result.image_diff_stats and result.image_diff_stats.keys() >=
            {'maxDifference', 'totalPixels'}):
            tags.append(
                pair('web_tests_image_diff_max_difference',
                     str(result.image_diff_stats['maxDifference'])))
            tags.append(
                pair('web_tests_image_diff_total_pixels',
                     str(result.image_diff_stats['totalPixels'])))

        for test_type_str in sorted(result.test_type):
            tags.append(pair('web_tests_test_type', test_type_str))

        for used_file in self._port.used_expectations_files():
            tags.append(
                pair('web_tests_used_expectations_file',
                     self._port.relative_test_filename(used_file)))

        if expectations:
            expectation_tags = expectations.system_condition_tags
            test_expectation = expectations.get_expectations(result.test_name)
            raw_expected_results = test_expectation.raw_results
            for expectation in raw_expected_results:
                tags.append(pair('raw_typ_expectation', expectation))
            for tag in expectation_tags:
                tags.append(pair('typ_tag', tag))

        return tags

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
        for name, paths in result.artifacts.artifacts.items():
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

        # Sort summaries to display "command" at the top of the summary.
        return sorted(summaries), ret

    def sink(self, expected, result, expectations):
        """Reports the test result to ResultSink.

        Args:
            expected: True if the test was expected to fail and actually failed.
                False, otherwise.
            result: The TestResult object to report.
            expectations: A test_expectations.TestExpectations object to pull
                expectation data from.
        Exceptions:
            requests.exceptions.ConnectionError, if there was a network
              connection error.
            requests.exceptions.HTTPError, if ResultSink responded an error
              for the request.
            ResultSinkClosed, if sink.close() was called prior to sink().
        """
        if self.is_closed:
            raise TestResultSinkClosed('sink() cannot be called after close()')

        # fileName refers to the real file path instead of the test path
        # that might be virtualized.
        path = (self._port.get_file_path_for_wpt_test(result.test_name)
                or self._port.name_for_test(result.test_name))
        if self._port.host.filesystem.sep != '/':
            path = path.replace(self._port.host.filesystem.sep, '/')
        loc_fn = '//%s%s' % (RELATIVE_WEB_TESTS, path)
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
            'tags': self._tags(result, expectations),
            'testId': result.test_name,
            'testMetadata': {
                'name': result.test_name,
                # location is where the test is defined. It is used to find
                # the associated component/team/os information in flakiness
                # and disabled-test dashboards.
                'location': {
                    'repo': 'https://chromium.googlesource.com/chromium/src',
                    'fileName': loc_fn,
                    # skip: 'line'
                },
            },
        }
        if summaries:
            r['summaryHtml'] = '\n'.join(summaries)

        if result.failure_reason:
            primary_error_message = _truncate_to_utf8_bytes(
                result.failure_reason.primary_error_message, 1024)
            r['failureReason'] = {
                'primaryErrorMessage': primary_error_message,
            }

        self._send({'testResults': [r]})

    def close(self):
        """Close closes all the active connections to SinkServer.

        The sink object is no longer usable after being closed.
        """
        if not self.is_closed:
            self.is_closed = True
            self._session.close()


def _truncate_to_utf8_bytes(s, length):
    """ Truncates a string to a given number of bytes when encoded as UTF-8.

    Ensures the given string does not take more than length bytes when encoded
    as UTF-8. Adds trailing ellipsis (...) if truncation occurred. A truncated
    string may end up encoding to a length slightly shorter than length
    because only whole Unicode codepoints are dropped.

    Args:
        s: The string to truncate.
        length: the length (in bytes) to truncate to.
    """
    try:
        encoded = s.encode('utf-8')
    # When encode throws UnicodeDecodeError in py2, it usually means the str is
    # already encoded and has non-ascii chars. So skip re-encoding it.
    except UnicodeDecodeError:
        encoded = s
    if len(encoded) > length:
        # Truncate, leaving space for trailing ellipsis (...).
        encoded = encoded[:length - 3]
        # Truncating the string encoded as UTF-8 may have left the final
        # codepoint only partially present. Pass 'ignore' to acknowledge
        # and ensure this is dropped.
        return encoded.decode('utf-8', 'ignore') + "..."
    return s
