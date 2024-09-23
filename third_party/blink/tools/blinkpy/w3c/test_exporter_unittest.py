# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.chromium_commit_mock import MockChromiumCommit
from blinkpy.w3c.gerrit import GerritError
from blinkpy.w3c.gerrit_mock import MockGerritAPI, MockGerritCL
from blinkpy.w3c.test_exporter import TestExporter
from blinkpy.w3c.wpt_github import PullRequest
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub


class TestExporterTest(LoggingTestCase):
    def setUp(self):
        super(TestExporterTest, self).setUp()
        host = MockHost()
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps({
                'GH_USER': 'github-username',
                'GH_TOKEN': 'github-token',
                'GERRIT_USER': 'gerrit-username',
                'GERRIT_TOKEN': 'gerrit-token',
            }))
        self.host = host

    def test_dry_run_stops_before_creating_pr(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='',
                        state='open',
                        node_id='PR_123_',
                        labels=[]),
        ])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.exportable_cls = [
            MockGerritCL(data={
                'change_id': 'I001',
                'subject': 'subject',
                '_number': 1234,
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers'
                    }
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(self.host,
                                                            subject='subject',
                                                            body='fake body',
                                                            change_id='I001'))
        ]
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458475}'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458476}'),
        ], [])
        success = test_exporter.main(
            ['--credentials-json', '/tmp/credentials.json', '--dry-run'])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_with_change_id',
            'pr_for_chromium_commit',
            'pr_for_chromium_commit',
        ])
        self.assertEqual(len(test_exporter.gerrit.request_posted), 0)

    def test_creates_pull_request_for_all_exportable_commits(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(
            pull_requests=[], create_pr_fail_index=1)
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#1}', change_id='I001', subject='subject 1', body='body 1'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#2}', change_id='I002', subject='subject 2', body='body 2'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#3}', change_id='I003', subject='subject 3', body='body 3'),
        ], [])
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(
            test_exporter.github.calls,
            [
                # 1
                'pr_for_chromium_commit',
                'create_pr',
                'add_label "chromium-export"',
                # 2
                'pr_for_chromium_commit',
                'create_pr',
                'add_label "chromium-export"',
                # 3
                'pr_for_chromium_commit',
                'create_pr',
                'add_label "chromium-export"',
            ])
        self.assertEqual(test_exporter.github.pull_requests_created, [
            ('chromium-export-7db6c89e05', 'subject 1',
             'body 1\n\nChange-Id: I001\n'),
            ('chromium-export-f8c201ca95', 'subject 3',
             'body 3\n\nChange-Id: I003\n'),
        ])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests created:
                * https://github.com/web-platform-tests/wpt/pull/5678
                * https://github.com/web-platform-tests/wpt/pull/5679
                * https://github.com/web-platform-tests/wpt/pull/5680

                """))

    def test_creates_and_merges_pull_requests(self):
        # This tests 4 exportable commits:
        # 1. #458475 has a provisional in-flight PR associated with it. The PR needs to be updated but not merged.
        # 2. #458476 has no PR associated with it and should have one created.
        # 3. #458477 has a closed PR associated with it and should be skipped.
        # 4. #458478 has an in-flight PR associated with it and should be merged successfully.
        # 5. #458479 has an in-flight PR associated with it but can not be merged.
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(
            pull_requests=[
                PullRequest(
                    title='Open PR',
                    number=1234,
                    body=
                    'rutabaga\nCr-Commit-Position: refs/heads/main@{#458475}\nChange-Id: I0005',
                    state='open',
                    node_id='PR_123_',
                    labels=['do not merge yet']),
                PullRequest(
                    title='Merged PR',
                    number=2345,
                    body=
                    'rutabaga\nCr-Commit-Position: refs/heads/main@{#458477}\nChange-Id: Idead',
                    state='closed',
                    node_id='PR_123_',
                    labels=[]),
                PullRequest(
                    title='Open PR',
                    number=3456,
                    body=
                    'rutabaga\nCr-Commit-Position: refs/heads/main@{#458478}\nChange-Id: I0118',
                    state='open',
                    node_id='PR_123_',
                    labels=[]  # It's important that this is empty.
                ),
                PullRequest(
                    title='Open PR',
                    number=4747,
                    body=
                    'rutabaga\nCr-Commit-Position: refs/heads/main@{#458479}\nChange-Id: I0147',
                    state='open',
                    node_id='PR_123_',
                    labels=[]  # It's important that this is empty.
                ),
            ],
            unsuccessful_merge_index=3)  # Mark the last PR as unmergable.
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458475}', change_id='I0005'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458476}', change_id='I0476'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458477}', change_id='Idead'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458478}', change_id='I0118'),
            MockChromiumCommit(
                self.host, position='refs/heads/main@{#458479}', change_id='I0147'),
        ], [])
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(
            test_exporter.github.calls,
            [
                # 1. #458475
                'pr_for_chromium_commit',
                'get_pr_branch',
                'update_pr',
                'remove_label "do not merge yet"',
                # 2. #458476
                'pr_for_chromium_commit',
                'create_pr',
                'add_label "chromium-export"',
                # 3. #458477
                'pr_for_chromium_commit',
                # 4. #458478
                'pr_for_chromium_commit',
                # Testing the lack of remove_label here. The exporter should not
                # try to remove the provisional label from PRs it has already
                # removed it from.
                'get_pr_branch',
                'merge_pr',
                # 5. #458479
                'pr_for_chromium_commit',
                'get_pr_branch',
                'merge_pr',
            ])
        self.assertEqual(test_exporter.github.pull_requests_created, [
            ('chromium-export-981776f989', 'Fake subject',
             'Fake body\n\nChange-Id: I0476\n'),
        ])
        self.assertEqual(test_exporter.github.pull_requests_merged, [3456])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests created:
                * https://github.com/web-platform-tests/wpt/pull/5678

                Pull requests that failed to merge:
                * https://github.com/web-platform-tests/wpt/pull/4747

                Pull requests marked as ready for review:
                * https://github.com/web-platform-tests/wpt/pull/1234

                Pull requests merged:
                * https://github.com/web-platform-tests/wpt/pull/3456

                """))

    def test_new_gerrit_cl(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.exportable_cls = [
            MockGerritCL(data={
                'change_id': 'I001',
                'subject': 'subject',
                '_number': 1234,
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers'
                    }
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(
                             self.host,
                             subject='subject',
                             body='fake body <html>',
                             change_id='I001')),
            MockGerritCL(data={
                'change_id': 'I002',
                'subject': 'subject',
                '_number': 1235,
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers'
                    }
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(self.host,
                                                            subject='subject',
                                                            body='body',
                                                            change_id=None)),
        ]
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_with_change_id',
            'create_pr',
            'add_label "chromium-export"',
            'add_label "do not merge yet"',
            'pr_with_change_id',
            'create_pr',
            'add_label "chromium-export"',
            'add_label "do not merge yet"',
        ])
        self.assertEqual(test_exporter.github.pull_requests_created, [
            ('chromium-export-cl-1234', 'subject',
             'fake body \\<html>\n\nChange-Id: I001\nReviewed-on: '
             'https://chromium-review.googlesource.com/1234\n'
             'WPT-Export-Revision: 1'),
            ('chromium-export-cl-1235', 'subject',
             'body\nChange-Id: I002\nReviewed-on: '
             'https://chromium-review.googlesource.com/1235\n'
             'WPT-Export-Revision: 1'),
        ])
        self.assertEqual(test_exporter.github.pull_requests_merged, [])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests created:
                * https://github.com/web-platform-tests/wpt/pull/5678
                * https://github.com/web-platform-tests/wpt/pull/5679

                """))

    def test_gerrit_cl_no_update_if_pr_with_same_revision(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='description\nWPT-Export-Revision: 1',
                        state='open',
                        node_id='PR_123_',
                        labels=[]),
        ])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.exportable_cls = [
            MockGerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': 1,
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers'
                    }
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(self.host))
        ]
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_with_change_id',
        ])
        self.assertEqual(test_exporter.github.pull_requests_created, [])
        self.assertEqual(test_exporter.github.pull_requests_merged, [])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            'No pull requests modified.\n')

    def test_gerrit_cl_updates_if_cl_has_new_revision(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='description\nWPT-Export-Revision: 1',
                        state='open',
                        node_id='PR_123_',
                        labels=[]),
        ])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.exportable_cls = [
            MockGerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': 1,
                'current_revision': '2',
                'has_review_started': True,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers 1',
                        'description': 'subject 1',
                    },
                    '2': {
                        'commit_with_footers': 'a commit with footers 2',
                        'description': 'subject 2',
                    },
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(self.host))
        ]
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_with_change_id',
            'get_pr_branch',
            'update_pr',
        ])
        self.assertEqual(test_exporter.github.pull_requests_created, [])
        self.assertEqual(test_exporter.github.pull_requests_merged, [])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests updated to a new revision:
                * https://github.com/web-platform-tests/wpt/pull/1234

                """))

    def test_attempts_to_merge_landed_gerrit_cl(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[
            PullRequest(
                title='title1',
                number=1234,
                body='description\nWPT-Export-Revision: 9\nChange-Id: decafbad',
                state='open',
                node_id='PR_123_',
                labels=['do not merge yet']),
        ])
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(self.host, change_id='decafbad'), ], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_for_chromium_commit',
            'get_pr_branch',
            'update_pr',
            'remove_label "do not merge yet"',
        ])
        self.assertEqual(test_exporter.github.pull_requests_created, [])
        self.assertEqual(test_exporter.github.pull_requests_merged, [])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests marked as ready for review:
                * https://github.com/web-platform-tests/wpt/pull/1234

                """))

    def test_merges_non_provisional_pr(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[
            PullRequest(
                title='title1',
                number=1234,
                body='description\nWPT-Export-Revision: 9\nChange-Id: decafbad',
                state='open',
                node_id='PR_123_',
                labels=['']),
        ])
        test_exporter.get_exportable_commits = lambda: ([
            MockChromiumCommit(self.host, change_id='decafbad'), ], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [
            'pr_for_chromium_commit',
            'get_pr_branch',
            'merge_pr',
        ])
        self.assertEqual(test_exporter.github.pull_requests_created, [])
        self.assertEqual(test_exporter.github.pull_requests_merged, [1234])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            textwrap.dedent("""\
                Pull requests merged:
                * https://github.com/web-platform-tests/wpt/pull/1234

                """))

    def test_does_not_create_pr_if_cl_review_has_not_started(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.exportable_cls = [
            MockGerritCL(data={
                'change_id': '1',
                'subject': 'subject',
                '_number': 1,
                'current_revision': '2',
                'has_review_started': False,
                'revisions': {
                    '1': {
                        'commit_with_footers': 'a commit with footers 1',
                        'description': 'subject 1',
                    },
                    '2': {
                        'commit_with_footers': 'a commit with footers 2',
                        'description': 'subject 2',
                    },
                },
                'owner': {
                    'email': 'test@chromium.org'
                },
            },
                         api=test_exporter.gerrit,
                         chromium_commit=MockChromiumCommit(self.host))
        ]
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertTrue(success)
        self.assertEqual(test_exporter.github.calls, [])
        self.assertEqual(test_exporter.github.pull_requests_created, [])
        self.assertEqual(test_exporter.github.pull_requests_merged, [])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            'No pull requests modified.\n')

    @unittest.skip('Unskip after crbug.com/346392205 is fixed')
    def test_run_returns_false_on_gerrit_search_error(self):
        def raise_gerrit_error():
            raise GerritError('Gerrit API fails.')

        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: ([], [])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.gerrit.query_exportable_cls = raise_gerrit_error
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertFalse(success)
        self.assertLog([
            'INFO: Cloning GitHub web-platform-tests/wpt into /tmp/wpt\n',
            'INFO: Setting git user name & email in /tmp/wpt\n',
            'INFO: Searching for exportable in-flight CLs.\n',
            'INFO: In-flight CLs cannot be exported due to the following error:\n',
            'ERROR: Gerrit API fails.\n',
            'INFO: Searching for exportable Chromium commits.\n'
        ])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            'No pull requests modified.\n')

    def test_run_returns_false_on_patch_failure(self):
        test_exporter = TestExporter(self.host)
        test_exporter.create_draft_pr = False
        test_exporter.github = MockWPTGitHub(pull_requests=[])
        test_exporter.get_exportable_commits = lambda: (
            [], ['There was an error with the rutabaga.'])
        test_exporter.gerrit = MockGerritAPI()
        test_exporter.pr_cleaner.run = lambda x, y: None
        success = test_exporter.main([
            '--credentials-json=/tmp/credentials.json',
            '--summary-markdown=/tmp/summary.md',
        ])

        self.assertFalse(success)
        self.assertLog([
            'INFO: Cloning GitHub web-platform-tests/wpt into /tmp/wpt\n',
            'INFO: Setting git user name & email in /tmp/wpt\n',
            'INFO: Searching for exportable in-flight CLs.\n',
            'INFO: Searching for exportable Chromium commits.\n',
            'INFO: Attention: The following errors have prevented some commits from being exported:\n',
            'ERROR: There was an error with the rutabaga.\n'
        ])
        self.assertEqual(
            self.host.filesystem.read_text_file('/tmp/summary.md'),
            'No pull requests modified.\n')
