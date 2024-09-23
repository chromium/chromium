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
      actionResultMetrics.get('testsFailedCount', {}).get('_value', 0) == 0)
  # After certain types of test failures action results metrics might be missing
  # but root metrics may still be present, indicating that some tests still
  # ran successfully and the entire test suite should not be considered crashed
  rootMetricsMissing = (
      root.get('metrics', {}).get('testsCount', {}).get('_value', 0) == 0 and
      root.get('metrics', {}).get('testsFailedCount', {}).get('_value', 0) == 0)
  # if both metrics are missing then consider the test app to have crashed
  return actionResultMetricsMissing and rootMetricsMissing


def get_test_suites(summary, xcode_parallel_enabled):
  # On Xcode16+, enabling test parallelization will cause test result format
  # to vary slightly
  if xcode_parallel_enabled and xcode_util.using_xcode_16_or_higher():
    return summary['tests']['_values']
  return summary['tests']['_values'][0]['subtests']['_values'][0]['subtests'][
      '_values']


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
  def _get_app_side_failure(test_name, output_path):
    """Parses and returns app side failure reason in the event that a test
    causes the app to crash.

    Args:
      test_name: (str) The name of the test that crashed. In the format
          [TestCase/TestMethod]
      output_path: (str) An output path passed in --resultBundlePath when
          running xcodebuild.

    Returns:
      (str) Formatted app side failure message or a message saying failure
        reason is missing
    """
    attempt_num = output_path.split('/')[-1]
    app_side_failure_message = ''
    parent_output_dir = os.path.join(output_path, os.pardir)
    files = os.listdir(parent_output_dir)
    log_file_name = ''

    for file in files:
      # the '-' is important since it distinguishes the app side log file from
      # the test app log file
      if ('StandardOutputAndStandardError-' in file and
          file.startswith(attempt_num)):
        with open(os.path.join(parent_output_dir, file), 'r') as f:
          fmt_test_name = test_name.replace('/', ' ')
          lines = f.readlines()

          for line in lines:
            if 'Starting test: -[%s]' % fmt_test_name in line:
              app_side_failure_message += line
            elif app_side_failure_message:
              # end at start of next test or when app restarts
              if ('Starting test' in line or
                  'Standard output and standard error from' in line):
                # test name is only expected to appear in a single log file
                # so it's safe to return early
                log_file_name = file
                break
              else:
                app_side_failure_message += line

    if not app_side_failure_message:
      failure_reason_missing = 'App side failure reason not found for ' + \
        'crashed test: %s. For complete logs see CAS outputs, which can be ' + \
        'found in the swarming task of the shard this test ran on.\n'
      return failure_reason_missing % test_name

    # omit layout constraint warnings since they can clutter logs and make the
    # actual reason why the app crashed difficult to find
    layout_constraint_warning_pattern = \
      r'Unable to simultaneously satisfy constraints.(.*?)may also be helpful'
    app_side_failure_message = re.sub(
        layout_constraint_warning_pattern,
        constants.LAYOUT_CONSTRAINT_MSG,
        app_side_failure_message,
        flags=re.DOTALL)

    app_crashed_message = '%s\nShowing logs from application under test. ' + \
      'For complete logs see %s in CAS outputs, which can be found in the ' + \
      'swarming task of the shard this test ran on.\n\n%s\n'
    return app_crashed_message % (constants.CRASH_MESSAGE, log_file_name,
                                  app_side_failure_message)

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
            # Parse data for failed test by its id. See SINGLE_TEST_SUMMARY_REF
            # in xcode_log_parser_test.py for an example of |summary_ref|.
            summary_ref = json.loads(
                XcodeLogParser._xcresulttool_get(
                    xcresult, test['summaryRef']['id']['_value']))

            failure_message = 'Logs from "failureSummaries" in .xcresult:\n'
            # On rare occasions rootFailure doesn't have 'failureSummaries'.
            for failure in summary_ref.get('failureSummaries',
                                           {}).get('_values', []):
              file_name = _sanitize_str(
                  failure.get('fileName', {}).get('_value', ''))
              line_number = _sanitize_str(
                  failure.get('lineNumber', {}).get('_value', ''))
              failure_location = 'file: %s, line: %s' % (file_name, line_number)
              failure_message += failure_location + '\n'

              if (constants.CRASH_MESSAGE in failure['message']['_value']):
                failure_message += \
                  XcodeLogParser._get_app_side_failure(
                    test_name, output_path)
              else:
                failure_message += _sanitize_str(
                    failure['message']['_value']) + '\n'

            attachments = XcodeLogParser._extract_artifacts_for_test(
                test_name, summary_ref, xcresult)

            result.add_test_result(
                TestResult(
                    test_name,
                    TestStatus.FAIL,
                    duration=duration,
                    test_log=failure_message,
                    attachments=attachments))
    return result

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
