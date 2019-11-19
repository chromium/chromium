# Copyright 2019 The Chromium Authors. All rights reserved.
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

import test_runner


LOGGER = logging.getLogger(__name__)


def parse_passed_tests_for_interrupted_run(output):
  """Parses xcode runner output to get passed tests only.

  Args:
    output: [str] An output of test run.

  Returns:
    The list of passed tests only that will be a filter for next attempt.
  """
  passed_tests = []
  # Test has format:
  # [09:04:42:INFO] Test case '-[Test_class test_method]' passed.
  # [09:04:42:INFO] Test Case '-[Test_class test_method]' passed.
  passed_test_regex = re.compile(r'Test [Cc]ase \'\-\[(.+?)\s(.+?)\]\' passed')

  for test_line in output:
    m_test = passed_test_regex.search(test_line)
    if m_test:
      passed_tests.append('%s/%s' % (m_test.group(1), m_test.group(2)))
  LOGGER.info('%d passed tests for interrupted build.' % len(passed_tests))
  return passed_tests


def format_test_case(test_case):
  """Format test case from `-[TestClass TestMethod]` to `TestClass_TestMethod`.

  Args:
    test_case: (str) Test case id in format `-[TestClass TestMethod]` or
               `[TestClass/TestMethod]`

  Returns:
    Test case id in format TestClass_TestMethod.
  """
  return test_case.replace('[', '').replace(']', '').replace(
      '-', '').replace(' ', '/')


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


class Xcode11LogParser(object):
  """Xcode 11 log parser. Parse Xcode result types v3."""

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
      data = json.loads(Xcode11LogParser._xcresulttool_get(xcresult_path))
      # Redefine ref_id to get only the reference data
      ref_id = data['actions']['_values'][0]['actionResult'][
          ref_id]['id']['_value']
    # If no ref_id then xcresulttool will use default(root) id.
    id_params = ['--id', ref_id] if ref_id else []
    xcresult_command = ['xcresulttool', 'get', '--format', 'json',
                        '--path', xcresult_path] + id_params
    return subprocess.check_output(xcresult_command).strip()

  @staticmethod
  def _list_of_failed_tests(actions_invocation_record):
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

    Returns:
      Failed tests as a map:
      {
          'failed_test': ['StackTrace']
      }
    """
    failed = {}
    if 'testFailureSummaries' not in actions_invocation_record['issues']:
      return failed
    for failure_summary in actions_invocation_record['issues'][
        'testFailureSummaries']['_values']:
      error_line = failure_summary['documentLocationInCreatingWorkspace'][
          'url']['_value']
      fail_message = [error_line] + failure_summary['message'][
          '_value'].splitlines()
      test_case_id = format_test_case(failure_summary['testCaseName']['_value'])
      failed[test_case_id] = fail_message
    return failed

  @staticmethod
  def _list_of_passed_tests(xcresult):
    """Gets list of passed tests from xcresult.

    Args:
      xcresult: (str) A path to xcresult.

    Returns:
      A list of passed tests.
    """
    root = json.loads(Xcode11LogParser._xcresulttool_get(xcresult, 'testsRef'))
    passed_tests = []
    for summary in root['summaries']['_values'][0][
        'testableSummaries']['_values']:
      if not summary['tests']:
        continue
      for test_suite in summary['tests']['_values'][0]['subtests'][
          '_values'][0]['subtests']['_values']:
        if 'subtests' not in test_suite:
          # Sometimes(if crash occurs) `subtests` node does not upload.
          # It happens only for failed tests that and a list of failures
          # can be parsed from root.
          continue
        for test in test_suite['subtests']['_values']:
          if test['testStatus']['_value'] == 'Success':
            passed_tests.append(test['identifier']['_value'])
    return passed_tests

  @staticmethod
  def collect_test_results(xcresult, output):
    """Gets test result and diagnostic data from xcresult.

    Args:
      xcresult: (str) A path to xcresult.
      output: [str] An output of test run.

    Returns:
      Test result as a map:
        {
          'passed': [passed_tests],
          'failed': {
              'failed_test': ['StackTrace']
          }
        }
    """
    LOGGER.info('Reading %s' % xcresult)
    test_results = {
        'passed': [],
        'failed': {}
    }
    if not os.path.exists(xcresult):
      test_results['failed']['TESTS_DID_NOT_START'] = [
          '%s with test results does not exist.' % xcresult]
      return test_results

    plist_path = os.path.join(xcresult + '.xcresult', 'Info.plist')
    if not os.path.exists(plist_path):
      test_results['failed']['BUILD_INTERRUPTED'] = [
          '%s with test results does not exist.' % plist_path] + output
      test_results['passed'] = parse_passed_tests_for_interrupted_run(output)
      return test_results

    root = json.loads(Xcode11LogParser._xcresulttool_get(xcresult))
    metrics = root['metrics']
    # In case of test crash both numbers of run and failed tests are equal to 0.
    if (metrics.get('testsCount', {}).get('_value', 0) == 0 and
        metrics.get('testsFailedCount', {}).get('_value', 0) == 0):
      test_results['failed']['TESTS_DID_NOT_START'] = ['0 tests executed!']
    else:
      test_results['failed'] = Xcode11LogParser._list_of_failed_tests(root)
      test_results['passed'] = Xcode11LogParser._list_of_passed_tests(xcresult)
    Xcode11LogParser._export_diagnostic_data(xcresult + '.xcresult')
    return test_results

  @staticmethod
  def copy_screenshots(output_folder):
    """Copy screenshots of failed tests to output folder.

    Args:
      output_folder: (str) A full path to folder where
    """
    plist_path = os.path.join(output_folder + '.xcresult', 'Info.plist')
    if not os.path.exists(plist_path):
      LOGGER.info('%s does not exist.' % plist_path)
      return

    root = json.loads(Xcode11LogParser._xcresulttool_get(output_folder))
    if 'testFailureSummaries' not in root['issues']:
      LOGGER.info('No failures in %s' % output_folder)
      return

    for failure_summary in root['issues']['testFailureSummaries']['_values']:
      test_case = failure_summary['testCaseName']['_value']
      test_case_folder = os.path.join(output_folder, 'failures',
                                      format_test_case(test_case))
      copy_screenshots_for_failed_test(failure_summary['message']['_value'],
                                       test_case_folder)

  @staticmethod
  def _export_diagnostic_data(xcresult):
    """Exports diagnostic data from xcresult to xcresult_diagnostic folder.

    Since Xcode 11 format of result bundles changed, to get diagnostic data
    need to run command below:
    xcresulttool export --type directory --id DIAGNOSTICS_REF --output-path
    ./export_folder --path ./RB.xcresult

    Args:
      xcresult: (str) A path to xcresult directory.
    """
    plist_path = os.path.join(xcresult, 'Info.plist')
    if not (os.path.exists(xcresult) and os.path.exists(plist_path)):
      return
    root = json.loads(Xcode11LogParser._xcresulttool_get(xcresult))
    try:
      diagnostics_ref = root['actions']['_values'][0]['actionResult'][
          'diagnosticsRef']['id']['_value']
      export_command = ['xcresulttool', 'export',
                        '--type', 'directory',
                        '--id', diagnostics_ref,
                        '--path', xcresult,
                        '--output-path', '%s_diagnostic' % xcresult]
      subprocess.check_output(export_command).strip()
    except KeyError:
      LOGGER.warn('Did not parse diagnosticsRef from %s!' % xcresult)


class XcodeLogParser(object):
  """Xcode log parser. Parses logs for Xcode until version 11."""

  @staticmethod
  def _test_status_summary(summary_plist):
    """Gets status summary from TestSummaries.plist.

    Args:
      summary_plist: (str) A path to plist-file.

    Returns:
      A dict that contains all passed and failed tests from the egtests.app.
      e.g.
      {
          'passed': [passed_tests],
          'failed': {
              'failed_test': ['StackTrace']
          }
      }
    """
    root_summary = plistlib.readPlist(summary_plist)
    status_summary = {
        'passed': [],
        'failed': {}
    }
    for summary in root_summary['TestableSummaries']:
      failed_egtests = {}  # Contains test identifier and message
      passed_egtests = []
      if not summary['Tests']:
        continue
      for test_suite in summary['Tests'][0]['Subtests'][0]['Subtests']:
        for test in test_suite['Subtests']:
          if test['TestStatus'] == 'Success':
            passed_egtests.append(test['TestIdentifier'])
          else:
            message = []
            for failure_summary in test['FailureSummaries']:
              failure_message = failure_summary['FileName']
              if failure_summary['LineNumber']:
                failure_message = '%s: line %s' % (
                    failure_message, failure_summary['LineNumber'])
              message.append(failure_message)
              message.extend(failure_summary['Message'].splitlines())
            failed_egtests[test['TestIdentifier']] = message
      if failed_egtests:
        status_summary['failed'] = failed_egtests
      if passed_egtests:
        status_summary['passed'] = passed_egtests
    return status_summary

  @staticmethod
  def collect_test_results(output_folder, output):
    """Gets test result data from Info.plist.

    Args:
      output_folder: (str) A path to output folder.
      output: [str] An output of test run.
    Returns:
      Test result as a map:
        {
          'passed': [passed_tests],
          'failed': {
              'failed_test': ['StackTrace']
          }
      }
    """
    test_results = {
        'passed': [],
        'failed': {}
    }
    plist_path = os.path.join(output_folder, 'Info.plist')
    if not os.path.exists(plist_path):
      test_results['failed']['BUILD_INTERRUPTED'] = [
          '%s with test results does not exist.' % plist_path] + output
      test_results['passed'] = parse_passed_tests_for_interrupted_run(output)
      return test_results

    root = plistlib.readPlist(plist_path)

    for action in root['Actions']:
      action_result = action['ActionResult']
      if ((root['TestsCount'] == 0 and
           root['TestsFailedCount'] == 0)
          or 'TestSummaryPath' not in action_result):
        test_results['failed']['TESTS_DID_NOT_START'] = []
        if ('ErrorSummaries' in action_result
            and action_result['ErrorSummaries']):
          test_results['failed']['TESTS_DID_NOT_START'].append('\n'.join(
              error_summary['Message']
              for error_summary in action_result['ErrorSummaries']))
      else:
        summary_plist = os.path.join(os.path.dirname(plist_path),
                                     action_result['TestSummaryPath'])
        summary = XcodeLogParser._test_status_summary(summary_plist)
        test_results['failed'] = summary['failed']
        test_results['passed'] = summary['passed']
    return test_results

  @staticmethod
  def copy_screenshots(output_folder):
    """Copy screenshots of failed tests to output folder.

    Args:
      output_folder: (str) A full path to folder where
    """
    info_plist_path = os.path.join(output_folder, 'Info.plist')
    if not os.path.exists(info_plist_path):
      LOGGER.info('%s does not exist.' % info_plist_path)
      return

    plist = plistlib.readPlist(info_plist_path)
    if 'TestFailureSummaries' not in plist or not plist['TestFailureSummaries']:
      LOGGER.info('No failures in %s' % info_plist_path)
      return

    for failure_summary in plist['TestFailureSummaries']:
      # Screenshot folder has format 'TestClass_test_method'
      test_case_id = format_test_case(failure_summary['TestCase'])
      test_case_folder = os.path.join(output_folder, 'failures', test_case_id)
      copy_screenshots_for_failed_test(failure_summary['Message'],
                                       test_case_folder)
