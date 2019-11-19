"""Tests for pr_cleanup_tool."""
import json

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
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

    def test_main_successful_close_abandoned_cl(self):
        pr_cleanup = PrCleanupTool(self.host)
        pr_cleanup.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='Change-Id: 88', state='open', labels=[]),
        ])
        pr_cleanup.gerrit = MockGerritAPI()
        pr_cleanup.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'I001',
                'subject': 'subject',
                '_number': 1234,
                'status': 'ABANDONED',
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {'commit_with_footers': 'a commit with footers'}
                },
                'owner': {'email': 'test@chromium.org'},
            },
            api=pr_cleanup.gerrit)
        pr_cleanup.main(['--credentials-json', '/tmp/credentials.json'])
        self.assertEqual(pr_cleanup.gerrit.cls_queried, ['88'])
        self.assertEqual(pr_cleanup.wpt_github.calls, [
            'all_pull_requests',
            'add_comment "Close this PR because the Chromium CL has been abandoned."',
            'update_pr', 'get_pr_branch', 'delete_remote_branch'])

    def test_main_successful_close_no_exportable_changes(self):
        pr_cleanup = PrCleanupTool(self.host)
        pr_cleanup.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='Change-Id: 99', state='open', labels=[]),
        ])
        pr_cleanup.gerrit = MockGerritAPI()
        pr_cleanup.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'I001',
                'subject': 'subject',
                '_number': 1234,
                'status': 'MERGED',
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {'commit_with_footers': 'a commit with footers',
                          'files': {
                              RELATIVE_WEB_TESTS + 'foo/bar.html': '',
                          }}
                },
                'owner': {'email': 'test@chromium.org'},
            },
            api=pr_cleanup.gerrit)
        pr_cleanup.main(['--credentials-json', '/tmp/credentials.json'])
        self.assertEqual(pr_cleanup.gerrit.cls_queried, ['99'])
        self.assertEqual(pr_cleanup.wpt_github.calls, [
            'all_pull_requests',
            'add_comment "Close this PR because the Chromium'
            ' CL does not have exportable changes."',
            'update_pr', 'get_pr_branch', 'delete_remote_branch'])

    def test_query_cl_raise_exception(self):
        pr_cleanup = PrCleanupTool(self.host)
        pr_cleanup.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(title='title1', number=1234,
                        body='Change-Id: 88', state='open', labels=[]),
        ])
        pr_cleanup.gerrit = MockGerritAPI(raise_error=True)
        pr_cleanup.gerrit.cl = MockGerritCL(
            data={
                'change_id': 'I001',
                'subject': 'subject',
                '_number': 1234,
                'status': 'ABANDONED',
                'current_revision': '1',
                'has_review_started': True,
                'revisions': {
                    '1': {'commit_with_footers': 'a commit with footers'}
                },
                'owner': {'email': 'test@chromium.org'},
            },
            api=pr_cleanup.gerrit)
        pr_cleanup.main(['--credentials-json', '/tmp/credentials.json'])
        self.assertEqual(pr_cleanup.gerrit.cls_queried, ['88'])
        self.assertEqual(pr_cleanup.wpt_github.calls, ['all_pull_requests'])
