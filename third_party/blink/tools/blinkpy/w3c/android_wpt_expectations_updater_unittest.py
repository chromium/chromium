# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.web_tests.port.android import (
    PRODUCTS_TO_EXPECTATION_FILE_PATHS, ANDROID_DISABLED_TESTS,
    ANDROID_WEBLAYER, ANDROID_WEBVIEW, CHROME_ANDROID,
    PRODUCTS_TO_STEPNAMES)
from blinkpy.w3c.android_wpt_expectations_updater import (
    AndroidWPTExpectationsUpdater)

WEBLAYER_WPT_STEP = PRODUCTS_TO_STEPNAMES[ANDROID_WEBLAYER]
WEBVIEW_WPT_STEP = PRODUCTS_TO_STEPNAMES[ANDROID_WEBVIEW]
CHROME_ANDROID_WPT_STEP = PRODUCTS_TO_STEPNAMES[CHROME_ANDROID]

class AndroidWPTExpectationsUpdaterTest(LoggingTestCase):
    _raw_baseline_expectations = (
        '# results: [ Failure Crash Timeout]\n'
        '\n'
        'crbug.com/1111111 external/wpt/new1.html [ Failure Timeout ]\n')

    # baseline for external/wpt/new2.html
    _raw_expected_text = (
        'This is a testharness.js-based test.\n'
        'FAIL a failed subtest\n'
        'Harness: the test ran to completion.\n')

    _raw_android_never_fix_tests = (
        '# tags: [ android-weblayer android-webview chrome-android ]\n'
        '# results: [ Skip ]\n'
        '\n'
        '# Add untriaged disabled tests in this block\n'
        'crbug.com/1050754 [ android-webview ] external/wpt/disabled.html [ Skip ]\n')

    def _setup_host(self, raw_android_expectations):
        """Returns a mock host with fake values set up for testing."""
        self.set_logging_level(logging.DEBUG)
        host = MockHost()
        host.port_factory = MockPortFactory(host)
        host.executive._output = ''

        # Set up a fake list of try builders.
        host.builders = BuilderList({
            'MOCK Android Weblayer - Pie': {
                'port_name': 'test-android-pie',
                'specifiers': ['Precise', 'Release',
                               'anDroid', 'android_Weblayer'],
                'is_try_builder': True,
            },
        })

        host.filesystem.write_text_file(
            host.port_factory.get().web_tests_dir() + '/external/' +
            BASE_MANIFEST_NAME,
            json.dumps({
                'items': {
                    'testharness': {
                        'foo1.html': ['abcdef123', [None, {}]],
                        'foo2.html': ['abcdef123', [None, {}]],
                        'bar.html': ['abcdef123', [None, {}]],
                    },
                },
            }))

        # Write dummy expectations
        path = host.port_factory.get().web_tests_dir() + '/TestExpectations'
        host.filesystem.write_text_file(
            path, self._raw_baseline_expectations)
        path = host.port_factory.get().web_tests_dir() + '/platform/generic/external/wpt/new2-expected.txt'
        host.filesystem.write_text_file(
            path, self._raw_expected_text)

        for path in PRODUCTS_TO_EXPECTATION_FILE_PATHS.values():
            host.filesystem.write_text_file(
                path, raw_android_expectations)

        host.filesystem.write_text_file(
            ANDROID_DISABLED_TESTS, self._raw_android_never_fix_tests)
        return host

    def testUpdateTestExpectationsForWeblayer(self):
        raw_android_expectations = (
            '# results: [ Failure Crash Timeout]\n'
            '\n'
            'crbug.com/1000754 external/wpt/foo.html [ Failure ]\n'
            '\n'
            '# Add untriaged failures in this block\n'
            'crbug.com/1050754 external/wpt/bar.html [ Failure ]\n'
            '\n'
            '# This comment will not be deleted\n')
        host = self._setup_host(raw_android_expectations)
        # Add results for Weblayer
        # new1.html is covered by default expectations
        # new2.html is covered by baseline
        # new3.html is a new test. We should create WebLayer expectation for it.
        result = """
            [{
                "testId": "ninja://weblayer/shell/android:weblayer_shell_wpt/external/wpt/new1.html",
                "variant": {
                    "def": {
                        "builder": "android-weblayer-pie-x86-wpt-fyi-rel",
                        "os": "Ubuntu-16.04",
                        "test_suite": "weblayer_shell_wpt"
                    }
                },
                "status": "FAIL"
            },
            {
                "testId": "ninja://weblayer/shell/android:weblayer_shell_wpt/external/wpt/new2.html",
                "variant": {
                    "def": {
                        "builder": "android-weblayer-pie-x86-wpt-fyi-rel",
                        "os": "Ubuntu-16.04",
                        "test_suite": "weblayer_shell_wpt"
                    }
                },
                "status": "FAIL"
            },
            {
                "testId": "ninja://weblayer/shell/android:weblayer_shell_wpt/external/wpt/new3.html",
                "variant": {
                    "def": {
                        "builder": "android-weblayer-pie-x86-wpt-fyi-rel",
                        "os": "Ubuntu-16.04",
                        "test_suite": "weblayer_shell_wpt"
                    }
                },
                "status": "CRASH"
            },
            {
                "testId": "ninja://weblayer/shell/android:weblayer_shell_wpt/external/wpt/new3.html",
                "variant": {
                    "def": {
                        "builder": "android-weblayer-pie-x86-wpt-fyi-rel",
                        "os": "Ubuntu-16.04",
                        "test_suite": "weblayer_shell_wpt"
                    }
                },
                "status": "FAIL"
            }]"""
        host.results_fetcher.set_results_to_resultdb(
            Build('MOCK Android Weblayer - Pie', 123, '123'),
            json.loads(result) * 3)

        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv', '--android-product', ANDROID_WEBLAYER,
                   '--include-unexpected-pass'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123, '123'):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Get new expectations
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBLAYER])
        _new_expectations = (
            '# results: [ Failure Crash Timeout]\n'
            '\n'
            'crbug.com/1000754 external/wpt/foo.html [ Failure ]\n'
            '\n'
            '# Add untriaged failures in this block\n'
            'crbug.com/1050754 external/wpt/bar.html [ Failure ]\n'
            'crbug.com/1050754 external/wpt/new3.html [ Crash Failure ]\n'
            '\n'
            '# This comment will not be deleted\n')
        self.assertEqual(content, _new_expectations)

        # Check that ANDROID_DISABLED_TESTS expectation files were not changed
        self.assertEqual(
            self._raw_android_never_fix_tests,
            host.filesystem.read_text_file(ANDROID_DISABLED_TESTS))

    def testCleanupAndUpdateTestExpectationsForAll(self):
        # Full integration test for expectations cleanup and update
        # using builder results.
        raw_android_expectations = (
            '# results: [ Failure Crash Timeout]\n'
            '\n'
            'crbug.com/1000754 external/wpt/foo1.html [ Failure ]\n'
            'crbug.com/1000754 external/wpt/foo2.html [ Failure ]\n'
            'crbug.com/1000754 external/wpt/bar.html [ Failure ]\n'
            '\n'
            '# Add untriaged failures in this block\n'
            '\n'
            '# This comment will not be deleted\n')
        host = self._setup_host(raw_android_expectations)
        # Add results for Weblayer
        result = """
            {
                "testId": "ninja://weblayer/shell/android:weblayer_shell_wpt/external/wpt/bar.html",
                "variant": {
                    "def": {
                        "builder": "android-weblayer-pie-x86-wpt-fyi-rel",
                        "os": "Ubuntu-16.04",
                        "test_suite": "weblayer_shell_wpt"
                    }
                },
                "status": "CRASH"
            }"""
        host.results_fetcher.set_results_to_resultdb(
            Build('MOCK Android Weblayer - Pie', 123, '123'),
            [json.loads(result)] * 3)
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv',
                   '--clean-up-test-expectations',
                   '--clean-up-affected-tests-only',
                   '--include-unexpected-pass',
                   '--android-product', ANDROID_WEBLAYER])

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return 'external/wpt/foo2.html'
            if '--diff-filter=R' in cmd:
                return 'C\texternal/wpt/foo1.html\texternal/wpt/foo3.html'
            if '--diff-filter=M' in cmd:
                return 'external/wpt/bar.html'
            return ''

        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123, '123'):
            TryJobStatus('COMPLETED', 'FAILURE')})

        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path

        # Run command
        updater.run()

        # Check expectations for weblayer
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBLAYER])
        _new_expectations = (
            '# results: [ Failure Crash Timeout]\n'
            '\n'
            'crbug.com/1000754 external/wpt/foo3.html [ Failure ]\n'
            '\n'
            '# Add untriaged failures in this block\n'
            'crbug.com/1050754 external/wpt/bar.html [ Crash ]\n'
            '\n'
            '# This comment will not be deleted\n')
        self.assertEqual(content, _new_expectations)
