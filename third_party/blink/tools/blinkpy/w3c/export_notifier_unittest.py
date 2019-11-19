# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.gerrit_mock import MockGerritAPI, MockGerritCL
from blinkpy.w3c.export_notifier import ExportNotifier, PRStatusInfo
from blinkpy.w3c.wpt_github import PullRequest
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub


class ExportNotifierTest(LoggingTestCase):

    def setUp(self):
        super(ExportNotifierTest, self).setUp()
        self.host = MockHost()
        self.git = self.host.git()
        self.gerrit = MockGerritAPI()
        self.notifier = ExportNotifier(self.host, self.git, self.gerrit)

    def test_from_gerrit_comment_success(self):
        gerrit_comment = ('The exported PR for the current patch failed Taskcluster check(s) '
                          'on GitHub, which could indict cross-broswer failures on the '
                          'exportable changes. Please contact ecosystem-infra@ team for '
                          'more information.\n\n'
                          'Taskcluster Node ID: foo\n'
                          'Taskcluster Link: bar\n'
                          'Gerrit CL SHA: num')

        actual = PRStatusInfo.from_gerrit_comment(gerrit_comment)

        self.assertEqual(actual.node_id, 'foo')
        self.assertEqual(actual.link, 'bar')
        self.assertEqual(actual.gerrit_sha, 'num')

    def test_from_gerrit_comment_missing_info(self):
        gerrit_comment = ('The exported PR for the current patch failed Taskcluster check(s) '
                          'on GitHub, which could indict cross-broswer failures on the '
                          'exportable changes. Please contact ecosystem-infra@ team for '
                          'more information.\n\n'
                          'Taskcluster Node ID: \n'
                          'Taskcluster Link: bar\n'
                          'Gerrit CL SHA: num')

        actual = PRStatusInfo.from_gerrit_comment(gerrit_comment)

        self.assertIsNone(actual)

    def test_from_gerrit_comment_fail(self):
        gerrit_comment = 'ABC'

        actual = PRStatusInfo.from_gerrit_comment(gerrit_comment)

        self.assertIsNone(actual)

    def test_to_gerrit_comment(self):
        pr_status_info = PRStatusInfo('foo', 'bar', 'num')
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: num')

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_latest(self):
        pr_status_info = PRStatusInfo('foo', 'bar', None)
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: Latest')

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_with_patchset(self):
        pr_status_info = PRStatusInfo('foo', 'bar', 'num')
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: num\n'
                    'Patchset Number: 3')

        actual = pr_status_info.to_gerrit_comment(3)

        self.assertEqual(expected, actual)

    def test_get_failure_taskcluster_status_success(self):
        taskcluster_status = [
            {
                "state": "failure",
                "context": "Community-TC (pull_request)",
            },
            {
                "state": "success",
                "context": "random",
            }
        ]

        self.assertEqual(
            self.notifier.get_failure_taskcluster_status(
                taskcluster_status, 123),
            {
                "state": "failure",
                "context": "Community-TC (pull_request)",
            }
        )

    def test_get_failure_taskcluster_status_fail(self):
        taskcluster_status = [
            {
                "state": "success",
                "context": "Community-TC (pull_request)",
            },
        ]

        self.assertEqual(self.notifier.get_failure_taskcluster_status(
            taskcluster_status, 123), None)

    def test_has_latest_taskcluster_status_commented_false(self):
        pr_status_info = PRStatusInfo('foo', 'bar', 'num')
        messages = [
            {
                "date": "2019-08-20 17:42:05.000000000",
                "message": "Uploaded patch set 1.\nInitial upload",
                "_revision_number": 1
            }
        ]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertFalse(actual)

    def test_has_latest_taskcluster_status_commented_true(self):
        pr_status_info = PRStatusInfo('foo', 'bar', 'num')
        messages = [
            {
                "date": "2019-08-20 17:42:05.000000000",
                "message": "Uploaded patch set 1.\nInitial upload",
                "_revision_number": 1
            },
            {
                "date": "2019-08-21 17:41:05.000000000",
                "message": ('The exported PR for the current patch failed Taskcluster check(s) '
                            'on GitHub, which could indict cross-broswer failures on the '
                            'exportable changes. Please contact ecosystem-infra@ team for '
                            'more information.\n\n'
                            'Taskcluster Node ID: foo\n'
                            'Taskcluster Link: bar\n'
                            'Gerrit CL SHA: num\n'
                            'Patchset Number: 3'),
                "_revision_number": 2
            },
        ]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertTrue(actual)

        self.assertTrue(actual)

    def test_get_taskcluster_statuses_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: 1',
                        state='open', labels=[]),
        ])
        status = [
            {
                "description": "foo"
            }
        ]
        self.notifier.wpt_github.status = status
        actual = self.notifier.get_taskcluster_statuses(123)

        self.assertEqual(actual, status)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'get_pr_branch',
            'get_branch_statuses',
        ])

    def test_process_failing_prs_success(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date": "2019-08-21 17:41:05.000000000",
                        "message": ('The exported PR for the current patch failed Taskcluster check(s) '
                                    'on GitHub, which could indict cross-broswer failures on the '
                                    'exportable changes. Please contact ecosystem-infra@ team for '
                                    'more information.\n\n'
                                    'Taskcluster Node ID: notfoo\n'
                                    'Taskcluster Link: bar\n'
                                    'Gerrit CL SHA: notnum\n'
                                    'Patchset Number: 3'),
                        "_revision_number": 2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit
        )
        gerrit_dict = {'abc': PRStatusInfo('foo', 'bar', 'num')}
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: num\n'
                    'Patchset Number: 1')

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [
            ('/a/changes/abc/revisions/current/review', {'message': expected})])

    def test_process_failing_prs_has_commented(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date": "2019-08-21 17:41:05.000000000",
                        "message": ('The exported PR for the current patch failed Taskcluster check(s) '
                                    'on GitHub, which could indict cross-broswer failures on the '
                                    'exportable changes. Please contact ecosystem-infra@ team for '
                                    'more information.\n\n'
                                    'Taskcluster Node ID: foo\n'
                                    'Taskcluster Link: bar\n'
                                    'Gerrit CL SHA: notnum\n'
                                    'Patchset Number: 3'),
                        "_revision_number": 2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit
        )
        gerrit_dict = {'abc': PRStatusInfo('foo', 'bar', 'num')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])

    def test_process_failing_prs_with_latest_sha(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date": "2019-08-21 17:41:05.000000000",
                        "message": ('The exported PR for the current patch failed Taskcluster check(s) '
                                    'on GitHub, which could indict cross-broswer failures on the '
                                    'exportable changes. Please contact ecosystem-infra@ team for '
                                    'more information.\n\n'
                                    'Taskcluster Node ID: not foo\n'
                                    'Taskcluster Link: bar\n'
                                    'Gerrit CL SHA: notnum\n'
                                    'Patchset Number: 3'),
                        "_revision_number": 2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit
        )
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: Latest')
        gerrit_dict = {'abc': PRStatusInfo('foo', 'bar', None)}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [
            ('/a/changes/abc/revisions/current/review', {'message': expected})])

    def test_process_failing_prs_raise_gerrit_error(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI(raise_error=True)
        gerrit_dict = {'abc': PRStatusInfo('foo', 'bar', 'num')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])
        self.assertLog(
            ['INFO: Processing 1 CLs with failed Taskcluster status.\n',
             'ERROR: Could not process Gerrit CL abc: Error from query_cl\n'])

    def test_export_notifier_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[])
        self.notifier.wpt_github.recent_failing_pull_requests = [
            PullRequest(title='title1', number=1234,
                        body='description\nWPT-Export-Revision: hash\nChange-Id: decafbad',
                        state='open', labels=[''])]
        status = [
            {
                "state": "failure",
                "context": "Community-TC (pull_request)",
                "node_id": "foo",
                "target_url": "bar"
            }
        ]
        self.notifier.wpt_github.status = status

        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'decafbad',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date": "2019-08-21 17:41:05.000000000",
                        "message": ('The exported PR for the current patch failed Taskcluster check(s) '
                                    'on GitHub, which could indict cross-broswer failures on the '
                                    'exportable changes. Please contact ecosystem-infra@ team for '
                                    'more information.\n\n'
                                    'Taskcluster Node ID: notfoo\n'
                                    'Taskcluster Link: bar\n'
                                    'Gerrit CL SHA: notnum\n'
                                    'Patchset Number: 3'),
                        "_revision_number": 2
                    },
                ],
                'revisions': {
                    'hash': {
                        '_number': 2
                    }
                }
            },
            api=self.notifier.gerrit
        )
        expected = ('The exported PR for the current patch failed Taskcluster check(s) '
                    'on GitHub, which could indict cross-broswer failures on the '
                    'exportable changes. Please contact ecosystem-infra@ team for '
                    'more information.\n\n'
                    'Taskcluster Node ID: foo\n'
                    'Taskcluster Link: bar\n'
                    'Gerrit CL SHA: hash\n'
                    'Patchset Number: 2')

        exit_code = self.notifier.main()

        self.assertFalse(exit_code)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'recent_failing_chromium_exports',
            'get_pr_branch',
            'get_branch_statuses',
        ])
        self.assertEqual(self.notifier.gerrit.cls_queried, ['decafbad'])
        self.assertEqual(self.notifier.gerrit.request_posted, [
            ('/a/changes/decafbad/revisions/current/review', {'message': expected})])
