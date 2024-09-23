# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import copy
import json
import logging
import re

from test_result_util import ResultCollection, TestResult, TestStatus

LOGGER = logging.getLogger(__name__)


# These labels should match the ones output by gtest's JSON.
TEST_UNKNOWN_LABEL = 'UNKNOWN'
TEST_SUCCESS_LABEL = 'SUCCESS'
TEST_FAILURE_LABEL = 'FAILURE'
TEST_SKIPPED_LABEL = 'SKIPPED'
TEST_TIMEOUT_LABEL = 'TIMEOUT'
TEST_WARNING_LABEL = 'WARNING'

DID_NOT_COMPLETE = 'Did not complete.'


class GTestResult(object):
  """A result of gtest.

  The class will be depreacated soon. Please use
  |test_result_util.ResultCollection| instead. (crbug.com/1132476)

  Properties:
    command: The command argv.
    crashed: Whether or not the test crashed.
    crashed_test: The name of the test during which execution crashed, or
      None if a particular test didn't crash.
    failed_tests: A dict mapping the names of failed tests to a list of
      lines of output from those tests.
    flaked_tests: A dict mapping the names of failed flaky tests to a list
      of lines of output from those tests.
    passed_tests: A list of passed tests.
    perf_links: A dict mapping the names of perf data points collected
      to links to view those graphs.
    return_code: The return code of the command.
    success: Whether or not this run of the command was considered a
      successful GTest execution.
  """
  @property
  def crashed(self):
    return self._crashed

  @property
  def crashed_test(self):
    return self._crashed_test

  @property
  def command(self):
    return self._command

  @property
  def disabled_tests_from_compiled_tests_file(self):
    if self.__finalized:
      return copy.deepcopy(self._disabled_tests_from_compiled_tests_file)
    return self._disabled_tests_from_compiled_tests_file

  @property
  def failed_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._failed_tests)
    return self._failed_tests

  @property
  def flaked_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._flaked_tests)
    return self._flaked_tests

  @property
  def passed_tests(self):
    if self.__finalized:
      return copy.deepcopy(self._passed_tests)
    return self._passed_tests

  @property
  def perf_links(self):
    if self.__finalized:
      return copy.deepcopy(self._perf_links)
    return self._perf_links

  @property
  def return_code(self):
    return self._return_code

  @property
  def success(self):
    return self._success

  def __init__(self, command):
    if not isinstance(command, collections.Iterable):
      raise ValueError('Expected an iterable of command arguments.', command)

    if not command:
      raise ValueError('Expected a non-empty command.', command)

    self._command = tuple(command)
    self._crashed = False
    self._crashed_test = None
    self._disabled_tests_from_compiled_tests_file = []
    self._failed_tests = collections.OrderedDict()
    self._flaked_tests = collections.OrderedDict()
    self._passed_tests = []
    self._perf_links = collections.OrderedDict()
    self._return_code = None
    self._success = None
    self.__finalized = False

  def finalize(self, return_code, success):
    self._return_code = return_code
    self._success = success

    # If the test was not considered to be a GTest success, but had no
    # failing tests, conclude that it must have crashed.
    if not self._success and not self._failed_tests and not self._flaked_tests:
      self._crashed = True

    # At most one test can crash the entire app in a given parsing.
    for test, log_lines in self._failed_tests.items():
      # A test with no output would have crashed. No output is replaced
      # by the GTestLogParser by a sentence indicating non-completion.
      if DID_NOT_COMPLETE in log_lines:
        self._crashed = True
        self._crashed_test = test

    # A test marked as flaky may also have crashed the app.
    for test, log_lines in self._flaked_tests.items():
      if DID_NOT_COMPLETE in log_lines:
        self._crashed = True
        self._crashed_test = test

    self.__finalized = True


class GTestLogParser(object):
  """This helper class process GTest test output."""

  def __init__(self):
    # Test results from the parser.
    self._result_collection = ResultCollection()

    # State tracking for log parsing
    self.completed = False
    self._current_test = ''
    self._failure_description = []
    self._parsing_failures = False
    self._spawning_launcher = False

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

    # Disabled tests by parsing the compiled tests json file output from GTest.
    self._disabled_tests_from_compiled_tests_file = []
    self._flaky_tests = 0

    # Regular expressions for parsing GTest logs. Test names look like
    # "x.y", with 0 or more "w/" prefixes and 0 or more "/z" suffixes.
    # e.g.:
    #   SomeName/SomeTestCase.SomeTest/1
    #   SomeName/SomeTestCase/1.SomeTest
    #   SomeName/SomeTestCase/1.SomeTest/SomeModifider
    test_name_regexp = r'((\w+/)*\w+\.\w+(/\w+)*)'

    self._master_name_re = re.compile(r'\[Running for master: "([^"]*)"')
    self.master_name = ''

    self._test_name = re.compile(test_name_regexp)
    self._test_start = re.compile(r'\[\s+RUN\s+\] ' + test_name_regexp)
    self._test_ok = re.compile(r'\[\s+OK\s+\] ' + test_name_regexp)
    self._test_fail = re.compile(r'\[\s+FAILED\s+\] ' + test_name_regexp)
    self._test_passed = re.compile(r'\[\s+PASSED\s+\] \d+ tests?.')
    self._spawning_test_launcher = re.compile(r'Using \d+ parallel jobs?.')
    self._test_run_spawning = re.compile(r'\[\d+\/\d+\] ' + test_name_regexp +
                                         ' \([0-9]+ ms\)')
    self._test_passed_spawning = re.compile(r'Tests took \d+ seconds?.')
    self._test_retry_spawning = re.compile(
        r'Retrying \d+ test[s]? \(retry #\d+\)')
    self._test_skipped = re.compile(r'\[\s+SKIPPED\s+\] ' + test_name_regexp)
    self._run_test_cases_line = re.compile(
        r'\[\s*\d+\/\d+\]\s+[0-9\.]+s ' + test_name_regexp + ' .+')
    self._test_timeout = re.compile(
        r'Test timeout \([0-9]+ ms\) exceeded for ' + test_name_regexp)
    self._disabled = re.compile(r'\s*YOU HAVE (\d+) DISABLED TEST')
    self._flaky = re.compile(r'\s*YOU HAVE (\d+) FLAKY TEST')

    self._retry_message = re.compile('RETRYING FAILED TESTS:')
    self.retrying_failed = False

    self._compiled_tests_file_path_re = re.compile(
        '.*Wrote compiled tests to file: (\S+)')

    self.TEST_STATUS_MAP = {
        'OK': TEST_SUCCESS_LABEL,
        'failed': TEST_FAILURE_LABEL,
        'skipped': TEST_SKIPPED_LABEL,
        'timeout': TEST_TIMEOUT_LABEL,
        'warning': TEST_WARNING_LABEL
    }

    self.compiled_tests_file_path = None
    self._tests_loc_map = {}

  def GetCurrentTest(self):
    return self._current_test

  def GetResultCollection(self):
    return self._result_collection

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

  def SkippedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that were skipped"""
    return self._TestsByStatus('skipped', include_fails, include_flaky)

  def TriesForTest(self, test):
    """Returns a list containing the state for all tries of the given test.
    This parser doesn't support retries so a single result is returned."""
    return [self.TEST_STATUS_MAP.get(self._StatusOfTest(test),
                                    TEST_UNKNOWN_LABEL)]

  def DisabledTests(self):
    """Returns the name of the disabled test (if there is only 1) or the number
    of disabled tests.
    """
    return self._disabled_tests

  def DisabledTestsFromCompiledTestsFile(self):
    """Returns the list of disabled tests in format '{TestCaseName}/{TestName}'.

       Find all test names starting with DISABLED_ from the compiled test json
       file if there is one. If there isn't or error in parsing, returns an
       empty list.
    """
    return self._disabled_tests_from_compiled_tests_file

  def FlakyTests(self):
    """Returns the name of the flaky test (if there is only 1) or the number
    of flaky tests.
    """
    return self._flaky_tests

  def FailureDescription(self, test):
    """Returns a list containing the failure description for the given test.

    If the test didn't fail or timeout, returns [].
    """
    test_status = self._test_status.get(test, ('', []))
    return ['%s: ' % test] + test_status[1]

  def CompletedWithoutFailure(self):
    """Returns True if all tests completed and no tests failed unexpectedly."""
    return self.completed and not self.FailedTests()

  def Finalize(self):
    """Finalize for |self._result_collection|.

    Called at the end to add unfinished tests and crash status for
        self._result_collection.
    """
    # Remaining logs after crash before exit.
    raw_remaining_logs = self._failure_description
    for test in self.RunningTests():
      self._test_status[test][1].extend(
          ['Potential test logs from crash until the end of test program:'])
      self._test_status[test][1].extend(raw_remaining_logs)
      self._result_collection.add_test_result(
          TestResult(
              test,
              TestStatus.CRASH,
              test_log='\n'.join(self._test_status[test][1])))
      self._result_collection.crashed = True

    if not self.completed:
      self._result_collection.crashed = True

  def _ParseDuration(self, line):
    """Returns test duration in milliseconds, None if not present."""
    # Test duration appears as suffix of status line like:
    # "[       OK ] SomeTest.SomeTestName (539 ms)"
    test_duration_regex = re.compile(r'\s+\(([0-9]+)\s+ms\)')
    results = test_duration_regex.search(line)
    if results:
      return int(results.group(1))
    return None

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
        self._test_passed,
        self._test_skipped,
        self._spawning_test_launcher,
        self._test_run_spawning,
        self._test_passed_spawning,
        self._test_retry_spawning,
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
    # Note: When sharding, the number of disabled and flaky tests will be read
    # multiple times, so this will only show the most recent values (but they
    # should all be the same anyway).

    # Is it a line listing the master name?
    if not self.master_name:
      results = self._master_name_re.match(line)
      if results:
        self.master_name = results.group(1)

    results = self._run_test_cases_line.match(line)
    if results:
      # A run_test_cases.py output.
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
          self._result_collection.add_test_result(
              TestResult(
                  self._current_test,
                  TestStatus.ABORT,
                  test_log='\n'.join(self._failure_description)))
      self._current_test = ''
      self._failure_description = []
      return

    # Is it a line declaring spawning test runner? If so then
    # we really don't need to parse any of the test cases since 1) they are
    # in a different format 2) retries happen inside the test launcher itself.
    results = self._spawning_test_launcher.match(line)
    if results:
      self._spawning_launcher = True
      self._result_collection.spawning_test_launcher = True
      return

    # With the spawning test launcher the log format is slightly different.
    # On failures the original gtest log format will be spit out to stdout
    # from the subprocess and the regular log parsing will be used. If all
    # tests succeeded in the subprocess only completion lines will be written
    # out.
    if self._spawning_launcher:
      results = self._test_passed_spawning.match(line)
      if results:
        self.completed = True
        self._current_test = ''
        return
      results = self._test_run_spawning.match(line)
      if results:
        test_name = results.group(1)
        duration = self._ParseDuration(line)
        status = self._StatusOfTest(test_name)
        # If we encountered this line and it is not known then we know it
        # passed.
        if status in ('started', 'not known'):
          self._test_status[test_name] = ('OK', [])
          self._result_collection.add_test_result(
              TestResult(test_name, TestStatus.PASS, duration=duration))
          self._failure_description = []
          self._current_test = ''
        return
      results = self._test_retry_spawning.match(line)
      # We are retrying failed tests so mark them all as started again
      # so that we will be in the correct state when we either see
      # a failure or a completion result.
      if results:
        for test in self.FailedTests():
          self._test_status[test] = ('started', [DID_NOT_COMPLETE])
        self.retrying_failed = True

    # Is it a line declaring all tests passed?
    results = self._test_passed.match(line)
    if results:
      self.completed = True
      self._current_test = ''
      return

    # Is it a line reporting disabled tests?
    results = self._disabled.match(line)
    if results:
      try:
        disabled = int(results.group(1))
      except ValueError:
        disabled = 0
      if disabled > 0 and isinstance(self._disabled_tests, int):
        self._disabled_tests = disabled
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._disabled_tests = 'some'
      return

    # Is it a line reporting flaky tests?
    results = self._flaky.match(line)
    if results:
      try:
        flaky = int(results.group(1))
      except ValueError:
        flaky = 0
      if flaky > 0 and isinstance(self._flaky_tests, int):
        self._flaky_tests = flaky
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._flaky_tests = 'some'
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
      test_name = results.group(1)
      self._test_status[test_name] = ('started', [DID_NOT_COMPLETE])
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
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      duration = self._ParseDuration(line)
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
                duration=duration,
                test_log='\n'.join(self._failure_description)))
      else:
        self._test_status[test_name] = ('OK', [])
        self._result_collection.add_test_result(
            TestResult(test_name, TestStatus.PASS, duration=duration))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test skipped line?
    results = self._test_skipped.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      # Skipped tests are listed again in the summary.
      if status not in ('started', 'skipped'):
        self._RecordError(line, 'skipped while in status %s' % status)
      self._test_status[test_name] = ('skipped', [])
      self._result_collection.add_test_result(
          TestResult(
              test_name,
              TestStatus.SKIP,
              expected_status=TestStatus.SKIP,
              test_log='Test skipped when running suite.'))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test failure line?
    results = self._test_fail.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      duration = self._ParseDuration(line)
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
              duration=duration,
              test_log='\n'.join(self._failure_description)))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test timeout line?
    results = self._test_timeout.search(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed'):
        self._RecordError(line, 'timeout while in status %s' % status)
      logs = self._failure_description + ['Killed (timed out).']
      self._test_status[test_name] = ('timeout', logs)
      self._result_collection.add_test_result(
          TestResult(
              test_name,
              TestStatus.ABORT,
              test_log='\n'.join(logs),
          ))
      self._failure_description = []
      self._current_test = ''
      return

    # Is it the start of the retry tests?
    results = self._retry_message.match(line)
    if results:
      self.retrying_failed = True
      return

    # Is it the line containing path to the compiled tests json file?
    results = self._compiled_tests_file_path_re.match(line)
    if results:
      self.compiled_tests_file_path = results.group(1)
      LOGGER.info('Compiled tests json file path: %s' %
                  self.compiled_tests_file_path)
      return

    # Random line: if we're in a test, collect it for the failure description.
    # Tests may run simultaneously, so this might be off, but it's worth a try.
    # This also won't work if a test times out before it begins running.
    if self._current_test:
      self._failure_description.append(line)

    # Parse the "Failing tests:" list at the end of the output, and add any
    # additional failed tests to the list. For example, this includes tests
    # that crash after the OK line.
    if self._parsing_failures:
      results = self._test_name.match(line)
      if results:
        test_name = results.group(1)
        status = self._StatusOfTest(test_name)
        if status in ('not known', 'OK'):
          unknown_error_log = 'Unknown error, see stdio log.'
          self._test_status[test_name] = ('failed', [unknown_error_log])
          self._result_collection.add_test_result(
              TestResult(
                  test_name, TestStatus.FAIL, test_log=unknown_error_log))
      else:
        self._parsing_failures = False
    elif line.startswith('Failing tests:'):
      self._parsing_failures = True

  # host_test_file_path is compiled_tests_file_path by default on simulators.
  # However, for device tests,
  # it needs to be overridden with a path that exists on the host because
  # compiled_tests_file_path is the path on the device in this case
  def ParseAndPopulateTestResultLocations(self,
                                          test_repo,
                                          output_disabled_tests,
                                          host_test_file_path=None):
    try:
      # TODO(crbug.com/40134137): Read the file when running on device.
      # Parse compiled test file first. If output_disabled_tests is true,
      # then include disabled tests in the test results.
      if host_test_file_path is None:
        host_test_file_path = self.compiled_tests_file_path
      if host_test_file_path is None:
        return
      with open(host_test_file_path) as f:
        disabled_tests_from_json = []
        compiled_tests = json.load(f)
        for single_test in compiled_tests:
          test_case_name = single_test.get('test_case_name')
          test_name = single_test.get('test_name')
          test_file = single_test.get('file')
          test_file = test_file.replace('../../', '//')
          full_test_name = str('%s.%s' % (test_case_name, test_name))
          self._tests_loc_map[full_test_name] = test_file
          if test_case_name and test_name and test_name.startswith('DISABLED_'):
            if output_disabled_tests:
              test_loc = {'repo': test_repo, 'fileName': test_file}
              disabled_tests_from_json.append(full_test_name)
              self._result_collection.add_test_result(
                  TestResult(
                      full_test_name,
                      TestStatus.SKIP,
                      expected_status=TestStatus.SKIP,
                      test_log='Test disabled.',
                      test_loc=test_loc))
        self._disabled_tests_from_compiled_tests_file = (
            disabled_tests_from_json)

      # Populate location info for test results in result collections
      for test_result in self._result_collection.test_results:
        test_file = self._tests_loc_map.get(test_result.name, None)
        if test_file:
          test_loc = {'repo': test_repo, 'fileName': test_file}
          test_result.test_loc = test_loc
    except Exception as e:
      LOGGER.warning(
          'Error when finding disabled tests in compiled tests json file: %s' %
          e)
    return
