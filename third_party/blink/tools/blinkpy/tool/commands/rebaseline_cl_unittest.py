# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
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
from unittest import mock


# Do not re-request try build information to check for interrupted steps.
@mock.patch(
    'blinkpy.common.net.rpc.BuildbucketClient.execute_batch', lambda self: [])
class RebaselineCLTest(BaseTestCase, LoggingTestCase):

    command_constructor = RebaselineCL

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
            # Highdpi is an experimental builder whose status
            # is returned as ('COMPLETED', 'SUCCESS') even with failures.
            Build('MOCK Try Highdpi', 8000, 'Build-4'):
            TryJobStatus('COMPLETED', 'SUCCESS'),
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
            # Only some tests use the highdpi builder. Omitting
            # `is_try_builder` hides this builder by default.
            'MOCK Try Highdpi': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['trusty', 'Release'],
                'steps': {
                    'high_dpi_blink_web_tests (with patch)': {
                        'flag_specific': 'highdpi',
                    },
                },
            },
            'MOCK Try Linux (CQ duplicate)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'is_cq_builder': True,
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
                            'expected_text': ['expected-fail-expected.txt'],
                            'actual_text': ['expected-fail-actual.txt']
                        }
                    },
                    'flaky-fail.html': {
                        'expected': 'PASS',
                        'actual': 'PASS FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'expected_audio': ['flaky-fail-expected.wav'],
                            'actual_audio': ['flaky-fail-actual.wav']
                        }
                    },
                    'missing.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_image': ['missing-actual.png']
                        },
                        'is_missing_image': True
                    },
                    'slow-fail.html': {
                        'expected': 'SLOW',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_text': ['slow-fail-actual.txt'],
                            'expected_text': ['slow-fail-expected.txt']
                        }
                    },
                    'text-fail.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                        'artifacts': {
                            'actual_text': ['text-fail-actual.txt'],
                            'expected_text': ['text-fail-expected.txt']
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
                            'actual_image': ['image-fail-actual.png'],
                            'expected_image': ['image-fail-expected.png']
                        }
                    }
                },
            },
        }
        # TODO(crbug.com/1213998): Fix the example web test result format.
        raw_test_results = [{
            "name":
            "invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Ftwo%2Fimage-fail.html",
            "testId": "ninja://:blink_web_tests/two/image-fail.html",
            "resultId": "2",
            "variant": {
                "def": {
                    "builder": "",
                    "os": "",
                    "test_suite": "blink_web_tests"
                }
            },
            "status": "FAIL"
        }, {
            "name":
            "invocations/task-chromium-swarm.appspot.com-1/tests/ninja:%2F%2F:blink_wpt_tests%2Fone%2Fmissing.html",
            "testId": "ninja://:blink_wpt_tests/one/missing.html",
            "resultId": "1",
            "variant": {
                "def": {
                    "builder": "",
                    "os": "",
                    "test_suite": "blink_web_tests"
                }
            },
            "status": "FAIL"
        }, {
            "name":
            "invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Fone%2Fcrash.html",
            "testId": "ninja://:blink_web_tests/one/crash.html",
            "resultId": "3",
            "variant": {
                "def": {
                    "builder": "",
                    "os": "",
                    "test_suite": "blink_web_tests"
                }
            },
            "status": "CRASH"
        }]
        raw_artifacts = [{
            "name":
            "invocations/task-chromium-swarm.appspot.com-1/tests/ninja:%2F%2F:blink_wpt_tests%2Fone%2Fmissing.html/results/1",
            "artifactId": "actual_image",
            "fetchUrl":
            "https://results.usercontent.cr.dev/invocations/task-chromium-swarm.appspot.com-1/tests/ninja:%2F%2F:blink_wpt_tests%2Fone%2Fmissing.html/results/artifacts/actual_image?token=1",
            "contentType": "image/png",
        }, {
            "name":
            "invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Ftwo%2Fimage-fail.html/results/2",
            "artifactId": "actual_image",
            "fetchUrl":
            "https://results.usercontent.cr.dev/invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Ftwo%2Fimage-fail.html/results/artifacts/actual_image?token=2",
            "contentType": "image/png",
        }, {
            "name":
            "invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Fone%2Fcrash.html/results/3",
            "artifactId": "actual_text",
            "fetchUrl":
            "https://results.usercontent.cr.dev/invocations/task-chromium-swarm.appspot.com-2/tests/ninja:%2F%2F:blink_web_tests%2Fone%2Fcrash.html/results/artifacts/actual_text?token=3",
            "contentType": "text",
        }]
        # TODO(crbug.com/1376646): Need to test the ResultDB flag-specific path.
        # Ideally, we would run all the same tests on both the ResultDB-enabled
        # and ResultDB-disabled paths, only changing what `WebTestResults` are
        # returned.
        self.web_test_resultdb = self.tool.results_fetcher.make_results_from_raw_rdb(
            raw_test_results,
            raw_artifacts,
            step_name='blink_web_tests (with patch)')

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
            'flag_specific': None,
            'resultDB': None
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

    def test_execute_basic_dry_run(self):
        """Dry running does not execute any commands or write any files."""
        self.set_logging_level(logging.DEBUG)
        # This shallow copy prevents a spurious pass when the filesystem
        # contents mapping mutates.
        files_before = dict(self.tool.filesystem.files)
        exit_code = self.command.execute(self.command_options(dry_run=True),
                                         [], self.tool)
        self.assertEqual(exit_code, 0)
        messages = self.logMessages()
        # Asserting an exact count is brittle.
        # Just verify one of each internal command.
        self.assertTrue(
            any(
                message.startswith('DEBUG: Would have run: "python echo '
                                   'copy-existing-baselines-internal ')
                for message in messages))
        self.assertTrue(
            any(
                message.startswith('DEBUG: Would have run: "python echo '
                                   'rebaseline-test-internal ')
                for message in messages))
        # `optimize-baselines` commands are not useful to look at, since many
        # are no-ops anyways. We don't look at them here.
        self.assertEqual(self.tool.executive.calls, [])
        self.assertEqual(self.command.git_cl.calls, [])
        self.assertEqual(self.tool.filesystem.files, files_before)
        self.assertEqual(self.tool.git().added_paths, set())

    def test_execute_basic_resultDB(self):
        # By default, with no arguments or options, rebaseline-cl rebaselines
        # all of the tests that unexpectedly failed.
        for build in self.builds:
            self.tool.results_fetcher.set_results(build,
                                                  self.web_test_resultdb)
        with mock.patch('blinkpy.common.message_pool.get'):
            exit_code = self.command.execute(
                self.command_options(resultDB=True), [], self.tool)
            self.assertEqual(exit_code, 0)
            self.assertLog([
                'INFO: All builds finished.\n',
                'INFO: Rebaselining one/missing.html\n',
                'INFO: Rebaselining two/image-fail.html\n',
            ])

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

            one/does-not-exist.html
            one/text-fail.html
                two/image-fail.html   '''))
        exit_code = self.command.execute(
            self.command_options(test_name_file=test_name_file), [], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Reading list of tests to rebaseline from %s\n' %
            test_name_file,
            'INFO: Rebaselining one/flaky-fail.html\n',
            'INFO: Rebaselining one/missing.html\n',
            'INFO: Rebaselining one/text-fail.html\n',
            'INFO: Rebaselining two/image-fail.html\n',
        ])

    def test_execute_with_test_name_file_resultDB(self):
        fs = self.mac_port.host.filesystem
        test_name_file = fs.mktemp()
        fs.write_text_file(
            test_name_file,
            textwrap.dedent('''
            one/missing.html
              two/missing.html
            # one/slow-fail.html
            #

            ones/does-not-exist.html
                two/image-fail.html   '''))
        for build in self.builds:
            self.tool.results_fetcher.set_results(build,
                                                  self.web_test_resultdb)
        with mock.patch('blinkpy.common.message_pool.get'):
            exit_code = self.command.execute(
                self.command_options(test_name_file=test_name_file,
                                     resultDB=True), [], self.tool)
            self.assertEqual(exit_code, 0)
            self.assertLog([
                'INFO: All builds finished.\n',
                'INFO: Reading list of tests to rebaseline from %s\n' %
                test_name_file,
                'INFO: Rebaselining one/missing.html\n',
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
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_with_canceled_job(self):
        builds = {
            Build('MOCK Try Win', 5000, 'Build-1'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Mac', 4000, 'Build-2'):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Try Linux', 6000, 'Build-3'):
            TryJobStatus('COMPLETED', 'CANCELED'),
        }
        self.command.git_cl = MockGitCL(self.tool, builds)
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Linux\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
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
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
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
        # First write test contents to the mock filesystem so that
        # one/flaky-fail.html is considered a real test to rebaseline.
        port = self.tool.port_factory.get('test-win-win7')
        path = port.host.filesystem.join(port.web_tests_dir(),
                                         'one/flaky-fail.html')
        self._write(path, 'contents')
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('one/flaky-fail.html',
                              Build('MOCK Try Win', 5000, 'Build-1'),
                              'blink_web_tests (with patch)')
        self.command.rebaseline(self.command_options(), test_baseline_set)
        self.assertEqual(self.tool.executive.calls,
                         [[[
                             'python',
                             'echo',
                             'copy-existing-baselines-internal',
                             '--test',
                             'one/flaky-fail.html',
                             '--suffixes',
                             'wav',
                             '--port-name',
                             'test-win-win7',
                         ]],
                          [[
                              'python',
                              'echo',
                              'rebaseline-test-internal',
                              '--test',
                              'one/flaky-fail.html',
                              '--suffixes',
                              'wav',
                              '--port-name',
                              'test-win-win7',
                              '--builder',
                              'MOCK Try Win',
                              '--build-number',
                              '5000',
                              '--step-name',
                              'blink_web_tests (with patch)',
                          ]],
                          [
                              'python',
                              'echo',
                              'optimize-baselines',
                              '--no-manifest-update',
                              'one/flaky-fail.html',
                          ]])

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
        self.assertEqual(sorted(self.tool.executive.calls[0]), [
            [
                'python', 'echo', 'copy-existing-baselines-internal', '--test',
                'one/text-fail.html', '--suffixes', 'txt', '--port-name',
                'test-linux-trusty'
            ],
            [
                'python', 'echo', 'copy-existing-baselines-internal', '--test',
                'one/text-fail.html', '--suffixes', 'txt', '--port-name',
                'test-linux-trusty', '--flag-specific',
                'disable-site-isolation-trials'
            ],
        ])
        self.assertEqual(sorted(self.tool.executive.calls[1]), [
            [
                'python', 'echo', 'rebaseline-test-internal', '--test',
                'one/text-fail.html', '--suffixes', 'txt', '--port-name',
                'test-linux-trusty', '--builder',
                'MOCK Try Linux Multiple Steps', '--build-number', '9000',
                '--step-name', 'blink_web_tests (with patch)'
            ],
            [
                'python', 'echo', 'rebaseline-test-internal', '--test',
                'one/text-fail.html', '--suffixes', 'txt', '--port-name',
                'test-linux-trusty', '--flag-specific',
                'disable-site-isolation-trials', '--builder',
                'MOCK Try Linux Multiple Steps', '--build-number', '9000',
                '--step-name',
                'not_site_per_process_blink_web_tests (with patch)'
            ],
        ])
        self.assertEqual(self.tool.executive.calls[2], [
            'python', 'echo', 'optimize-baselines', '--no-manifest-update',
            'one/text-fail.html'
        ])

    def test_execute_missing_results_with_no_fill_missing_prompts(self):
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Win', 5000, 'Build-1'), None,
            'blink_web_tests (with patch)')
        exit_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: Failed to fetch some results for "MOCK Try Win".\n',
            ('WARNING: Results URL: https://test-results.appspot.com/data/layout_results/'
             'MOCK_Try_Win/5000/blink_web_tests%20%28with%20patch%29/layout-test-results/results.html\n'
             ),
            'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Win\n',
            'INFO: Would you like to continue?\n',
            'INFO: Aborting.\n',
        ])

    def test_execute_interrupted_results_with_fill_missing(self):
        build = Build('MOCK Try Win', 5000, 'Build-1')
        self.builds[build] = TryJobStatus.from_bb_status('INFRA_FAILURE')
        self.tool.user.set_canned_responses(['y', 'n'])
        exit_code = self.command.execute(self.command_options(),
                                         ['one/flaky-fail.html'], self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: Some builders have infrastructure failures:\n',
            'WARNING:   MOCK Try Win\n',
            'WARNING: Examples of infrastructure failures include:\n',
            'WARNING:   * Shard terminated the harness after timing out.\n',
            'WARNING:   * Harness exited early due to excessive unexpected failures.\n',
            'WARNING:   * Build failed on a non-test step.\n',
            'WARNING: Please consider retrying the failed builders or '
            'giving the builders more shards.\n',
            'WARNING: See https://chromium.googlesource.com/chromium/src/+/'
            'HEAD/docs/testing/web_test_expectations.md'
            '#rebaselining-using-try-jobs\n',
            'INFO: Would you like to continue?\n',
            'INFO: Would you like to try to fill in missing results '
            'with available results?\n'
            'Note: This is generally not suggested unless the results '
            'are platform agnostic.\n',
            'INFO: Please rebaseline again for builders '
            'with incomplete results later.\n',
            'INFO: Rebaselining one/flaky-fail.html\n',
        ])

    def test_execute_missing_results_with_fill_missing_continues(self):
        self.tool.results_fetcher.set_results(
            Build('MOCK Try Win', 5000, 'Build-1'), None,
            'blink_web_tests (with patch)')
        exit_code = self.command.execute(
            self.command_options(fill_missing=True), ['one/flaky-fail.html'],
            self.tool)
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'WARNING: Failed to fetch some results for "MOCK Try Win".\n',
            ('WARNING: Results URL: https://test-results.appspot.com/data/layout_results/'
             'MOCK_Try_Win/5000/blink_web_tests%20%28with%20patch%29/layout-test-results/results.html\n'
             ), 'WARNING: Some builders have no results:\n',
            'WARNING:   MOCK Try Win\n', 'INFO: For one/flaky-fail.html:\n',
            'INFO: Using "MOCK Try Linux" build 6000 for test-win-win7.\n',
            'INFO: Rebaselining one/flaky-fail.html\n'
        ])

    def test_fill_in_missing_results(self):
        test_baseline_set = TestBaselineSet(self.tool)
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
