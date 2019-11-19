# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import textwrap

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.commands.rebaseline import TestBaselineSet
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL
from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase
from blinkpy.web_tests.builder_list import BuilderList


class RebaselineCLTest(BaseTestCase, LoggingTestCase):

    command_constructor = RebaselineCL

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)

        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000): TryJobStatus('COMPLETED', 'FAILURE'),
        }

        self.command.git_cl = MockGitCL(self.tool, builds)

        git = MockGit(filesystem=self.tool.filesystem, executive=self.tool.executive)
        git.changed_files = lambda **_: [
            RELATIVE_WEB_TESTS + 'one/text-fail.html',
            RELATIVE_WEB_TESTS + 'one/flaky-fail.html',
        ]
        self.tool.git = lambda: git

        self.tool.builders = BuilderList({
            'MOCK Try Win': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
        })
        web_test_results = WebTestResults({
            'tests': {
                'one': {
                    'crash.html': {
                        'expected': 'PASS', 'actual': 'CRASH', 'is_unexpected': True,
                        'artifacts': {'crash_log': ['crash.log']}},
                    'expected-fail.html': {
                        'expected': 'FAIL', 'actual': 'FAIL',
                        'artifacts': {'expected_text': ['expected-fail-expected.txt'],
                                      'actual_text': ['expected-fail-actual.txt']}},
                    'flaky-fail.html': {
                        'expected': 'PASS', 'actual': 'PASS FAIL', 'is_unexpected': True,
                        'artifacts': {'expected_audio': ['flaky-fail-expected.wav'],
                                      'actual_audio': ['flaky-fail-actual.wav']}},
                    'missing.html': {
                        'expected': 'PASS', 'actual': 'FAIL', 'is_unexpected': True,
                        'artifacts': {'actual_image': ['missing-actual.png']}, 'is_missing_image': True},
                    'slow-fail.html': {
                        'expected': 'SLOW', 'actual': 'FAIL', 'is_unexpected': True,
                        'artifacts': {'actual_text': ['slow-fail-actual.txt'],
                                      'expected_text': ['slow-fail-expected.txt']}},
                    'text-fail.html': {
                        'expected': 'PASS', 'actual': 'FAIL', 'is_unexpected': True,
                        'artifacts': {'actual_text': ['text-fail-actual.txt'],
                                      'expected_text': ['text-fail-expected.txt']}},
                    'unexpected-pass.html': {'expected': 'FAIL', 'actual': 'PASS', 'is_unexpected': True},
                },
                'two': {
                    'image-fail.html': {
                        'expected': 'PASS', 'actual': 'FAIL', 'is_unexpected': True,
                        'artifacts': {'actual_image': ['image-fail-actual.png'],
                                      'expected_image': ['image-fail-expected.png']}}},
            },
        })

        for build in builds:
            self.tool.results_fetcher.set_results(build, web_test_results)
            self.tool.results_fetcher.set_retry_sumary_json(build, json.dumps({
                'failures': [
                    'one/flaky-fail.html',
                    'one/missing.html',
                    'one/slow-fail.html',
                    'one/text-fail.html',
                    'two/image-fail.html',
                ],
                'ignored': [],
            }))

        # Write to the mock filesystem so that these tests are considered to exist.
        tests = [
            'one/flaky-fail.html',
            'one/missing.html',
            'one/slow-fail.html',
            'one/text-fail.html',
            'two/image-fail.html',
        ]
        for test in tests:
            path = self.mac_port.host.filesystem.join(
                self.mac_port.web_tests_dir(), test)
            self._write(path, 'contents')

        self.mac_port.host.filesystem.write_text_file(
            '/test.checkout/web_tests/external/wpt/MANIFEST.json', '{}')

    def tearDown(self):
        BaseTestCase.tearDown(self)
        LoggingTestCase.tearDown(self)

    @staticmethod
    def command_options(**kwargs):
        options = {
            'dry_run': False,
            'only_changed_tests': False,
            'trigger_jobs': True,
            'fill_missing': None,
            'optimize': True,
            'results_directory': None,
            'test_name_file': None,
            'verbose': False,
            'builders': [],
            'patchset': None,
        }
        options.update(kwargs)
        return optparse.Values(dict(**options))

    def test_execute_basic(self):
        # By default, with no arguments or options, rebaseline-cl rebaselines
        # all of the tests that unexpectedly failed.
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_execute_with_test_name_file(self):
        fs = self.mac_port.host.filesystem
        test_name_file = fs.mktemp()
        fs.write_text_file(test_name_file, textwrap.dedent('''
            one/flaky-fail.html
              one/missing.html
            # one/slow-fail.html
            #

            one/text-fail.html
                two/image-fail.html   '''))
        exit_code = self.command.execute(
            self.command_options(test_name_file=test_name_file), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Reading list of tests to rebaseline from %s\n' % test_name_file,
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_execute_with_no_issue_number_aborts(self):
        # If the user hasn't uploaded a CL, an error message is printed.
        self.command.git_cl = MockGitCL(self.tool, issue_number='None')
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog(['ERROR: No issue number for current branch.\n'])

    def test_execute_with_unstaged_baselines_aborts(self):
        git = self.tool.git()
        git.unstaged_changes = lambda: {
            RELATIVE_WEB_TESTS + 'my-test-expected.txt': '?'
        }
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'ERROR: Aborting: there are unstaged baselines:\n',
            'ERROR:   /mock-checkout/' + RELATIVE_WEB_TESTS +
            'my-test-expected.txt\n',
        ])

    def test_execute_no_try_jobs_started_triggers_jobs(self):
        # If there are no try jobs started yet, by default the tool will
        # trigger new try jobs.
        self.command.git_cl = MockGitCL(self.tool, {})
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: No finished try jobs.\n',
            'INFO: Triggering try jobs:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Mac\n',
            'INFO:   MOCK Try Win\n',
            'INFO: Once all pending try jobs have finished, please re-run\n'
            'blink_tool.py rebaseline-cl to fetch new baselines.\n'
        ])

    def test_execute_no_try_jobs_started_and_no_trigger_jobs(self):
        # If there are no try jobs started yet and --no-trigger-jobs is passed,
        # then we just abort immediately.
        self.command.git_cl = MockGitCL(self.tool, {})
        exit_code = self.command.execute(
            self.command_options(trigger_jobs=False), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: No finished try jobs.\n',
            'INFO: Aborted: no try jobs and --no-trigger-jobs passed.\n',
        ])

    def test_execute_one_missing_build(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished try jobs:\n',
            'INFO:   MOCK Try Mac\n',
            'INFO:   MOCK Try Win\n',
            'INFO: Triggering try jobs:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Once all pending try jobs have finished, please re-run\n'
            'blink_tool.py rebaseline-cl to fetch new baselines.\n',
        ])

    def test_execute_with_unfinished_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('STARTED'),
            Build('MOCK Try Linux', 6000): TryJobStatus('SCHEDULED'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished try jobs:\n',
            'INFO:   MOCK Try Win\n',
            'INFO: Scheduled or started try jobs:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Mac\n',
            'INFO: There are some builders with no results:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Mac\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_with_canceled_job(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000): TryJobStatus('COMPLETED', 'CANCELED'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: There are some builders with no results:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_with_passing_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('MOCK Try Linux', 6000): TryJobStatus('COMPLETED', 'SUCCESS'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n'
        ])

    def test_execute_with_no_trigger_jobs_option(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(
            self.command_options(trigger_jobs=False), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished try jobs:\n',
            'INFO:   MOCK Try Mac\n',
            'INFO:   MOCK Try Win\n',
            'INFO: There are some builders with no results:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_with_only_changed_tests_option(self):
        # When --only-changed-tests is passed, the tool only rebaselines tests
        # that were modified in the CL.
        exit_code = self.command.execute(
            self.command_options(only_changed_tests=True), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])

    def test_execute_with_test_that_fails_on_retry(self):
        # In this example, one test failed both with and without the patch
        # in the try job, so it is not rebaselined.
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000): TryJobStatus('COMPLETED', 'FAILURE'),
        }
        for build in builds:
            self.tool.results_fetcher.set_retry_sumary_json(build, json.dumps({
                'failures': ['one/text-fail.html'],
                'ignored': ['two/image-fail.html'],
            }))
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])

    def test_execute_with_no_retry_summary_downloaded(self):
        # In this example, the retry summary could not be downloaded, so
        # a warning is printed and all tests are rebaselined.
        self.tool.results_fetcher.set_retry_sumary_json(
            Build('MOCK Try Win', 5000), None)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'WARNING: No retry summary available for "MOCK Try Win".\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_rebaseline_command_invocations(self):
        """Tests the list of commands that are called for rebaselining."""
        # First write test contents to the mock filesystem so that
        # one/flaky-fail.html is considered a real test to rebaseline.
        port = self.tool.port_factory.get('test-win-win7')
        path = port.host.filesystem.join(
            port.web_tests_dir(), 'one/flaky-fail.html')
        self._write(path, 'contents')
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add(
            'one/flaky-fail.html', Build('MOCK Try Win', 5000))
        self.command.rebaseline(self.command_options(), test_baseline_set)
        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--test', 'one/flaky-fail.html',
                    '--suffixes', 'wav',
                    '--port-name', 'test-win-win7',
                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--test', 'one/flaky-fail.html',
                    '--suffixes', 'wav',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Try Win',
                    '--build-number', '5000',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]],
                [[
                    'python', 'echo', 'optimize-baselines',
                    '--no-manifest-update',
                    '--suffixes', 'wav',
                    'one/flaky-fail.html',
                ]]
            ])

    def test_trigger_try_jobs(self):
        # The trigger_try_jobs method just uses git cl to trigger jobs for
        # the given builders.
        self.command.trigger_try_jobs(['MOCK Try Linux', 'MOCK Try Win'])
        self.assertEqual(
            self.command.git_cl.calls,
            [['git', 'cl', 'try', '-B', 'luci.chromium.try',
              '-b', 'MOCK Try Linux', '-b', 'MOCK Try Win']])
        self.assertLog([
            'INFO: Triggering try jobs:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Win\n',
            'INFO: Once all pending try jobs have finished, please re-run\n'
            'blink_tool.py rebaseline-cl to fetch new baselines.\n',
        ])

    def test_execute_missing_results_with_no_fill_missing_prompts(self):
        self.tool.results_fetcher.set_results(Build('MOCK Try Win', 5000), None)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Failed to fetch results for "MOCK Try Win".\n',
            ('INFO: Results URL: https://test-results.appspot.com/data/layout_results/'
             'MOCK_Try_Win/5000/webkit_layout_tests%20%28with%20patch%29/layout-test-results/results.html\n'),
            'INFO: There are some builders with no results:\n',
            'INFO:   MOCK Try Win\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_missing_results_with_fill_missing_continues(self):
        self.tool.results_fetcher.set_results(Build('MOCK Try Win', 5000), None)
        exit_code = self.command.execute(
            self.command_options(fill_missing=True),
            ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Failed to fetch results for "MOCK Try Win".\n',
            ('INFO: Results URL: https://test-results.appspot.com/data/layout_results/'
             'MOCK_Try_Win/5000/webkit_layout_tests%20%28with%20patch%29/layout-test-results/results.html\n'),
            'INFO: There are some builders with no results:\n',
            'INFO:   MOCK Try Win\n',
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Try Linux" build 6000 for test-win-win7.\n',
            'INFO: Rebaselining one/flaky-fail.html\n'
        ])

    def test_fill_in_missing_results(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Try Linux', 100))
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Try Win', 200))
        self.command.fill_in_missing_results(test_baseline_set)
        self.assertEqual(
            test_baseline_set.build_port_pairs('one/flaky-fail.html'),
            [
                (Build('MOCK Try Linux', 100), 'test-linux-trusty'),
                (Build('MOCK Try Win', 200), 'test-win-win7'),
                (Build('MOCK Try Linux', 100), 'test-mac-mac10.11'),
            ])
        self.assertLog([
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Try Linux" build 100 for test-mac-mac10.11.\n',
        ])

    def test_fill_in_missing_results_prefers_build_with_same_os_type(self):
        self.tool.builders = BuilderList({
            'MOCK Foo12': {
                'port_name': 'foo-foo12',
                'specifiers': ['Foo12', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Foo45': {
                'port_name': 'foo-foo45',
                'specifiers': ['Foo45', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Bar3': {
                'port_name': 'bar-bar3',
                'specifiers': ['Bar3', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Bar4': {
                'port_name': 'bar-bar4',
                'specifiers': ['Bar4', 'Release'],
                'is_try_builder': True,
            },
        })
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Foo12', 100))
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Bar4', 200))
        self.command.fill_in_missing_results(test_baseline_set)
        self.assertEqual(
            test_baseline_set.build_port_pairs('one/flaky-fail.html'),
            [
                (Build('MOCK Foo12', 100), 'foo-foo12'),
                (Build('MOCK Bar4', 200), 'bar-bar4'),
                (Build('MOCK Foo12', 100), 'foo-foo45'),
                (Build('MOCK Bar4', 200), 'bar-bar3'),
            ])
        self.assertLog([
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Foo12" build 100 for foo-foo45.\n',
            'INFO: Using "MOCK Bar4" build 200 for bar-bar3.\n',
        ])

    def test_explicit_builder_list(self):
        builders = ['MOCK Try Linux', 'MOCK Try Mac']
        options = self.command_options(builders=builders)
        exit_code = self.command.execute(options, [], self.tool)
        self.assertLog([
            'INFO: Finished try jobs found for all try bots.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])
        self.assertEqual(exit_code, 0)
        self.assertEqual(self.command.selected_try_bots, frozenset(builders))
