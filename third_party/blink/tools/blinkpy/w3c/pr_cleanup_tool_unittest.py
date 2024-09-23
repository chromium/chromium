"""Tests for pr_cleanup_tool."""
import json
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.w3c.gerrit import GerritNotFoundError
from blinkpy.w3c.gerrit_mock import MockGerritAPI, MockGerritCL
from blinkpy.w3c.pr_cleanup_tool import PrCleanupTool
from blinkpy.w3c.wpt_github import PullRequest
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub


class PrCleanupToolTest(LoggingTestCase):
    def setUp(self):
        super(PrCleanupToolTest, self).setUp()
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

    def test_successful_close_abandoned_cl(self):
        wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='Change-Id: 88',
                        state='open',
                        node_id='PR_kwDOADc1Vc5jhje_',
                        labels=[]),
        ])
        gerrit = MockGerritAPI()
        pr_cleanup = PrCleanupTool(self.host)
        gerrit.cl = MockGerritCL(data={
            'change_id': 'I001',
            'subject': 'subject',
            '_number': 1234,
            'status': 'ABANDONED',
            'updated': '2022-10-15 15:17:50.000000000',
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
                                 api=gerrit)
        pr_cleanup.run(wpt_github, gerrit)
        self.assertEqual(gerrit.cls_queried, ['88'])
        self.assertEqual(wpt_github.calls, [
            'all_provisional_pull_requests',
            'add_comment "Close this PR because the Chromium CL has been abandoned."',
            'update_pr', 'get_pr_branch', 'delete_remote_branch'
        ])

    def test_not_close_pr_for_abandoned_cl(self):
        wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='Change-Id: 88',
                        state='open',
                        node_id='PR_kwDOADc1Vc5jhje_',
                        labels=[]),
        ])
        gerrit = MockGerritAPI()
        pr_cleanup = PrCleanupTool(self.host)
        gerrit.cl = MockGerritCL(data={
            'change_id': 'I001',
            'subject': 'subject',
            '_number': 1234,
            'status': 'ABANDONED',
            'updated': '2222-10-15 15:17:50.000000000',
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
                                 api=gerrit)
        pr_cleanup.run(wpt_github, gerrit)
        self.assertEqual(gerrit.cls_queried, ['88'])
        self.assertEqual(wpt_github.calls, ['all_provisional_pull_requests'])

    def test_successful_close_no_exportable_changes(self):
        wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='Change-Id: 99',
                        state='open',
                        node_id='PR_kwDOADc1Vc5jhje_',
                        labels=[]),
        ])
        gerrit = MockGerritAPI()
        pr_cleanup = PrCleanupTool(self.host)
        gerrit.cl = MockGerritCL(data={
            'change_id': 'I001',
            'subject': 'subject',
            '_number': 1234,
            'status': 'MERGED',
            'updated': '2022-10-15 15:17:50.000000000',
            'current_revision': '1',
            'has_review_started': True,
            'revisions': {
                '1': {
                    'commit_with_footers': 'a commit with footers',
                    'files': {
                        RELATIVE_WEB_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        },
                                 api=gerrit)
        pr_cleanup.run(wpt_github, gerrit)
        self.assertEqual(gerrit.cls_queried, ['99'])
        self.assertEqual(wpt_github.calls, [
            'all_provisional_pull_requests',
            'add_comment "Close this PR because the Chromium'
            ' CL does not have exportable changes."', 'update_pr',
            'get_pr_branch', 'delete_remote_branch'
        ])

    def test_query_cl_raise_exception(self):
        wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='Change-Id: 88',
                        state='open',
                        node_id='PR_kwDOADc1Vc5jhje_',
                        labels=[]),
        ])
        gerrit = MockGerritAPI(raise_error=True)
        pr_cleanup = PrCleanupTool(self.host)
        gerrit.cl = MockGerritCL(data={
            'change_id': 'I001',
            'subject': 'subject',
            '_number': 1234,
            'status': 'ABANDONED',
            'updated': '2022-10-15 15:17:50.000000000',
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
                                 api=gerrit)
        pr_cleanup.run(wpt_github, gerrit)
        self.assertEqual(gerrit.cls_queried, ['88'])
        self.assertEqual(wpt_github.calls, ['all_provisional_pull_requests'])

    def test_query_cl_change_id_not_found(self):
        wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1',
                        number=1234,
                        body='Change-Id: 88',
                        state='open',
                        node_id='PR_kwDOADc1Vc5jhje_',
                        labels=[]),
        ])
        gerrit = MockGerritAPI()
        pr_cleanup = PrCleanupTool(self.host)
        with mock.patch.object(gerrit,
                               'query_cl',
                               side_effect=GerritNotFoundError):
            pr_cleanup.run(wpt_github, gerrit)
        self.assertEqual(gerrit.cls_queried, [])
        self.assertEqual(wpt_github.calls, [
            'all_provisional_pull_requests',
            'add_comment "Close this PR because the corresponding CL has been deleted."',
            'update_pr', 'get_pr_branch', 'delete_remote_branch'
        ])
