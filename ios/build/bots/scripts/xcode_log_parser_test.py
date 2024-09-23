#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcode_log_parser.py."""

import json
import mock
import os
import unittest

from test_result_util import TestStatus
import test_runner
import test_runner_test
import xcode_log_parser
import constants


OUTPUT_PATH = '/tmp/attempt_0'
XCRESULT_PATH = '/tmp/attempt_0.xcresult'
XCODE11_DICT = {
    'path': '/Users/user1/Xcode.app',
    'version': '11.0',
    'build': '11M336w',
}
# A sample of json result when executing xcresulttool on .xcresult dir without
# --id. Some unused keys and values were removed.
XCRESULT_ROOT = """
{
  "_type" : {
    "_name" : "ActionsInvocationRecord"
  },
  "actions" : {
    "_values" : [
      {
        "actionResult" : {
          "_type" : {
            "_name" : "ActionResult"
          },
          "diagnosticsRef" : {
            "id" : {
              "_value" : "DIAGNOSTICS_REF_ID"
            }
          },
          "logRef" : {
            "id" : {
              "_value" : "0~6jr1GkZxoWVzWfcUNA5feff3l7g8fPHJ1rqKetCBa3QXhCGY74PnEuRwzktleMTFounMfCdDpSr1hRfhUGIUEQ=="
            }
          },
          "testsRef" : {
            "id" : {
              "_value" : "0~iRbOkDnmtKVIvHSV2jkeuNcg4RDTUaCLZV7KijyxdCqvhqtp08MKxl0MwjBAPpjmruoI7qNHzBR1RJQAlANNHA=="
            }
          },
          "metrics" : {
            "_type" : {
              "_name" : "ResultMetrics"
            },
            "testsCount" : {
              "_type" : {
                "_name" : "Int"
              },
              "_value" : "2"
            },
            "testsFailedCount" : {
              "_type" : {
                "_name" : "Int"
              },
              "_value" : "2"
            }
          }
        }
      }
    ]
  },
  "issues" : {
    "testFailureSummaries" : {
      "_values" : [
        {
          "documentLocationInCreatingWorkspace" : {
            "url" : {
              "_value" : "file:\/\/\/..\/..\/ios\/web\/shell\/test\/page_state_egtest.mm#CharacterRangeLen=0&EndingLineNumber=130&StartingLineNumber=130"
            }
          },
          "message" : {
            "_value": "Fail. Screenshots: {\\n\\"Failure\\": \\"path.png\\"\\n}"
          },
          "testCaseName" : {
            "_value": "-[PageStateTestCase testZeroContentOffsetAfterLoad]"
          }
        }
      ]
    }
  },
  "metrics" : {
    "testsCount" : {
      "_value" : "2"
    },
    "testsFailedCount" : {
      "_value" : "1"
    }
  }
}"""

XCRESULT_MISSING_ACTIONRESULT_METRICS = b"""
{
  "_type" : {
    "_name" : "ActionsInvocationRecord"
  },
  "actions" : {
    "_values" : [
      {
        "actionResult" : {
          "metrics" : {
            "_type" : {
              "_name" : "ResultMetrics"
            },
            "errorCount" : {
              "_type" : {
                "_name" : "Int"
              },
              "_value" : "1"
            }
          }
        }
      }
    ]
  },
  "metrics" : {
    "errorCount" : {
      "_type" : {
        "_name" : "Int"
      },
      "_value" : "1"
    },
    "testsCount" : {
      "_type" : {
        "_name" : "Int"
      },
      "_value" : "30"
    }
  }
}"""

REF_ID = b"""
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

# A sample of json result when executing xcresulttool on .xcresult dir with
# "testsRef" as --id input. Some unused keys and values were removed.
TESTS_REF = """
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
                "identifier" : {
                  "_value" : "All tests"
                },
                "name" : {
                  "_value" : "All tests"
                },
                "subtests": {
                  "_values": [{
                    "identifier" : {
                      "_value" : "ios_web_shell_eg2tests_module.xctest"
                    },
                    "name" : {
                      "_value" : "ios_web_shell_eg2tests_module.xctest"
                    },
                    "subtests": {
                      "_values": [{
                        "identifier" : {
                          "_value" : "PageStateTestCase"
                        },
                        "name" : {
                          "_value" : "PageStateTestCase"
                        },
                        "subtests": {
                          "_values": [{
                            "testStatus": {
                              "_value": "Success"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "35.38412606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod1"
                            },
                            "name": {
                              "_value": "testMethod1"
                            }
                          },
                          {
                            "summaryRef": {
                              "id": {
                                "_value": "0~7Q_uAuUSJtx9gtHM08psXFm3g_xiTTg5bpdoDO88nMXo_iMwQTXpqlrlMe5AtkYmnZ7Ux5uEgAe83kJBfoIckw=="
                              }
                            },
                            "testStatus": {
                              "_value": "Failure"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase\/testZeroContentOffsetAfterLoad"
                            },
                            "name": {
                              "_value": "testZeroContentOffsetAfterLoad"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Expected Failure"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "28.988606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod2"
                            },
                            "name": {
                              "_value": "testMethod2"
                            }
                          },
                          {
                            "testStatus": {
                              "_value": "Skipped"
                            },
                            "duration" : {
                              "_type" : {
                                 "_name" : "Double"
                              },
                              "_value" : "0.0606716156"
                            },
                            "identifier": {
                              "_value": "PageStateTestCase/testMethod3"
                            },
                            "name": {
                              "_value": "testMethod3"
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

# A sample of json result when executing xcresulttool on .xcresult dir with
# a single test summaryRef id value as --id input. Some unused keys and values
# were removed.
SINGLE_TEST_SUMMARY_REF = """
{
  "_type" : {
    "_name" : "ActionTestSummary",
    "_supertype" : {
      "_name" : "ActionTestSummaryIdentifiableObject",
      "_supertype" : {
        "_name" : "ActionAbstractTestSummary"
      }
    }
  },
  "activitySummaries" : {
    "_values" : [
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "Screenshot_25659115-F3E4-47AE-AA34-551C94333D7E.jpg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_1"
                }
              }
            }
          ]
        },
        "title" : {
          "_value" : "Start Test at 2020-10-19 14:12:58.111"
        }
      },
      {
        "subactivities" : {
          "_values" : [
            {
              "attachments" : {
                "_values" : [
                  {
                    "filename" : {
                      "_value" : "Screenshot_23D95D0E-8B97-4F99-BE3C-A46EDE5999D7.jpg"
                    },
                    "payloadRef" : {
                      "id" : {
                        "_value" : "SCREENSHOT_REF_ID_2"
                      }
                    }
                  }
                ]
              },
              "subactivities" : {
                "_values" : [
                  {
                    "subactivities" : {
                      "_values" : [
                        {
                          "attachments" : {
                            "_values" : [
                              {
                                "filename" : {
                                  "_value" : "Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash"
                                },
                                "payloadRef" : {
                                  "id" : {
                                    "_value" : "CRASH_REF_ID_IN_ACTIVITY_SUMMARIES"
                                  }
                                }
                              }
                            ]
                          },
                          "title" : {
                            "_value" : "Wait for org.chromium.ios-web-shell-eg2tests to idle"
                          }
                        }
                      ]
                    },
                    "title" : {
                      "_value" : "Activate org.chromium.ios-web-shell-eg2tests"
                    }
                  }
                ]
              },
              "title" : {
                "_value" : "Open org.chromium.ios-web-shell-eg2tests"
              }
            }
          ]
        },
        "title" : {
          "_value" : "Set Up"
        }
      },
      {
        "title" : {
          "_value" : "Find the Target Application 'org.chromium.ios-web-shell-eg2tests'"
        }
      },
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "Screenshot_278BA84B-2196-4CCD-9D31-2C07DDDC9DFC.jpg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_3"
                }
              }

            }
          ]
        },
        "title" : {
          "_value" : "Uncaught Exception at page_state_egtest.mm:131: \\nCannot scroll, the..."
        }
      },
      {
        "title" : {
          "_value" : "Uncaught Exception: Immediately halt execution of testcase (EarlGreyInternalTestInterruptException)"
        }
      },
      {
        "title" : {
          "_value" : "Tear Down"
        }
      }
    ]
  },
  "failureSummaries" : {
    "_values" : [
      {
        "attachments" : {
          "_values" : [
            {
              "filename" : {
                "_value" : "kXCTAttachmentLegacyScreenImageData_1_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg"
              },
              "payloadRef" : {
                "id" : {
                  "_value" : "SCREENSHOT_REF_ID_IN_FAILURE_SUMMARIES"
                }
              }
            }
          ]
        },
        "fileName" : {
          "_value" : "\/..\/..\/ios\/web\/shell\/test\/page_state_egtest.mm"
        },
        "lineNumber" : {
          "_value" : "131"
        },
        "message" : {
          "_value" : "Some logs."
        }
      },
      {
        "message" : {
          "_value" : "Immediately halt execution of testcase (EarlGreyInternalTestInterruptException)"
        }
      }
    ]
  },
  "identifier" : {
    "_value" : "PageStateTestCase\/testZeroContentOffsetAfterLoad"
  },
  "name" : {
    "_value" : "testZeroContentOffsetAfterLoad"
  },
  "testStatus" : {
    "_value" : "Failure"
  }
}"""

APP_SIDE_FAILURE_LOG = """Will attempt to recover by breaking constraint
<NSLayoutConstraint:0x1000444e0e0 H:[UIView:0x100031b0fc0]-(4)-[UIView:0x100031b1180]   (active)>

Make a symbolic breakpoint at UIViewAlertForUnsatisfiableConstraints to catch this in the debugger.
The methods in the UIConstraintBasedLayoutDebugging category on UIView listed in <UIKitCore/UIView.h> may also be helpful.
[1009/111922.254778:WARNING:base_earl_grey_test_case_app_interface.mm(21)] *********************************
Starting test: -[SmokeTestCase testOpenTab]
2023-10-09 09:25:00.318076-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] Unable to simultaneously satisfy constraints.
  Probably at least one of the constraints in the following list is one you don't want.
  Try this:
    (1) look at each constraint and try to figure out which you don't expect;
    (2) find the code that added the unwanted constraint or constraints and fix it.
(
    "<NSLayoutConstraint:0x12003d1a6e0 H:|-(0)-[UIView:0x12002478a80]   (active, names: '|':UIView:0x120029d4380 )>",
    "<NSLayoutConstraint:0x12003d1a680 UIView:0x12002478a80.trailing == UIView:0x120029d4380.trailing   (active)>",
    "<NSLayoutConstraint:0x12003d1a440 H:|-(8)-[UIView:0x1200247d080]   (active, names: '|':UIView:0x12002478a80 )>",
    "<NSLayoutConstraint:0x12003d18340 H:[UIView:0x1200247d080]-(4)-[UIView:0x1200247ec80]   (active)>",
    "<NSLayoutConstraint:0x12003d182e0 UIView:0x1200247ec80.trailing == UIView:0x12002478a80.trailing - 8   (active)>",
    "<NSLayoutConstraint:0x12003d42a00 'UIView-Encapsulated-Layout-Width' UIView:0x120029d4380.width == 0   (active)>"
)

Will attempt to recover by breaking constraint
<NSLayoutConstraint:0x12003d18340 H:[UIView:0x1200247d080]-(4)-[UIView:0x1200247ec80]   (active)>

Make a symbolic breakpoint at UIViewAlertForUnsatisfiableConstraints to catch this in the debugger.
The methods in the UIConstraintBasedLayoutDebugging category on UIView listed in <UIKitCore/UIView.h> may also be helpful.
2023-10-09 09:25:01.480402-0700 ios_chrome_eg2tests[56690:4719583] [unspecified] container_create_or_lookup_app_group_path_by_app_group_identifier: client is not entitled
2023-10-09 09:25:02.383910-0700 ios_chrome_eg2tests[56690:4719754] [VoiceShortcutClient] -[VCVoiceShortcutClient unsafeSetupXPCConnection]_block_invoke Client connection to VCVoiceShortcut XPC server interrupted
2023-10-09 09:25:02.385353-0700 ios_chrome_eg2tests[56690:4719754] [Intents] -[INVoiceShortcutCenter getAllVoiceShortcutsWithCompletion:]_block_invoke Error from -getVoiceShortcutsWithCompletion: Error Domain=NSCocoaErrorDomain Code=4097 "Couldn’t communicate with a helper application."
[1009/092503.896821:ERROR:loopback_server.cc(907)] Loopback sync cannot read the persistent state file (/Users/chrome-bot/Library/Developer/CoreSimulator/Devices/82CF3734-9FF2-4C1B-920C-B3345C0CA891/data/Containers/Data/Application/F2938797-9668-4622-947A-895503B62BCD/tmp/.org.chromium.ost.chrome.unittests.dev.lxaMyc/profile.pb) with error FILE_ERROR_NOT_FOUND
2023-10-09 09:25:03.964248-0700 ios_chrome_eg2tests[56690:4719583] [unspecified] container_create_or_lookup_app_group_path_by_app_group_identifier: client is not entitled
2023-10-09 09:25:06.074170-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] Unable to simultaneously satisfy constraints.
  Probably at least one of the constraints in the following list is one you don't want.
  Try this:
    (1) look at each constraint and try to figure out which you don't expect;
    (2) find the code that added the unwanted constraint or constraints and fix it.
(
    "<NSLayoutConstraint:0x12003d2a8c0 H:|-(0)-[UIView:0x1200235e300]   (active, names: '|':UIView:0x12002a10fc0 )>",
    "<NSLayoutConstraint:0x12003d20360 UIView:0x1200235e300.trailing == UIView:0x12002a10fc0.trailing   (active)>",
    "<NSLayoutConstraint:0x12003d21d40 H:|-(8)-[UIView:0x1200235f640]   (active, names: '|':UIView:0x1200235e300 )>",
    "<NSLayoutConstraint:0x12003d24b60 UIView:0x1200235f640.width == 16   (active)>",
    "<NSLayoutConstraint:0x12003d9c200 H:[UIView:0x1200235f640]-(4)-[UIView:0x1200235c380]   (active)>",
    "<NSLayoutConstraint:0x12003d98a20 UIView:0x1200235c380.trailing == UIView:0x1200235e300.trailing - 8   (active)>",
    "<NSLayoutConstraint:0x12001326f60 'UIView-Encapsulated-Layout-Width' UIView:0x12002a10fc0.width == 0   (active)>"
)

Will attempt to recover by breaking constraint
<NSLayoutConstraint:0x12003d24b60 UIView:0x1200235f640.width == 16   (active)>

Make a symbolic breakpoint at UIViewAlertForUnsatisfiableConstraints to catch this in the debugger.
The methods in the UIConstraintBasedLayoutDebugging category on UIView listed in <UIKitCore/UIView.h> may also be helpful.
2023-10-09 09:25:06.075364-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] Unable to simultaneously satisfy constraints.
  Probably at least one of the constraints in the following list is one you don't want.
  Try this:
    (1) look at each constraint and try to figure out which you don't expect;
    (2) find the code that added the unwanted constraint or constraints and fix it.
(
    "<NSLayoutConstraint:0x12003d2a8c0 H:|-(0)-[UIView:0x1200235e300]   (active, names: '|':UIView:0x12002a10fc0 )>",
    "<NSLayoutConstraint:0x12003d20360 UIView:0x1200235e300.trailing == UIView:0x12002a10fc0.trailing   (active)>",
    "<NSLayoutConstraint:0x12003d21d40 H:|-(8)-[UIView:0x1200235f640]   (active, names: '|':UIView:0x1200235e300 )>",
    "<NSLayoutConstraint:0x12003d9c200 H:[UIView:0x1200235f640]-(4)-[UIView:0x1200235c380]   (active)>",
    "<NSLayoutConstraint:0x12003d98a20 UIView:0x1200235c380.trailing == UIView:0x1200235e300.trailing - 8   (active)>",
    "<NSLayoutConstraint:0x12001326f60 'UIView-Encapsulated-Layout-Width' UIView:0x12002a10fc0.width == 0   (active)>"
)

Will attempt to recover by breaking constraint
<NSLayoutConstraint:0x12003d9c200 H:[UIView:0x1200235f640]-(4)-[UIView:0x1200235c380]   (active)>

Make a symbolic breakpoint at UIViewAlertForUnsatisfiableConstraints to catch this in the debugger.
The methods in the UIConstraintBasedLayoutDebugging category on UIView listed in <UIKitCore/UIView.h> may also be helpful.
[1009/111926.491858:FATAL:chrome_earl_grey_app_interface.mm(147)] Check failed: NO.
0   ios_chrome_eg2testsMain             0x000000012a31b174 base::debug::CollectStackTrace(void const**, unsigned long) + 48
1   ios_chrome_eg2testsMain             0x000000012a2ed878 base::debug::StackTrace::StackTrace(unsigned long) + 92
2   ios_chrome_eg2testsMain             0x000000012a2ed910 base::debug::StackTrace::StackTrace(unsigned long) + 36
3   ios_chrome_eg2testsMain             0x000000012a2ed8dc base::debug::StackTrace::StackTrace() + 40
4   ios_chrome_eg2testsMain             0x000000012a03d7e0 logging::LogMessage::~LogMessage() + 204
5   ios_chrome_eg2testsMain             0x000000012a03e748 logging::LogMessage::~LogMessage() + 28
6   ios_chrome_eg2testsMain             0x000000012a03e774 logging::LogMessage::~LogMessage() + 28
7   ios_chrome_eg2testsMain             0x000000012a00bba8 logging::CheckError::~CheckError() + 112
8   ios_chrome_eg2testsMain             0x000000012a00bc08 logging::CheckError::~CheckError() + 28
9   ios_chrome_eg2testsMain             0x0000000126745008 +[ChromeEarlGreyAppInterface crashApp] + 104
more of the stack trace and crash report logs...

Standard output and standard error from com.google.chrome.unittests.dev with process ID 1358 beginning at 2023-10-09 15:19:36 +0000

2023-10-09 11:19:37.449520-0400 ios_chrome_eg2tests[1358:24891823] [User Defaults] Not updating lastKnownShmemState in CFPrefsPlistSource<0x6000030083f0> (Domain: com.apple.keyboard.preferences.plist, User: kCFPreferencesCurrentUser, ByHost: No, Container: (null), Contents Need Refresh: Yes): 0 -> 323
2023-10-09 11:19:37.449594-0400 ios_chrome_eg2tests[1358:24891823] [User Defaults] Source was stale because shmem was null: CFPrefsPlistSource<0x6000030083f0> (Domain: com.apple.keyboard.preferences.plist, User: kCFPreferencesCurrentUser, ByHost: No, Container: (null), Contents Need Refresh: Yes)

"""

APP_SIDE_FAILURE_LOG_EXPECTED = f"""App crashed and disconnected.
Showing logs from application under test. For complete logs see attempt_0_simulator#0_StandardOutputAndStandardError-com.google.chrome.unittests.dev.txt in CAS outputs, which can be found in the swarming task of the shard this test ran on.

Starting test: -[SmokeTestCase testOpenTab]
2023-10-09 09:25:00.318076-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] {constants.LAYOUT_CONSTRAINT_MSG}.
2023-10-09 09:25:01.480402-0700 ios_chrome_eg2tests[56690:4719583] [unspecified] container_create_or_lookup_app_group_path_by_app_group_identifier: client is not entitled
2023-10-09 09:25:02.383910-0700 ios_chrome_eg2tests[56690:4719754] [VoiceShortcutClient] -[VCVoiceShortcutClient unsafeSetupXPCConnection]_block_invoke Client connection to VCVoiceShortcut XPC server interrupted
2023-10-09 09:25:02.385353-0700 ios_chrome_eg2tests[56690:4719754] [Intents] -[INVoiceShortcutCenter getAllVoiceShortcutsWithCompletion:]_block_invoke Error from -getVoiceShortcutsWithCompletion: Error Domain=NSCocoaErrorDomain Code=4097 "Couldn’t communicate with a helper application."
[1009/092503.896821:ERROR:loopback_server.cc(907)] Loopback sync cannot read the persistent state file (/Users/chrome-bot/Library/Developer/CoreSimulator/Devices/82CF3734-9FF2-4C1B-920C-B3345C0CA891/data/Containers/Data/Application/F2938797-9668-4622-947A-895503B62BCD/tmp/.org.chromium.ost.chrome.unittests.dev.lxaMyc/profile.pb) with error FILE_ERROR_NOT_FOUND
2023-10-09 09:25:03.964248-0700 ios_chrome_eg2tests[56690:4719583] [unspecified] container_create_or_lookup_app_group_path_by_app_group_identifier: client is not entitled
2023-10-09 09:25:06.074170-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] {constants.LAYOUT_CONSTRAINT_MSG}.
2023-10-09 09:25:06.075364-0700 ios_chrome_eg2tests[56690:4719583] [LayoutConstraints] {constants.LAYOUT_CONSTRAINT_MSG}.
[1009/111926.491858:FATAL:chrome_earl_grey_app_interface.mm(147)] Check failed: NO.
0   ios_chrome_eg2testsMain             0x000000012a31b174 base::debug::CollectStackTrace(void const**, unsigned long) + 48
1   ios_chrome_eg2testsMain             0x000000012a2ed878 base::debug::StackTrace::StackTrace(unsigned long) + 92
2   ios_chrome_eg2testsMain             0x000000012a2ed910 base::debug::StackTrace::StackTrace(unsigned long) + 36
3   ios_chrome_eg2testsMain             0x000000012a2ed8dc base::debug::StackTrace::StackTrace() + 40
4   ios_chrome_eg2testsMain             0x000000012a03d7e0 logging::LogMessage::~LogMessage() + 204
5   ios_chrome_eg2testsMain             0x000000012a03e748 logging::LogMessage::~LogMessage() + 28
6   ios_chrome_eg2testsMain             0x000000012a03e774 logging::LogMessage::~LogMessage() + 28
7   ios_chrome_eg2testsMain             0x000000012a00bba8 logging::CheckError::~CheckError() + 112
8   ios_chrome_eg2testsMain             0x000000012a00bc08 logging::CheckError::~CheckError() + 28
9   ios_chrome_eg2testsMain             0x0000000126745008 +[ChromeEarlGreyAppInterface crashApp] + 104
more of the stack trace and crash report logs...


"""


def _xcresulttool_get_side_effect(xcresult_path, ref_id=None):
  """Side effect for _xcresulttool_get in XcodeLogParser tested."""
  if ref_id is None:
    return XCRESULT_ROOT
  if ref_id == 'testsRef':
    return TESTS_REF
  # Other situation in use cases of xcode_log_parser is asking for single test
  # summary ref.
  return SINGLE_TEST_SUMMARY_REF


class UtilMethodsTest(test_runner_test.TestCase):
  """Test case for utility methods not related with Parser class."""

  def setUp(self):
    self.summary_xcode16_with_parallel = {
        'tests': {
            '_values': ['TestSuite1', 'TestSuite2']
        }
    }

    # Example test summary when running xcode version lower than 16.
    # It could also be when running xcode version 16 without xcode
    # parallelization enabled.
    self.summary_pre_xcode16 = {
        'tests': {
            '_values': [{
                'subtests': {
                    '_values': [{
                        'subtests': {
                            '_values': ['TestSuite1', 'TestSuite2']
                        }
                    }]
                }
            }]
        }
    }

  def testParseTestsForInterruptedRun(self):
    test_output = """
    Test case '-[DownloadManagerTestCase testVisibleFileNameAndOpenInDownloads]' passed on 'Clone 2 of iPhone X 15.0 test simulator - ios_chrome_ui_eg2tests_module-Runner (34498)' (20.715 seconds)
    Test case '-[SyncFakeServerTestCase testSyncDownloadBookmark]' passed on 'Clone 1 of iPhone X 15.0 test simulator - ios_chrome_ui_eg2tests_module-Runner (34249)' (14.880 seconds)
    Random lines
         t =    53.90s Tear Down
    Test Case '-[LinkToTextTestCase testGenerateLinkForSimpleText]' failed (55.316 seconds).
     t =      nans Suite Tear Down
    Test Suite 'LinkToTextTestCase' failed at 2021-06-15 07:13:17.406.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.338) seconds
    Test Suite 'ios_chrome_ui_eg2tests_module.xctest' failed at 2021-06-15 07:13:17.407.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.340) seconds
    Test Suite 'Selected tests' failed at 2021-06-15 07:13:17.408.
      Executed 1 test, with 6 failures (6 unexpected) in 55.316 (55.342) seconds
    """
    test_output_list = test_output.split('\n')
    expected_passed = set([
        'DownloadManagerTestCase/testVisibleFileNameAndOpenInDownloads',
        'SyncFakeServerTestCase/testSyncDownloadBookmark'
    ])
    expected_failed = set(['LinkToTextTestCase/testGenerateLinkForSimpleText'])
    expected_failed_message = 'Test failed in interrupted(timedout) run.'

    results = xcode_log_parser.parse_passed_failed_tests_for_interrupted_run(
        test_output_list)
    self.assertEqual(results.expected_tests(), expected_passed)
    self.assertEqual(results.unexpected_tests(), expected_failed)
    for result in results.test_results:
      if result.name == 'LinkToTextTestCase/testGenerateLinkForSimpleText':
        self.assertEqual(result.test_log, expected_failed_message)

  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def test_xcode16_parallel(self, mock_xcode_version):
    mock_xcode_version.return_value = True
    result = xcode_log_parser.get_test_suites(
        self.summary_xcode16_with_parallel, True)
    self.assertEqual(result, ['TestSuite1', 'TestSuite2'])

  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def test_xcode16_not_parallel(self, mock_xcode_version):
    mock_xcode_version.return_value = True
    result = xcode_log_parser.get_test_suites(self.summary_pre_xcode16, False)
    self.assertEqual(result, ['TestSuite1', 'TestSuite2'])

  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def test_pre_xcode16_parallel(self, mock_xcode_version):
    mock_xcode_version.return_value = False
    result = xcode_log_parser.get_test_suites(self.summary_pre_xcode16, True)
    self.assertEqual(result, ['TestSuite1', 'TestSuite2'])


class XcodeLogParserTest(test_runner_test.TestCase):
  """Test case to test XcodeLogParser."""

  def setUp(self):
    super(XcodeLogParserTest, self).setUp()
    self.mock(test_runner, 'get_current_xcode_info', lambda: XCODE11_DICT)

  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def testXcresulttoolGetRoot(self, mock_xcode_version, mock_process):
    mock_xcode_version.return_value = False
    mock_process.return_value = b'%JSON%'
    xcode_log_parser.XcodeLogParser()._xcresulttool_get('xcresult_path')
    self.assertTrue(
        os.path.join(XCODE11_DICT['path'], 'usr', 'bin') in os.environ['PATH'])
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])

  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def testXcresulttoolGetRef(self, mock_xcode_version, mock_process):
    mock_xcode_version.return_value = False
    mock_process.side_effect = [REF_ID, b'JSON']
    xcode_log_parser.XcodeLogParser()._xcresulttool_get('xcresult_path',
                                                          'testsRef')
    self.assertEqual(
        ['xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path'],
        mock_process.mock_calls[0][1][0])
    self.assertEqual([
        'xcresulttool', 'get', '--format', 'json', '--path', 'xcresult_path',
        '--id', 'REF_ID'], mock_process.mock_calls[1][1][0])

  def testXcresulttoolListFailedTests(self):
    failure_message = (
        'file:///../../ios/web/shell/test/page_state_egtest.mm#'
        'CharacterRangeLen=0&EndingLineNumber=130&StartingLineNumber=130\n'
        'Fail. Screenshots: {\n\"Failure\": \"path.png\"\n}')
    expected = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])
    results = xcode_log_parser.XcodeLogParser()._list_of_failed_tests(
        json.loads(XCRESULT_ROOT))
    self.assertEqual(expected, results.failed_tests())
    log = results.test_results[0].test_log
    self.assertEqual(log, failure_message)

  def testXcresulttoolListFailedTestsExclude(self):
    excluded = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])
    results = xcode_log_parser.XcodeLogParser()._list_of_failed_tests(
        json.loads(XCRESULT_ROOT), excluded=excluded)
    self.assertEqual(set([]), results.all_test_names())

  @mock.patch('xcode_log_parser.XcodeLogParser._export_data')
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  def testGetTestStatuses(self, mock_xcresult, mock_export):
    mock_xcresult.side_effect = _xcresulttool_get_side_effect
    #   self.assertEqual(test_result.test_log, lo
    expected_failure_log = (
        'Logs from "failureSummaries" in .xcresult:\n'
        'file: /../../ios/web/shell/test/page_state_egtest.mm, line: 131\n'
        'Some logs.\n'
        'file: , line: \n'
        'Immediately halt execution of testcase '
        '(EarlGreyInternalTestInterruptException)\n')
    expected_expected_tests = set([
        'PageStateTestCase/testMethod1', 'PageStateTestCase/testMethod2',
        'PageStateTestCase/testMethod3'
    ])
    results = xcode_log_parser.XcodeLogParser()._get_test_statuses(
        OUTPUT_PATH, False)
    self.assertEqual(expected_expected_tests, results.expected_tests())
    seen_failed_test = False
    for test_result in results.test_results:
      if test_result.name == 'PageStateTestCase/testZeroContentOffsetAfterLoad':
        seen_failed_test = True
        self.assertEqual(test_result.test_log, expected_failure_log)
        self.assertEqual(test_result.duration, None)
        crash_file_name = (
            'attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad_'
            'Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash'
        )
        jpeg_file_name = (
            'attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
            '_kXCTAttachmentLegacyScreenImageData_1'
            '_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg')
        self.assertDictEqual(
            {
                crash_file_name: '/tmp/%s' % crash_file_name,
                jpeg_file_name: '/tmp/%s' % jpeg_file_name,
            }, test_result.attachments)
      if test_result.name == 'PageStateTestCase/testMethod1':
        self.assertEqual(test_result.duration, 35384)
      if test_result.name == 'PageStateTestCase/testMethod2':
        self.assertEqual(test_result.duration, 28988)
      if test_result.name == 'PageStateTestCase/testMethod3':
        self.assertEqual(test_result.duration, 60)

    self.assertTrue(seen_failed_test)

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.XcodeLogParser._extract_artifacts_for_test')
  @mock.patch('xcode_log_parser.XcodeLogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  def testCollectTestTesults(self, mock_root, mock_exist_file, *args):
    expected_passed = set([
        'PageStateTestCase/testMethod1', 'PageStateTestCase/testMethod2',
        'PageStateTestCase/testMethod3'
    ])
    expected_failed = set(['PageStateTestCase/testZeroContentOffsetAfterLoad'])

    mock_root.side_effect = _xcresulttool_get_side_effect
    mock_exist_file.return_value = True
    results = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, [])

    # Length ensures no duplicate results from |_get_test_statuses| and
    # |_list_of_failed_tests|.
    self.assertEqual(len(results.test_results), 4)
    self.assertEqual(expected_passed, results.expected_tests())
    self.assertEqual(expected_failed, results.unexpected_tests())
    # Ensure format.
    for test in results.test_results:
      self.assertTrue(isinstance(test.name, str))
      if test.status == TestStatus.FAIL:
        self.assertTrue(isinstance(test.test_log, str))

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.XcodeLogParser.copy_artifacts')
  @mock.patch('xcode_log_parser.XcodeLogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  def testCollectTestsRanZeroTests(self, mock_root, mock_exist_file, *args):
    metrics_json = '{"actions": {}}'
    mock_root.return_value = metrics_json
    mock_exist_file.return_value = True
    results = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(results.crash_message, '0 tests executed!')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('xcode_log_parser.XcodeLogParser._list_of_failed_tests')
  @mock.patch('xcode_log_parser.XcodeLogParser._get_test_statuses')
  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.XcodeLogParser.copy_artifacts')
  @mock.patch('xcode_log_parser.XcodeLogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  def testFallbackOnRootMetrics(self, mock_root, mock_exist_file, *args):
    mock_root.return_value = XCRESULT_MISSING_ACTIONRESULT_METRICS
    mock_exist_file.return_value = True
    results = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed != True)
    self.assertNotEqual(results.crash_message, '0 tests executed!')

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsDidNotRun(self, mock_exist_file):
    mock_exist_file.return_value = False
    results = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(results.crash_message,
                     '/tmp/attempt_0 with staging data does not exist.\n')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestsInterruptedRun(self, mock_exist_file):
    mock_exist_file.side_effect = [True, False]
    results = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, [])
    self.assertTrue(results.crashed)
    self.assertEqual(
        results.crash_message,
        '/tmp/attempt_0.xcresult with test results does not exist.\n')
    self.assertEqual(len(results.all_test_names()), 0)

  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def testCopyScreenshots(self, mock_xcode_version, mock_xcresulttool_get,
                          mock_path_exists, mock_process):
    mock_xcode_version.return_value = False
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.XcodeLogParser().copy_artifacts(OUTPUT_PATH)
    mock_process.assert_any_call([
        'xcresulttool', 'export', '--type', 'file', '--id',
        'SCREENSHOT_REF_ID_IN_FAILURE_SUMMARIES', '--path', XCRESULT_PATH,
        '--output-path',
        '/tmp/attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
        '_kXCTAttachmentLegacyScreenImageData_1'
        '_6CED1FE5-96CA-47EA-9852-6FADED687262.jpeg'
    ])
    mock_process.assert_any_call([
        'xcresulttool', 'export', '--type', 'file', '--id',
        'CRASH_REF_ID_IN_ACTIVITY_SUMMARIES', '--path', XCRESULT_PATH,
        '--output-path',
        '/tmp/attempt_0_PageStateTestCase_testZeroContentOffsetAfterLoad'
        '_Crash_3F0A2B1C-7ADA-436E-A54C-D4C39B8411F8.crash'
    ])
    # Ensures screenshots in activitySummaries are not copied.
    self.assertEqual(2, mock_process.call_count)

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def testExportDiagnosticData(self, mock_xcode_version, mock_xcresulttool_get,
                               mock_path_exists, mock_process, _):
    mock_xcode_version.return_value = False
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.XcodeLogParser.export_diagnostic_data(OUTPUT_PATH)
    mock_process.assert_called_with([
        'xcresulttool', 'export', '--type', 'directory', '--id',
        'DIAGNOSTICS_REF_ID', '--path', XCRESULT_PATH, '--output-path',
        '/tmp/attempt_0.xcresult_diagnostic'
    ])

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('shutil.copy')
  @mock.patch('subprocess.check_output', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  @mock.patch('xcode_util.using_xcode_16_or_higher')
  def testStdoutCopiedInExportDiagnosticData(self, mock_xcode_version,
                                             mock_xcresulttool_get,
                                             mock_path_exists, mock_process,
                                             mock_copy, _):
    mock_xcode_version.return_value = False
    output_path_in_test = 'test_data/attempt_0'
    xcresult_path_in_test = 'test_data/attempt_0.xcresult'
    mock_path_exists.return_value = True
    mock_xcresulttool_get.side_effect = _xcresulttool_get_side_effect
    xcode_log_parser.XcodeLogParser.export_diagnostic_data(
        output_path_in_test)
    # os.walk() walks folders in unknown sequence. Use try-except blocks to
    # assert that any of the 2 assertions is true.
    try:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID1/StandardOutputAndStandardError.txt',
          'test_data/attempt_0/../attempt_0_simulator#1_StandardOutputAndStandardError.txt'
      )
    except AssertionError:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID1/StandardOutputAndStandardError.txt',
          'test_data/attempt_0/../attempt_0_simulator#0_StandardOutputAndStandardError.txt'
      )
    try:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID2/StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt',
          'test_data/attempt_0/../attempt_0_simulator#1_StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt'
      )
    except AssertionError:
      mock_copy.assert_any_call(
          'test_data/attempt_0.xcresult_diagnostic/test_module-UUID/test_module-UUID2/StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt',
          'test_data/attempt_0/../attempt_0_simulator#0_StandardOutputAndStandardError-org.chromium.gtest.ios-chrome-eg2tests.txt'
      )

  @mock.patch('os.path.exists', autospec=True)
  def testCollectTestResults_interruptedTests(self, mock_path_exists):
    mock_path_exists.side_effect = [True, False]
    output = [
        '[09:03:42:INFO] Test case \'-[TestCase1 method1]\' passed on device.',
        '[09:06:40:INFO] Test Case \'-[TestCase2 method1]\' passed on device.',
        '[09:09:00:INFO] Test case \'-[TestCase2 method1]\' failed on device.',
        '** BUILD INTERRUPTED **',
    ]
    not_found_message = ['%s with test results does not exist.' % XCRESULT_PATH]
    res = xcode_log_parser.XcodeLogParser().collect_test_results(
        OUTPUT_PATH, output)
    self.assertTrue(res.crashed)
    self.assertEqual('\n'.join(not_found_message + output), res.crash_message)
    self.assertEqual(
        set(['TestCase1/method1', 'TestCase2/method1']), res.expected_tests())

  @mock.patch('file_util.zip_and_remove_folder')
  @mock.patch('xcode_log_parser.XcodeLogParser._extract_artifacts_for_test')
  @mock.patch('xcode_log_parser.XcodeLogParser.export_diagnostic_data')
  @mock.patch('os.path.exists', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser._xcresulttool_get')
  @mock.patch('xcode_log_parser.XcodeLogParser._list_of_failed_tests')
  def testArtifactsDiagnosticLogsExportedInCollectTestTesults(
      self, mock_get_failed_tests, mock_root, mock_exist_file,
      mock_export_diagnostic_data, mock_extract_artifacts, mock_zip):
    mock_root.side_effect = _xcresulttool_get_side_effect
    mock_exist_file.return_value = True
    xcode_log_parser.XcodeLogParser().collect_test_results(OUTPUT_PATH, [])
    mock_export_diagnostic_data.assert_called_with(OUTPUT_PATH)
    mock_extract_artifacts.assert_called()

  @mock.patch('os.listdir')
  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=APP_SIDE_FAILURE_LOG))
  def testLogAppSideFailureReason(self, mock_listdir):
    test_name = 'SmokeTestCase/testOpenTab'
    mock_listdir.return_value = [
        'run_1696864672.xctestrun', 'attempt_0.xcresult.zip',
        'attempt_0.xcresult_diagnostic.zip',
        'attempt_0_simulator#0_StandardOutputAndStandardError-com.google.chrome.unittests.dev.txt',
        'attempt_0_simulator#0_StandardOutputAndStandardError.txt',
    ]
    app_side_failure_message = \
      xcode_log_parser.XcodeLogParser()._get_app_side_failure(
        test_name, OUTPUT_PATH)
    self.assertEqual(app_side_failure_message, APP_SIDE_FAILURE_LOG_EXPECTED)


if __name__ == '__main__':
  unittest.main()
