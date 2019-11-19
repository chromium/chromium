# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcode_log_parser.py."""

import json
import mock
import os

import test_runner
import test_runner_test
import xcode_log_parser


_XTEST_RESULT = '/tmp/temp_file.xcresult'
XCODE11_DICT = {
    'path': '/Users/user1/Xcode.app',
    'version': '11.0',
    'build': '11M336w',
}
REF_ID = """
  {
    "actions": {
      "_values": [{
        "actionResult": {
          "testsRef": {
            "id": {
              "_value": "REF_ID"
            }
          }
        }
      }]
    }
  }"""

ACTIONS_RECORD_FAILED_TEST = """
  {
    "issues": {
      "testFailureSummaries": {
        "_values": [{
          "documentLocationInCreatingWorkspace": {
            "url": {
              "_value": "file://<unknown>#CharacterRangeLen=0"
            }
          },
          "message": {
            "_value": "Fail. Screenshots: {\\n\\"Failure\\": \\"path.png\\"\\n}"
          },
          "testCaseName": {
            "_value": "-[WebUITestCase testBackForwardFromWebURL]"
          }
        }]
      }
    }
  }"""

PASSED_TESTS = """
  {
    "summaries": {
      "_values": [{
        "testableSummaries": {
          "_type": {
            "_name": "Array"
          },
          "_values": [{
            "tests": {
              "_type": {
                "_name": "Array"
              },
              "_values": [{
                "subtests": {
                  "_values": [{
                    "subtests": {
                      "_values": [{
                        "subtests": {
                          "_values": [{
                            "testStatus": {
                              "_value": "Success"
                            },
                            "identifier": {
                              "_value": "TestCase1/testMethod1"
                            },
                            "name": {
                              "_value": "testMethod1"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Failure"
                            },
                            "identifier": {
                              "_value": "TestCase1/testFailed1"
                            },
                            "name": {
                              "_value": "testFailed1"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Success"
                            },
                            "identifier": {
                              "_value": "TestCase2/testMethod1"
                            },
                            "name": {
                              "_value": "testMethod1"
                            }
                          }]
                        }
                      }]
                    }
                  }]
                }
              }]
            }
          }]
        }
      }]
    }
  }
"""


class XCode11LogParserTest(test_runner_test.TestCase):
  """Test case to test Xcode11LogParser."""

  def setUp(self):
    super(XCode11LogParserTest, self).setUp()
    self.mock(test_runner, 'get_current_xcode_info', lambda: XCODE11_DICT)

  @mock.patch('subprocess.check_output', autospec=True)
  def testXcresulttoolGetRoot(self, mock_process):
    mock_process.return_value = '%JSON%'
    xcode_log_parser.Xcode11LogParser()._xcresulttool_get('xcresult_path')
    self.assertTrue(
        os.path.join(XCODE11_DICT['path'], 'usr', 'bin') in os.environ['PATH'])
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])

  @mock.patch('subprocess.check_output', autospec=True)
  def testXcresulttoolGetRef(self, mock_process):
    mock_process.side_effect = [REF_ID, 'JSON']
    xcode_log_parser.Xcode11LogParser()._xcresulttool_get('xcresult_path',
                                                          'testsRef')
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])
    self.assertEqual([
        'xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path',
        '--id', 'REF_ID'], mock_process.mock_calls[1][1][0])

  def testXcresulttoolListFailedTests(self):
    failure_message = [
        'file://<unknown>#CharacterRangeLen=0'
    ] + 'Fail. Screenshots: {\n\"Failure\": \"path.png\"\n}'.splitlines()
    expected = {
        'WebUITestCase/testBackForwardFromWebURL': failure_message
    }
    self.assertEqual(expected,
                     xcode_log_parser.Xcode11LogParser()._list_of_failed_tests(
                         json.loads(ACTIONS_RECORD_FAILED_TEST)))

  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testXcresulttoolListPassedTests(self, mock_xcresult):
    mock_xcresult.return_value = PASSED_TESTS
    expected = ['TestCase1/testMethod1', 'TestCase2/testMethod1']
    self.assertEqual(expected,
                     xcode_log_parser.Xcode11LogParser()._list_of_passed_tests(
                         _XTEST_RESULT))

  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  @mock.patch('xcode_log_parser.Xcode11LogParser._list_of_failed_tests')
  @mock.patch('xcode_log_parser.Xcode11LogParser._list_of_passed_tests')
  def testCollectTestTesults(self, mock_get_passed_tests, mock_get_failed_tests,
                             mock_root, mock_exist_file):
    metrics_json = """
    {
      "metrics": {
        "testsCount": {
          "_value": "7"
        },
        "testsFailedCount": {
          "_value": "14"
        }
      }
    }"""
    expected_test_results = {
        'passed': [
            'TestCase1/testMethod1', 'TestCase2/testMethod1'],
        'failed': {
            'WebUITestCase/testBackForwardFromWebURL': [
                'file://<unknown>#CharacterRangeLen=0',
                'Test crashed in <external symbol>'
            ]
        }
    }
    mock_get_passed_tests.return_value = expected_test_results['passed']
    mock_get_failed_tests.return_value = expected_test_results['failed']
    mock_root.return_value = metrics_json
    mock_exist_file.return_value = True
    self.assertEqual(expected_test_results,
                     xcode_log_parser.Xcode11LogParser().collect_test_results(
                         _XTEST_RESULT, []))

  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  def testCollectTestsRanZeroTests(self, mock_root, mock_exist_file):
    metrics_json = '{"metrics": {}}'
    expected_test_results = {
        'passed': [],
        'failed': {'TESTS_DID_NOT_START': ['0 tests executed!']}}
    mock_root.return_value = metrics_json
    mock_exist_file.return_value = True
    self.assertEqual(expected_test_results,
                     xcode_log_parser.Xcode11LogParser().collect_test_results(
                         _XTEST_RESULT, []))

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsDidNotRun(self, mock_exist_file):
    mock_exist_file.return_value = False
    expected_test_results = {
        'passed': [],
        'failed': {'TESTS_DID_NOT_START': [
            '%s with test results does not exist.' % _XTEST_RESULT]}}
    self.assertEqual(expected_test_results,
                     xcode_log_parser.Xcode11LogParser().collect_test_results(
                         _XTEST_RESULT, []))

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsInterruptedRun(self, mock_exist_file):
    mock_exist_file.side_effect = [True, False]
    expected_test_results = {
        'passed': [],
        'failed': {'BUILD_INTERRUPTED': [
            '%s with test results does not exist.' % os.path.join(
                _XTEST_RESULT + '.xcresult', 'Info.plist')]}}
    self.assertEqual(expected_test_results,
                     xcode_log_parser.Xcode11LogParser().collect_test_results(
                         _XTEST_RESULT, []))

  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.Xcode11LogParser._xcresulttool_get')
  @mock.patch('shutil.copyfile', autospec=True)
  def testCopyScreenshots(self, mock_copy, mock_xcresulttool_get,
                          mock_exist_file):
    mock_exist_file.return_value = True
    mock_xcresulttool_get.return_value = ACTIONS_RECORD_FAILED_TEST
    xcode_log_parser.Xcode11LogParser().copy_screenshots(_XTEST_RESULT)
    self.assertEqual(1, mock_copy.call_count)

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestResults_interruptedTests(self, mock_path_exists):
    mock_path_exists.side_effect = [True, False]
    output = [
        '[09:03:42:INFO] Test case \'-[TestCase1 method1]\' passed on device.',
        '[09:06:40:INFO] Test Case \'-[TestCase2 method1]\' passed on device.',
        '[09:09:00:INFO] Test case \'-[TestCase2 method1]\' failed on device.',
        '** BUILD INTERRUPTED **',
    ]
    not_found_message = [
        'Info.plist.xcresult/Info.plist with test results does not exist.']
    res = xcode_log_parser.Xcode11LogParser().collect_test_results(
        'Info.plist', output)
    self.assertIn('BUILD_INTERRUPTED', res['failed'])
    self.assertEqual(not_found_message + output,
                     res['failed']['BUILD_INTERRUPTED'])
    self.assertEqual(['TestCase1/method1', 'TestCase2/method1'],
                     res['passed'])
