# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.rpc import (RESPONSE_PREFIX as
                                    SEARCHBUILDS_RESPONSE_PREFIX)
from blinkpy.common.net.git_cl import CLStatus, CLSummary
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.net.git_cl import BuildStatus
from blinkpy.common.net.web_mock import MockWeb
from blinkpy.common.system.executive_mock import MockExecutive


class GitCLTest(unittest.TestCase):
    def test_run(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host)
        output = git_cl.run(['command'])
        self.assertEqual(output, 'mock-output')
        self.assertEqual(host.executive.calls, [['git', 'cl', 'command']])

    def test_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host)
        git_cl.run(['try', '-b', 'win10_blink_rel'])
        self.assertEqual(host.executive.calls, [[
            'git',
            'cl',
            'try',
            '-b',
            'win10_blink_rel',
        ]])

    def test_some_commands_not_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host)
        git_cl.run(['issue'])
        self.assertEqual(host.executive.calls, [['git', 'cl', 'issue']])

    def test_trigger_try_jobs_with_list(self):
        # When no bucket is specified, luci.chromium.try is used by
        # default. Besides, `git cl try` invocations are grouped by buckets.
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.trigger_try_jobs([
            'android_blink_rel', 'fake_blink_try_linux', 'fake_blink_try_win'
        ])
        self.assertEqual(host.executive.calls, [
            [
                'git',
                'cl',
                'try',
                '-B',
                'luci.chromium.try',
                '-b',
                'fake_blink_try_linux',
                '-b',
                'fake_blink_try_win',
            ],
            [
                'git',
                'cl',
                'try',
                '-B',
                'luci.chromium.android',
                '-b',
                'android_blink_rel',
            ],
        ])

    def test_trigger_try_jobs_with_frozenset(self):
        # The trigger_try_jobs method may be called with an immutable set.
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.trigger_try_jobs(
            frozenset(['fake_blink_try_linux', 'fake_blink_try_win']))
        self.assertEqual(host.executive.calls, [
            [
                'git',
                'cl',
                'try',
                '-B',
                'luci.chromium.try',
                '-b',
                'fake_blink_try_linux',
                '-b',
                'fake_blink_try_win',
            ],
        ])

    def test_trigger_try_jobs_with_explicit_bucket(self):
        # An explicit bucket overrides configured or default buckets.
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.trigger_try_jobs(['fake_blink_try_linux', 'android_blink_rel'],
                                bucket='luci.dummy')
        self.assertEqual(host.executive.calls, [
            [
                'git',
                'cl',
                'try',
                '-B',
                'luci.dummy',
                '-b',
                'android_blink_rel',
                '-b',
                'fake_blink_try_linux',
            ],
        ])

    def test_get_issue_number(self):
        host = MockHost()
        host.executive = MockExecutive(
            output='Foo\nIssue number: 12345 (http://crrev.com/12345)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), '12345')

    def test_get_issue_number_none(self):
        host = MockHost()
        host.executive = MockExecutive(output='Issue number: None (None)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), 'None')

    def test_get_issue_number_nothing_in_output(self):
        host = MockHost()
        host.executive = MockExecutive(output='Bogus output')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), 'None')

    def test_wait_for_try_jobs_timeout(self):
        response = {
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                "builds": [
                    {
                        "status": "STARTED",
                        "builder": {
                            "builder": "some-builder"
                        },
                        "number": 100
                    }
                ]
            }"""
        }
        # Specify the same response 10 times to ensure each poll gets ones.
        web = MockWeb(responses=[response] * 10)
        host = MockHost(web=web)
        host.executive = MockExecutive(output='dry-run\n')
        git_cl = GitCL(host)
        self.assertIsNone(git_cl.wait_for_try_jobs())
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for try jobs, timeout: 7200 seconds.\n'
            'Waiting for try jobs. 600 seconds passed.\n'
            'Waiting for try jobs. 1800 seconds passed.\n'
            'Waiting for try jobs. 3000 seconds passed.\n'
            'Waiting for try jobs. 4200 seconds passed.\n'
            'Waiting for try jobs. 5400 seconds passed.\n'
            'Waiting for try jobs. 6600 seconds passed.\n'
            'Timed out waiting for try jobs.\n')

    def test_wait_for_try_jobs_no_results_not_considered_finished(self):
        response = {
            'status_code': 200,
            'body': SEARCHBUILDS_RESPONSE_PREFIX + b"{}"
        }
        # Specify the same response 10 times to ensure each poll gets ones.
        web = MockWeb(responses=[response] * 10)
        host = MockHost(web=web)
        host.executive = MockExecutive(output='dry-run\n')
        git_cl = GitCL(host)
        self.assertIsNone(git_cl.wait_for_try_jobs())
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for try jobs, timeout: 7200 seconds.\n'
            'Waiting for try jobs. 600 seconds passed.\n'
            'Waiting for try jobs. 1800 seconds passed.\n'
            'Waiting for try jobs. 3000 seconds passed.\n'
            'Waiting for try jobs. 4200 seconds passed.\n'
            'Waiting for try jobs. 5400 seconds passed.\n'
            'Waiting for try jobs. 6600 seconds passed.\n'
            'Timed out waiting for try jobs.\n')

    def test_wait_for_try_jobs_cl_closed(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "STARTED",
                            "builder": {
                                "builder": "some-builder"
                            }
                        }
                    ]
                }"""
        }])
        host = MockHost(web=web)
        host.executive = MockExecutive(output='closed')
        git_cl = GitCL(host)
        self.assertEqual(
            git_cl.wait_for_try_jobs(),
            CLSummary(
                status=CLStatus.CLOSED,
                try_job_results={
                    Build('some-builder', None): BuildStatus.STARTED,
                },
            ))
        self.assertEqual(host.stdout.getvalue(),
                         'Waiting for try jobs, timeout: 7200 seconds.\n')

    def test_wait_for_try_jobs_done(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "FAILURE",
                            "builder": {
                                "builder": "some-builder"
                            },
                            "number": 100
                        }
                    ]
                }"""
        }])
        host = MockHost(web=web)
        host.executive = MockExecutive(output='lgtm')
        git_cl = GitCL(host)
        self.assertEqual(
            git_cl.wait_for_try_jobs(),
            CLSummary(status=CLStatus.LGTM,
                      try_job_results={
                          Build('some-builder', 100): BuildStatus.FAILURE,
                      }))
        self.assertEqual(host.stdout.getvalue(),
                         'Waiting for try jobs, timeout: 7200 seconds.\n')

    def test_wait_for_closed_status_timeout(self):
        host = MockHost()
        host.executive = MockExecutive(output='commit')
        git_cl = GitCL(host)
        self.assertIsNone(git_cl.wait_for_closed_status())
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for closed status, timeout: 1800 seconds.\n'
            'Waiting for closed status. 120 seconds passed.\n'
            'Waiting for closed status. 360 seconds passed.\n'
            'Waiting for closed status. 600 seconds passed.\n'
            'Waiting for closed status. 840 seconds passed.\n'
            'Waiting for closed status. 1080 seconds passed.\n'
            'Waiting for closed status. 1320 seconds passed.\n'
            'Waiting for closed status. 1560 seconds passed.\n'
            'Waiting for closed status. 1800 seconds passed.\n'
            'Timed out waiting for closed status.\n')

    def test_wait_for_closed_status_timeout_async(self):
        # 1400s of the timeout has already elapsed, so only busy-wait 400s.
        host = MockHost(time_return_val=1400)
        host.executive = MockExecutive(output='commit')
        git_cl = GitCL(host)
        self.assertIsNone(
            git_cl.wait_for_closed_status(poll_delay_seconds=100, start=0))
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for closed status, timeout: 1800 seconds.\n'
            'Waiting for closed status. 1500 seconds passed.\n'
            'Waiting for closed status. 1700 seconds passed.\n'
            'Timed out waiting for closed status.\n')

    def test_wait_for_closed_status_timeout_already_expired(self):
        host = MockHost(time_return_val=2000)
        host.executive = MockExecutive(output='closed')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.wait_for_closed_status(start=0),
                         CLStatus.CLOSED)
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for closed status, timeout: 1800 seconds.\n'
            'Timed out waiting for closed status.\n'
            'CL is closed.\n')

    def test_wait_for_closed_status_closed(self):
        host = MockHost()
        host.executive = MockExecutive(output='closed')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.wait_for_closed_status(), CLStatus.CLOSED)
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for closed status, timeout: 1800 seconds.\n'
            'CL is closed.\n')

    def test_has_failing_try_results_empty(self):
        self.assertFalse(GitCL.some_failed({}))

    def test_has_failing_try_results_only_success_and_started(self):
        self.assertFalse(
            GitCL.some_failed({
                Build('some-builder', 90): BuildStatus.SUCCESS,
                Build('some-builder', 100): BuildStatus.STARTED,
            }))

    def test_has_failing_try_results_with_failing_results(self):
        self.assertTrue(
            GitCL.some_failed({
                Build('some-builder', 1): BuildStatus.FAILURE,
            }))

    def test_all_success_empty(self):
        self.assertTrue(GitCL.all_success({}))

    def test_all_success_true(self):
        self.assertTrue(
            GitCL.all_success({
                Build('some-builder', 1): BuildStatus.SUCCESS,
            }))

    def test_all_success_with_started_build(self):
        self.assertFalse(
            GitCL.all_success({
                Build('some-builder', 1): BuildStatus.SUCCESS,
                Build('some-builder', 2): BuildStatus.STARTED,
            }))

    def test_latest_try_jobs_cq_only(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "cq-a"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "cq"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "cq-b"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "cq"},
                                {"key": "cq_experimental", "value": "false"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "cq-c"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "cq"},
                                {"key": "cq_experimental", "value": "false"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "cq-a-experimental"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "cq"},
                                {"key": "cq_experimental", "value": "true"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "cq-b-experimental"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "cq"},
                                {"key": "cq_experimental", "value": "true"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "other-a"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "git_cl_try"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "other-b"
                            },
                            "tags": [
                                {"key": "user_agent", "value": "git_cl_try"},
                                {"key": "cq_experimental", "value": "false"}
                            ]
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(
            git_cl.latest_try_jobs(cq_only=True), {
                Build('cq-a'): BuildStatus.SCHEDULED,
                Build('cq-b'): BuildStatus.SCHEDULED,
                Build('cq-c'): BuildStatus.SCHEDULED,
            })

    def test_latest_try_jobs(self):
        # Here we have multiple builds with the same name, but we only take the
        # latest one (based on build number).
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-b"
                            },
                            "number": 100,
                            "id": "100"
                        },
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-b"
                            },
                            "number": 90,
                            "id": "90"
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "builder-a"
                            }
                        },
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-c"
                            },
                            "number": 123,
                            "id": "123"
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(
            git_cl.latest_try_jobs(builder_names=['builder-a', 'builder-b']), {
                Build('builder-a'): BuildStatus.SCHEDULED,
                Build('builder-b', 100, "100"): BuildStatus.SUCCESS,
            })

    def test_latest_try_jobs_started(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "STARTED",
                            "builder": {
                                "builder": "builder-a"
                            },
                            "number": 100
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(git_cl.latest_try_jobs(builder_names=['builder-a']),
                         {Build('builder-a', 100): BuildStatus.STARTED})

    def test_latest_try_jobs_failures(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "FAILURE",
                            "builder": {
                                "builder": "builder-a"
                            },
                            "number": 100
                        },
                        {
                            "status": "INFRA_FAILURE",
                            "builder": {
                                "builder": "builder-b"
                            },
                            "number": 200
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(
            git_cl.latest_try_jobs(builder_names=['builder-a', 'builder-b']), {
                Build('builder-a', 100): BuildStatus.FAILURE,
                Build('builder-b', 200): BuildStatus.INFRA_FAILURE,
            })

    def test_filter_latest(self):
        try_job_results = {
            Build('builder-a', 100): BuildStatus.FAILURE,
            Build('builder-a', 200): BuildStatus.SUCCESS,
            Build('builder-b', 50): BuildStatus.SCHEDULED,
        }
        self.assertEqual(
            GitCL.filter_latest(try_job_results), {
                Build('builder-a', 200): BuildStatus.SUCCESS,
                Build('builder-b', 50): BuildStatus.SCHEDULED,
            })

    def test_filter_latest_none(self):
        self.assertIsNone(GitCL.filter_latest(None))

    def test_try_job_results_with_other_builder(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "FAILURE",
                            "builder": {
                                "builder": "builder-a"
                            },
                            "number": 100,
                            "tags": [
                                {"key": "user_agent", "value": "cq"}
                            ]
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        # We ignore builders that we explicitly don't care about;
        # so if we only care about other-builder, not builder-a,
        # then no exception is raised.
        self.assertEqual(
            git_cl.try_job_results(builder_names=['other-builder']), {})

    def test_try_job_results(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-a"
                            },
                            "number": 111,
                            "tags": [
                                {"key": "user_agent", "value": "cq"}
                            ]
                        },
                        {
                            "status": "SCHEDULED",
                            "builder": {
                                "builder": "builder-b"
                            },
                            "number": 222
                        },
                        {
                            "status": "INFRA_FAILURE",
                            "builder": {
                                "builder": "builder-c"
                            },
                            "number": 333
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(
            git_cl.try_job_results(issue_number=None), {
                Build('builder-a', 111): BuildStatus.SUCCESS,
                Build('builder-b', 222): BuildStatus.SCHEDULED,
                Build('builder-c', 333): BuildStatus.INFRA_FAILURE,
            })

    def test_try_job_results_skip_experimental_cq(self):
        web = MockWeb(responses=[{
            'status_code':
            200,
            'body':
            SEARCHBUILDS_RESPONSE_PREFIX + b"""{
                    "builds": [
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-a"
                            },
                            "number": 111,
                            "tags": [
                                {"key": "user_agent", "value": "cq"}
                            ]
                        },
                        {
                            "status": "SUCCESS",
                            "builder": {
                                "builder": "builder-b"
                            },
                            "number": 222,
                            "tags": [
                                {"key": "user_agent", "value": "cq"},
                                {"key": "cq_experimental", "value": "true"}
                            ]
                        }
                    ]
                }"""
        }])
        git_cl = GitCL(MockHost(web=web))
        self.assertEqual(
            # Only one build appears - builder-b is ignored because it is
            # experimental.
            git_cl.try_job_results(issue_number=None, cq_only=True),
            {
                Build('builder-a', 111): BuildStatus.SUCCESS,
            })
