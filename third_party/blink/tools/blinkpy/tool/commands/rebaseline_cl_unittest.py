# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import logging
import optparse
import textwrap
from unittest import mock

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import (
    Artifact,
    WebTestResult,
    WebTestResults,
)
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.rebaseline import TestBaselineSet
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL
from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase
from blinkpy.web_tests.builder_list import BuilderList


@mock.patch.object(logging.getLogger('blinkpy.web_tests.port.base'),
                   'propagate', False)
# Do not re-request try build information to check for interrupted steps.
@mock.patch(
    'blinkpy.common.net.rpc.BuildbucketClient.execute_batch', lambda self: [])
class RebaselineCLTest(BaseTestCase, LoggingTestCase):

    command_constructor = lambda self: RebaselineCL(MockBlinkTool())

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)
        self.maxDiff = None
        self.builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000, 'Build-3'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux Multiple Steps', 9000, 'Build-5'):
            TryJobStatus('COMPLETED', 'FAILURE'),
        }

        self.command.git_cl = MockGitCL(self.tool, self.builds)

        git = MockGit(
            filesystem=self.tool.filesystem, executive=self.tool.executive)
        git.changed_files = lambda **_: [
            RELATIVE_WEB_TESTS + 'one/text-fail.html',
            RELATIVE_WEB_TESTS + 'one/flaky-fail.html', ]
        self.tool.git = lambda: git

        self.tool.builders = BuilderList({
            'MOCK Try Win': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Try Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Try Mac': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Try Linux (CQ duplicate)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'is_cq_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
        })

        self.raw_web_test_results = {
            'tests': {
                'one': {
                    'crash.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH',
                        'is_unexpected': True,
                        'artifacts': {
                            'crash_log': ['crash.log']
                        }
                    },
                    'expected-fail.html': {
                        'expected': 'FAIL',
                        'actual': 'FAIL',
                        'artifacts': {
                            'expected_text':
                            ['https://results.api.cr.dev/expected_text'],
                            'actual_text':
                            ['https://results.api.cr.dev/actual_text']
                        }
                    },
                    'flaky-fail.html': {
                        'expected': 'PASS',
                        'actual': 'PASS FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'expected_audio':
                            ['https://results.api.cr.dev/expected_audio'],
                            'actual_audio':
                            ['https://results.api.cr.dev/actual_audio']
                        }
                    },
                    'missing.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_image':
                            ['https://results.api.cr.dev/actual_image']
                        },
                        'is_missing_image': True
                    },
                    'slow-fail.html': {
                        'expected': 'SLOW',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_text':
                            ['https://results.api.cr.dev/actual_text'],
                            'expected_text':
                            ['https://results.api.cr.dev/expected_text']
                        }
                    },
                    'text-fail.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_text':
                            ['https://results.api.cr.dev/actual_text'],
                            'expected_text':
                            ['https://results.api.cr.dev/expected_text']
                        }
                    },
                    'unexpected-pass.html': {
                        'expected': 'FAIL',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                },
                'two': {
                    'image-fail.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_image':
                            ['https://results.api.cr.dev/actual_image'],
                            'expected_image':
                            ['https://results.api.cr.dev/expected_image']
                        }
                    }
                },
            },
        }

        for build in self.builds:
            self.tool.results_fetcher.set_results(
                build,
                WebTestResults.from_json(
                    self.raw_web_test_results,
                    step_name='blink_web_tests (with patch)'))
            self.tool.results_fetcher.set_retry_sumary_json(
                build,
                json.dumps({
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
        # Also, write their generic baselines to disable the implicit all-pass
        # warning by default.
        tests = [
            ('one/flaky-fail.html', 'wav'),
            ('one/missing.html', 'png'),
            ('one/slow-fail.html', 'txt'),
            ('one/text-fail.html', 'txt'),
            ('two/image-fail.html', 'png'),
        ]
        for test, suffix in tests:
            self._write(test, 'contents')
            baseline_name = self.mac_port.output_filename(
                test, self.mac_port.BASELINE_SUFFIX, '.' + suffix)
            self._write(baseline_name, 'contents')

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
            'flag_specific': None,
        }
        options.update(kwargs)
        return optparse.Values(options)

    def test_execute_basic(self):
        # By default, with no arguments or options, rebaseline-cl rebaselines
        # all of the tests that unexpectedly failed.
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_execute_with_explicit_dir(self):
        exit_code = self.command.execute(self.command_options(), ['one/'],
                                         self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])

    def test_execute_basic_dry_run(self):
        """Dry running does not execute any commands or write any files."""
        self.set_logging_level(logging.DEBUG)
        # This shallow copy prevents a spurious pass when the filesystem
        # contents mapping mutates.
        files_before = dict(self.tool.filesystem.files)
        exit_code = self.command.execute(self.command_options(dry_run=True),
                                         [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertEqual(self.command.git_cl.calls, [])
        self.assertEqual(self.tool.filesystem.files, files_before)
        self.assertEqual(self.tool.git().added_paths, set())

    def test_execute_with_test_name_file(self):
        fs = self.mac_port.host.filesystem
        test_name_file = fs.mktemp()
        fs.write_text_file(
            test_name_file,
            textwrap.dedent('''
            one/flaky-fail.html
              one/missing.html
            # one/slow-fail.html
            #

            one/not-a-test.html
            one/text-fail.html
                two/   '''))
        exit_code = self.command.execute(
            self.command_options(test_name_file=test_name_file), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Reading list of tests to rebaseline from %s\n' %
            test_name_file,
            "WARNING: 'one/not-a-test.html' does not represent any tests "
            'and may be misspelled.\n',
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
            'INFO: No finished builds.\n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux       --      TRIGGERED try   \n',
            'INFO:   MOCK Try Mac         --      TRIGGERED try   \n',
            'INFO:   MOCK Try Win         --      TRIGGERED try   \n',
            'ERROR: Once all pending try jobs have finished, '
            'please re-run the tool to fetch new results.\n',
        ])
        self.assertEqual(self.command.git_cl.calls, [[
            'git',
            'cl',
            'try',
            '-B',
            'luci.chromium.try',
            '-b',
            'MOCK Try Linux',
            '-b',
            'MOCK Try Mac',
            '-b',
            'MOCK Try Win',
        ]])

    def test_execute_no_try_jobs_started_and_no_trigger_jobs(self):
        # If there are no try jobs started yet and '--no-trigger-jobs' or
        # '--dry-run' is passed, then we just abort immediately.
        self.command.git_cl = MockGitCL(self.tool, {})
        exit_code = self.command.execute(
            self.command_options(trigger_jobs=False), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        exit_code = self.command.execute(self.command_options(dry_run=True),
                                         [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        self.assertEqual(self.command.git_cl.calls, [])

    def test_execute_one_missing_build(self):
        builds = {
            Build('MOCK Try Win', 5000): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000): TryJobStatus('COMPLETED', 'FAILURE'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Mac         4000    FAILURE   try   \n',
            'INFO:   MOCK Try Win         5000    FAILURE   try   \n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux       --      TRIGGERED try   \n',
            'ERROR: Once all pending try jobs have finished, '
            'please re-run the tool to fetch new results.\n',
        ])
        self.assertEqual(self.command.git_cl.calls, [
            [
                'git', 'cl', 'try', '-B', 'luci.chromium.try', '-b',
                'MOCK Try Linux'
            ],
        ])

    def test_execute_with_unfinished_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('STARTED'),
            Build('MOCK Try Linux', 6000, 'Build-3'):
            TryJobStatus('SCHEDULED'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Win         5000    FAILURE   try   \n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux       6000    SCHEDULED try   \n',
            'INFO:   MOCK Try Mac         4000    STARTED   try   \n',
            'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Linux\n',
            'WARNING:   MOCK Try Mac\n',
            'INFO: Would you like to continue?\n'
            'Note: This will try to fill in missing results '
            'with available results.\n'
            'This is generally not suggested unless the results are '
            'platform agnostic.\n',
            'INFO: Aborting. Please retry builders with no results.\n',
        ])

    def test_execute_with_passing_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('MOCK Try Linux', 6000, 'Build-3'):
            TryJobStatus('COMPLETED', 'SUCCESS'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n'
        ])

    def test_execute_with_only_unrelated_failing_suites(self):
        """A build without web test failures should not be treated as missing.

        The build may still fail because of other non-web test suites.

        See Also:
            crbug.com/1475247#c1
        """
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000, 'Build-3'):
            TryJobStatus('COMPLETED', 'FAILURE'),
        }
        for build in builds:
            self.tool.results_fetcher.set_results(
                build,
                WebTestResults([], step_name='blink_web_tests (with patch)'))
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
        ])

    def test_execute_with_no_trigger_jobs_option(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('COMPLETED', 'FAILURE'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(
            self.command_options(trigger_jobs=False), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Finished builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Mac         4000    FAILURE   try   \n',
            'INFO:   MOCK Try Win         5000    FAILURE   try   \n',
            'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Linux\n',
            'INFO: Would you like to continue?\n'
            'Note: This will try to fill in missing results '
            'with available results.\n'
            'This is generally not suggested unless the results are '
            'platform agnostic.\n',
            'INFO: Aborting. Please retry builders with no results.\n',
        ])
        self.assertEqual(self.command.git_cl.calls, [])

    def test_execute_with_only_changed_tests_option(self):
        # When --only-changed-tests is passed, the tool only rebaselines tests
        # that were modified in the CL.
        exit_code = self.command.execute(
            self.command_options(only_changed_tests=True), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])

    def test_execute_with_test_that_fails_on_retry(self):
        # In this example, one test failed both with and without the patch
        # in the try job, so it is not rebaselined.
        for build in self.builds:
            self.tool.results_fetcher.set_retry_sumary_json(
                build,
                json.dumps({
                    'failures': ['one/text-fail.html'],
                    'ignored': ['two/image-fail.html'],
                }))
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])

    def test_execute_with_no_retry_summary_downloaded(self):
        # In this example, the retry summary could not be downloaded, so
        # a warning is printed and all tests are rebaselined.
        self.tool.results_fetcher.set_retry_sumary_json(
            Build('MOCK Try Win', 5000, 'Build-1'), None)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: No retry summary available for ("MOCK Try Win", "blink_web_tests").\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_rebaseline_command_invocations(self):
        """Tests the list of commands that are called for rebaselining."""
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('one/flaky-fail.html',
                              Build('MOCK Try Win', 5000, 'Build-1'),
                              'blink_web_tests (with patch)')
        self.command.rebaseline(self.command_options(), test_baseline_set)
        self._mock_copier.find_baselines_to_copy.assert_called_once_with(
            'one/flaky-fail.html', 'wav', test_baseline_set)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/actual_audio',
            'platform/test-win-win7/one/flaky-fail-expected.wav')
        self.tool.main.assert_called_once_with([
            'echo',
            'optimize-baselines',
            '--no-manifest-update',
            'one/flaky-fail.html',
        ])

    def test_rebaseline_command_invocations_multiple_steps(self):
        """Test the rebaseline tool handles multiple steps on the same builder.

        In this example, the builder runs one generic web test step and two
        flag-specific ones.
        """
        self.tool.builders = BuilderList({
            'MOCK Try Linux Multiple Steps': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                    'not_site_per_process_blink_web_tests (with patch)': {
                        'flag_specific': 'disable-site-isolation-trials',
                    },
                },
            },
        })

        # Add results for the other flag-specific steps on this builder.
        multiple_step_build = Build('MOCK Try Linux Multiple Steps', 9000,
                                    'Build-5')
        self.tool.results_fetcher.set_results(
            multiple_step_build,
            WebTestResults.from_json(
                self.raw_web_test_results,
                step_name='not_site_per_process_blink_web_tests (with patch)'))

        exit_code = self.command.execute(
            self.command_options(builders=['MOCK Try Linux Multiple Steps']),
            ['one/text-fail.html', 'one/does-not-exist.html'], self.tool)
        self.assertEqual(exit_code, 0)
        baseline_set = TestBaselineSet(self.tool.builders)
        baseline_set.add('one/text-fail.html', multiple_step_build,
                         'blink_web_tests (with patch)')
        baseline_set.add('one/text-fail.html', multiple_step_build,
                         'not_site_per_process_blink_web_tests (with patch)')
        self._mock_copier.find_baselines_to_copy.assert_called_once_with(
            'one/text-fail.html', 'txt', baseline_set)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/actual_text',
            'platform/test-linux-trusty/one/text-fail-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/actual_text',
            'flag-specific/disable-site-isolation-trials/one/text-fail-expected.txt'
        )
        self.tool.main.assert_called_once_with([
            'echo',
            'optimize-baselines',
            '--no-manifest-update',
            'one/text-fail.html',
        ])

    def test_execute_missing_results_with_no_fill_missing_prompts(self):
        build = Build('MOCK Try Win', 5000, 'Build-1')
        self.builds[build] = TryJobStatus.from_bb_status('CANCELED')
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Win\n',
            'INFO: Would you like to continue?\n'
            'Note: This will try to fill in missing results '
            'with available results.\n'
            'This is generally not suggested unless the results are '
            'platform agnostic.\n',
            'INFO: Aborting. Please retry builders with no results.\n',
        ])

    def test_execute_interrupted_results_with_fill_missing(self):
        build = Build('MOCK Try Win', 5000, 'Build-1')
        self.builds[build] = TryJobStatus.from_bb_status('INFRA_FAILURE')
        self.tool.user.set_canned_responses(['y'])
        # TODO(crbug.com/1383284): After `--fill-missing` is fully deprecated,
        # stop checking for the deprecation warning.
        exit_code = self.command.execute(
            self.command_options(fill_missing=True), ['one/flaky-fail.html'],
            self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'WARNING: `--{no-,}fill-missing` is deprecated and will be removed '
            'soon due to limited utility (crbug.com/1383284).\n',
            'WARNING: Some builds have infrastructure failures:\n',
            'WARNING:   "MOCK Try Win" build 5000\n',
            'WARNING: Examples of infrastructure failures include:\n',
            'WARNING:   * Shard terminated the harness after timing out.\n',
            'WARNING:   * Harness exited early due to excessive unexpected failures.\n',
            'WARNING:   * Build failed on a non-test step.\n',
            'WARNING: Please consider retrying the failed builders or '
            'giving the builders more shards.\n',
            'WARNING: See https://chromium.googlesource.com/chromium/src/+/'
            'HEAD/docs/testing/web_test_expectations.md#handle-bot-timeouts\n',
            'INFO: All builds finished.\n',
            'INFO: Would you like to continue?\n'
            'Note: This will try to fill in missing results '
            'with available results.\n'
            'This is generally not suggested unless the results are '
            'platform agnostic.\n',
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Try Linux" build 6000 for test-win-win7.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
        ])

    def test_detect_reftest_failure(self):
        self._write('two/image-fail-expected.html', 'reference')
        exit_code = self.command.execute(
            self.command_options(builders=['MOCK Try Linux']),
            ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining two/image-fail.html\n',
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Trusty ] two/image-fail.html [ Failure ]  # Reftest image failure\n',
        ])
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-linux-trusty/'
                             'two/image-fail-expected.png')))

    def test_detect_flaky_baseline(self):
        result = WebTestResult('one/flaky-fail.html', {
            'actual': 'FAIL FAIL',
            'is_unexpected': True,
        }, {
            'actual_audio': [
                Artifact('https://results.usercontent.cr.dev/1/actual_audio'),
                Artifact('https://results.usercontent.cr.dev/2/actual_audio'),
            ],
        })
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Mac', 4000, 'Build-2'),
            WebTestResults([result], step_name='blink_web_tests (with patch)'))
        exit_code = self.command.execute(self.command_options(),
                                         ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Mac10.11 ] one/flaky-fail.html [ Failure ]  # Flaky output\n',
        ])
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-mac-mac10.11/'
                             'one/flaky-fail-expected.wav')))

    def test_detect_flaky_but_within_existing_fuzzy_params(self):
        self._write('two/image-fail.html',
                    '<meta name="fuzzy" content="0-5;0-100">')
        result = WebTestResult('two/image-fail.html', {
            'actual': 'FAIL FAIL FAIL',
            'is_unexpected': True,
        }, {
            'actual_image': [
                Artifact('https://results.usercontent.cr.dev/1/actual_image',
                         '5a428fb'),
                Artifact('https://results.usercontent.cr.dev/2/actual_image',
                         '928ba6e'),
                Artifact('https://results.usercontent.cr.dev/3/actual_image',
                         '398c8be'),
            ],
        })
        self.tool.web.get_binary.side_effect = lambda url: {
            # The contents encode a total pixels different "distance" (i.e.,
            # assume total pixel differences also hold pairwise).
            'https://results.usercontent.cr.dev/1/actual_image': b'100',
            'https://results.usercontent.cr.dev/2/actual_image': b'200',
            'https://results.usercontent.cr.dev/3/actual_image': b'300',
        }[url]
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Mac', 4000, 'Build-2'),
            WebTestResults([result], step_name='blink_web_tests (with patch)'))

        with mock.patch.object(self._test_port,
                               'diff_image',
                               side_effect=self._diff_image) as diff_image:
            exit_code = self.command.execute(
                self.command_options(builders=['MOCK Try Mac']),
                ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])
        # Image diffing is relatively expensive. Verify that the minimal number
        # of calls is made to get all mutual differences.
        self.assertEqual(diff_image.call_count, 3)
        # The second retry's image must be selected because it's the only one
        # that is within the fuzzy range of the other retries. The other retries
        # need at least 200 total pixels different allowed to be selected.
        self.assertEqual(
            self._read('platform/test-mac-mac10.11/'
                       'two/image-fail-expected.png'), '200')

    def test_detect_flaky_beyond_existing_fuzzy_params(self):
        self._write('two/image-fail.html',
                    '<meta name="fuzzy" content="0-5;0-99">')
        result = WebTestResult('two/image-fail.html', {
            'actual': 'FAIL FAIL',
            'is_unexpected': True,
        }, {
            'actual_image': [
                Artifact('https://results.usercontent.cr.dev/1/actual_image',
                         '5a428fb'),
                Artifact('https://results.usercontent.cr.dev/2/actual_image',
                         '928ba6e'),
            ],
        })
        self.tool.web.get_binary.side_effect = lambda url: {
            'https://results.usercontent.cr.dev/1/actual_image': b'100',
            'https://results.usercontent.cr.dev/2/actual_image': b'200',
        }[url]
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Mac', 4000, 'Build-2'),
            WebTestResults([result], step_name='blink_web_tests (with patch)'))

        with mock.patch.object(self._test_port,
                               'diff_image',
                               side_effect=self._diff_image) as diff_image:
            exit_code = self.command.execute(
                self.command_options(builders=['MOCK Try Mac']),
                ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining two/image-fail.html\n',
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Mac10.11 ] two/image-fail.html [ Failure ]  # Flaky output\n',
        ])
        self.assertEqual(diff_image.call_count, 1)
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-mac-mac10.11/'
                             'two/image-fail-expected.png')))

    def _diff_image(self, expected, actual):
        stats = {
            'totalPixels': abs(int(actual) - int(expected)),
            'maxDifference': 1,
        }
        return b'diff', stats, None

    def test_fill_in_missing_results(self):
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('one/flaky-fail.html',
                              Build('MOCK Try Linux', 100),
                              'blink_web_tests (with patch)')
        test_baseline_set.add('one/flaky-fail.html',
                              Build('MOCK Try Win',
                                    200), 'blink_web_tests (with patch)')
        self.command.fill_in_missing_results(test_baseline_set)
        self.assertEqual(
            test_baseline_set.build_port_pairs('one/flaky-fail.html'), [
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
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Foo45': {
                'port_name': 'foo-foo45',
                'specifiers': ['Foo45', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Bar3': {
                'port_name': 'bar-bar3',
                'specifiers': ['Bar3', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
            'MOCK Bar4': {
                'port_name': 'bar-bar4',
                'specifiers': ['Bar4', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                },
            },
        })
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Foo12', 100),
                              'blink_web_tests (with patch)')
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Bar4', 200),
                              'blink_web_tests (with patch)')
        self.command.fill_in_missing_results(test_baseline_set)
        self.assertEqual(
            sorted(test_baseline_set.build_port_pairs('one/flaky-fail.html')),
            [
                (Build('MOCK Bar4', 200), 'bar-bar3'),
                (Build('MOCK Bar4', 200), 'bar-bar4'),
                (Build('MOCK Foo12', 100), 'foo-foo12'),
                (Build('MOCK Foo12', 100), 'foo-foo45'),
            ])
        self.assertLog([
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Bar4" build 200 for bar-bar3.\n',
            'INFO: Using "MOCK Foo12" build 100 for foo-foo45.\n',
        ])

    def test_fill_in_missing_results_partition_by_steps(self):
        self.tool.builders = BuilderList({
            'MOCK Foo12': {
                'port_name': 'foo-foo12',
                'specifiers': ['Foo12', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                    'blink_wpt_tests (with patch)': {},
                },
            },
            'MOCK Foo45': {
                'port_name': 'foo-foo45',
                'specifiers': ['Foo45', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests (with patch)': {},
                    'blink_wpt_tests (with patch)': {},
                },
            },
        })
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('one/flaky-fail.html', Build('MOCK Foo12', 100),
                              'blink_web_tests (with patch)')
        test_baseline_set.add('two/image-fail.html', Build('MOCK Foo45', 200),
                              'blink_wpt_tests (with patch)')
        self.command.fill_in_missing_results(test_baseline_set)
        self.assertEqual(
            sorted(test_baseline_set.runs_for_test('one/flaky-fail.html')),
            [
                # Do not add this test to `blink_wpt_tests`.
                (Build('MOCK Foo12',
                       100), 'blink_web_tests (with patch)', 'foo-foo12'),
                (Build('MOCK Foo12',
                       100), 'blink_web_tests (with patch)', 'foo-foo45'),
            ])
        self.assertEqual(
            sorted(test_baseline_set.runs_for_test('two/image-fail.html')),
            [
                # Do not add this test to `blink_web_tests`.
                (Build('MOCK Foo45',
                       200), 'blink_wpt_tests (with patch)', 'foo-foo12'),
                (Build('MOCK Foo45',
                       200), 'blink_wpt_tests (with patch)', 'foo-foo45'),
            ])
        self.assertLog([
            'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Foo12" build 100 for foo-foo45.\n',
            'INFO: For two/image-fail.html:\n',
            'INFO: Using "MOCK Foo45" build 200 for foo-foo12.\n',
        ])

    def test_explicit_builder_list(self):
        builders = ['MOCK Try Linux', 'MOCK Try Mac']
        options = self.command_options(builders=builders)
        exit_code = self.command.execute(options, [], self.tool)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/slow-fail.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])
        self.assertEqual(exit_code, 0)
        self.assertEqual(self.command.selected_try_bots, frozenset(builders))

    def test_invalid_explicit_builder_list(self):
        message = io.StringIO()
        with contextlib.redirect_stderr(message):
            with self.assertRaises(SystemExit):
                self.command.main(['--builders=does-not-exist'])
        self.assertRegex(message.getvalue(),
                         "'does-not-exist' is not a try builder")
        self.assertRegex(message.getvalue(), 'MOCK Try Linux\n')
        self.assertRegex(message.getvalue(),
                         'MOCK Try Linux \(CQ duplicate\)\n')
        self.assertRegex(message.getvalue(), 'MOCK Try Mac\n')
        self.assertRegex(message.getvalue(), 'MOCK Try Win\n')

    def test_abbreviated_all_pass_generation(self):
        baseline_name = self.mac_port.output_filename(
            'one/text-fail.html', self.mac_port.BASELINE_SUFFIX, '.txt')
        self._remove(baseline_name)
        options = self.command_options(builders=['MOCK Try Linux'])
        exit_code = self.command.execute(options, ['one/text-fail.html'],
                                         self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Rebaselining one/text-fail.html\n',
        ])
        self.assertRegex(
            self._read('flag-specific/disable-site-isolation-trials/'
                       'one/text-fail-expected.txt'),
            'All subtests passed and are omitted for brevity')
        self.assertRegex(
            self._read('platform/test-linux-precise/'
                       'one/text-fail-expected.txt'),
            'All subtests passed and are omitted for brevity')
        # Since `test-mac-mac10.11` is not rebaselined, there should not be an
        # abbreviated all-pass baseline for `test-mac-mac10.10`.
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-mac-mac10.10/'
                             'one/text-fail-expected.txt')))
