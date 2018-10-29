# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.buildbot import Build
from blinkpy.common.net.git_cl import CLStatus
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.net.git_cl import TryJobStatus
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
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['try', '-b', 'win10_blink_rel'])
        self.assertEqual(
            host.executive.calls,
            [['git', 'cl', 'try', '-b', 'win10_blink_rel', '--auth-refresh-token-json', 'token.json']])

    def test_some_commands_not_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['issue'])
        self.assertEqual(host.executive.calls, [['git', 'cl', 'issue']])

    def test_trigger_try_jobs_with_list(self):
        # When no bucket is specified, master.tryserver.blink is used by
        # default. Besides, `git cl try` invocations are grouped by buckets.
        host = MockHost()
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.trigger_try_jobs(['android_blink_rel', 'fake_blink_try_linux', 'fake_blink_try_win'])
        self.assertEqual(host.executive.calls, [
            [
                'git', 'cl', 'try',
                '-b', 'fake_blink_try_linux', '-b', 'fake_blink_try_win',
                '--auth-refresh-token-json', 'token.json'
            ],
            [
                'git', 'cl', 'try',
                '-B', 'master.tryserver.chromium.android',
                '-b', 'android_blink_rel',
                '--auth-refresh-token-json', 'token.json'
            ],
        ])

    def test_trigger_try_jobs_with_frozenset(self):
        # The trigger_try_jobs method may be called with an immutable set.
        host = MockHost()
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.trigger_try_jobs(frozenset(['fake_blink_try_linux', 'fake_blink_try_win']))
        self.assertEqual(host.executive.calls, [
            [
                'git', 'cl', 'try',
                '-b', 'fake_blink_try_linux', '-b', 'fake_blink_try_win',
                '--auth-refresh-token-json', 'token.json'
            ],
        ])

    def test_trigger_try_jobs_with_explicit_bucket(self):
        # An explicit bucket overrides configured or default buckets.
        host = MockHost()
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.trigger_try_jobs(['fake_blink_try_linux', 'android_blink_rel'],
                                bucket='luci.dummy')
        self.assertEqual(host.executive.calls, [
            [
                'git', 'cl', 'try',
                '-B', 'luci.dummy',
                '-b', 'android_blink_rel', '-b', 'fake_blink_try_linux',
                '--auth-refresh-token-json', 'token.json'
            ],
        ])

    def test_fetch_raw_try_job_results(self):
        # Fetching raw try job results has a side effect of writing to and
        # reading from a temporary JSON file. This test method verifies the
        # command line used to fetch try job results.
        host = MockHost()
        host.filesystem.write_text_file(
            '/__im_tmp/tmp_0_/try-results.json', '{}')
        host.filesystem.write_text_file(
            '/__im_tmp/tmp_1_/try-results.json', '{}')
        git_cl = GitCL(host)
        git_cl.fetch_raw_try_job_results()
        git_cl.fetch_raw_try_job_results(patchset=7)
        self.assertEqual(host.executive.calls, [
            [
                'git', 'cl', 'try-results',
                '--json', '/__im_tmp/tmp_0_/try-results.json',
            ],
            [
                'git', 'cl', 'try-results',
                '--json', '/__im_tmp/tmp_1_/try-results.json',
                '--patchset', '7'
            ]
        ])

    def test_get_issue_number(self):
        host = MockHost()
        host.executive = MockExecutive(output='Foo\nIssue number: 12345 (http://crrev.com/12345)')
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
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
                'url': None,
            },
        ]
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
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.fetch_raw_try_job_results = lambda **_: []
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
        host = MockHost()
        host.executive = MockExecutive(output='closed')
        git_cl = GitCL(host)
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
                'url': None,
            },
        ]
        self.assertEqual(
            git_cl.wait_for_try_jobs(),
            CLStatus(
                status='closed',
                try_job_results={
                    Build('some-builder', None): TryJobStatus('STARTED', None),
                },
            )
        )
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for try jobs, timeout: 7200 seconds.\n')

    def test_wait_for_try_jobs_done(self):
        host = MockHost()
        host.executive = MockExecutive(output='lgtm')
        git_cl = GitCL(host)
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/100',
                ],
                'url': 'http://ci.chromium.org/b/8931586523737389552',
            },
        ]
        self.assertEqual(
            git_cl.wait_for_try_jobs(),
            CLStatus(
                status='lgtm',
                try_job_results={
                    Build('some-builder', 100): TryJobStatus('COMPLETED', 'FAILURE'),
                }
            )
        )
        self.assertEqual(
            host.stdout.getvalue(),
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

    def test_wait_for_closed_status_closed(self):
        host = MockHost()
        host.executive = MockExecutive(output='closed')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.wait_for_closed_status(), 'closed')
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for closed status, timeout: 1800 seconds.\n'
            'CL is closed.\n')

    def test_has_failing_try_results_empty(self):
        self.assertFalse(GitCL.some_failed({}))

    def test_has_failing_try_results_only_success_and_started(self):
        self.assertFalse(GitCL.some_failed({
            Build('some-builder', 90): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('some-builder', 100): TryJobStatus('STARTED'),
        }))

    def test_has_failing_try_results_with_failing_results(self):
        self.assertTrue(GitCL.some_failed({
            Build('some-builder', 1): TryJobStatus('COMPLETED', 'FAILURE'),
        }))

    def test_all_success_empty(self):
        self.assertTrue(GitCL.all_success({}))

    def test_all_success_true(self):
        self.assertTrue(GitCL.all_success({
            Build('some-builder', 1): TryJobStatus('COMPLETED', 'SUCCESS'),
        }))

    def test_all_success_with_started_build(self):
        self.assertFalse(GitCL.all_success({
            Build('some-builder', 1): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('some-builder', 2): TryJobStatus('STARTED'),
        }))

    def test_latest_try_jobs_cq_only(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'cq-a',
                'experimental': False,
                'result': None,
                'status': 'SCHEDULED',
                'tags': ['user_agent:cq'],
                'url': None,
            },
            {
                'builder_name': 'cq-b',
                'experimental': False,
                'result': None,
                'status': 'SCHEDULED',
                'tags': ['cq_experimental:false', 'user_agent:cq'],
                'url': None,
            },
            {
                'builder_name': 'cq-c',
                'experimental': True,
                'result': None,
                'status': 'SCHEDULED',
                'tags': ['cq_experimental:false', 'user_agent:cq'],
                'url': None,
            },
            {
                'builder_name': 'cq-a-experimental',
                'experimental': True,
                'result': None,
                'status': 'SCHEDULED',
                'tags': ['cq_experimental:true', 'user_agent:cq'],
                'url': None,
            },
            {
                'builder_name': 'cq-b-experimental',
                'experimental': False,
                'result': None,
                'status': 'SCHEDULED',
                'tags': ['cq_experimental:true', 'user_agent:cq'],
                'url': None,
            },
            {
                'builder_name': 'other-a',
                'experimental': False,
                'status': 'SCHEDULED',
                'result': None,
                'tags': ['user_agent:git_cl_try'],
                'url': None,
            },
            {
                'builder_name': 'other-b',
                'experimental': False,
                'status': 'SCHEDULED',
                'result': None,
                'tags': ['is_experimental:false', 'user_agent:git_cl_try'],
                'url': None,
            },
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(cq_only=True),
            {
                Build('cq-a'): TryJobStatus('SCHEDULED'),
                Build('cq-b'): TryJobStatus('SCHEDULED'),
                Build('cq-c'): TryJobStatus('SCHEDULED'),
            })

    def test_latest_try_jobs(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/100',
                ],
                'url': 'http://build.chromium.org/b/123123123132123123',
            },
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/90',
                ],
                'url': 'http://build.chromium.org/p/master/builders/builder-b/builds/90',
            },
            {
                'builder_name': 'builder-a',
                'status': 'SCHEDULED',
                'result': None,
                'url': None,
            },
            {
                'builder_name': 'builder-c',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/123',
                ],
                'url': 'http://ci.chromium.org/b/123123123123123123',
            },
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(['builder-a', 'builder-b']),
            {
                Build('builder-a'): TryJobStatus('SCHEDULED'),
                Build('builder-b', 100): TryJobStatus('COMPLETED', 'SUCCESS'),
            })

    def test_latest_try_jobs_started_build_luci_url(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-a',
                'status': 'STARTED',
                'result': None,
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/100',
                ],
                'url': 'http://ci.chromium.org/b/123123123123123',
            },
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(['builder-a']),
            {Build('builder-a', 100): TryJobStatus('STARTED')})

    def test_latest_try_jobs_failures(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-a',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'failure_reason': 'BUILD_FAILURE',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/100',
                ],
                'url': 'http://ci.chromium.org/b/123123123123123123',
            },
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'failure_reason': 'INFRA_FAILURE',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/200',
                ],
                'url': 'http://ci.chromium.org/b/1293871928371923719',
            },
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(['builder-a', 'builder-b']),
            {
                Build('builder-a', 100): TryJobStatus('COMPLETED', 'FAILURE'),
                Build('builder-b', 200): TryJobStatus('COMPLETED', 'FAILURE'),
            })

    def test_latest_try_jobs_ignores_swarming_task(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/10',
                ],
                'url': 'https://ci.chromium.org/b/123918239182739',
            },
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': ('https://ci.chromium.org/swarming/task/'
                        '1234abcd1234abcd?server=chromium-swarm.appspot.com'),
            }
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(['builder-b']),
            {
                Build('builder-b', 10): TryJobStatus('COMPLETED', 'SUCCESS'),
            })

    def test_filter_latest(self):
        try_job_results = {
            Build('builder-a', 100): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('builder-a', 200): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('builder-b', 50): TryJobStatus('SCHEDULED'),
        }
        self.assertEqual(
            GitCL.filter_latest(try_job_results),
            {
                Build('builder-a', 200): TryJobStatus('COMPLETED', 'SUCCESS'),
                Build('builder-b', 50): TryJobStatus('SCHEDULED'),
            })

    def test_filter_latest_none(self):
        self.assertIsNone(GitCL.filter_latest(None))

    def test_try_job_results_url_format_fallback(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-a',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'tags': [
                    'build_address:luci.chromium.try/chromium_presubmit/100',
                ],
                'url': 'http://ci.chromium.org/p/master/builders/builder-b/builds/10',
            },
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'url': 'http://ci.chromium.org/p/master/builders/builder-b/builds/20',
            },
            {
                'builder_name': 'builder-c',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'url': 'https://ci.chromium.org/swarming/task/36a767f405d9ee10',
            },
        ]
        self.assertEqual(
            git_cl.try_job_results(),
            {
                Build('builder-a', 100): TryJobStatus('COMPLETED', 'FAILURE'),
                Build('builder-b', 20): TryJobStatus('COMPLETED', 'FAILURE'),
                Build('builder-c', '36a767f405d9ee10'): TryJobStatus('COMPLETED', 'FAILURE'),
            })

    def test_try_job_results_with_swarming_url_with_query(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': ('https://ci.chromium.org/swarming/task/'
                        '38740befcd9c0010?server=chromium-swarm.appspot.com'),
            },
        ]
        self.assertEqual(
            git_cl.try_job_results(),
            {
                Build('builder-b', '38740befcd9c0010'): TryJobStatus('COMPLETED', 'SUCCESS'),
            })

    def test_try_job_results_with_unexpected_url_format(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_raw_try_job_results = lambda **_: [
            {
                'builder_name': 'builder-a',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'failure_reason': 'BUILD_FAILURE',
                'url': 'https://example.com/',
            },
        ]
        # We try to parse a build number or task ID from the URL.
        with self.assertRaisesRegexp(AssertionError, 'https://example.com/ did not match expected format'):
            git_cl.try_job_results()
        # We ignore builders that we explicitly don't care about;
        # so if we only care about other-builder, not builder-a,
        # then no exception is raised.
        self.assertEqual(git_cl.try_job_results(['other-builder']), {})
