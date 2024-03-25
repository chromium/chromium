# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test result related classes."""

from collections import OrderedDict
import shard_util
import time

from result_sink_util import ResultSinkClient

_VALID_RESULT_COLLECTION_INIT_KWARGS = set(['test_results', 'crashed'])
_VALID_TEST_RESULT_INIT_KWARGS = set(
    ['attachments', 'duration', 'expected_status', 'test_log', 'test_loc'])
_VALID_TEST_STATUSES = set(['PASS', 'FAIL', 'CRASH', 'ABORT', 'SKIP'])


class TestStatus:
  """Enum storing possible test status(outcome).

  Confirms to ResultDB TestStatus definitions:
      https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto
  """
  PASS = 'PASS'
  FAIL = 'FAIL'
  CRASH = 'CRASH'
  ABORT = 'ABORT'
  SKIP = 'SKIP'


def _validate_kwargs(kwargs, valid_args_set):
  """Validates if keywords in kwargs are accepted."""
  diff = set(kwargs.keys()) - valid_args_set
  assert len(diff) == 0, 'Invalid keyword argument(s) in %s passed in!' % diff


def _validate_test_status(status):
  """Raises if input isn't valid."""
  if not status in _VALID_TEST_STATUSES:
    raise TypeError('Invalid test status: %s. Should be one of %s.' %
                    (status, _VALID_TEST_STATUSES))


def _to_standard_json_literal(status):
  """Converts TestStatus literal to standard JSON format requirement.

  Standard JSON format defined at:
    https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_result.proto

  ABORT is reported as "TIMEOUT" in standard JSON. The rest are the same.
  """
  _validate_test_status(status)
  return 'TIMEOUT' if status == TestStatus.ABORT else status


class TestResult(object):
  """Stores test outcome information of a single test run."""

  def __init__(self, name, status, **kwargs):
    """Initializes an object.

    Args:
      name: (str) Name of a test. Typically includes
      status: (str) Outcome of the test.
      (Following are possible arguments in **kwargs):
      attachments: (dict): Dict of unique attachment name to abs path mapping.
      duration: (int) Test duration in milliseconds or None if unknown.
      expected_status: (str) Expected test outcome for the run.
      test_log: (str) Logs of the test.
      test_loc: (dict): This is used to report test location info to resultSink.
          data required in the dict can be found in
          https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/test_metadata.proto;l=32;drc=37488404d1c8aa8fccca8caae4809ece08828bae
    """
    _validate_kwargs(kwargs, _VALID_TEST_RESULT_INIT_KWARGS)
    assert isinstance(name, str), (
        'Test name should be an instance of str. We got: %s') % type(name)
    self.name = name
    _validate_test_status(status)
    self.status = status

    self.attachments = kwargs.get('attachments', {})
    self.duration = kwargs.get('duration')
    self.expected_status = kwargs.get('expected_status', TestStatus.PASS)
    self.test_log = kwargs.get('test_log', '')
    self.test_loc = kwargs.get('test_loc', None)

    # Use the var to avoid duplicate reporting.
    self._reported_to_result_sink = False

  def _compose_result_sink_tags(self):
    """Composes tags received by Result Sink from test result info."""
    tags = [('test_name', self.name)]
    # Only SKIP results have tags other than test name, to distinguish whether
    # the SKIP is expected (disabled test) or not.
    if self.status == TestStatus.SKIP:
      if self.disabled():
        tags.append(('disabled_test', 'true'))
      else:
        tags.append(('disabled_test', 'false'))
    return tags

  def disabled(self):
    """Returns whether the result represents a disabled test."""
    return self.expected() and self.status == TestStatus.SKIP

  def expected(self):
    """Returns whether the result is expected."""
    return self.expected_status == self.status

  def report_to_result_sink(self, result_sink_client):
    """Reports the single result to result sink if never reported.

    Args:
      result_sink_client: (result_sink_util.ResultSinkClient) Result sink client
          to report test result.
    """
    if not self._reported_to_result_sink:
      result_sink_client.post(
          self.name,
          self.status,
          self.expected(),
          duration=self.duration,
          test_log=self.test_log,
          test_loc=self.test_loc,
          tags=self._compose_result_sink_tags(),
          file_artifacts=self.attachments)
      self._reported_to_result_sink = True


class ResultCollection(object):
  """Stores a collection of TestResult for one or more test app launches."""

  def __init__(self, **kwargs):
    """Initializes the object.

    Args:
      (Following are possible arguments in **kwargs):
      crashed: (bool) Whether the ResultCollection is of a crashed test launch.
      test_results: (list) A list of test_results to initialize the collection.
    """
    _validate_kwargs(kwargs, _VALID_RESULT_COLLECTION_INIT_KWARGS)
    self._test_results = []
    self._crashed = kwargs.get('crashed', False)
    self._crash_message = ''
    self._spawning_test_launcher = False
    self.add_results(kwargs.get('test_results', []))

  @property
  def crashed(self):
    """Whether the invocation(s) of the collection is regarded as crashed.

    Crash indicates there might be tests unexpectedly not run that's not
    included in |_test_results| in the collection.
    """
    return self._crashed

  @crashed.setter
  def crashed(self, value):
    """Sets crash value."""
    assert (type(value) == bool)
    self._crashed = value

  @property
  def crash_message(self):
    """Logs from crashes in collection which are unrelated to single tests."""
    return self._crash_message

  @crash_message.setter
  def crash_message(self, value):
    """Sets crash_message value."""
    self._crash_message = value

  @property
  def test_results(self):
    return self._test_results

  @property
  def spawning_test_launcher(self):
    return self._spawning_test_launcher

  @spawning_test_launcher.setter
  def spawning_test_launcher(self, value):
    """Sets spawning_test_launcher value."""
    assert (type(value) == bool)
    self._spawning_test_launcher = value

  def add_test_result(self, test_result):
    """Adds a single test result to collection.

    Any new test addition should go through this method for all needed setups.
    """
    self._test_results.append(test_result)

  def add_result_collection(self,
                            another_collection,
                            ignore_crash=False,
                            overwrite_crash=False):
    """Adds results and status from another ResultCollection.

    Args:
      another_collection: (ResultCollection) The other collection to be added.
      ignore_crash: (bool) Ignore any crashes from newly added collection.
      overwrite_crash: (bool) Overwrite crash status of |self| and crash
          message. Only applicable when ignore_crash=False.
    """
    assert (not (ignore_crash and overwrite_crash))
    if not ignore_crash:
      if overwrite_crash:
        self._crashed = False
        self._crash_message = ''
      self._crashed = self.crashed or another_collection.crashed
      self.append_crash_message(another_collection.crash_message)
    for test_result in another_collection.test_results:
      self.add_test_result(test_result)

  def add_results(self, test_results):
    """Adds a list of |TestResult|."""
    for test_result in test_results:
      self.add_test_result(test_result)

  def add_name_prefix_to_tests(self, prefix):
    """Adds a prefix to all test names of results."""
    for test_result in self._test_results:
      test_result.name = '%s%s' % (prefix, test_result.name)

  def add_test_names_status(self, test_names, test_status, **kwargs):
    """Adds a list of test names with given test status.

    Args:
      test_names: (list) A list of names of tests to add.
      test_status: (str) The test outcome of the tests to add.
      **kwargs: See possible **kwargs in TestResult.__init__ docstring.
    """
    for test_name in test_names:
      self.add_test_result(TestResult(test_name, test_status, **kwargs))

  def add_and_report_test_names_status(self, test_names, test_status, **kwargs):
    """Adds a list of test names with status and report these to ResultSink.

    Args:
      test_names: (list) A list of names of tests to add.
      test_status: (str) The test outcome of the tests to add.
      **kwargs: See possible **kwargs in TestResult.__init__ docstring.
    """
    another_collection = ResultCollection()
    another_collection.add_test_names_status(test_names, test_status, **kwargs)
    another_collection.report_to_result_sink()
    self.add_result_collection(another_collection)

  def append_crash_message(self, message):
    """Appends crash message str to current."""
    if not message:
      return
    if self._crash_message:
      self._crash_message += '\n'
    self._crash_message += message

  def all_test_names(self):
    """Returns a set of all test names in collection."""
    return self.tests_by_expression(lambda result: True)

  def tests_by_expression(self, expression):
    """A set of test names by filtering test results with given |expression|.

    Args:
      expression: (TestResult -> bool) A function or lambda expression which
          accepts a TestResult object and returns bool.
    """
    return set(
        map(lambda result: result.name, filter(expression, self._test_results)))

  def crashed_tests(self):
    """A set of test names with any crashed status in the collection."""
    return self.tests_by_expression(lambda result: result.status == TestStatus.
                                    CRASH)

  def disabled_tests(self):
    """A set of disabled test names in the collection."""
    return self.tests_by_expression(lambda result: result.disabled())

  def expected_tests(self):
    """A set of test names with any expected status in the collection."""
    return self.tests_by_expression(lambda result: result.expected())

  def unexpected_tests(self):
    """A set of test names with any unexpected status in the collection."""
    return self.tests_by_expression(lambda result: not result.expected())

  def passed_tests(self):
    """A set of test names with any passed status in the collection."""
    return self.tests_by_expression(lambda result: result.status == TestStatus.
                                    PASS)

  def failed_tests(self):
    """A set of test names with any failed status in the collection."""
    return self.tests_by_expression(lambda result: result.status == TestStatus.
                                    FAIL)

  def flaky_tests(self):
    """A set of flaky test names in the collection."""
    return self.expected_tests().intersection(self.unexpected_tests())

  def never_expected_tests(self):
    """A set of test names with only unexpected status in the collection."""
    return self.unexpected_tests().difference(self.expected_tests())

  def pure_expected_tests(self):
    """A set of test names with only expected status in the collection."""
    return self.expected_tests().difference(self.unexpected_tests())

  def set_crashed_with_prefix(self, crash_message_prefix_line=''):
    """Updates collection with the crash status and add prefix to crash message.

    Typically called at the end of runner run when runner reports failure due to
    crash but there isn't unexpected tests. The crash status and crash message
    will reflect in LUCI build page step log.
    """
    self._crashed = True
    if crash_message_prefix_line:
      crash_message_prefix_line += '\n'
    self._crash_message = crash_message_prefix_line + self.crash_message

  def report_to_result_sink(self):
    """Reports current results to result sink once.

    Note that each |TestResult| object stores whether it's been reported and
    will only report itself once.
    """
    result_sink_client = ResultSinkClient()
    for test_result in self._test_results:
      test_result.report_to_result_sink(result_sink_client)
    result_sink_client.close()

  def standard_json_output(self, path_delimiter='.'):
    """Returns a dict object confirming to Chromium standard format.

    Format defined at:
      https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md
    """
    num_failures_by_type = {}
    tests = OrderedDict()
    seen_names = set()
    shard_index = shard_util.gtest_shard_index()

    for test_result in self._test_results:
      test_name = test_result.name

      # For "num_failures_by_type" field. The field contains result count map of
      # the first result of each test.
      if test_name not in seen_names:
        seen_names.add(test_name)
        result_type = _to_standard_json_literal(test_result.status)
        num_failures_by_type[result_type] = num_failures_by_type.get(
            result_type, 0) + 1

      # For "tests" field.
      if test_name not in tests:
        tests[test_name] = {
            'expected': _to_standard_json_literal(test_result.expected_status),
            'actual': _to_standard_json_literal(test_result.status),
            'shard': shard_index,
            'is_unexpected': not test_result.expected()
        }
      else:
        tests[test_name]['actual'] += (
            ' ' + _to_standard_json_literal(test_result.status))
        # This means there are both expected & unexpected results for the test.
        # Thus, the overall status would be expected (is_unexpected = False)
        # and the test is regarded flaky.
        if tests[test_name]['is_unexpected'] != (not test_result.expected()):
          tests[test_name]['is_unexpected'] = False
          tests[test_name]['is_flaky'] = True

    return {
        'version': 3,
        'path_delimiter': path_delimiter,
        'seconds_since_epoch': int(time.time()),
        'interrupted': self.crashed,
        'num_failures_by_type': num_failures_by_type,
        'tests': tests
    }

  def test_runner_logs(self):
    """Returns a dict object with test results as part of test runner logs."""
    # Test name to merged test log in all unexpected results. Logs are
    # only preserved for unexpected results.
    unexpected_logs = {}
    name_count = {}
    for test_result in self._test_results:
      if not test_result.expected():
        test_name = test_result.name
        name_count[test_name] = name_count.get(test_name, 0) + 1
        logs = unexpected_logs.get(test_name, [])
        logs.append('Failure log of attempt %d:' % name_count[test_name])
        logs.extend(test_result.test_log.split('\n'))
        unexpected_logs[test_name] = logs

    passed = list(self.passed_tests() & self.pure_expected_tests())
    disabled = list(self.disabled_tests())
    flaked = {
        test_name: unexpected_logs[test_name]
        for test_name in self.flaky_tests()
    }
    # "failed" in test runner logs are all unexpected failures (including
    # crash, etc).
    failed = {
        test_name: unexpected_logs[test_name]
        for test_name in self.never_expected_tests()
    }

    logs = OrderedDict()
    logs['passed tests'] = passed
    if disabled:
      logs['disabled tests'] = disabled
    if flaked:
      logs['flaked tests'] = flaked
    if failed:
      logs['failed tests'] = failed
    for test, log_lines in failed.items():
      logs[test] = log_lines
    for test, log_lines in flaked.items():
      logs[test] = log_lines

    if self.crashed:
      logs['test suite crash'] = self.crash_message.split('\n')

    return logs
