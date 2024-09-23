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

from blinkpy.common.checkout.git import FileStatus, FileStatusType
from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.net.git_cl import BuildStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import (
    Artifact,
    IncompleteResultsReason,
    WebTestResult,
    WebTestResults,
)
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.rebaseline import TestBaselineSet
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL
from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase
from blinkpy.w3c.gerrit_mock import MockGerritAPI
from blinkpy.web_tests.builder_list import BuilderList


@mock.patch.object(logging.getLogger('blinkpy.web_tests.port.base'),
                   'propagate', False)
# Do not re-request try build information to check for interrupted steps.
@mock.patch(
    'blinkpy.common.net.rpc.BuildbucketClient.execute_batch', lambda self: [])
@mock.patch('blinkpy.tool.commands.build_resolver.GerritAPI', MockGerritAPI)
class RebaselineCLTest(BaseTestCase, LoggingTestCase):

    command_constructor = lambda self: RebaselineCL(MockBlinkTool())

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)
        self.maxDiff = None
        self.builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000, 'Build-2'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Linux', 6000, 'Build-3'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Linux (CQ duplicate)', 7000, 'Build-4'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Linux Multiple Steps', 9000, 'Build-5'):
            BuildStatus.TEST_FAILURE,
        }

        self.command.git_cl = MockGitCL(self.tool, self.builds)

        git = MockGit(
            filesystem=self.tool.filesystem, executive=self.tool.executive)
        git.changed_files = lambda **_: {
            RELATIVE_WEB_TESTS + 'one/text-fail.html':
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'one/flaky-fail.html':
            FileStatus(FileStatusType.MODIFY),
        }
        self.tool.git = lambda: git

        self.tool.builders = BuilderList({
            'MOCK Try Win': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Try Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Try Mac': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Try Linux (CQ duplicate)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'is_cq_builder': True,
                'steps': {
                    'blink_web_tests': {},
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
                WebTestResults.from_json(self.raw_web_test_results,
                                         step_name='blink_web_tests'))
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

    def tearDown(self):
        BaseTestCase.tearDown(self)
        LoggingTestCase.tearDown(self)

    @staticmethod
    def command_options(**kwargs):
        options = {
            'dry_run': False,
            'only_changed_tests': False,
            'trigger_jobs': True,
            'optimize': True,
            'results_directory': None,
            'test_name_file': None,
            'verbose': False,
            'builders': [],
            'patchset': None,
            'manifest_update': False,
        }
        options.update(kwargs)
        return optparse.Values(options)

    def test_execute_basic(self):
        # By default, with no arguments or options, rebaseline-cl rebaselines
        # all of the tests that unexpectedly failed.
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 5 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/5)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/5)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/5)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (4/5)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (5/5)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/5)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/5)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/5)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (4/5)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (5/5)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])

    def test_execute_with_explicit_dir(self):
        exit_code = self.command.execute(self.command_options(), ['one/'],
                                         self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 4 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/4)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/4)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/4)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (4/4)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/4)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/4)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/4)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (4/4)\n",
            'INFO: Staging 0 baselines with git.\n',
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
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Reading list of tests to rebaseline from %s\n' %
            test_name_file,
            "WARNING: 'one/not-a-test.html' does not represent any tests "
            'and may be misspelled.\n',
            'INFO: Rebaselining 4 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/4)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/4)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (3/4)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (4/4)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/4)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/4)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (3/4)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (4/4)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])

    def test_execute_with_tests_missing_locally(self):
        self._remove('one/text-fail.html')
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'WARNING: Skipping rebaselining for 1 test missing from the local '
            'checkout:\n',
            'WARNING:   one/text-fail.html\n',
            'WARNING: You may want to rebase or trigger new builds.\n',
            'INFO: Rebaselining 4 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/4)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/4)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/4)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (4/4)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/4)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/4)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/4)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (4/4)\n",
            'INFO: Staging 0 baselines with git.\n',
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
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: No finished builds.\n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER                       NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux                --      TRIGGERED try   \n',
            'INFO:   MOCK Try Linux (CQ duplicate) --      TRIGGERED try   \n',
            'INFO:   MOCK Try Mac                  --      TRIGGERED try   \n',
            'INFO:   MOCK Try Win                  --      TRIGGERED try   \n',
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
            'MOCK Try Linux (CQ duplicate)',
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
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        exit_code = self.command.execute(self.command_options(dry_run=True),
                                         [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        self.assertEqual(self.command.git_cl.calls, [])

    def test_execute_one_missing_build(self):
        builds = {
            Build('MOCK Try Win', 5000): BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000): BuildStatus.TEST_FAILURE,
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: Finished builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS       BUCKET\n',
            'INFO:   MOCK Try Mac         4000    TEST_FAILURE try   \n',
            'INFO:   MOCK Try Win         5000    TEST_FAILURE try   \n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER                       NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux                --      TRIGGERED try   \n',
            'INFO:   MOCK Try Linux (CQ duplicate) --      TRIGGERED try   \n',
            'ERROR: Once all pending try jobs have finished, '
            'please re-run the tool to fetch new results.\n',
        ])
        self.assertEqual(self.command.git_cl.calls, [
            [
                'git',
                'cl',
                'try',
                '-B',
                'luci.chromium.try',
                '-b',
                'MOCK Try Linux',
                '-b',
                'MOCK Try Linux (CQ duplicate)',
            ],
        ])

    def test_execute_with_unfinished_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000, 'Build-2'):
            BuildStatus.STARTED,
            Build('MOCK Try Linux', 6000, 'Build-3'):
            BuildStatus.SCHEDULED,
            Build('MOCK Try Linux (CQ duplicate)', 7000, 'Build-4'):
            BuildStatus.TEST_FAILURE,
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: Finished builds:\n',
            'INFO:   BUILDER                       NUMBER  STATUS       BUCKET\n',
            'INFO:   MOCK Try Linux (CQ duplicate) 7000    TEST_FAILURE try   \n',
            'INFO:   MOCK Try Win                  5000    TEST_FAILURE try   \n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   MOCK Try Linux       6000    SCHEDULED try   \n',
            'INFO:   MOCK Try Mac         4000    STARTED   try   \n',
            'INFO: Fetching test results for 2 suites.\n',
            'WARNING: Some builds have incomplete results:\n',
            'WARNING:   "MOCK Try Linux" build 6000, "blink_web_tests": '
            'build is not complete\n',
            'WARNING:   "MOCK Try Mac" build 4000, "blink_web_tests": '
            'build is not complete\n',
            'INFO: Would you like to fill in missing results with available results?\n'
            'This is generally not suggested unless the results are '
            'platform agnostic or the needed results happen to be not '
            'missing.\n',
            'INFO: Aborting. Please retry builders with no results.\n',
        ])

    def test_execute_with_passing_jobs(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000, 'Build-2'):
            BuildStatus.SUCCESS,
            Build('MOCK Try Linux', 6000, 'Build-3'):
            BuildStatus.SUCCESS,
            Build('MOCK Try Linux (CQ duplicate)', 7000, 'Build-4'):
            BuildStatus.SUCCESS,
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'INFO: Rebaselining 5 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/5)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/5)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/5)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (4/5)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (5/5)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/5)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/5)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/5)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (4/5)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (5/5)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])

    def test_execute_with_only_unrelated_failing_suites(self):
        """A build without web test failures should not be treated as missing.

        The build may still fail because of other non-web test suites.

        See Also:
            crbug.com/1475247#c1
        """
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000, 'Build-2'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Linux', 6000, 'Build-3'):
            BuildStatus.TEST_FAILURE,
            Build('MOCK Try Linux (CQ duplicate)', 7000, 'Build-4'):
            BuildStatus.SUCCESS,
        }
        for build in builds:
            self.tool.results_fetcher.set_results(
                build, WebTestResults([], step_name='blink_web_tests'))
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 3 suites.\n',
            'INFO: No tests to rebaseline.\n',
        ])

    def test_execute_with_no_trigger_jobs_option(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'): BuildStatus.TEST_FAILURE,
            Build('MOCK Try Mac', 4000, 'Build-2'): BuildStatus.TEST_FAILURE,
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(
            self.command_options(trigger_jobs=False), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: Finished builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS       BUCKET\n',
            'INFO:   MOCK Try Mac         4000    TEST_FAILURE try   \n',
            'INFO:   MOCK Try Win         5000    TEST_FAILURE try   \n',
            'INFO: Fetching test results for 2 suites.\n',
            'WARNING: Some builds have incomplete results:\n',
            'WARNING:   "MOCK Try Linux", "blink_web_tests": '
            'build is missing and not triggered\n',
            'WARNING:   "MOCK Try Linux (CQ duplicate)", "blink_web_tests": '
            'build is missing and not triggered\n',
            'INFO: Would you like to fill in missing results with available results?\n'
            'This is generally not suggested unless the results are '
            'platform agnostic or the needed results happen to be not '
            'missing.\n',
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
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 2 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/2)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (2/2)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/2)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (2/2)\n",
            'INFO: Staging 0 baselines with git.\n',
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
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (1/1)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (1/1)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])

    def test_execute_with_no_retry_summary_downloaded(self):
        # In this example, the retry summary could not be downloaded, so
        # a warning is printed and all tests are rebaselined.
        self.tool.results_fetcher.set_retry_sumary_json(
            Build('MOCK Try Win', 5000, 'Build-1'), None)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'WARNING: No retry summary available for '
            '("MOCK Try Win" build 5000, "blink_web_tests").\n',
            'INFO: Rebaselining 5 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/5)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/5)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/5)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (4/5)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (5/5)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/5)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/5)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/5)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (4/5)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (5/5)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])

    def test_rebaseline_command_invocations(self):
        """Tests the list of commands that are called for rebaselining."""
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('one/flaky-fail.html',
                              Build('MOCK Try Win', 5000, 'Build-1'),
                              'blink_web_tests')
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
                    'blink_web_tests': {},
                    'not_site_per_process_blink_web_tests': {
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
                step_name='not_site_per_process_blink_web_tests'))

        exit_code = self.command.execute(
            self.command_options(builders=['MOCK Try Linux Multiple Steps']),
            ['one/text-fail.html', 'one/does-not-exist.html'], self.tool)
        self.assertEqual(exit_code, 0)
        baseline_set = TestBaselineSet(self.tool.builders)
        baseline_set.add('one/text-fail.html', multiple_step_build,
                         'blink_web_tests')
        baseline_set.add('one/text-fail.html', multiple_step_build,
                         'not_site_per_process_blink_web_tests')
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
        self.builds[build] = BuildStatus.CANCELED
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 3 suites.\n',
            'WARNING: Some builds have incomplete results:\n',
            'WARNING:   "MOCK Try Win" build 5000, "blink_web_tests": '
            'build was canceled\n',
            'INFO: Would you like to fill in missing results with available results?\n'
            'This is generally not suggested unless the results are '
            'platform agnostic or the needed results happen to be not missing.\n',
            'INFO: Aborting. Please retry builders with no results.\n',
        ])

    def test_execute_interrupted_results_with_fill_missing_failing(self):
        raw_failing_results = {
            'tests': {
                'one': {
                    'flaky-fail.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_audio': [
                                'https://results.api.cr.dev/mac/actual_audio',
                            ],
                        },
                    },
                },
            },
        }
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Mac', 4000, 'Build-2'),
            WebTestResults.from_json(raw_failing_results,
                                     step_name='blink_web_tests'))
        build = Build('MOCK Try Win', 5000, 'Build-1')
        self.builds[build] = BuildStatus.INFRA_FAILURE
        self.tool.user.set_canned_responses(['y'])

        options = self.command_options(
            builders=['MOCK Try Mac', 'MOCK Try Win'])
        exit_code = self.command.execute(options, ['one/flaky-fail.html'],
                                         self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 2 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'WARNING: Some builds have incomplete results:\n',
            'WARNING:   "MOCK Try Win" build 5000, "blink_web_tests": '
            'build failed due to infra\n',
            'INFO: Would you like to fill in missing results with available results?\n'
            'This is generally not suggested unless the results are '
            'platform agnostic or the needed results happen to be not missing.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/1)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/1)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/mac/actual_audio',
            'platform/test-mac-mac10.11/one/flaky-fail-expected.wav')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/mac/actual_audio',
            'platform/test-win-win7/one/flaky-fail-expected.wav')

    def test_execute_interrupted_results_with_fill_missing_passing(self):
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Linux (CQ duplicate)', 7000, 'Build-4'),
            WebTestResults([], step_name='blink_web_tests'))
        build = Build('MOCK Try Linux', 6000, 'Build-3')
        self.builds[build] = BuildStatus.INFRA_FAILURE
        self.tool.user.set_canned_responses(['y'])

        exit_code = self.command.execute(self.command_options(),
                                         ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 3 suites.\n',
            'WARNING: Some builds have incomplete results:\n',
            'WARNING:   "MOCK Try Linux" build 6000, "blink_web_tests": '
            'build failed due to infra\n',
            'INFO: Would you like to fill in missing results with available results?\n'
            'This is generally not suggested unless the results are '
            'platform agnostic or the needed results happen to be not missing.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/1)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/1)\n",
            'INFO: Staging 0 baselines with git.\n',
        ])
        # "MOCK Try Linux" inherits from "MOCK Try Linux (CQ duplicate)" where
        # the test ran expectedly, so no baseline should be downloaded. See
        # https://crbug.com/350775866 for a related regression.
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-linux-trusty/'
                             'one/flaky-fail-expected.wav')))

    def test_detect_reftest_failure(self):
        self._write('two/image-fail-expected.html', 'reference')
        exit_code = self.command.execute(
            self.command_options(builders=['MOCK Try Linux']),
            ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 1 build from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Downloaded baselines for 'two/image-fail.html' (1/1)\n",
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Trusty ] two/image-fail.html [ Failure ]  # Reftest image failure\n',
            'INFO: Staging 0 baselines with git.\n',
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
            WebTestResults([result], step_name='blink_web_tests'))
        exit_code = self.command.execute(self.command_options(),
                                         ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/1)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/1)\n",
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Mac10.11 ] one/flaky-fail.html [ Failure ]  # Flaky output\n',
            'INFO: Staging 0 baselines with git.\n',
        ])
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-mac-mac10.11/'
                             'one/flaky-fail-expected.wav')))

    def test_detect_flaky_status(self):
        result = WebTestResult('one/flaky-fail.html', {
            'actual': 'FAIL TIMEOUT',
            'is_unexpected': True,
        }, {
            'actual_audio': [
                Artifact('https://results.usercontent.cr.dev/1/actual_audio'),
            ],
        })
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Mac', 4000, 'Build-2'),
            WebTestResults([result], step_name='blink_web_tests'))
        exit_code = self.command.execute(self.command_options(),
                                         ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 4 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 4 suites.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/1)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/1)\n",
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Mac10.11 ] one/flaky-fail.html [ Pass Timeout ]\n',
            'INFO: Staging 0 baselines with git.\n',
        ])
        self.assertTrue(
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
            WebTestResults([result], step_name='blink_web_tests'))

        with mock.patch.object(self._test_port,
                               'diff_image',
                               side_effect=self._diff_image) as diff_image:
            exit_code = self.command.execute(
                self.command_options(builders=['MOCK Try Mac']),
                ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 1 build from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'two/image-fail.html' (png) (1/1)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (1/1)\n",
            'INFO: Staging 0 baselines with git.\n',
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
            WebTestResults([result], step_name='blink_web_tests'))

        with mock.patch.object(self._test_port,
                               'diff_image',
                               side_effect=self._diff_image) as diff_image:
            exit_code = self.command.execute(
                self.command_options(builders=['MOCK Try Mac']),
                ['two/image-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 1 build from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'two/image-fail.html' (png) (1/1)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (1/1)\n",
            'WARNING: Some test failures should be suppressed in '
            'TestExpectations instead of being rebaselined.\n',
            'WARNING: Consider adding the following lines to '
            '/mock-checkout/third_party/blink/web_tests/TestExpectations:\n'
            '[ Mac10.11 ] two/image-fail.html [ Failure ]  # Flaky output\n',
            'INFO: Staging 0 baselines with git.\n',
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
        complete_results = {
            'blink_web_tests': {
                Build('MOCK Try Linux'):
                WebTestResults([], build=Build('MOCK Try Linux')),
                Build('MOCK Try Win'):
                WebTestResults([], build=Build('MOCK Try Win')),
            },
        }
        incomplete_results = {
            'blink_web_tests': {
                Build('MOCK Try Mac'):
                WebTestResults([],
                               incomplete_reason=IncompleteResultsReason()),
            },
        }

        merged_results = self.command.fill_in_missing_results(
            incomplete_results, complete_results)['blink_web_tests']
        self.assertEqual(len(merged_results), 3, merged_results)
        self.assertEqual(merged_results[Build('MOCK Try Linux')].build,
                         Build('MOCK Try Linux'))
        self.assertEqual(merged_results[Build('MOCK Try Win')].build,
                         Build('MOCK Try Win'))
        substituted_results = merged_results[Build('MOCK Try Mac')]
        self.assertIsNotNone(substituted_results.build)
        self.assertIsNone(substituted_results.incomplete_reason)

    def test_fill_in_missing_results_prefers_build_with_same_os_type(self):
        self.tool.builders = BuilderList({
            'MOCK Linux Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Linux Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                },
            },
        })
        complete_results = {
            'blink_web_tests': {
                Build('MOCK Linux Trusty'):
                WebTestResults([], build=Build('MOCK Linux Trusty')),
                Build('MOCK Mac10.10'):
                WebTestResults([], build=Build('MOCK Mac10.10')),
            },
        }
        incomplete_results = {
            'blink_web_tests': {
                Build('MOCK Linux Precise'):
                WebTestResults([],
                               incomplete_reason=IncompleteResultsReason()),
                Build('MOCK Mac10.11'):
                WebTestResults([],
                               incomplete_reason=IncompleteResultsReason()),
            },
        }

        merged_results = self.command.fill_in_missing_results(
            incomplete_results, complete_results)['blink_web_tests']
        self.assertEqual(len(merged_results), 4, merged_results)
        self.assertEqual(merged_results[Build('MOCK Linux Trusty')].build,
                         Build('MOCK Linux Trusty'))
        self.assertEqual(merged_results[Build('MOCK Linux Precise')].build,
                         Build('MOCK Linux Trusty'))
        self.assertEqual(merged_results[Build('MOCK Mac10.10')].build,
                         Build('MOCK Mac10.10'))
        self.assertEqual(merged_results[Build('MOCK Mac10.11')].build,
                         Build('MOCK Mac10.10'))

    def test_fill_in_missing_results_partition_by_steps(self):
        self.tool.builders = BuilderList({
            'MOCK Linux Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                    'chrome_wpt_tests': {},
                },
            },
            'MOCK Linux Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                    'chrome_wpt_tests': {},
                },
            },
        })
        complete_results = {
            'blink_web_tests': {
                Build('MOCK Linux Trusty'):
                WebTestResults([], build=Build('MOCK Linux Trusty')),
            },
            'blink_wpt_tests': {
                Build('MOCK Linux Precise'):
                WebTestResults([], build=Build('MOCK Linux Precise')),
            },
            'chrome_wpt_tests': {
                Build('MOCK Linux Precise'):
                WebTestResults([], build=Build('MOCK Linux Precise')),
                Build('MOCK Linux Trusty'):
                WebTestResults([], build=Build('MOCK Linux Trusty')),
            },
        }
        incomplete_results = {
            'blink_web_tests': {
                Build('MOCK Linux Precise'):
                WebTestResults([],
                               incomplete_reason=IncompleteResultsReason()),
            },
            'blink_wpt_tests': {
                Build('MOCK Linux Trusty'):
                WebTestResults([],
                               incomplete_reason=IncompleteResultsReason()),
            },
        }

        merged_results = self.command.fill_in_missing_results(
            incomplete_results, complete_results)

        results_for_suite = merged_results['blink_web_tests']
        self.assertEqual(len(results_for_suite), 2, results_for_suite)
        self.assertEqual(results_for_suite[Build('MOCK Linux Trusty')].build,
                         Build('MOCK Linux Trusty'))
        self.assertEqual(results_for_suite[Build('MOCK Linux Precise')].build,
                         Build('MOCK Linux Trusty'))

        results_for_suite = merged_results['blink_wpt_tests']
        self.assertEqual(len(results_for_suite), 2, results_for_suite)
        self.assertEqual(results_for_suite[Build('MOCK Linux Trusty')].build,
                         Build('MOCK Linux Precise'))
        self.assertEqual(results_for_suite[Build('MOCK Linux Precise')].build,
                         Build('MOCK Linux Precise'))

        results_for_suite = merged_results['chrome_wpt_tests']
        self.assertEqual(len(results_for_suite), 2, results_for_suite)
        self.assertEqual(results_for_suite[Build('MOCK Linux Precise')].build,
                         Build('MOCK Linux Precise'))
        self.assertEqual(results_for_suite[Build('MOCK Linux Trusty')].build,
                         Build('MOCK Linux Trusty'))

    def test_explicit_builder_list(self):
        builders = ['MOCK Try Linux', 'MOCK Try Mac']
        options = self.command_options(builders=builders)
        exit_code = self.command.execute(options, [], self.tool)
        self.assertLog([
            'INFO: Fetching status for 2 builds from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 2 suites.\n',
            'INFO: Rebaselining 5 tests.\n',
            "INFO: Copied baselines for 'one/flaky-fail.html' (wav) (1/5)\n",
            "INFO: Copied baselines for 'one/missing.html' (png) (2/5)\n",
            "INFO: Copied baselines for 'one/slow-fail.html' (txt) (3/5)\n",
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (4/5)\n",
            "INFO: Copied baselines for 'two/image-fail.html' (png) (5/5)\n",
            "INFO: Downloaded baselines for 'one/flaky-fail.html' (1/5)\n",
            "INFO: Downloaded baselines for 'one/missing.html' (2/5)\n",
            "INFO: Downloaded baselines for 'one/slow-fail.html' (3/5)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (4/5)\n",
            "INFO: Downloaded baselines for 'two/image-fail.html' (5/5)\n",
            'INFO: Staging 0 baselines with git.\n',
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
        self._write('one/text-fail.html',
                    '<script src="/resources/testharness.js"></script>')
        self._remove(baseline_name)
        options = self.command_options(builders=['MOCK Try Linux'])
        exit_code = self.command.execute(options, ['one/text-fail.html'],
                                         self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: Fetching status for 1 build from https://crrev.com/c/1234/1.\n',
            'INFO: All builds finished.\n',
            'INFO: Fetching test results for 1 suite.\n',
            'INFO: Rebaselining 1 test.\n',
            "INFO: Copied baselines for 'one/text-fail.html' (txt) (1/1)\n",
            "INFO: Downloaded baselines for 'one/text-fail.html' (1/1)\n",
            'INFO: Staging 0 baselines with git.\n',
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
