# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.w3c.chromium_commit_mock import MockChromiumCommit
from blinkpy.w3c.common import EXPORT_PR_LABEL
from blinkpy.w3c.wpt_github import MAX_PR_HISTORY_WINDOW, GitHubError, MergeError, PullRequest, WPTGitHub


class WPTGitHubTest(unittest.TestCase):
    def generate_pr_item(self, pr_number, state='closed'):
        return {
            'title': 'Foobar',
            'number': pr_number,
            'body': 'description',
            'state': state,
            'node_id': 'PR_kwDOADc1Vc5jhje_',
            'labels': [{
                'name': EXPORT_PR_LABEL
            }]
        }

    def setUp(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad')

    def test_init(self):
        self.assertEqual(self.wpt_github.user, 'rutabaga')
        self.assertEqual(self.wpt_github.token, 'decafbad')

    def test_constructor_throws_on_pr_history_window_too_large(self):
        with self.assertRaises(ValueError):
            self.wpt_github = WPTGitHub(
                MockHost(),
                user='rutabaga',
                token='decafbad',
                pr_history_window=MAX_PR_HISTORY_WINDOW + 1)

    def test_auth_token(self):
        self.assertEqual(
            self.wpt_github.auth_token(),
            base64.encodebytes(
                'rutabaga:decafbad'.encode('utf-8')).strip().decode('utf-8'))

    def test_extract_link_next(self):
        link_header = (
            '<https://api.github.com/user/repos?page=1&per_page=100>; rel="first", '
            '<https://api.github.com/user/repos?page=2&per_page=100>; rel="prev", '
            '<https://api.github.com/user/repos?page=4&per_page=100>; rel="next", '
            '<https://api.github.com/user/repos?page=50&per_page=100>; rel="last"'
        )
        self.assertEqual(
            self.wpt_github.extract_link_next(link_header),
            '/user/repos?page=4&per_page=100')

    def test_extract_link_next_not_found(self):
        self.assertIsNone(self.wpt_github.extract_link_next(''))

    def test_recent_failing_chromium_exports_single_page(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'headers': {
                    'Link': ''
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                })
            },
        ]

        self.assertEqual(
            len(self.wpt_github.recent_failing_chromium_exports()), 1)

    def test_recent_failing_chromium_exports_all_pages(self):
        self.wpt_github = WPTGitHub(MockHost(),
                                    user='rutabaga',
                                    token='decafbad',
                                    pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'headers': {
                    'Link':
                    '<https://api.github.com/resources?page=2>; rel="next"'
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                }),
                'node_id':
                'PR_kwDOADc1Vc5jhje_'
            },
            {
                'status_code':
                200,
                'headers': {
                    'Link': ''
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(2)]
                }),
                'node_id':
                'PR_kwDOADc1Vc5jhje_'
            },
        ]
        self.assertEqual(
            len(self.wpt_github.recent_failing_chromium_exports()), 2)

    def test_recent_failing_chromium_exports_throws_github_error(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 204
            },
        ]
        with self.assertRaises(GitHubError):
            self.wpt_github.recent_failing_chromium_exports()

    def test_all_pull_requests_single_page(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'headers': {
                    'Link': ''
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                })
            },
        ]
        self.assertEqual(len(self.wpt_github.all_pull_requests()), 1)

    def test_all_pull_requests_all_pages(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=2)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'headers': {
                    'Link':
                    '<https://api.github.com/resources?page=2>; rel="next"'
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                })
            },
            {
                'status_code':
                200,
                'headers': {
                    'Link': ''
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(2)]
                })
            },
        ]
        self.assertEqual(len(self.wpt_github.all_pull_requests()), 2)

    def test_all_pull_requests_reaches_pr_history_window(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=2)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'headers': {
                    'Link':
                    '<https://api.github.com/resources?page=2>; rel="next"'
                },
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                })
            },
            {
                'status_code':
                200,
                'headers': {
                    'Link': ''
                },
                'body':
                json.dumps({
                    'incomplete_results':
                    False,
                    'items':
                    [self.generate_pr_item(2),
                     self.generate_pr_item(3)]
                })
            },
        ]
        self.assertEqual(len(self.wpt_github.all_pull_requests()), 2)

    def test_all_pull_requests_throws_github_error_on_non_200(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 204
            },
        ]
        with self.assertRaises(GitHubError):
            self.wpt_github.all_pull_requests()

    def test_all_pull_requests_throws_github_error_when_incomplete(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'body':
                json.dumps({
                    'incomplete_results': True,
                    'items': [self.generate_pr_item(1)]
                })
            },
        ]
        with self.assertRaises(GitHubError):
            self.wpt_github.all_pull_requests()

    def test_all_pull_requests_throws_github_error_when_too_few_prs(self):
        self.wpt_github = WPTGitHub(
            MockHost(), user='rutabaga', token='decafbad', pr_history_window=2)
        self.wpt_github.host.web.responses = [
            {
                'status_code':
                200,
                'body':
                json.dumps({
                    'incomplete_results': False,
                    'items': [self.generate_pr_item(1)]
                })
            },
        ]
        with self.assertRaises(GitHubError):
            self.wpt_github.all_pull_requests()

    def test_create_pr_success(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 201,
                'body': json.dumps({
                    'number': 1234
                })
            },
        ]
        self.assertEqual(
            self.wpt_github.create_pr('branch', 'title', 'body'), 1234)

    def test_create_pr_throws_github_error_on_non_201(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200
            },
        ]
        with self.assertRaises(GitHubError):
            self.wpt_github.create_pr('branch', 'title', 'body')

    def test_branch_check_runs_single_page(self):
        self.wpt_github = WPTGitHub(MockHost(),
                                    user='rutabaga',
                                    token='decafbad',
                                    pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200,
                'headers': {
                    'Link': ''
                },
                'body': json.dumps({'check_runs': [{
                    'conclusion': 'success'
                }]})
            },
        ]
        self.assertEqual(
            self.wpt_github.get_branch_check_runs('1')[0]['conclusion'],
            'success')

    def test_branch_check_runs_all_pages(self):
        self.wpt_github = WPTGitHub(MockHost(),
                                    user='rutabaga',
                                    token='decafbad',
                                    pr_history_window=1)
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200,
                'headers': {
                    'Link':
                    '<https://api.github.com/resources?page=2>; rel="next"'
                },
                'body': json.dumps({'check_runs': [{
                    'conclusion': 'success'
                }]})
            },
            {
                'status_code': 200,
                'headers': {
                    'Link': ''
                },
                'body': json.dumps({'check_runs': [{
                    'conclusion': 'failure'
                }]})
            },
        ]

        check_runs = self.wpt_github.get_branch_check_runs('1')
        self.assertEqual(check_runs[0]['conclusion'], 'success')
        self.assertEqual(check_runs[1]['conclusion'], 'failure')

    def test_get_pr_branch(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200,
                'body': json.dumps({
                    'head': {
                        'ref': 'fake_branch'
                    }
                })
            },
        ]
        self.assertEqual(self.wpt_github.get_pr_branch(1234), 'fake_branch')

    def test_is_pr_merged_receives_204(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 204
            },
        ]
        self.assertTrue(self.wpt_github.is_pr_merged(1234))

    def test_is_pr_merged_receives_404(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 404
            },
        ]
        self.assertFalse(self.wpt_github.is_pr_merged(1234))

    def test_merge_pr_success(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200
            },
        ]
        self.wpt_github.merge_pr(1234)

    def test_merge_pr_throws_merge_error_on_405(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 405
            },
        ]

        with self.assertRaises(MergeError):
            self.wpt_github.merge_pr(5678)

    def test_remove_label_throws_github_error_on_non_200_or_204(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 201
            },
        ]

        with self.assertRaises(GitHubError):
            self.wpt_github.remove_label(1234, 'rutabaga')

    def test_delete_remote_branch_throws_github_error_on_non_204(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200
            },
        ]

        with self.assertRaises(GitHubError):
            self.wpt_github.delete_remote_branch('rutabaga')

    def test_add_comment_throws_github_error_on_non_201(self):
        self.wpt_github.host.web.responses = [
            {
                'status_code': 200
            },
        ]

        with self.assertRaises(GitHubError):
            self.wpt_github.add_comment(123, 'rutabaga')

    def test_pr_for_chromium_commit_change_id_only(self):
        self.wpt_github.all_pull_requests = lambda: [
            PullRequest('PR1', 1, 'body\nChange-Id: I00c0ffee', 'open',
                        'PR_kwDOADc1Vc5jhje_', []),
            PullRequest('PR2', 2, 'body\nChange-Id: I00decade', 'open',
                        'PR_kwDOADc1Vc5jhje_', []),
        ]
        chromium_commit = MockChromiumCommit(
            MockHost(),
            change_id='I00decade',
            position='refs/heads/master@{#10}')
        pull_request = self.wpt_github.pr_for_chromium_commit(chromium_commit)
        self.assertEqual(pull_request.number, 2)

    def test_pr_for_chromium_commit_prefers_change_id(self):
        self.wpt_github.all_pull_requests = lambda: [
            PullRequest(
                'PR1', 1,
                'body\nChange-Id: I00c0ffee\nCr-Commit-Position: refs/heads/master@{#10}',
                'open', 'PR_kwDOADc1Vc5jhje_', []),
            PullRequest(
                'PR2', 2,
                'body\nChange-Id: I00decade\nCr-Commit-Position: refs/heads/master@{#33}',
                'open', 'PR_kwDOADc1Vc5jhje_', []),
        ]
        chromium_commit = MockChromiumCommit(
            MockHost(),
            change_id='I00decade',
            position='refs/heads/master@{#10}')
        pull_request = self.wpt_github.pr_for_chromium_commit(chromium_commit)
        self.assertEqual(pull_request.number, 2)

    def test_pr_for_chromium_commit_multiple_change_ids(self):
        self.wpt_github.all_pull_requests = lambda: [
            PullRequest('PR1', 1,
                        'body\nChange-Id: I00c0ffee\nChange-Id: I00decade',
                        'open', 'PR_kwDOADc1Vc5jhje_', []),
        ]

        chromium_commit = MockChromiumCommit(
            MockHost(),
            change_id='I00c0ffee',
            position='refs/heads/master@{#10}')
        pull_request = self.wpt_github.pr_for_chromium_commit(chromium_commit)
        self.assertEqual(pull_request.number, 1)

        chromium_commit = MockChromiumCommit(
            MockHost(),
            change_id='I00decade',
            position='refs/heads/master@{#33}')
        pull_request = self.wpt_github.pr_for_chromium_commit(chromium_commit)
        self.assertEqual(pull_request.number, 1)
