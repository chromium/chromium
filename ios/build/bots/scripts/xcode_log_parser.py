# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""XCode test log parser."""

import json
import logging
import os
import plistlib
import re
import shutil
import subprocess
import sys

import constants
import file_util
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner
import xcode_util


# Some system errors are reported as failed tests in Xcode test result log in
# Xcode 12, e.g. test app crash in xctest parallel testing. This is reported
# as 'BUILD_INTERRUPTED' if it's in final attempt. If not in final attempt, it
# will be ignored since future attempts will cover tests not ran.
SYSTEM_ERROR_TEST_NAME_SUFFIXES = ['encountered an error']
LOGGER = logging.getLogger(__name__)

_XCRESULT_SUFFIX = '.xcresult'

IPS_REGEX = re.compile(r'ios_.*chrome.+\.ips')

# Messages checked for in EG test logs to determine if the app crashed
# see: https://github.com/google/EarlGrey/blob/earlgrey2/TestLib/DistantObject/GREYTestApplicationDistantObject.m
CRASH_REGEX = re.compile(
    r'(App crashed and disconnected\.)|'
    r'(App process is hanging\.)|'
    r'(Crash: ios_chrome_.+_eg2tests_module-Runner \(\d+\) )')


def _sanitize_str(line):
  """Encodes str when in python 2."""
  if sys.version_info.major == 2:
    if isinstance(line, unicode):
      line = line.encode('utf-8')
  return line


def _sanitize_str_list(lines):
  """Encodes any unicode in list when in python 2."""
  sanitized_lines = []
  for line in lines:
    sanitized_lines.append(_sanitize_str(line))
  return sanitized_lines


def parse_passed_failed_tests_for_interrupted_run(output):
  """Parses xcode runner output to get passed & failed tests.

  Args:
    output: [str] An output of test run.

  Returns:
    test_result_util.ResultCollection: Results of tests parsed.
  """
  result = ResultCollection()
  passed_tests = []
  failed_tests = []
  # Test has format:
  # [09:04:42:INFO] Test case '-[Test_class test_method]' passed.
  # [09:04:42:INFO] Test Case '-[Test_class test_method]' failed.
  passed_test_regex = re.compile(r'Test [Cc]ase \'\-\[(.+?)\s(.+?)\]\' passed')
  failed_test_regex = re.compile(r'Test [Cc]ase \'\-\[(.+?)\s(.+?)\]\' failed')

  def _find_list_of_tests(tests, regex):
    """Adds test names matched by regex to result list."""
    for test_line in output:
      m_test = regex.search(test_line)
      if m_test:
        tests.append('%s/%s' % (m_test.group(1), m_test.group(2)))

  _find_list_of_tests(passed_tests, passed_test_regex)
  _find_list_of_tests(failed_tests, failed_test_regex)
  result.add_test_names_status(passed_tests, TestStatus.PASS)
  result.add_test_names_status(
      failed_tests,
      TestStatus.FAIL,
      test_log='Test failed in interrupted(timedout) run.')

  LOGGER.info('%d passed tests for interrupted build.' % len(passed_tests))
  LOGGER.info('%d failed tests for interrupted build.' % len(failed_tests))
  return result


def format_test_case(test_case):
  """Format test case from `-[TestClass TestMethod]` to `TestClass_TestMethod`.

  Args:
    test_case: (basestring) Test case id in format `-[TestClass TestMethod]` or
               `[TestClass/TestMethod]`

  Returns:
    (str) Test case id in format TestClass/TestMethod.
  """
  test_case = _sanitize_str(test_case)
  test = test_case.replace('[', '').replace(']',
                                            '').replace('-',
                                                        '').replace(' ', '/')
  return test


def copy_screenshots_for_failed_test(failure_message, test_case_folder):
  screenshot_regex = re.compile(r'Screenshots:\s({(\n.*)+?\n})')
  screenshots = screenshot_regex.search(failure_message)
  if not os.path.exists(test_case_folder):
    os.makedirs(test_case_folder)
  if screenshots:
    screenshots_files = screenshots.group(1).strip()
    # For some failures xcodebuild attaches screenshots in the `Attachments`
    # folder and in plist no paths to them, only references e.g.
    # "Screenshot At Failure" : <UIImage: 0x6000032ab410>, {768, 1024}
    if 'UIImage:' in screenshots_files:
      return
    d = json.loads(screenshots_files)
    for f in d.values():
      if not os.path.exists(f):
        continue
      screenshot = os.path.join(test_case_folder, os.path.basename(f))
      shutil.copyfile(f, screenshot)


def test_crashed(root):
  actionResultMetrics = root.get('actions',
                                 {}).get('_values',
                                         [{}])[0].get('actionResult',
                                                      {}).get('metrics', {})

  # In case of test crash both numbers of run and failed tests are equal to 0.
  actionResultMetricsMissing = (
      actionResultMetrics.get('testsCount', {}).get('_value', 0) == 0 and
      actionResultMetrics.get('testsFailedCount', {}).get('_value', 0) == 0 and
      actionResultMetrics.get('errorCount', {}).get('_value', 0) == 0)
  # After certain types of test failures action results metrics might be missing
  # but root metrics may still be present, indicating that some tests still
  # ran successfully and the entire test suite should not be considered crashed
  rootMetricsMissing = (
      root.get('metrics', {}).get('testsCount', {}).get('_value', 0) == 0 and
      root.get('metrics', {}).get('testsFailedCount', {}).get('_value', 0) == 0
      and root.get('metrics', {}).get('errorCount', {}).get('_value', 0) == 0)
  # if both metrics are missing then consider the test app to have crashed
  return actionResultMetricsMissing and rootMetricsMissing


def xcode16_test_crashed(summary):
  # both numbers of passed and failed tests are equal to 0.
  crashed = (
      summary.get('failedTests', 0) == 0 and summary.get('passedTests', 0) == 0)
  return crashed


def get_test_suites(summary, xcode_parallel_enabled):
  # On Xcode16+, enabling test parallelization will cause test result format
  # to vary slightly
  if xcode_parallel_enabled and xcode_util.using_xcode_16_or_higher():
    return summary['tests']['_values']
  return summary['tests']['_values'][0]['subtests']['_values'][0]['subtests'][
      '_values']


def duration_to_milliseconds(duration_str):
  """Converts a duration string (e.g., "11s", "3m 10s") to milliseconds.

    Args:
        duration_str: The duration string to convert.

    Returns:
        The duration in milliseconds (as a float), or None if the
          format is invalid.
    """

  # Matches optional minutes and seconds
  pattern = r"(?:(\d+)m\s*)?(?:(\d+)s)?$"
  match = re.match(pattern, duration_str)

  if not match:
    return None  # Invalid format

  minutes, seconds = match.groups()

  # If both minutes and seconds are None, return None
  if minutes is None and seconds is None:
    return None

  milliseconds = 0.0
  if minutes:
    milliseconds += int(minutes) * 60000  # Minutes to milliseconds
  if seconds:
    milliseconds += int(seconds) * 1000  # Seconds to milliseconds

  return milliseconds


class XcodeLogParser(object):
  """Xcode log parser. Parse Xcode result types v3."""

  @staticmethod
  def _xcresulttool_get(xcresult_path, ref_id=None):
    """Runs `xcresulttool get` command and returns JSON output.

    Xcresult folder contains test result in Xcode Result Types v. 3.19.
    Documentation of xcresulttool usage is in
    https://help.apple.com/xcode/mac/current/#/devc38fc7392?sub=dev0fe9c3ea3

    Args:
      xcresult_path: A full path to xcresult folder that must have Info.plist.
      ref_id: A reference id used in a command and can be used to get test data.
          If id is from ['timelineRef', 'logRef', 'testsRef', 'diagnosticsRef']
          method will run xcresulttool 2 times:
          1. to get specific id value running command without id parameter.
            xcresulttool get --path %xcresul%
          2. to get data based on id
            xcresulttool get --path %xcresul% --id %id%

    Returns:
      An output of a command in JSON format.
    """
    xcode_info = test_runner.get_current_xcode_info()
    folder = os.path.join(xcode_info['path'], 'usr', 'bin')
    # By default xcresulttool is %Xcode%/usr/bin,
    # that is not in directories from $PATH
    # Need to check whether %Xcode%/usr/bin is in a $PATH
    # and then call xcresulttool
    if folder not in os.environ['PATH']:
      os.environ['PATH'] += ':%s' % folder
    reference_types = ['timelineRef', 'logRef', 'testsRef', 'diagnosticsRef']
    if ref_id in reference_types:
      data = json.loads(XcodeLogParser._xcresulttool_get(xcresult_path))
      # Redefine ref_id to get only the reference data
      ref_id = data['actions']['_values'][0]['actionResult'][
          ref_id]['id']['_value']
    # If no ref_id then xcresulttool will use default(root) id.
    id_params = ['--id', ref_id] if ref_id else []
    xcresult_command = ['xcresulttool', 'get', '--format', 'json',
                        '--path', xcresult_path] + id_params
    if xcode_util.using_xcode_16_or_higher():
      xcresult_command.append('--legacy')
    return subprocess.check_output(xcresult_command).decode('utf-8').strip()

  @staticmethod
  def _list_of_failed_tests(actions_invocation_record, excluded=None):
    """Gets failed tests from xcresult root data.

    ActionsInvocationRecord is an object that contains properties:
      + metadataRef: id of the record that can be get as
        `xcresult get --path xcresult --id metadataRef`
      + metrics: number of run and failed tests.
      + issues: contains TestFailureIssueSummary in case of failure otherwise
        it contains just declaration of `issues` node.
      + actions: a list of ActionRecord.

    Args:
      actions_invocation_record: An output of `xcresult get --path xcresult`.
      excluded: A set of tests that will be excluded.

    Returns:
      test_results.ResultCollection: Results of failed tests.
    """
    excluded = excluded or set()
    result = ResultCollection()
    if 'testFailureSummaries' not in actions_invocation_record['issues']:
      return result
    for failure_summary in actions_invocation_record['issues'][
        'testFailureSummaries']['_values']:
      test_case_id = format_test_case(failure_summary['testCaseName']['_value'])
      if test_case_id in excluded:
        continue
      error_line = _sanitize_str(
          failure_summary['documentLocationInCreatingWorkspace'].get(
              'url', {}).get('_value', ''))
      fail_message = error_line + '\n' + _sanitize_str(
          failure_summary['message']['_value'])
      result.add_test_result(
          TestResult(test_case_id, TestStatus.FAIL, test_log=fail_message))
    return result

  @staticmethod
  def _get_app_side_failure(test_result, output_path):
    """Parses and returns app side failure reason in the event that a test
    causes the app to crash. Also has the side effect of adding host app log
    files to the test_result object's attachments as well as marking the
    test_result as containing an asan failure if one is detected.

    Args:
      test_result: (TestResult) The TestResult object that represents this
        failure.
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.

    Returns:
      (str) Formatted app side failure message or a message saying failure
        reason is missing
    """
    app_side_failure_message = ''
    parent_output_dir = os.path.realpath(os.path.join(output_path, os.pardir))

    # get host app stdout logs
    attempt_num = output_path.split('/')[-1]
    regex = re.compile(rf'{attempt_num}.*StandardOutputAndStandardError-')
    files = {
      file: os.path.join(parent_output_dir, file)
      for file in os.listdir(parent_output_dir)
      if regex.match(file)
    }
    test_result.attachments.update(files)

    # look for logs printed during the failing test method
    formatted_test_name = test_result.name.replace('/', ' ')
    error_message_regex = (
        rf'(Starting test: -\[{formatted_test_name}\].*?)'
        rf'((Standard output and standard error from)|(Starting test: -)|(\Z))')
    for file_path in files.values():
      with open(file_path, 'r') as f:
        contents = f.read()
        match = re.search(error_message_regex, contents, flags=re.DOTALL)
        if match:
          app_side_failure_message = match.group(1)
          break

    log_file_names = ', '.join(files.keys())
    if not app_side_failure_message:
      failure_reason_missing = (
          f'{constants.CRASH_MESSAGE}\n'
          f'App side failure reason not found for {test_result.name}.\n'
          f'For complete logs see {log_file_names} in Artifacts.\n')
      return failure_reason_missing

    app_crashed_message = f'{constants.CRASH_MESSAGE}\n'
    if constants.ASAN_ERROR in app_side_failure_message:
      test_result.asan_failure_detected = True
      app_crashed_message += f'{constants.ASAN_ERROR}\n'

    # omit layout constraint warnings since they can clutter logs and make the
    # actual reason why the app crashed difficult to find
    app_side_failure_message = re.sub(
        r'Unable to simultaneously satisfy constraints.(.*?)'
        r'may also be helpful',
        constants.LAYOUT_CONSTRAINT_MSG,
        app_side_failure_message,
        flags=re.DOTALL)

    app_crashed_message += (
        f'Showing logs from application under test. For complete logs see '
        f'{log_file_names} in Artifacts.\n\n{app_side_failure_message}\n')
    return app_crashed_message

  @staticmethod
  def _get_test_statuses(output_path, xcode_parallel_enabled):
    """Returns test results from xcresult.

    Also extracts and stores attachments for failed tests

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
      xcode_parallel_enabled: whether xcode parrallelization is enabled on
          the test run, which might cause test result format to vary slightly.

    Returns:
      test_result.ResultCollection: Test results.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    result = ResultCollection()
    # See TESTS_REF in xcode_log_parser_test.py for an example of |root|.
    root = json.loads(XcodeLogParser._xcresulttool_get(xcresult, 'testsRef'))
    for summary in root['summaries']['_values'][0][
        'testableSummaries']['_values']:
      if not summary['tests']:
        continue
      test_suites = get_test_suites(summary, xcode_parallel_enabled)
      for test_suite in test_suites:
        if 'subtests' not in test_suite:
          # Sometimes(if crash occurs) `subtests` node does not upload.
          # It happens only for failed tests that and a list of failures
          # can be parsed from root.
          continue
        for test in test_suite['subtests']['_values']:
          test_name = _sanitize_str(test['identifier']['_value'])
          duration = test.get('duration', {}).get('_value')
          if duration:
            # Raw duration is a str in seconds with decimals if it exists.
            # Convert to milliseconds as int as used in |TestResult|.
            duration = int(float(duration) * 1000)
          if any(
              test_name.endswith(suffix)
              for suffix in SYSTEM_ERROR_TEST_NAME_SUFFIXES):
            result.crashed = True
            result.crash_message += 'System error in %s: %s\n' % (xcresult,
                                                                  test_name)
            continue
          # If a test case was executed multiple times, there will be multiple
          # |test| objects of it. Each |test| corresponds to an execution of the
          # test case.
          test_status_value = test['testStatus']['_value']
          if test_status_value == 'Success':
            result.add_test_result(
                TestResult(test_name, TestStatus.PASS, duration=duration))
          elif test_status_value == 'Expected Failure':
            result.add_test_result(
                TestResult(
                    test_name,
                    TestStatus.FAIL,
                    expected_status=TestStatus.FAIL,
                    duration=duration))
          elif test_status_value == 'Skipped':
            result.add_test_result(
                TestResult(
                    test_name,
                    TestStatus.SKIP,
                    expected_status=TestStatus.SKIP,
                    duration=duration))
          else:
            result.add_test_result(
                XcodeLogParser._create_failed_test_result(
                    test_name, duration, xcresult, test, output_path))
    return result

  @staticmethod
  def _create_failed_test_result(test_name, duration, xcresult, test,
                                 output_path):
    test_result = TestResult(
        test_name,
        TestStatus.FAIL,
        duration=duration,
        test_log='Logs from "failureSummaries" in .xcresult:\n')
    # Parse data for failed test by its id. See SINGLE_TEST_SUMMARY_REF
    # in xcode_log_parser_test.py for an example of |summary_ref|.
    summary_ref = json.loads(
        XcodeLogParser._xcresulttool_get(xcresult,
                                         test['summaryRef']['id']['_value']))
    # On rare occasions rootFailure doesn't have 'failureSummaries'.
    for failure in summary_ref.get('failureSummaries', {}).get('_values', []):
      file_name = _sanitize_str(failure.get('fileName', {}).get('_value', ''))
      line_number = _sanitize_str(
          failure.get('lineNumber', {}).get('_value', ''))
      test_result.test_log += f'file: {file_name}, line: {line_number}\n'

      if CRASH_REGEX.search(failure['message']['_value']):
        test_result.test_log += XcodeLogParser._get_app_side_failure(
            test_result, output_path)
      else:
        test_result.test_log += _sanitize_str(
            failure['message']['_value']) + '\n'

    attachments = XcodeLogParser._extract_artifacts_for_test(
        test_name, summary_ref, xcresult)
    test_result.attachments.update(attachments)

    return test_result

  @staticmethod
  def collect_test_results(output_path, output, xcode_parallel_enabled=False):
    """Gets XCTest results, diagnostic data & artifacts from xcresult.

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
      output: [str] An output of test run.
      xcode_parallel_enabled: whether xcode parrallelization is enabled on
          the test run, which might cause test result format to vary slightly.
          False by default.

    Returns:
      test_result.ResultCollection: Test results.
    """
    output_path = _sanitize_str(output_path)
    output = _sanitize_str_list(output)
    LOGGER.info('Reading %s' % output_path)
    overall_collected_result = ResultCollection()

    # Xcodebuild writes staging data to |output_path| folder during test
    # execution. If |output_path| doesn't exist, it means tests didn't start at
    # all.
    if not os.path.exists(output_path):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = (
          '%s with staging data does not exist.\n' % output_path +
          '\n'.join(output))
      return overall_collected_result

    # During a run `xcodebuild .. -resultBundlePath %output_path%`
    # that generates output_path folder,
    # but Xcode 11+ generates `output_path.xcresult` and `output_path`
    # where output_path.xcresult is a folder with results and `output_path`
    # is symlink to the `output_path.xcresult` folder.
    # `xcresulttool` with folder/symlink behaves in different way on laptop and
    # on bots. This piece of code uses .xcresult folder.
    xcresult = output_path + _XCRESULT_SUFFIX

    # |output_path|.xcresult folder is created at the end of tests. If
    # |output_path| folder exists but |output_path|.xcresult folder doesn't
    # exist, it means xcodebuild exited or was killed half way during tests.
    if not os.path.exists(xcresult):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = (
          '%s with test results does not exist.\n' % xcresult +
          '\n'.join(output))
      overall_collected_result.add_result_collection(
          parse_passed_failed_tests_for_interrupted_run(output))
      return overall_collected_result

    # See XCRESULT_ROOT in xcode_log_parser_test.py for an example of |root|.
    root = json.loads(XcodeLogParser._xcresulttool_get(xcresult))

    XcodeLogParser.export_diagnostic_data(output_path)

    if (test_crashed(root)):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = '0 tests executed!'
    else:
      overall_collected_result.add_result_collection(
          XcodeLogParser._get_test_statuses(output_path,
                                            xcode_parallel_enabled))
      # For some crashed tests info about error contained only in root node.
      overall_collected_result.add_result_collection(
          XcodeLogParser._list_of_failed_tests(
              root, excluded=overall_collected_result.all_test_names()))
    # Remove the symbol link file.
    if os.path.islink(output_path):
      os.unlink(output_path)
    file_util.zip_and_remove_folder(xcresult)
    return overall_collected_result

  @staticmethod
  def copy_artifacts(output_path):
    """Copy screenshots, crash logs of failed tests to output folder.

    Warning: This method contains duplicate logic as |collect_test_results|
    method. Do not use these on the same test output path.

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    if not os.path.exists(xcresult):
      LOGGER.warn('%s does not exist.' % xcresult)
      return

    root = json.loads(XcodeLogParser._xcresulttool_get(xcresult))
    if 'testFailureSummaries' not in root.get('issues', {}):
      LOGGER.info('No failures in %s' % xcresult)
      return

    # See TESTS_REF['summaries']['_values'] in xcode_log_parser_test.py.
    test_summaries = json.loads(
        XcodeLogParser._xcresulttool_get(xcresult, 'testsRef')).get(
            'summaries', {}).get('_values', [])

    test_summary_refs = {}

    for summaries in test_summaries:
      for summary in summaries.get('testableSummaries', {}).get('_values', []):
        for all_tests in summary.get('tests', {}).get('_values', []):
          for test_suite in all_tests.get('subtests', {}).get('_values', []):
            for test_case in test_suite.get('subtests', {}).get('_values', []):
              for test in test_case.get('subtests', {}).get('_values', []):
                test_status_value = test['testStatus']['_value']
                if test_status_value not in [
                    'Success', 'Expected Failure', 'Skipped'
                ]:
                  summary_ref = test['summaryRef']['id']['_value']
                  test_summary_refs[test['identifier']['_value']] = summary_ref

    for test, summary_ref_id in test_summary_refs.items():
      # See SINGLE_TEST_SUMMARY_REF in xcode_log_parser_test.py for an example
      # of |test_summary|.
      test_summary = json.loads(
          XcodeLogParser._xcresulttool_get(xcresult, summary_ref_id))
      XcodeLogParser._extract_artifacts_for_test(test, test_summary, xcresult)

  @staticmethod
  def export_diagnostic_data(output_path):
    """Exports diagnostic data from xcresult to xcresult_diagnostic.zip.

    Since Xcode 11 format of result bundles changed, to get diagnostic data
    need to run command below:
    xcresulttool export --type directory --id DIAGNOSTICS_REF --output-path
    ./export_folder --path ./RB.xcresult

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    if not os.path.exists(xcresult):
      LOGGER.warn('%s does not exist.' % xcresult)
      return
    root = json.loads(XcodeLogParser._xcresulttool_get(xcresult))
    try:
      diagnostics_ref = root['actions']['_values'][0]['actionResult'][
          'diagnosticsRef']['id']['_value']
      diagnostic_folder = '%s_diagnostic' % xcresult
      XcodeLogParser._export_data(xcresult, diagnostics_ref, 'directory',
                                    diagnostic_folder)
      # Copy log files out of diagnostic_folder if any. Use |name_count| to
      # generate an index for same name files produced from Xcode parallel
      # testing.
      name_count = {}
      for root, dirs, files in os.walk(diagnostic_folder):
        for filename in files:
          if IPS_REGEX.match(filename):
            # TODO(crbug.com/378086419): Improve IPS crash report logging
            crash_reports_dir = os.path.join(output_path, os.pardir,
                                             'Crash Reports')
            os.makedirs(crash_reports_dir, exist_ok=True)
            output_filepath = os.path.join(crash_reports_dir, filename)
            # crash report files with the same name from previous attempt_#'s
            # will be overwritten
            shutil.copy(os.path.join(root, filename), output_filepath)

          if 'StandardOutputAndStandardError' in filename:
            file_index = name_count.get(filename, 0)
            output_filename = (
                '%s_simulator#%d_%s' %
                (os.path.basename(output_path), file_index, filename))
            output_filepath = os.path.join(output_path, os.pardir,
                                           output_filename)
            shutil.copy(os.path.join(root, filename), output_filepath)
            name_count[filename] = name_count.get(filename, 0) + 1
      file_util.zip_and_remove_folder(diagnostic_folder)
    except KeyError:
      LOGGER.warn('Did not parse diagnosticsRef from %s!' % xcresult)

  @staticmethod
  def _export_data(xcresult, ref_id, output_type, output_path):
    """Exports data from xcresult using xcresulttool.

    Since Xcode 11 format of result bundles changed, to get diagnostic data
    need to run command below:
    xcresulttool export --type directory --id DIAGNOSTICS_REF --output-path
    ./export_folder --path ./RB.xcresult

    Args:
      xcresult: (str) A path to xcresult directory.
      ref_id: (str) A reference id of exporting entity.
      output_type: (str) An export type (can be directory or file).
      output_path: (str) An output location.
    """
    export_command = [
        'xcresulttool', 'export', '--type', output_type, '--id', ref_id,
        '--path', xcresult, '--output-path', output_path
    ]
    if xcode_util.using_xcode_16_or_higher():
      export_command.append('--legacy')
    subprocess.check_output(export_command).decode('utf-8').strip()

  @staticmethod
  def _extract_attachments(test,
                           test_activities,
                           xcresult,
                           attachments,
                           include_jpg=True):
    """Exrtact attachments from xcresult folder for a single test result.

    Copies all attachments under test_activities and nested subactivities (if
    any) to the same directory as xcresult directory. Saves abs paths of
    extracted attachments in |attachments|.

    Filenames are in format `${output}_TestCase_testMethod_${index}`, where
    ${output} is the basename of |xcresult| folder, ${index} is the index of
    attachment for a test case, e.g.:
        attempt_0_TestCase_testMethod_1.jpg
        ....
        attempt_0_TestCase_testMethod_3.crash

    Args:
      test: (str) Test name.
      test_activities: (list) List of test activities (dict) that
          store data about each test step.
      xcresult: (str) A path to test results.
      attachments: (dict) File basename to abs path mapping for extracted
          attachments to be stored in. Its length is also used as part of file
          name to avoid duplicated filename.
      include_jpg: (bool) Whether include jpg or jpeg attachments.
    """
    for activity_summary in test_activities:
      if 'subactivities' in activity_summary:
        XcodeLogParser._extract_attachments(
            test,
            activity_summary.get('subactivities', {}).get('_values', []),
            xcresult, attachments, include_jpg)
      for attachment in activity_summary.get('attachments',
                                             {}).get('_values', []):
        raw_file_name = str(attachment['filename']['_value'])
        if 'payloadRef' not in attachment:
          LOGGER.warning(
              'Unable to export attachment %s because payloadRef is undefined' %
              raw_file_name)
          continue
        payload_ref = attachment['payloadRef']['id']['_value']
        _, file_name_extension = os.path.splitext(raw_file_name)

        if not include_jpg and file_name_extension in ['.jpg', '.jpeg']:
          continue

        attachment_filename = (
            '%s_%s_%s' %
            (os.path.splitext(os.path.basename(xcresult))[0],
             test.replace('/', '_'), raw_file_name))
        # Extracts attachment to the same folder containing xcresult.
        attachment_output_path = os.path.abspath(
            os.path.join(xcresult, os.pardir, attachment_filename))
        XcodeLogParser._export_data(xcresult, payload_ref, 'file',
                                      attachment_output_path)
        attachments[attachment_filename] = attachment_output_path

  @staticmethod
  def _extract_artifacts_for_test(test, summary_ref, xcresult):
    """Extracts artifacts for a test case result.

    Args:
      test: (str) Test name.
      summary_ref: (dict) Summary ref field of a test result parsed by
          xcresulttool . See SINGLE_TEST_SUMMARY_REF in xcode_log_parser_test.py
          for an example.
      xcresult: (str) A path to test results.

    Returns:
      (dict) File basename to abs path mapping for extracted attachments.
    """
    attachments = {}
    # Extract all attachments except for screenshots from each step of the
    # test.
    XcodeLogParser._extract_attachments(
        test,
        summary_ref.get('activitySummaries', {}).get('_values', []),
        xcresult,
        attachments,
        include_jpg=False)
    # Extract all attachments of the failure step (applied to failed tests).
    XcodeLogParser._extract_attachments(
        test,
        summary_ref.get('failureSummaries', {}).get('_values', []),
        xcresult,
        attachments,
        include_jpg=True)
    return attachments


class Xcode16LogParser(object):
  """Xcode log parser. Parse Xcode16+ test results."""

  @staticmethod
  def _xcresulttool_get_summary(xcresult_path):
    """Runs `xcresulttool get test-results summary` and returns JSON output.

    Args:
      xcresult_path: A full path to xcresult folder that must have Info.plist.

    Returns:
      A test report summary in JSON format.
    """
    xcode_info = test_runner.get_current_xcode_info()
    folder = os.path.join(xcode_info['path'], 'usr', 'bin')
    # By default xcresulttool is %Xcode%/usr/bin,
    # that is not in directories from $PATH
    # Need to check whether %Xcode%/usr/bin is in a $PATH
    # and then call xcresulttool
    if folder not in os.environ['PATH']:
      os.environ['PATH'] += ':%s' % folder

    xcresult_command = [
        'xcresulttool', 'get', 'test-results', 'summary', '--format', 'json',
        '--path', xcresult_path
    ]
    return subprocess.check_output(xcresult_command).decode('utf-8').strip()

  @staticmethod
  def _xcresulttool_get_tests(xcresult_path):
    """Runs `xcresulttool get test-results tests` and returns JSON output.

    Args:
      xcresult_path: A full path to xcresult folder that must have Info.plist.

    Returns:
      All tests that were executed from test report.
    """
    xcode_info = test_runner.get_current_xcode_info()
    folder = os.path.join(xcode_info['path'], 'usr', 'bin')
    # By default xcresulttool is %Xcode%/usr/bin,
    # that is not in directories from $PATH
    # Need to check whether %Xcode%/usr/bin is in a $PATH
    # and then call xcresulttool
    if folder not in os.environ['PATH']:
      os.environ['PATH'] += ':%s' % folder

    xcresult_command = [
        'xcresulttool', 'get', 'test-results', 'tests', '--format', 'json',
        '--path', xcresult_path
    ]
    return subprocess.check_output(xcresult_command).decode('utf-8').strip()

  @staticmethod
  def _get_test_statuses(output_path):
    """Returns test results from xcresult.

    Also extracts and stores attachments for failed tests

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.

    Returns:
      test_result.ResultCollection: Test results.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    result = ResultCollection()
    root = json.loads(Xcode16LogParser._xcresulttool_get_tests(xcresult))
    # testNodes -> Test Plan -> Test Module -> Test Suites
    for test_suite in root['testNodes'][0]['children'][0]['children']:
      if test_suite['nodeType'] != 'Test Suite':
        # Unsure if there are other node types, but just to be safe
        continue
      for test in test_suite['children']:
        if test['nodeType'] != 'Test Case':
          # Unsure if there are other node types, but just to be safe
          continue
        test_name = test['nodeIdentifier']
        # crashed tests don't have duration in the test results
        duration = None
        if 'duration' in test:
          duration = duration_to_milliseconds(test['duration'])
        if any(
            test_name.endswith(suffix)
            for suffix in SYSTEM_ERROR_TEST_NAME_SUFFIXES):
          result.crashed = True
          result.crash_message += 'System error in %s: %s\n' % (xcresult,
                                                                test_name)
          continue
        # If a test case was executed multiple times, there will be multiple
        # |test| objects of it. Each |test| corresponds to an execution of the
        # test case.
        test_status_value = test['result']
        if test_status_value == 'Passed':
          result.add_test_result(
              TestResult(test_name, TestStatus.PASS, duration=duration))
        elif test_status_value == 'Expected Failure':
          result.add_test_result(
              TestResult(
                  test_name,
                  TestStatus.FAIL,
                  expected_status=TestStatus.FAIL,
                  duration=duration))
        elif test_status_value == 'Skipped':
          result.add_test_result(
              TestResult(
                  test_name,
                  TestStatus.SKIP,
                  expected_status=TestStatus.SKIP,
                  duration=duration))
        else:
          result.add_test_result(
              Xcode16LogParser._create_failed_test_result(
                  test_name, duration, test, output_path, xcresult))
    return result

  def _create_failed_test_result(test_name, duration, test, output_path,
                                 xcresult):
    test_result = TestResult(
        test_name,
        TestStatus.FAIL,
        duration=duration,
        test_log='Logs from "Failure Message" in .xcresult:\n')

    for failure in test['children']:
      if failure['nodeType'] != 'Failure Message':
        continue

      if CRASH_REGEX.search(failure['name']):
        test_result.test_log += XcodeLogParser._get_app_side_failure(
            test_result, output_path)
      else:
        test_result.test_log += failure['name'] + '\n'

    attachments = Xcode16LogParser._extract_artifacts_for_test(
        test_name, xcresult, only_failures=True)
    test_result.attachments.update(attachments)

    return test_result

  @staticmethod
  def collect_test_results(output_path, output):
    """Gets XCTest results, diagnostic data & artifacts from xcresult.

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
      output: [str] An output of test run.

    Returns:
      test_result.ResultCollection: Test results.
    """
    output_path = _sanitize_str(output_path)
    output = _sanitize_str_list(output)
    LOGGER.info('Reading %s' % output_path)
    overall_collected_result = ResultCollection()

    # Xcodebuild writes staging data to |output_path| folder during test
    # execution. If |output_path| doesn't exist, it means tests didn't start at
    # all.
    if not os.path.exists(output_path):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = (
          '%s with staging data does not exist.\n' % output_path +
          '\n'.join(output))
      return overall_collected_result

    xcresult = output_path + _XCRESULT_SUFFIX

    # |output_path|.xcresult folder is created at the end of tests. If
    # |output_path| folder exists but |output_path|.xcresult folder doesn't
    # exist, it means xcodebuild exited or was killed half way during tests.
    if not os.path.exists(xcresult):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = (
          '%s with test results does not exist.\n' % xcresult +
          '\n'.join(output))
      overall_collected_result.add_result_collection(
          parse_passed_failed_tests_for_interrupted_run(output))
      return overall_collected_result

    summary = json.loads(Xcode16LogParser._xcresulttool_get_summary(xcresult))

    Xcode16LogParser.export_diagnostic_data(output_path)

    if xcode16_test_crashed(summary):
      overall_collected_result.crashed = True
      overall_collected_result.crash_message = '0 tests executed!'
    else:
      overall_collected_result.add_result_collection(
          Xcode16LogParser._get_test_statuses(output_path))
    # Remove the symbol link file.
    if os.path.islink(output_path):
      os.unlink(output_path)
    file_util.zip_and_remove_folder(xcresult)
    return overall_collected_result

  @staticmethod
  def copy_artifacts(output_path):
    """Copy screenshots, crash logs of failed tests to output folder.

    Warning: This method contains duplicate logic as |collect_test_results|
    method. Do not use these on the same test output path.

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    if not os.path.exists(xcresult):
      LOGGER.warn('%s does not exist.' % xcresult)
      return

    root = json.loads(Xcode16LogParser._xcresulttool_get_tests(xcresult))
    for test_suite in root['testNodes'][0]['children'][0]['children']:
      if test_suite['nodeType'] != 'Test Suite':
        # Unsure if there are other node types, but just to be safe
        continue
      for test in test_suite['children']:
        if test['nodeType'] != 'Test Case':
          # Unsure if there are other node types, but just to be safe
          continue
        test_name = test['nodeIdentifier']
        Xcode16LogParser._extract_artifacts_for_test(test_name, xcresult)

  @staticmethod
  def export_diagnostic_data(output_path):
    """Exports diagnostic data from xcresult to xcresult_diagnostic.zip.

    Args:
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.
    """
    xcresult = output_path + _XCRESULT_SUFFIX
    if not os.path.exists(xcresult):
      LOGGER.warn('%s does not exist.' % xcresult)
      return
    diagnostic_folder = '%s_diagnostic' % xcresult
    try:
      export_command = [
          'xcresulttool', 'export', 'diagnostics', '--path', xcresult,
          '--output-path', diagnostic_folder
      ]
      subprocess.check_output(export_command).decode('utf-8').strip()
      # Copy log files out of diagnostic_folder if any. Use |name_count| to
      # generate an index for same name files produced from Xcode parallel
      # testing.
      name_count = {}
      for root, dirs, files in os.walk(diagnostic_folder):
        for filename in files:
          if IPS_REGEX.match(filename):
            # TODO(crbug.com/378086419): Improve IPS crash report logging
            crash_reports_dir = os.path.join(output_path, os.pardir,
                                             'Crash Reports')
            os.makedirs(crash_reports_dir, exist_ok=True)
            output_filepath = os.path.join(crash_reports_dir, filename)
            # crash report files with the same name from previous attempt_#'s
            # will be overwritten
            shutil.copy(os.path.join(root, filename), output_filepath)

          if 'StandardOutputAndStandardError' in filename:
            file_index = name_count.get(filename, 0)
            output_filename = (
                '%s_simulator#%d_%s' %
                (os.path.basename(output_path), file_index, filename))
            output_filepath = os.path.join(output_path, os.pardir,
                                           output_filename)
            shutil.copy(os.path.join(root, filename), output_filepath)
            name_count[filename] = name_count.get(filename, 0) + 1
      file_util.zip_and_remove_folder(diagnostic_folder)
    except KeyError:
      LOGGER.warn('Did not parse diagnosticsRef from %s!' % xcresult)

  @staticmethod
  def _extract_attachments(test, xcresult, attachments, only_failures=False):
    """Exrtact attachments from xcresult folder for a single test result.

    The attachments will be stored in a folder in the format of
      `${output}_attachments`,
      where ${output} is the basename of |xcresult| folder e.g.:
      attempt_0_attachments/

    Args:
      test: (str) Test name.
      xcresult: (str) A path to test results.
      attachments: (dict) File basename to abs path mapping for extracted
          attachments to be stored in. Its length is also used as part of file
          name to avoid duplicated filename.
    """
    attachment_foldername = ('%s_attachments' %
                             (os.path.splitext(os.path.basename(xcresult))[0]))
    # Extracts attachment to the same folder containing xcresult.
    attachment_output_path = os.path.abspath(
        os.path.join(xcresult, os.pardir, attachment_foldername, test))
    os.makedirs(attachment_output_path)
    export_command = [
        'xcresulttool', 'export', 'attachments', '--test-id', test, '--path',
        xcresult, '--output-path', attachment_output_path
    ]
    subprocess.check_output(export_command)

    manifest_file = os.path.join(attachment_output_path, 'manifest.json')
    if not os.path.exists(manifest_file):
      return
    with open(manifest_file, 'r') as f:
      data = json.load(f)
      if not data:
        return
      for attachment in data[0]['attachments']:
        is_mp4 = attachment['exportedFileName'].endswith('.mp4')
        if only_failures and not attachment[
            'isAssociatedWithFailure'] and not is_mp4:
          # Skip attachments not associated with failures, except for video
          # recording
          continue
        suggested_name = attachment['suggestedHumanReadableName']
        exported_file = attachment['exportedFileName']
        attachments[suggested_name] = os.path.join(attachment_output_path,
                                                   exported_file)

  @staticmethod
  def _extract_artifacts_for_test(test, xcresult, only_failures=False):
    """Extracts artifacts for a test case result.

    Args:
      test: (str) Test name.
      xcresult: (str) A path to test results.

    Returns:
      (dict) File basename to abs path mapping for extracted attachments.
    """
    attachments = {}
    Xcode16LogParser._extract_attachments(test, xcresult, attachments,
                                          only_failures)
    return attachments
