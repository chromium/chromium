#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for classes in gtest_utils.py."""

import os
import unittest

import gtest_utils
from test_result_util import TestStatus

FAILURES = [
    'NavigationControllerTest.Reload',
    'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0',
    'BadTest.TimesOut', 'MoreBadTest.TimesOutAndFails',
    'SomeOtherTest.SwitchTypes', 'SomeOtherTest.FAILS_ThisTestTimesOut'
]

FAILS_FAILURES = ['SomeOtherTest.FAILS_Bar']
FLAKY_FAILURES = ['SomeOtherTest.FLAKY_Baz']

CRASH_MESSAGE = ['Oops, this test crashed!']
TIMEOUT_MESSAGE = 'Killed (timed out).'

RELOAD_ERRORS = (r'C:\b\slave\chrome-release-snappy\build\chrome\browser'
                 r'\navigation_controller_unittest.cc:381: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""")

SPDY_ERRORS = (r'C:\b\slave\chrome-release-snappy\build\chrome\browser'
               r'\navigation_controller_unittest.cc:439: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""")

SWITCH_ERRORS = (r'C:\b\slave\chrome-release-snappy\build\chrome\browser'
                 r'\navigation_controller_unittest.cc:615: Failure' + """
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

""" + r'C:\b\slave\chrome-release-snappy\build\chrome\browser'
                 r'\navigation_controller_unittest.cc:617: Failure' + """
Value of: contents->controller()->GetPendingEntry()
  Actual: true
Expected: false

""")

# pylint: disable=line-too-long
TIMEOUT_ERRORS = (
    '[61613:263:0531/042613:2887943745568888:ERROR:/b/slave'
    '/chromium-rel-mac-builder/build/src/chrome/browser/extensions'
    '/extension_error_reporter.cc(56)] Extension error: Could not load extension '
    'from \'extensions/api_test/geolocation/no_permission\'. Manifest file is '
    'missing or unreadable.')

MOREBAD_ERRORS = """
Value of: entry->page_type()
  Actual: 2
Expected: NavigationEntry::NORMAL_PAGE
"""

TEST_DATA = (
    """
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
%(reload_errors)s
[  FAILED  ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
%(spdy_errors)s
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0 (2 ms)
[----------] 4 tests from NavigationControllerTest (74 ms total)

  YOU HAVE 2 FLAKY TESTS

[----------] 1 test from BadTest
[ RUN      ] BadTest.TimesOut
%(timeout_errors)s
""" % {
        'reload_errors': RELOAD_ERRORS,
        'spdy_errors': SPDY_ERRORS,
        'timeout_errors': TIMEOUT_ERRORS
    } + '[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome'
    '/test/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
    'exceeded for BadTest.TimesOut' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
""" +
    '[61613:19971:0531/042642:2887973024284693:INFO:/b/slave/chromium-rel-mac-'
    'builder/build/src/chrome/browser/browser_main.cc(285)] Handling shutdown for '
    'signal 15.' + """

[----------] 1 test from MoreBadTest
[ RUN      ] MoreBadTest.TimesOutAndFails
%(morebad_errors)s
""" % {
        'morebad_errors': MOREBAD_ERRORS
    } +
    '[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
    '/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
    'exceeded for MoreBadTest.TimesOutAndFails' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
[  FAILED  ] MoreBadTest.TimesOutAndFails (31000 ms)

[----------] 5 tests from SomeOtherTest
[ RUN      ] SomeOtherTest.SwitchTypes
%(switch_errors)s
[  FAILED  ] SomeOtherTest.SwitchTypes (40 ms)
[ RUN      ] SomeOtherTest.Foo
[       OK ] SomeOtherTest.Foo (20 ms)
[ RUN      ] SomeOtherTest.FAILS_Bar
Some error message for a failing test.
[  FAILED  ] SomeOtherTest.FAILS_Bar (40 ms)
[ RUN      ] SomeOtherTest.FAILS_ThisTestTimesOut
""" % {
        'switch_errors': SWITCH_ERRORS
    } + '[0521/041343:ERROR:test_launcher.cc(384)] Test timeout (5000 ms) '
    'exceeded for SomeOtherTest.FAILS_ThisTestTimesOut' + """
[ RUN      ] SomeOtherTest.FLAKY_Baz
Some error message for a flaky test.
[  FAILED  ] SomeOtherTest.FLAKY_Baz (40 ms)
[----------] 2 tests from SomeOtherTest (60 ms total)

[----------] Global test environment tear-down
[==========] 8 tests from 3 test cases ran. (3750 ms total)
[  PASSED  ] 4 tests.
[  FAILED  ] 4 tests, listed below:
[  FAILED  ] NavigationControllerTest.Reload
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
[  FAILED  ] SomeOtherTest.SwitchTypes
[  FAILED  ] SomeOtherTest.FAILS_ThisTestTimesOut

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS

program finished with exit code 1
""")

TEST_DATA_CRASH = """
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.Crashes
Oops, this test crashed!
"""

TEST_DATA_MIXED_STDOUT = """
[==========] Running 3 tests from 3 test cases.
[----------] Global test environment set-up.

[----------] 1 tests from WebSocketHandshakeHandlerSpdy3Test
[ RUN      ] WebSocketHandshakeHandlerSpdy3Test.RequestResponse
[       OK ] WebSocketHandshakeHandlerSpdy3Test.RequestResponse (1 ms)
[----------] 1 tests from WebSocketHandshakeHandlerSpdy3Test (1 ms total)

[----------] 1 test from URLRequestTestFTP
[ RUN      ] URLRequestTestFTP.UnsafePort
FTP server started on port 32841...
sending server_data: {"host": "127.0.0.1", "port": 32841} (36 bytes)
starting FTP server[       OK ] URLRequestTestFTP.UnsafePort (300 ms)
[----------] 1 test from URLRequestTestFTP (300 ms total)

[ RUN      ] TestFix.TestCase
[1:2/3:WARNING:extension_apitest.cc(169)] Workaround for 177163,
prematurely stopping test
[       OK ] X (1000ms total)

[----------] 1 test from Crash
[ RUN      ] Crash.Test
Oops, this test crashed!
"""

TEST_DATA_SKIPPED = """
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from ProcessReaderLinux
[ RUN      ] ProcessReaderLinux.AbortMessage
../../third_party/crashpad/crashpad/snapshot/linux/process_reader_linux_test.cc:842: Skipped

Stack trace:
#00 pc 0x00000000002350b7 /data/local/tmp/crashpad_tests__dist/crashpad_tests
#01 pc 0x0000000000218183 /data/local/tmp/crashpad_tests__dist/crashpad_tests

[  SKIPPED ] ProcessReaderLinux.AbortMessage (1 ms)
[----------] 1 test from ProcessReaderLinux (2 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (2 ms total)
[  PASSED  ] 0 tests.
[  SKIPPED ] 1 test, listed below:
[  SKIPPED ] ProcessReaderLinux.AbortMessage
"""

VALGRIND_HASH = 'B254345E4D3B6A00'

VALGRIND_REPORT = """Leak_DefinitelyLost
1 (1 direct, 0 indirect) bytes in 1 blocks are lost in loss record 1 of 1
  operator new(unsigned long) (m_replacemalloc/vg_replace_malloc.c:1140)
  content::NavigationControllerTest_Reload::TestBody() (a/b/c/d.cc:1150)
Suppression (error hash=#%(hash)s#):
{
   <insert_a_suppression_name_here>
   Memcheck:Leak
   fun:_Znw*
   fun:_ZN31NavigationControllerTest_Reload8TestBodyEv
}""" % {
    'hash': VALGRIND_HASH
}

TEST_DATA_VALGRIND = """
[==========] Running 5 tests from 2 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
[       OK ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
[       OK ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0 (2 ms)
[----------] 4 tests from NavigationControllerTest (74 ms total)

[----------] Global test environment tear-down
[==========] 5 tests from 1 test cases ran. (136 ms total)
[  PASSED  ] 5 tests.

### BEGIN MEMORY TOOL REPORT (error hash=#%(hash)s#)
%(report)s
### END MEMORY TOOL REPORT (error hash=#%(hash)s#)
program finished with exit code 255

""" % {
    'report': VALGRIND_REPORT,
    'hash': VALGRIND_HASH
}

FAILING_TESTS_OUTPUT = """
Failing tests:
ChromeRenderViewTest.FAILS_AllowDOMStorage
PrerenderBrowserTest.PrerenderHTML5VideoJs
"""

FAILING_TESTS_EXPECTED = [
    'ChromeRenderViewTest.FAILS_AllowDOMStorage',
    'PrerenderBrowserTest.PrerenderHTML5VideoJs'
]

TEST_DATA_SHARD_0 = (
    """Note: This is test shard 1 of 30.
[==========] Running 6 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.All
[       OK ] HunspellTest.All (62 ms)
[----------] 1 test from HunspellTest (62 ms total)

[----------] 1 test from BadTest
[ RUN      ] BadTest.TimesOut
%(timeout_errors)s
""" % {
        'timeout_errors': TIMEOUT_ERRORS
    } +
    '[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
    '/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
    'exceeded for BadTest.TimesOut' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
""" +
    '[61613:19971:0531/042642:2887973024284693:INFO:/b/slave/chromium-rel-mac-'
    'builder/build/src/chrome/browser/browser_main.cc(285)] Handling shutdown for '
    'signal 15.' + """

[----------] 4 tests from SomeOtherTest
[ RUN      ] SomeOtherTest.SwitchTypes
%(switch_errors)s
[  FAILED  ] SomeOtherTest.SwitchTypes (40 ms)
[ RUN      ] SomeOtherTest.Foo
[       OK ] SomeOtherTest.Foo (20 ms)
[ RUN      ] SomeOtherTest.FAILS_Bar
Some error message for a failing test.
[  FAILED  ] SomeOtherTest.FAILS_Bar (40 ms)
[ RUN      ] SomeOtherTest.FAILS_ThisTestTimesOut
""" % {
        'switch_errors': SWITCH_ERRORS
    } +
    '[0521/041343:ERROR:test_launcher.cc(384)] Test timeout (5000 ms) exceeded '
    'for SomeOtherTest.FAILS_ThisTestTimesOut' + """
[ RUN      ] SomeOtherTest.FLAKY_Baz
Some error message for a flaky test.
[  FAILED  ] SomeOtherTest.FLAKY_Baz (40 ms)
[----------] 2 tests from SomeOtherTest (60 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 3 test cases ran. (3750 ms total)
[  PASSED  ] 5 tests.
[  FAILED  ] 2 test, listed below:
[  FAILED  ] SomeOtherTest.SwitchTypes
[  FAILED  ] SomeOtherTest.FAILS_ThisTestTimesOut

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS
""")

TEST_DATA_SHARD_1 = (
    """Note: This is test shard 13 of 30.
[==========] Running 5 tests from 2 test cases.
[----------] Global test environment set-up.
[----------] 4 tests from NavigationControllerTest
[ RUN      ] NavigationControllerTest.Defaults
[       OK ] NavigationControllerTest.Defaults (48 ms)
[ RUN      ] NavigationControllerTest.Reload
%(reload_errors)s
[  FAILED  ] NavigationControllerTest.Reload (2 ms)
[ RUN      ] NavigationControllerTest.Reload_GeneratesNewPage
[       OK ] NavigationControllerTest.Reload_GeneratesNewPage (22 ms)
[ RUN      ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0
%(spdy_errors)s
""" % {
        'reload_errors': RELOAD_ERRORS,
        'spdy_errors': SPDY_ERRORS
    } + '[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor'
    '/0 (2 ms)' + """
[----------] 4 tests from NavigationControllerTest (74 ms total)

  YOU HAVE 2 FLAKY TESTS

[----------] 1 test from MoreBadTest
[ RUN      ] MoreBadTest.TimesOutAndFails
%(morebad_errors)s
""" % {
        'morebad_errors': MOREBAD_ERRORS
    } +
    '[0531/042642:ERROR:/b/slave/chromium-rel-mac-builder/build/src/chrome/test'
    '/test_launcher/out_of_proc_test_runner.cc(79)] Test timeout (30000 ms) '
    'exceeded for MoreBadTest.TimesOutAndFails' + """
Handling SIGTERM.
Successfully wrote to shutdown pipe, resetting signal handler.
[  FAILED  ] MoreBadTest.TimesOutAndFails (31000 ms)

[----------] Global test environment tear-down
[==========] 5 tests from 2 test cases ran. (3750 ms total)
[  PASSED  ] 3 tests.
[  FAILED  ] 2 tests, listed below:
[  FAILED  ] NavigationControllerTest.Reload
[  FAILED  ] NavigationControllerTest/SpdyNetworkTransTest.Constructor/0

 1 FAILED TEST
  YOU HAVE 10 DISABLED TESTS

  YOU HAVE 2 FLAKY TESTS
""")

TEST_DATA_SHARD_EXIT = 'program finished with exit code '

TEST_DATA_CRASH_SHARD = """Note: This is test shard 5 of 5.
[==========] Running 7 tests from 3 test cases.
[----------] Global test environment set-up.
[----------] 1 test from HunspellTest
[ RUN      ] HunspellTest.Crashes
Oops, this test crashed!"""

TEST_DATA_NESTED_RUNS = (
    """
[ 1/3] 1.0s Foo.Bar (45.5s)
Note: Google Test filter = Foo.Bar
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from Foo, where TypeParam =
[ RUN      ] Foo.Bar
""" +
    '[0725/050653:ERROR:test_launcher.cc(380)] Test timeout (45000 ms) exceeded '
    'for Foo.Bar' + """
Starting tests...
IMPORTANT DEBUGGING NOTE: each test is run inside its own process.
For debugging a test inside a debugger, use the
--gtest_filter=<your_test_name> flag along with either
--single_process (to run all tests in one launcher/browser process) or
--single-process (to do the above, and also run Chrome in single-
process mode).
1 test run
1 test failed (0 ignored)
Failing tests:
Foo.Bar
[ 2/2] 2.00s Foo.Pass (1.0s)""")

# Data generated with run_test_case.py
TEST_DATA_RUN_TEST_CASE_FAIL = """
[  6/422]   7.45s SUIDSandboxUITest.testSUIDSandboxEnabled (1.49s) - retry #2
[ RUN      ] SUIDSandboxUITest.testSUIDSandboxEnabled
[  FAILED  ] SUIDSandboxUITest.testSUIDSandboxEnabled (771 ms)
[  8/422]   7.76s PrintPreviewWebUITest.SourceIsPDFShowFitToPageOption (1.67s)
"""

TEST_DATA_RUN_TEST_CASE_TIMEOUT = """
[  6/422]   7.45s SUIDSandboxUITest.testSUIDSandboxEnabled (1.49s) - retry #2
[ RUN      ] SUIDSandboxUITest.testSUIDSandboxEnabled
(junk)
[  8/422]   7.76s PrintPreviewWebUITest.SourceIsPDFShowFitToPageOption (1.67s)
"""

# Data generated by swarming.py
TEST_DATA_SWARM_TEST_FAIL = """

================================================================
Begin output from shard index 0 (machine tag: swarm12.c, id: swarm12)
================================================================

[==========] Running 2 tests from linux_swarm_trigg-8-base_unittests test run.
Starting tests (using 2 parallel jobs)...
IMPORTANT DEBUGGING NOTE: batches of tests are run inside their
own process. For debugging a test inside a debugger, use the
--gtest_filter=<your_test_name> flag along with
--single-process-tests.
[1/1242] HistogramDeathTest.BadRangesTest (62 ms)
[2/1242] OutOfMemoryDeathTest.New (22 ms)
[1242/1242] ThreadIdNameManagerTest.ThreadNameInterning (0 ms)
Retrying 1 test (retry #1)
[ RUN      ] PickleTest.EncodeDecode
../../base/pickle_unittest.cc:69: Failure
Value of: false
  Actual: false
Expected: true
[  FAILED  ] PickleTest.EncodeDecode (0 ms)
[1243/1243] PickleTest.EncodeDecode (0 ms)
Retrying 1 test (retry #2)
[ RUN      ] PickleTest.EncodeDecode
../../base/pickle_unittest.cc:69: Failure
Value of: false
  Actual: false
Expected: true
[  FAILED  ] PickleTest.EncodeDecode (1 ms)
[1244/1244] PickleTest.EncodeDecode (1 ms)
Retrying 1 test (retry #3)
[ RUN      ] PickleTest.EncodeDecode
../../base/pickle_unittest.cc:69: Failure
Value of: false
  Actual: false
Expected: true
[  FAILED  ] PickleTest.EncodeDecode (0 ms)
[1245/1245] PickleTest.EncodeDecode (0 ms)
1245 tests run
1 test failed:
    PickleTest.EncodeDecode
Summary of all itest iterations:
1 test failed:
    PickleTest.EncodeDecode
End of the summary.
Tests took 31 seconds.


================================================================
End output from shard index 0 (machine tag: swarm12.c, id: swarm12). Return 1
================================================================

"""

TEST_DATA_COMPILED_FILE = """

Testing started
Wrote compiled tests to file: test_data/compiled_tests.json

[----------] 1 tests from test1
[ RUN      ] test1.test_method1
[       OK ] test1.test_method1 (5 ms)
[----------] 1 tests from test1 (5 ms total)

"""

COMPILED_FILE_PATH = 'test_data/compiled_tests.json'

TEST_DATA_LAUNCHER_SPAWN = """
[03:12:19:INFO] Using 8 parallel jobs.
[03:12:19:INFO] [1/2] TestFix.TestCase (8 ms)
[04:20:17:INFO] [ RUN      ] TextPaintTimingDetectorTest.LargestTextPaint
[04:20:17:INFO] ../../third_party/blink/renderer/core/paint/timing/text_paint_timing_detector_test.cc:361: Failure
[04:20:17:INFO] Expected equality of these values:
[04:20:17:INFO]   1u
[04:20:17:INFO]     Which is: 1
[04:20:17:INFO]   events.size()
[04:20:17:INFO]     Which is: 8
[04:20:17:INFO]
[04:20:17:INFO] [  FAILED  ] TextPaintTimingDetectorTest.LargestTextPaint (567 ms)
[04:22:58:INFO] Retrying 1 test (retry #0)
[04:23:01:INFO] [3/3] TextPaintTimingDetectorTest.LargestTextPaint (138 ms)
[03:12:19:INFO] SUCCESS: all tests passed.
[03:12:20:INFO] Tests took 46 seconds.

"""

TEST_DATA_LAUNCHER_SPAWN_CRASH = """

IMPORTANT DEBUGGING NOTE: batches of tests are run inside their
own process. For debugging a test inside a debugger, use the
--gtest_filter=<your_test_name> flag along with
--single-process-tests.
Using sharding settings from environment. This is shard 0/1
Using 1 parallel jobs.
[==========] Running 3 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 3 tests from LoggingTest
[ RUN      ] LoggingTest.FailedTest
../../base/logging_unittest.cc:143: Failure
Value of: (::logging::ShouldCreateLogMessage(::logging::LOGGING_INFO))
  Actual: true
Expected: false
Stack trace:
#0 0x560c1971de44 logging::(anonymous namespace)::LoggingTest_LogIsOn_Test::TestBody()

[  FAILED  ] LoggingTest.FailedTest (1 ms)
[ RUN      ] LoggingTest.StreamingWstringFindsCorrectOperator
[       OK ] LoggingTest.StreamingWstringFindsCorrectOperator (0 ms)
[ RUN      ] LoggingTest.CrashedTest
[1309853:1309853:FATAL:logging_unittest.cc(145)] Check failed: false.
#0 0x7f151e295152 base::debug::CollectStackTrace()

[1/3] LoggingTest.FailedTest (1 ms)
[2/3] LoggingTest.StreamingWstringFindsCorrectOperator (1 ms)
[3/3] LoggingTest.CrashedTest (CRASHED)
1 test failed:
    LoggingTest.FailedTest (../../base/logging_unittest.cc:141)
1 test crashed:
    LoggingTest.CrashedTest (../../base/logging_unittest.cc:141)
Tests took 0 seconds.

"""

TEST_REPO = 'https://test'
# pylint: enable=line-too-long


class TestGTestLogParserTests(unittest.TestCase):

  def testGTestLogParserNoSharding(self):
    # Tests for log parsing without sharding.
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertFalse(parser.RunningTests())

    self.assertEqual(sorted(FAILURES), sorted(parser.FailedTests()))
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES),
        sorted(parser.FailedTests(include_fails=True)))
    self.assertEqual(
        sorted(FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_flaky=True)))
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_fails=True, include_flaky=True)))

    self.assertEqual(10, parser.DisabledTests())
    self.assertEqual(2, parser.FlakyTests())

    test_name = 'NavigationControllerTest.Reload'
    self.assertEqual('\n'.join(['%s: ' % test_name, RELOAD_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = 'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0'
    self.assertEqual('\n'.join(['%s: ' % test_name, SPDY_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = 'SomeOtherTest.SwitchTypes'
    self.assertEqual('\n'.join(['%s: ' % test_name, SWITCH_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = 'BadTest.TimesOut'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name, TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['TIMEOUT'], parser.TriesForTest(test_name))

    test_name = 'MoreBadTest.TimesOutAndFails'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name, MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['TIMEOUT'], parser.TriesForTest(test_name))

    self.assertEqual(['SUCCESS'], parser.TriesForTest('SomeOtherTest.Foo'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(collection.never_expected_tests()))

    self.assertEqual(len(collection.test_results), 12)

    # To know that each condition branch in for loop is covered.
    cover_set = set()
    for test_result in collection.test_results:
      name = test_result.name
      if name == 'NavigationControllerTest.Reload':
        cover_set.add(name)
        self.assertEqual('\n'.join([RELOAD_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(2, test_result.duration)

      if name == 'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0':
        cover_set.add(name)
        self.assertEqual('\n'.join([SPDY_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(2, test_result.duration)

      if name == 'SomeOtherTest.SwitchTypes':
        cover_set.add(name)
        self.assertEqual('\n'.join([SWITCH_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(40, test_result.duration)

      if name == 'BadTest.TimesOut':
        cover_set.add(name)
        self.assertEqual('\n'.join([TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
                         test_result.test_log)
        self.assertEqual(TestStatus.ABORT, test_result.status)
        self.assertEqual(None, test_result.duration)

      if name == 'MoreBadTest.TimesOutAndFails':
        cover_set.add(name)
        self.assertEqual('\n'.join([MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
                         test_result.test_log)
        self.assertEqual(TestStatus.ABORT, test_result.status)
        self.assertEqual(None, test_result.duration)

      if name == 'SomeOtherTest.Foo':
        cover_set.add(name)
        self.assertEqual('', test_result.test_log)
        self.assertEqual(TestStatus.PASS, test_result.status)
        self.assertEqual(20, test_result.duration)

    test_list = [
        'BadTest.TimesOut', 'MoreBadTest.TimesOutAndFails',
        'NavigationControllerTest.Reload',
        'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0',
        'SomeOtherTest.Foo', 'SomeOtherTest.SwitchTypes'
    ]
    self.assertEqual(sorted(test_list), sorted(cover_set))

    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_CRASH.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertTrue(parser.RunningTests())
    self.assertEqual(['HunspellTest.Crashes'], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())

    test_name = 'HunspellTest.Crashes'
    expected_log_lines = [
        'Did not complete.',
        'Potential test logs from crash until the end of test program:'
    ] + CRASH_MESSAGE
    self.assertEqual('\n'.join(['%s: ' % test_name] + expected_log_lines),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['UNKNOWN'], parser.TriesForTest(test_name))

    collection = parser.GetResultCollection()
    self.assertEqual(
        set(['HunspellTest.Crashes']), collection.unexpected_tests())
    for result in collection.test_results:
      covered = False
      if result.name == 'HunspellTest.Crashes':
        covered = True
        self.assertEqual('\n'.join(expected_log_lines), result.test_log)
        self.assertEqual(TestStatus.CRASH, result.status)
    self.assertTrue(covered)

  def testGTestLogParserSharding(self):
    # Same tests for log parsing with sharding_supervisor.
    parser = gtest_utils.GTestLogParser()
    test_data_shard = TEST_DATA_SHARD_0 + TEST_DATA_SHARD_1
    for line in test_data_shard.splitlines():
      parser.ProcessLine(line)
    parser.ProcessLine(TEST_DATA_SHARD_EXIT + '2')
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertFalse(parser.RunningTests())

    self.assertEqual(sorted(FAILURES), sorted(parser.FailedTests()))
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES),
        sorted(parser.FailedTests(include_fails=True)))
    self.assertEqual(
        sorted(FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_flaky=True)))
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(parser.FailedTests(include_fails=True, include_flaky=True)))

    self.assertEqual(10, parser.DisabledTests())
    self.assertEqual(2, parser.FlakyTests())

    test_name = 'NavigationControllerTest.Reload'
    self.assertEqual('\n'.join(['%s: ' % test_name, RELOAD_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = ('NavigationControllerTest/SpdyNetworkTransTest.Constructor/0')
    self.assertEqual('\n'.join(['%s: ' % test_name, SPDY_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = 'SomeOtherTest.SwitchTypes'
    self.assertEqual('\n'.join(['%s: ' % test_name, SWITCH_ERRORS]),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['FAILURE'], parser.TriesForTest(test_name))

    test_name = 'BadTest.TimesOut'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name, TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['TIMEOUT'], parser.TriesForTest(test_name))

    test_name = 'MoreBadTest.TimesOutAndFails'
    self.assertEqual(
        '\n'.join(['%s: ' % test_name, MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
        '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['TIMEOUT'], parser.TriesForTest(test_name))

    self.assertEqual(['SUCCESS'], parser.TriesForTest('SomeOtherTest.Foo'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(
        sorted(FAILURES + FAILS_FAILURES + FLAKY_FAILURES),
        sorted(collection.never_expected_tests()))

    self.assertEqual(len(collection.test_results), 12)

    # To know that each condition branch in for loop is covered.
    cover_set = set()
    for test_result in collection.test_results:
      name = test_result.name
      if name == 'NavigationControllerTest.Reload':
        cover_set.add(name)
        self.assertEqual('\n'.join([RELOAD_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(2, test_result.duration)

      if name == 'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0':
        cover_set.add(name)
        self.assertEqual('\n'.join([SPDY_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(2, test_result.duration)

      if name == 'SomeOtherTest.SwitchTypes':
        cover_set.add(name)
        self.assertEqual('\n'.join([SWITCH_ERRORS]), test_result.test_log)
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual(40, test_result.duration)

      if name == 'BadTest.TimesOut':
        cover_set.add(name)
        self.assertEqual('\n'.join([TIMEOUT_ERRORS, TIMEOUT_MESSAGE]),
                         test_result.test_log)
        self.assertEqual(TestStatus.ABORT, test_result.status)
        self.assertEqual(None, test_result.duration)

      if name == 'MoreBadTest.TimesOutAndFails':
        cover_set.add(name)
        self.assertEqual('\n'.join([MOREBAD_ERRORS, TIMEOUT_MESSAGE]),
                         test_result.test_log)
        self.assertEqual(TestStatus.ABORT, test_result.status)
        self.assertEqual(None, test_result.duration)

      if name == 'SomeOtherTest.Foo':
        cover_set.add(name)
        self.assertEqual('', test_result.test_log)
        self.assertEqual(TestStatus.PASS, test_result.status)

    test_list = [
        'BadTest.TimesOut', 'MoreBadTest.TimesOutAndFails',
        'NavigationControllerTest.Reload',
        'NavigationControllerTest/SpdyNetworkTransTest.Constructor/0',
        'SomeOtherTest.Foo', 'SomeOtherTest.SwitchTypes'
    ]
    self.assertEqual(sorted(test_list), sorted(cover_set))

    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_CRASH.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertTrue(parser.RunningTests())
    self.assertEqual(['HunspellTest.Crashes'], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())

    test_name = 'HunspellTest.Crashes'
    expected_log_lines = [
        'Did not complete.',
        'Potential test logs from crash until the end of test program:'
    ] + CRASH_MESSAGE
    self.assertEqual('\n'.join(['%s: ' % test_name] + expected_log_lines),
                     '\n'.join(parser.FailureDescription(test_name)))
    self.assertEqual(['UNKNOWN'], parser.TriesForTest(test_name))

    collection = parser.GetResultCollection()
    self.assertEqual(
        set(['HunspellTest.Crashes']), collection.unexpected_tests())
    for result in collection.test_results:
      covered = False
      if result.name == 'HunspellTest.Crashes':
        covered = True
        self.assertEqual('\n'.join(expected_log_lines), result.test_log)
        self.assertEqual(TestStatus.CRASH, result.status)
    self.assertTrue(covered)

  def testGTestLogParserMixedStdout(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_MIXED_STDOUT.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual([], parser.ParsingErrors())
    self.assertEqual(['Crash.Test'], parser.RunningTests())
    self.assertEqual(['TestFix.TestCase', 'Crash.Test'], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())
    self.assertEqual(['UNKNOWN'], parser.TriesForTest('Crash.Test'))
    self.assertEqual(['TIMEOUT'], parser.TriesForTest('TestFix.TestCase'))
    self.assertEqual(['SUCCESS'],
                     parser.TriesForTest(
                         'WebSocketHandshakeHandlerSpdy3Test.RequestResponse'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(
        sorted(['TestFix.TestCase', 'Crash.Test']),
        sorted(collection.never_expected_tests()))

    # To know that each condition branch in for loop is covered.
    cover_set = set()
    for test_result in collection.test_results:
      name = test_result.name
      if name == 'Crash.Test':
        cover_set.add(name)
        self.assertEqual(TestStatus.CRASH, test_result.status)

      if name == 'TestFix.TestCase':
        cover_set.add(name)
        self.assertEqual(TestStatus.ABORT, test_result.status)

      if name == 'WebSocketHandshakeHandlerSpdy3Test.RequestResponse':
        cover_set.add(name)
        self.assertEqual(TestStatus.PASS, test_result.status)
        self.assertEqual(1, test_result.duration)
    test_list = [
        'Crash.Test', 'TestFix.TestCase',
        'WebSocketHandshakeHandlerSpdy3Test.RequestResponse'
    ]
    self.assertEqual(test_list, sorted(cover_set))

  def testGtestLogParserSkipped(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_SKIPPED.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual([], parser.ParsingErrors())
    self.assertEqual([], parser.RunningTests())
    self.assertEqual([], parser.FailedTests())
    self.assertEqual(['ProcessReaderLinux.AbortMessage'], parser.SkippedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())
    self.assertEqual(['SKIPPED'],
                     parser.TriesForTest('ProcessReaderLinux.AbortMessage'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(['ProcessReaderLinux.AbortMessage'],
                     sorted(
                         collection.tests_by_expression(lambda tr: tr.status ==
                                                        TestStatus.SKIP)))
    self.assertEqual([], sorted(collection.unexpected_tests()))

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'ProcessReaderLinux.AbortMessage':
        covered = True
        self.assertEqual(TestStatus.SKIP, test_result.status)
    self.assertTrue(covered)

  def testRunTestCaseFail(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_RUN_TEST_CASE_FAIL.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertEqual([], parser.RunningTests())
    self.assertEqual(['SUIDSandboxUITest.testSUIDSandboxEnabled'],
                     parser.FailedTests())
    self.assertEqual(
        ['SUIDSandboxUITest.testSUIDSandboxEnabled: '],
        parser.FailureDescription('SUIDSandboxUITest.testSUIDSandboxEnabled'))
    self.assertEqual(
        ['FAILURE'],
        parser.TriesForTest('SUIDSandboxUITest.testSUIDSandboxEnabled'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(['SUIDSandboxUITest.testSUIDSandboxEnabled'],
                     sorted(collection.failed_tests()))

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'SUIDSandboxUITest.testSUIDSandboxEnabled':
        covered = True
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual('', test_result.test_log)
        self.assertEqual(771, test_result.duration)

    self.assertTrue(covered)

  def testRunTestCaseTimeout(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_RUN_TEST_CASE_TIMEOUT.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertEqual([], parser.RunningTests())
    self.assertEqual(['SUIDSandboxUITest.testSUIDSandboxEnabled'],
                     parser.FailedTests())
    self.assertEqual(
        ['SUIDSandboxUITest.testSUIDSandboxEnabled: ', '(junk)'],
        parser.FailureDescription('SUIDSandboxUITest.testSUIDSandboxEnabled'))
    self.assertEqual(
        ['TIMEOUT'],
        parser.TriesForTest('SUIDSandboxUITest.testSUIDSandboxEnabled'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(['SUIDSandboxUITest.testSUIDSandboxEnabled'],
                     sorted(collection.never_expected_tests()))

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'SUIDSandboxUITest.testSUIDSandboxEnabled':
        covered = True
        self.assertEqual(TestStatus.ABORT, test_result.status)
        self.assertEqual('(junk)', test_result.test_log)
        self.assertEqual(None, test_result.duration)

    self.assertTrue(covered)

  def testRunTestCaseParseSwarm(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_SWARM_TEST_FAIL.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual(0, len(parser.ParsingErrors()))
    self.assertEqual([], parser.RunningTests())
    self.assertEqual(['PickleTest.EncodeDecode'], parser.FailedTests())
    log_lines = [
        'PickleTest.EncodeDecode: ',
        '../../base/pickle_unittest.cc:69: Failure',
        'Value of: false',
        '  Actual: false',
        'Expected: true',
    ]
    self.assertEqual(log_lines,
                     parser.FailureDescription('PickleTest.EncodeDecode'))
    self.assertEqual(['FAILURE'],
                     parser.TriesForTest('PickleTest.EncodeDecode'))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(['PickleTest.EncodeDecode'],
                     sorted(collection.never_expected_tests()))

    covered_count = 0
    for test_result in collection.test_results:
      if test_result.name == 'PickleTest.EncodeDecode':
        covered_count += 1
        self.assertEqual(TestStatus.FAIL, test_result.status)
        self.assertEqual('\n'.join(log_lines[1:]), test_result.test_log)

    self.assertEqual(3, covered_count)

  def testNestedGtests(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_NESTED_RUNS.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()
    self.assertEqual(['Foo.Bar'], parser.FailedTests(True, True))

    # Same unit tests (when applicable) using ResultCollection
    collection = parser.GetResultCollection()
    self.assertEqual(['Foo.Bar'], sorted(collection.never_expected_tests()))

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'Foo.Bar':
        covered = True
        self.assertEqual(TestStatus.ABORT, test_result.status)

    self.assertTrue(covered)

  def testParseCompiledFileLocation(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_COMPILED_FILE.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()
    self.assertEqual(COMPILED_FILE_PATH, parser.compiled_tests_file_path)

    # Just a hack so that we can point the compiled file path to right place
    parser.compiled_tests_file_path = os.path.join(
        os.getcwd(), parser.compiled_tests_file_path)
    parser.ParseAndPopulateTestResultLocations(TEST_REPO, False)
    collection = parser.GetResultCollection()

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'test1.test_method1':
        covered = True
        self.assertEqual(TestStatus.PASS, test_result.status)
        test_loc = {'repo': TEST_REPO, 'fileName': '//random/path/test1.cc'}
        self.assertEqual(test_loc, test_result.test_loc)
    self.assertTrue(covered)

    disabled_tests_covered = True
    # disabled tests shouldn't be included in the result
    # because output_disabled_tests is false
    for test_result in collection.test_results:
      if test_result.name == 'test2.DISABLED_test_method1':
        covered = False
    self.assertTrue(disabled_tests_covered)

  def testParseCompiledFileLocationWithCustomPath(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_COMPILED_FILE.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()
    self.assertEqual(COMPILED_FILE_PATH, parser.compiled_tests_file_path)

    # Just a hack so that we can point the compiled file path to right place
    parser.compiled_tests_file_path = os.path.join(
        os.getcwd(), parser.compiled_tests_file_path)

    host_file_path = parser.compiled_tests_file_path
    # setting it to None to make sure overriding the path arg really works
    parser.compiled_tests_file_path = None

    parser.ParseAndPopulateTestResultLocations(TEST_REPO, False, host_file_path)
    collection = parser.GetResultCollection()

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'test1.test_method1':
        covered = True
        self.assertEqual(TestStatus.PASS, test_result.status)
        test_loc = {'repo': TEST_REPO, 'fileName': '//random/path/test1.cc'}
        self.assertEqual(test_loc, test_result.test_loc)
    self.assertTrue(covered)

    disabled_tests_covered = True
    # disabled tests shouldn't be included in the result
    # because output_disabled_tests is false
    for test_result in collection.test_results:
      if test_result.name == 'test2.DISABLED_test_method1':
        covered = False
    self.assertTrue(disabled_tests_covered)

  def testParseCompiledFileLocationOutputDisabledTests(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_COMPILED_FILE.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()
    self.assertEqual(COMPILED_FILE_PATH, parser.compiled_tests_file_path)

    # Just a hack so that we can point the compiled file path to right place
    parser.compiled_tests_file_path = os.path.join(
        os.getcwd(), parser.compiled_tests_file_path)
    parser.ParseAndPopulateTestResultLocations(TEST_REPO, True)
    collection = parser.GetResultCollection()

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'test1.test_method1':
        covered = True
        self.assertEqual(TestStatus.PASS, test_result.status)
        test_loc = {'repo': TEST_REPO, 'fileName': '//random/path/test1.cc'}
        self.assertEqual(test_loc, test_result.test_loc)

    covered = False
    for test_result in collection.test_results:
      if test_result.name == 'test2.DISABLED_test_method1':
        covered = True
        self.assertEqual(TestStatus.SKIP, test_result.status)
        test_loc = {'repo': TEST_REPO, 'fileName': '//random/path/test2.cc'}
        self.assertEqual(test_loc, test_result.test_loc)
    self.assertTrue(covered)

  def testGTestLogLauncherSpawn(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_LAUNCHER_SPAWN.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual([], parser.ParsingErrors())
    self.assertEqual([], parser.RunningTests())
    self.assertEqual([], parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())
    self.assertEqual(
        ['TestFix.TestCase', 'TextPaintTimingDetectorTest.LargestTextPaint'],
        parser.PassedTests())
    collection = parser.GetResultCollection()
    self.assertFalse(collection.crashed)

  def testGTestLogLauncherSpawnCrash(self):
    parser = gtest_utils.GTestLogParser()
    for line in TEST_DATA_LAUNCHER_SPAWN_CRASH.splitlines():
      parser.ProcessLine(line)
    parser.Finalize()

    self.assertEqual([], parser.ParsingErrors())
    self.assertEqual(['LoggingTest.CrashedTest'], parser.RunningTests())
    self.assertEqual(['LoggingTest.FailedTest', 'LoggingTest.CrashedTest'],
                     parser.FailedTests())
    self.assertEqual(0, parser.DisabledTests())
    self.assertEqual(0, parser.FlakyTests())
    self.assertEqual(['LoggingTest.StreamingWstringFindsCorrectOperator'],
                     parser.PassedTests())
    collection = parser.GetResultCollection()
    self.assertTrue(collection.crashed)


if __name__ == '__main__':
  unittest.main()
