# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from test_result_util import ResultCollection, TestResult, TestStatus


# These labels should match the ones output by gtest's JSON.
TEST_UNKNOWN_LABEL = 'UNKNOWN'
TEST_SUCCESS_LABEL = 'SUCCESS'
TEST_FAILURE_LABEL = 'FAILURE'
TEST_CRASH_LABEL = 'CRASH'
TEST_TIMEOUT_LABEL = 'TIMEOUT'
TEST_WARNING_LABEL = 'WARNING'


class XCTestLogParser(object):
  """This helper class process XCTest test output."""

  def __init__(self):
    # Test results from the parser.
    self._result_collection = ResultCollection()

    # State tracking for log parsing
    self.completed = False
    self._current_test = ''
    self._failure_description = []
    self._current_report_hash = ''
    self._current_report = []
    self._parsing_failures = False

    # Line number currently being processed.
    self._line_number = 0

    # List of parsing errors, as human-readable strings.
    self._internal_error_lines = []

    # Tests are stored here as 'test.name': (status, [description]).
    # The status should be one of ('started', 'OK', 'failed', 'timeout',
    # 'warning'). Warning indicates that a test did not pass when run in
    # parallel with other tests but passed when run alone. The description is
    # a list of lines detailing the test's error, as reported in the log.
    self._test_status = {}

    # This may be either text or a number. It will be used in the phrase
    # '%s disabled' or '%s flaky' on the waterfall display.
    self._disabled_tests = 0
    self._flaky_tests = 0

    test_name_regexp = r'\-\[(\w+)\s(\w+)\]'
    self._test_name = re.compile(test_name_regexp)
    self._test_start = re.compile(
        r'Test Case \'' + test_name_regexp + '\' started\.')
    self._test_ok = re.compile(
        r'Test Case \'' + test_name_regexp +
          '\' passed\s+\(\d+\.\d+\s+seconds\)?.')
    self._test_fail = re.compile(
        r'Test Case \'' + test_name_regexp +
          '\' failed\s+\(\d+\.\d+\s+seconds\)?.')
    self._test_execute_succeeded = re.compile(
        r'\*\*\s+TEST\s+EXECUTE\s+SUCCEEDED\s+\*\*')
    self._test_execute_failed = re.compile(
        r'\*\*\s+TEST\s+EXECUTE\s+FAILED\s+\*\*')
    self._retry_message = re.compile('RETRYING FAILED TESTS:')
    self.retrying_failed = False

    self._system_alert_present_message = re.compile(
        r'\bSystem alert view is present, so skipping all tests\b')
    self.system_alert_present = False

    self.TEST_STATUS_MAP = {
      'OK': TEST_SUCCESS_LABEL,
      'failed': TEST_FAILURE_LABEL,
      'timeout': TEST_TIMEOUT_LABEL,
      'warning': TEST_WARNING_LABEL
    }

  def Finalize(self):
    """Finalize for |self._result_collection|.

    Called at the end to add unfinished tests and crash status for
        self._result_collection.
    """
    for test in self.RunningTests():
      self._result_collection.add_test_result(
          TestResult(test[0], TestStatus.CRASH, test_log='Did not complete.'))

    if not self.completed:
      self._result_collection.crashed = True

  def GetResultCollection(self):
    return self._result_collection

  def GetCurrentTest(self):
    return self._current_test

  def _StatusOfTest(self, test):
    """Returns the status code for the given test, or 'not known'."""
    test_status = self._test_status.get(test, ('not known', []))
    return test_status[0]

  def _TestsByStatus(self, status, include_fails, include_flaky):
    """Returns list of tests with the given status.

    Args:
      include_fails: If False, tests containing 'FAILS_' anywhere in their
          names will be excluded from the list.
      include_flaky: If False, tests containing 'FLAKY_' anywhere in their
          names will be excluded from the list.
    """
    test_list = [x[0] for x in self._test_status.items()
                 if self._StatusOfTest(x[0]) == status]

    if not include_fails:
      test_list = [x for x in test_list if x.find('FAILS_') == -1]
    if not include_flaky:
      test_list = [x for x in test_list if x.find('FLAKY_') == -1]

    return test_list

  def _RecordError(self, line, reason):
    """Record a log line that produced a parsing error.

    Args:
      line: text of the line at which the error occurred
      reason: a string describing the error
    """
    self._internal_error_lines.append('%s: %s [%s]' %
                                      (self._line_number, line.strip(), reason))

  def RunningTests(self):
    """Returns list of tests that appear to be currently running."""
    return self._TestsByStatus('started', True, True)

  def ParsingErrors(self):
    """Returns a list of lines that have caused parsing errors."""
    return self._internal_error_lines

  def ClearParsingErrors(self):
    """Clears the currently stored parsing errors."""
    self._internal_error_lines = ['Cleared.']

  def PassedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that passed."""
    return self._TestsByStatus('OK', include_fails, include_flaky)

  def FailedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that failed, timed out, or didn't finish
    (crashed).

    This list will be incorrect until the complete log has been processed,
    because it will show currently running tests as having failed.

    Args:
      include_fails: If true, all failing tests with FAILS_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.
      include_flaky: If true, all failing tests with FLAKY_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.

    """
    return (self._TestsByStatus('failed', include_fails, include_flaky) +
            self._TestsByStatus('timeout', True, True) +
            self._TestsByStatus('warning', include_fails, include_flaky) +
            self.RunningTests())

  def TriesForTest(self, test):
    """Returns a list containing the state for all tries of the given test.
    This parser doesn't support retries so a single result is returned."""
    return [self.TEST_STATUS_MAP.get(self._StatusOfTest(test),
                                    TEST_UNKNOWN_LABEL)]

  def FailureDescription(self, test):
    """Returns a list containing the failure description for the given test.

    If the test didn't fail or timeout, returns [].
    """
    test_status = self._test_status.get(test, ('', []))
    return ['%s: ' % test] + test_status[1]

  def CompletedWithoutFailure(self):
    """Returns True if all tests completed and no tests failed unexpectedly."""
    return self.completed and not self.FailedTests()

  def SystemAlertPresent(self):
    """Returns a bool indicating whether a system alert is shown on device."""
    return self.system_alert_present

  def ProcessLine(self, line):
    """This is called once with each line of the test log."""

    # Track line number for error messages.
    self._line_number += 1

    # Some tests (net_unittests in particular) run subprocesses which can write
    # stuff to shared stdout buffer. Sometimes such output appears between new
    # line and gtest directives ('[  RUN  ]', etc) which breaks the parser.
    # Code below tries to detect such cases and recognize a mixed line as two
    # separate lines.

    # List of regexps that parses expects to find at the start of a line but
    # which can be somewhere in the middle.
    gtest_regexps = [
        self._test_start,
        self._test_ok,
        self._test_fail,
        self._test_execute_failed,
        self._test_execute_succeeded,
    ]

    for regexp in gtest_regexps:
      match = regexp.search(line)
      if match:
        break

    if not match or match.start() == 0:
      self._ProcessLine(line)
    else:
      self._ProcessLine(line[:match.start()])
      self._ProcessLine(line[match.start():])

  def _ProcessLine(self, line):
    """Parses the line and changes the state of parsed tests accordingly.

    Will recognize newly started tests, OK or FAILED statuses, timeouts, etc.
    """

    # Is it a line declaring end of all tests?
    succeeded = self._test_execute_succeeded.match(line)
    failed = self._test_execute_failed.match(line)
    if succeeded or failed:
      self.completed = True
      self._current_test = ''
      return

    # Is it a line declaring a system alert is shown on the device?
    results = self._system_alert_present_message.search(line)
    if results:
      self.system_alert_present = True
      self._current_test = ''
      return

    # Is it the start of a test?
    results = self._test_start.match(line)
    if results:
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
          self._result_collection.add_test_result(
              TestResult(
                  self._current_test,
                  TestStatus.ABORT,
                  test_log='\n'.join(self._failure_description)))
      test_name = '%s/%s' % (results.group(1), results.group(2))
      self._test_status[test_name] = ('started', ['Did not complete.'])
      self._current_test = test_name
      if self.retrying_failed:
        self._failure_description = self._test_status[test_name][1]
        self._failure_description.extend(['', 'RETRY OUTPUT:', ''])
      else:
        self._failure_description = []
      return

    # Is it a test success line?
    results = self._test_ok.match(line)
    if results:
      test_name = '%s/%s' % (results.group(1), results.group(2))
      status = self._StatusOfTest(test_name)
      if status != 'started':
        self._RecordError(line, 'success while in status %s' % status)
      if self.retrying_failed:
        self._test_status[test_name] = ('warning', self._failure_description)
        # This is a passed result. Previous failures were reported in separate
        # TestResult objects.
        self._result_collection.add_test_result(
            TestResult(
                test_name,
                TestStatus.PASS,
                test_log='\n'.join(self._failure_description)))
      else:
        self._test_status[test_name] = ('OK', [])
        self._result_collection.add_test_result(
            TestResult(test_name, TestStatus.PASS))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test failure line?
    results = self._test_fail.match(line)
    if results:
      test_name = '%s/%s' % (results.group(1), results.group(2))
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed', 'timeout'):
        self._RecordError(line, 'failure while in status %s' % status)
      if self._current_test != test_name:
        if self._current_test:
          self._RecordError(
              line,
              '%s failure while in test %s' % (test_name, self._current_test))
        return
      # Don't overwrite the failure description when a failing test is listed a
      # second time in the summary, or if it was already recorded as timing
      # out.
      if status not in ('failed', 'timeout'):
        self._test_status[test_name] = ('failed', self._failure_description)
      # Add to |test_results| regardless whether the test ran before.
      self._result_collection.add_test_result(
          TestResult(
              test_name,
              TestStatus.FAIL,
              test_log='\n'.join(self._failure_description)))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it the start of the retry tests?
    results = self._retry_message.match(line)
    if results:
      self.retrying_failed = True
      return

    # Random line: if we're in a test, collect it for the failure description.
    # Tests may run simultaneously, so this might be off, but it's worth a try.
    # This also won't work if a test times out before it begins running.
    if self._current_test:
      self._failure_description.append(line)
