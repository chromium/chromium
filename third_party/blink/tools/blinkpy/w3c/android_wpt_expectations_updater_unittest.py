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

    _raw_android_expectations = (
        '# results: [ Failure Crash Timeout]\n'
        '\n'
        '# Add untriaged failures in this block\n'
        'external/wpt/abc.html [ Failure ]\n'
        'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
        'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
        'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
        'external/wpt/www.html [ Crash Failure ]\n'
        'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
        'external/wpt/dog.html [ Crash Timeout ]\n'
        'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
        'external/wpt/unexpected_pass.html [ Failure ]\n'
        '\n'
        '# This comment will not be deleted\n'
        'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n')

    _raw_android_never_fix_tests = (
        '# tags: [ android-weblayer android-webview chrome-android ]\n'
        '# results: [ Skip ]\n'
        '\n'
        '# Add untriaged disabled tests in this block\n'
        'crbug.com/1050754 [ android-webview ] external/wpt/disabled.html [ Skip ]\n')

    def _setup_host(self):
        """Returns a mock host with fake values set up for testing."""
        self.set_logging_level(logging.DEBUG)
        host = MockHost()
        host.port_factory = MockPortFactory(host)
        host.executive._output = ''

        # Set up a fake list of try builders.
        host.builders = BuilderList({
            'MOCK Try Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Android Weblayer - Pie': {
                'port_name': 'test-android-pie',
                'specifiers': ['Precise', 'Release',
                               'anDroid', 'android_Weblayer'],
                'is_try_builder': True,
            },
            'MOCK Android Pie': {
                'port_name': 'test-android-pie',
                'specifiers': ['Precise', 'Release', 'anDroid',
                               'Android_Webview', 'Chrome_Android'],
                'is_try_builder': True,
            },
        })
        host.filesystem.write_text_file(
            host.port_factory.get().web_tests_dir() + '/external/' +
            BASE_MANIFEST_NAME,
            json.dumps({
                'items': {
                    'testharness': {
                        'ghi.html': ['abcdef123', [None, {}]],
                        'van.html': ['abcdef123', [None, {}]],
                    },
                },
            }))

        # Write dummy expectations
        for path in PRODUCTS_TO_EXPECTATION_FILE_PATHS.values():
            host.filesystem.write_text_file(
                path, self._raw_android_expectations)

        host.filesystem.write_text_file(
            ANDROID_DISABLED_TESTS, self._raw_android_never_fix_tests)
        return host

    def testUpdateTestExpectationsForWebview(self):
        host = self._setup_host()
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'unexpected_pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                    'dog.html': {
                        'expected': 'SKIP',
                        'actual': 'SKIP',
                        'is_unexpected': True,
                    },
                },
            }, step_name=WEBVIEW_WPT_STEP + ' (with patch)'),
            step_name=WEBVIEW_WPT_STEP + ' (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv',  '--android-product', ANDROID_WEBVIEW,
                   '--clean-up-test-expectations',
                   '--clean-up-affected-tests-only',
                   '--include-unexpected-pass'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Get new expectations
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBVIEW])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/unexpected_pass.html [ Failure Pass ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        neverfix_content = host.filesystem.read_text_file(
            ANDROID_DISABLED_TESTS)
        self.assertEqual(
            neverfix_content,
            ('# tags: [ android-weblayer android-webview chrome-android ]\n'
             '# results: [ Skip ]\n'
             '\n'
             '# Add untriaged disabled tests in this block\n'
             'crbug.com/1050754 [ android-webview ] external/wpt/disabled.html [ Skip ]\n'
             'crbug.com/1050754 [ android-webview ] external/wpt/dog.html [ Skip ]\n'))
        # check that chrome android's expectation file was not modified
        # since the same bot is used to update chrome android & webview
        # expectations
        self.assertEqual(
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS[CHROME_ANDROID]),
            self._raw_android_expectations)
        # Check logs
        logs = ''.join(self.logMessages()).lower()
        self.assertNotIn(WEBLAYER_WPT_STEP, logs)
        self.assertNotIn(CHROME_ANDROID_WPT_STEP, logs)
        # Check that weblayer and chrome expectation files were not changed
        self.assertEqual(
            self._raw_android_expectations,
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS[CHROME_ANDROID]))
        self.assertEqual(
            self._raw_android_expectations,
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBLAYER]))

    def testUpdateTestExpectationsForWeblayer(self):
        host = self._setup_host()
        host.results_fetcher.set_results(
            Build('MOCK Android Weblayer - Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'unexpected_pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True,
                    },
                    'new.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH FAIL',
                        'is_unexpected': True,
                    },
                },
            }, step_name=WEBLAYER_WPT_STEP + ' (with patch)'),
            step_name=WEBLAYER_WPT_STEP + ' (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv', '--android-product', ANDROID_WEBLAYER,
                   '--clean-up-test-expectations',
                   '--clean-up-affected-tests-only',
                   '--include-unexpected-pass'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Get new expectations
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBLAYER])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/new.html [ Failure Crash ]\n'
             'crbug.com/1050754 external/wpt/unexpected_pass.html [ Failure Pass ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check logs
        logs = ''.join(self.logMessages()).lower()
        self.assertNotIn(WEBVIEW_WPT_STEP, logs)
        self.assertNotIn(CHROME_ANDROID_WPT_STEP, logs)
        # Check that webview and chrome expectation files were not changed
        self.assertEqual(
            self._raw_android_expectations,
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS[CHROME_ANDROID]))
        self.assertEqual(
            self._raw_android_expectations,
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBVIEW]))
        self.assertEqual(
            self._raw_android_never_fix_tests,
            host.filesystem.read_text_file(ANDROID_DISABLED_TESTS))

    def testCleanupAndUpdateTestExpectationsForAll(self):
        # Full integration test for expectations cleanup and update
        # using builder results.
        host = self._setup_host()
        # Add results for Weblayer
        host.results_fetcher.set_results(
            Build('MOCK Android Weblayer - Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'weblayer_only.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH FAIL',
                        'is_unexpected': True,
                    },
                    'disabled_weblayer_only.html': {
                        'expected': 'SKIP',
                        'actual': 'SKIP',
                        'is_unexpected': True,
                    },
                    'unexpected_pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                },
            }, step_name=WEBLAYER_WPT_STEP + ' (with patch)'),
            step_name=WEBLAYER_WPT_STEP + ' (with patch)')
        # Add Results for Webview
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 101),
            WebTestResults({
                'tests': {
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH FAIL TIMEOUT',
                        'is_unexpected': True,
                    },
                    'webview_only.html': {
                        'expected': 'PASS',
                        'actual': 'TIMEOUT',
                        'is_unexpected': True,
                    },
                    'disabled.html': {
                        'expected': 'SKIP',
                        'actual': 'SKIP',
                    },
                    'unexpected_pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                },
            }, step_name=WEBVIEW_WPT_STEP + ' (with patch)'),
            step_name=WEBVIEW_WPT_STEP + ' (with patch)')
        # Add Results for Chrome
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 101),
            WebTestResults({
                'tests': {
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'chrome_only.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'disabled.html': {
                        'expected': 'SKIP',
                        'actual': 'SKIP',
                        'is_unexpected': True,
                    },
                    'unexpected_pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                },
            }, step_name=CHROME_ANDROID_WPT_STEP + ' (with patch)'),
            step_name=CHROME_ANDROID_WPT_STEP + ' (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv',
                   '--clean-up-test-expectations',
                   '--clean-up-affected-tests-only',
                   '--include-unexpected-pass',
                   '--android-product', ANDROID_WEBLAYER,
                   '--android-product', CHROME_ANDROID,
                   '--android-product', ANDROID_WEBVIEW])

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return 'external/wpt/ghi.html'
            if '--diff-filter=R' in cmd:
                return 'C external/wpt/van.html external/wpt/wagon.html'
            return ''

        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Android Pie', 101):
            TryJobStatus('COMPLETED', 'FAILURE')})

        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path

        # Run command
        updater.run()
        # Check expectations for weblayer
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBLAYER])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/unexpected_pass.html [ Failure Pass ]\n'
             'crbug.com/6789043 external/wpt/wagon.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/weblayer_only.html [ Failure Crash ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check expectations for webview
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[ANDROID_WEBVIEW])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'external/wpt/abc.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/unexpected_pass.html [ Failure Pass ]\n'
             'crbug.com/6789043 external/wpt/wagon.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/webview_only.html [ Timeout ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check expectations chrome
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS[CHROME_ANDROID])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'external/wpt/abc.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/chrome_only.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/unexpected_pass.html [ Failure Pass ]\n'
             'crbug.com/6789043 external/wpt/wagon.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check disabled test file
        neverfix_content = host.filesystem.read_text_file(ANDROID_DISABLED_TESTS)
        self.assertEqual(
            neverfix_content,
            ('# tags: [ android-weblayer android-webview chrome-android ]\n'
             '# results: [ Skip ]\n'
             '\n'
             '# Add untriaged disabled tests in this block\n'
             'crbug.com/1050754 [ android-webview ] external/wpt/disabled.html [ Skip ]\n'
             'crbug.com/1050754 [ chrome-android ] external/wpt/disabled.html [ Skip ]\n'
             'crbug.com/1050754 [ android-weblayer ] external/wpt/disabled_weblayer_only.html [ Skip ]\n'))
