# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import datetime
import json
import logging
import re
import six

from collections import namedtuple
from requests.exceptions import HTTPError
from requests.exceptions import InvalidURL
from six.moves.urllib.parse import quote

from blinkpy.common.memoized import memoized
from blinkpy.w3c.common import (
    WPT_GH_ORG,
    WPT_GH_REPO_NAME,
    EXPORT_PR_LABEL,
    PROVISIONAL_PR_LABEL,
    LEGACY_MAIN_BRANCH_NAME,
)

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
MAX_PER_PAGE = 100
MAX_PR_HISTORY_WINDOW = 1000


class GitHubRepo(object):
    """An interface to GitHub for interacting with a github repo.

    This class contains methods for sending requests to the GitHub API.
    Unless mentioned otherwise, API calls are expected to succeed, and
    GitHubError will be raised if an API call fails.
    """
    def __init__(self, gh_org, gh_repo_name, export_pr_label,
                 provisional_pr_label, host, user, token, pr_history_window,
                 main_branch, min_expected_prs):
        if pr_history_window > MAX_PR_HISTORY_WINDOW:
            raise ValueError("GitHub only provides up to %d results per search"
                             % MAX_PR_HISTORY_WINDOW)
        self.gh_org = gh_org
        self.gh_repo_name = gh_repo_name
        self.export_pr_label = export_pr_label
        self.provisional_pr_label = provisional_pr_label
        self.host = host
        self.user = user
        self.token = token
        self._pr_history_window = pr_history_window
        self._main_branch = main_branch
        self.min_expected_prs = min_expected_prs
        self.create_draft_pr = (
            host.project_config.gerrit_project == 'chromium/src')

    @property
    def url(self):
        return f'https://github.com/{self.gh_org}/{self.gh_repo_name}/'

    def has_credentials(self):
        return self.user and self.token

    def auth_token(self):
        assert self.has_credentials()
        data = '{}:{}'.format(self.user, self.token).encode('utf-8')
        return base64.b64encode(data).decode('utf-8')

    def request(self, path, method, body=None, accept_header=None):
        """Sends a request to GitHub API and deserializes the response.

        Args:
            path: API endpoint without base URL (starting with '/').
            method: HTTP method to be used for this request.
            body: Optional payload in the request body (default=None).
            accept_header: Custom media type in the Accept header (default=None).

        Returns:
            A JSONResponse instance.
        """
        assert path.startswith('/')

        if body:
            if six.PY3:
                body = json.dumps(body).encode("utf-8")
            else:
                body = json.dumps(body)

        if accept_header:
            headers = {'Accept': accept_header}
        else:
            headers = {'Accept': 'application/vnd.github.v3+json'}

        if self.has_credentials():
            headers['Authorization'] = 'Basic {}'.format(self.auth_token())

        response = self.host.web.request(
            method=method, url=API_BASE + path, data=body, headers=headers)
        return JSONResponse(response)

    @staticmethod
    def extract_link_next(link_header):
        """Extracts the URI to the next page of results from a response.

        As per GitHub API specs, the link to the next page of results is
        extracted from the Link header -- the link with relation type "next".
        Docs: https://developer.github.com/v3/#pagination (and RFC 5988)

        Args:
            link_header: The value of the Link header in responses from GitHub.

        Returns:
            Path to the next page (without base URL), or None if not found.
        """
        # TODO(robertma): Investigate "may require expansion as URI templates" mentioned in docs.
        # Example Link header:
        # <https://api.github.com/resources?page=3>; rel="next", <https://api.github.com/resources?page=50>; rel="last"
        if link_header is None:
            return None
        link_re = re.compile(r'<(.+?)>; *rel="(.+?)"')
        match = link_re.search(link_header)
        while match:
            link, rel = match.groups()
            if rel.lower() == 'next':
                # Strip API_BASE so that the return value is useful for request().
                assert link.startswith(API_BASE)
                return link[len(API_BASE):]
            match = link_re.search(link_header, match.end())
        return None

    def create_pr(self, remote_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            The issue number of the created PR.
        """
        assert remote_branch_name
        assert desc_title
        assert body
        path = '/repos/%s/%s/pulls' % (self.gh_org, self.gh_repo_name)
        body = {
            'title': desc_title,
            'body': body,
            'head': remote_branch_name,
            'base': self._main_branch,
            'draft': self.create_draft_pr,
        }
        try:
            response = self.request(path, method='POST', body=body)
        except HTTPError as e:
            if hasattr(e, 'response'):
                _log.error(e.response.reason)
                if e.response.status_code == 422:
                    _log.error('Please check if branch already exists; If so, '
                               'please remove the PR description and '
                               'delete the branch')
                raise GitHubError(201, e.response.status_code,
                                  'create PR branch %s' % remote_branch_name)
            else:
                raise GitHubError(201, e,
                                  'create PR branch %s' % remote_branch_name)

        if response.status_code != 201:
            raise GitHubError(201, response.status_code, 'create PR')

        return response.data['number']

    def update_pr(self, pr_number, desc_title=None, body=None, state=None):
        """Updates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#update-a-pull-request
        """
        path = '/repos/{}/{}/pulls/{}'.format(self.gh_org, self.gh_repo_name,
                                              pr_number)
        payload = {}
        if desc_title:
            payload['title'] = desc_title
        if body:
            payload['body'] = body
        if state:
            payload['state'] = state
        response = self.request(path, method='PATCH', body=payload)

        if response.status_code != 200:
            raise GitHubError(200, response.status_code,
                              'update PR %d' % pr_number)

    def add_label(self, number, label):
        """Adds a label to a GitHub issue (or PR).

        API doc: https://developer.github.com/v3/issues/labels/#add-labels-to-an-issue
        """
        path = '/repos/%s/%s/issues/%d/labels' % (self.gh_org,
                                                  self.gh_repo_name, number)
        body = [label]
        response = self.request(path, method='POST', body=body)

        if response.status_code != 200:
            raise GitHubError(200, response.status_code,
                              'add label %s to issue %d' % (label, number))

    def remove_label(self, number, label):
        """Removes a label from a GitHub issue (or PR).

        API doc: https://developer.github.com/v3/issues/labels/#remove-a-label-from-an-issue
        """
        path = '/repos/%s/%s/issues/%d/labels/%s' % (
            self.gh_org,
            self.gh_repo_name,
            number,
            quote(label),
        )
        response = self.request(path, method='DELETE')

        # The GitHub API documentation claims that this endpoint returns a 204
        # on success. However in reality it returns a 200.
        if response.status_code not in (200, 204):
            raise GitHubError(
                (200, 204), response.status_code,
                'remove label %s from issue %d' % (label, number))

    def add_comment(self, number, comment_body):
        """Add a comment for an issue (or PR).

        API doc: https://developer.github.com/v3/issues/comments/#create-a-comment
        """
        path = '/repos/%s/%s/issues/%d/comments' % (self.gh_org,
                                                    self.gh_repo_name, number)
        body = {'body': comment_body}
        response = self.request(path, method='POST', body=body)

        if response.status_code != 201:
            raise GitHubError(
                201, response.status_code,
                'add comment %s to issue %d' % (comment_body, number))

    def make_pr_from_item(self, item):
        labels = [label['name'] for label in item['labels']]
        return PullRequest(title=item['title'],
                           number=item['number'],
                           body=item['body'],
                           state=item['state'],
                           node_id=item['node_id'],
                           labels=labels)

    def recent_failing_chromium_exports(self):
        """Fetches open PRs with an export label, failing status, and updated
        within the last month.

        API doc: https://developer.github.com/v3/search/#search-issues-and-pull-requests

        Returns:
            A list of PullRequest namedtuples.
        """
        one_month_ago = datetime.date.today() - datetime.timedelta(days=31)
        path = (
            '/search/issues'
            '?q=repo:{}/{}%20type:pr+is:open%20label:{}%20status:failure%20updated:>{}'
            '&sort=updated'
            '&page=1'
            '&per_page={}').format(self.gh_org, self.gh_repo_name,
                                   self.export_pr_label,
                                   one_month_ago.isoformat(), MAX_PER_PAGE)

        failing_prs = []
        while path is not None:
            response = self.request(path, method='GET')
            if response.status_code == 200:
                if response.data['incomplete_results']:
                    raise GitHubError('complete results', 'incomplete results',
                                      'fetch failing open chromium exports',
                                      path)

                prs = [
                    self.make_pr_from_item(item)
                    for item in response.data['items']
                ]
                failing_prs += prs
            else:
                raise GitHubError(200, response.status_code,
                                  'fetch failing open chromium exports', path)
            path = self.extract_link_next(response.getheader('Link'))

        _log.info('Fetched %d PRs from GitHub.', len(failing_prs))
        return failing_prs

    @memoized
    def all_provisional_pull_requests(self):
        """Fetches the most recent open PRs with export and provisional labels

        Returns:
            A list of PullRequest namedtuples.
        """
        # label name in query param with space require character escape and quotation
        escaped_provisional_pr_label = "\"{}\"".format(
            self.provisional_pr_label.replace(" ", "+"))
        path = ('/search/issues'
                '?q=repo:{}/{}%20type:pr%20label:{}%20label:{}'
                '&status:open'
                '&sort=updated'
                '&page=1'
                '&per_page={}').format(
                    self.gh_org, self.gh_repo_name, self.export_pr_label,
                    escaped_provisional_pr_label,
                    min(MAX_PER_PAGE, self._pr_history_window))
        return self.fetch_pull_requests_from_path(path)

    @memoized
    def all_pull_requests(self):
        """Fetches the most recent (open and closed) PRs with the export label.

        Returns:
            A list of PullRequest namedtuples.
        """
        path = ('/search/issues'
                '?q=repo:{}/{}%20type:pr%20label:{}'
                '&sort=updated'
                '&page=1'
                '&per_page={}').format(
                    self.gh_org, self.gh_repo_name, self.export_pr_label,
                    min(MAX_PER_PAGE, self._pr_history_window))
        return self.fetch_pull_requests_from_path(path)

    def fetch_pull_requests_from_path(self, path):
        """Fetches PRs from url path.

        The maximum number of PRs is pr_history_window. Search endpoint is used
        instead of listing PRs, because we need to filter by labels. Note that
        there are already more than MAX_PR_HISTORY_WINDOW export PRs, so we
        can't really find *all* of them; we fetch the most recently updated ones
        because we only check whether recent commits have been exported.

        API doc: https://developer.github.com/v3/search/#search-issues-and-pull-requests

        Returns:
            A list of PullRequest namedtuples."""
        all_prs = []
        while path is not None and len(all_prs) < self._pr_history_window:
            response = self.request(path, method='GET')
            if response.status_code == 200:
                if response.data['incomplete_results']:
                    raise GitHubError('complete results', 'incomplete results',
                                      'fetch all pull requests', path)

                prs = [
                    self.make_pr_from_item(item)
                    for item in response.data['items']
                ]
                all_prs += prs[:self._pr_history_window - len(all_prs)]
            else:
                raise GitHubError(200, response.status_code,
                                  'fetch all pull requests', path)
            path = self.extract_link_next(response.getheader('Link'))

        # Doing this check to mitigate Github API issues (crbug.com/814617).
        # Use a minimum based on which path it comes from
        min_prs = min(self._pr_history_window, self.min_expected_prs)
        if len(all_prs) < min_prs:
            raise GitHubError('at least %d commits' % min_prs, len(all_prs),
                              'fetch all pull requests')

        _log.info('Fetched %d PRs from GitHub.', len(all_prs))
        return all_prs

    def get_pr_branch(self, pr_number):
        """Gets the remote branch name of a PR.

        API doc: https://developer.github.com/v3/pulls/#get-a-single-pull-request

        Returns:
            The remote branch name.
        """
        path = '/repos/{}/{}/pulls/{}'.format(self.gh_org, self.gh_repo_name,
                                              pr_number)
        response = self.request(path, method='GET')

        if response.status_code != 200:
            raise GitHubError(200, response.status_code,
                              'get the branch of PR %d' % pr_number)

        return response.data['head']['ref']

    def get_branch_check_runs(self, remote_branch_name):
        """Returns the check runs of a remote branch.

        API doc: https://developer.github.com/v3/checks/runs/#list-check-runs-for-a-git-reference

        Returns:
            The list of check runs from the HEAD of the branch.
        """
        path = '/repos/%s/%s/commits/%s/check-runs?page=1&per_page=%d' % (
            self.gh_org, self.gh_repo_name, remote_branch_name, MAX_PER_PAGE)
        accept_header = 'application/vnd.github.antiope-preview+json'

        check_runs = []
        while path is not None:
            response = self.request(path,
                                    method='GET',
                                    accept_header=accept_header)
            if response.status_code != 200:
                raise GitHubError(
                    200, response.status_code,
                    'get branch check runs %s' % remote_branch_name)

            check_runs += response.data['check_runs']
            path = self.extract_link_next(response.getheader('Link'))

        return check_runs

    def is_pr_merged(self, pr_number):
        """Checks if a PR has been merged.

        API doc: https://developer.github.com/v3/pulls/#get-if-a-pull-request-has-been-merged

        Returns:
            True if merged, False if not.
        """
        path = '/repos/%s/%s/pulls/%d/merge' % (self.gh_org, self.gh_repo_name,
                                                pr_number)
        cached_error = None
        for i in range(5):
            try:
                response = self.request(path, method='GET')
                if response.status_code == 204:
                    return True
                else:
                    raise GitHubError(204, response.status_code,
                                      'check if PR %d is merged' % pr_number)
            except HTTPError as e:
                if hasattr(e, 'response') and e.response.status_code == 404:
                    return False
                else:
                    raise
            except InvalidURL as e:
                # After migrate to py3 we met random timeout issue here,
                # Retry this request in this case
                _log.warning("Meet URLError...")
                cached_error = e
        else:
            raise cached_error

    def merge_pr(self, pr_number):
        """Merges a PR.

        If merge cannot be performed, MergeError is raised. GitHubError is
        raised when other unknown errors happen.

        API doc: https://developer.github.com/v3/pulls/#merge-a-pull-request-merge-button
        """
        path = '/repos/%s/%s/pulls/%d/merge' % (self.gh_org, self.gh_repo_name,
                                                pr_number)
        body = {
            'merge_method': 'rebase',
        }

        try:
            response = self.request(path, method='PUT', body=body)
        except HTTPError as e:
            if hasattr(e, 'response') and e.response.status_code == 405:
                raise MergeError(pr_number)
            else:
                raise

        if response.status_code != 200:
            raise GitHubError(200, response.status_code,
                              'merge PR %d' % pr_number)

    def delete_remote_branch(self, remote_branch_name):
        """Deletes a remote branch.

        API doc: https://developer.github.com/v3/git/refs/#delete-a-reference
        """
        path = '/repos/%s/%s/git/refs/heads/%s' % (
            self.gh_org, self.gh_repo_name, remote_branch_name)
        response = self.request(path, method='DELETE')

        if response.status_code != 204:
            raise GitHubError(204, response.status_code,
                              'delete remote branch %s' % remote_branch_name)

    def pr_for_chromium_commit(self, chromium_commit):
        """Returns a PR corresponding to the given ChromiumCommit, or None."""
        # We rely on Change-Id because Gerrit returns ToT+1 as the commit
        # positions for in-flight CLs, whereas Change-Id is permanent.
        return self.pr_with_change_id(chromium_commit.change_id())

    def pr_with_change_id(self, target_change_id):
        all_prs = self.all_pull_requests()
        for pull_request in all_prs:
            # Note: Search all 'Change-Id's so that we can manually put multiple
            # CLs in one PR. (The exporter always creates one PR for each CL.)
            change_ids = self.extract_metadata(
                'Change-Id: ', pull_request.body, all_matches=True)
            if target_change_id in change_ids:
                return pull_request
        return None

    @staticmethod
    def extract_metadata(tag, commit_body, all_matches=False):
        values = []
        for line in commit_body.splitlines():
            if not line.startswith(tag):
                continue
            value = line[len(tag):]
            if all_matches:
                values.append(value)
            else:
                return value
        return values if all_matches else None


class WPTGitHub(GitHubRepo):
    """An interface to GitHub for interacting with the web-platform-tests repo.
    """
    def __init__(self,
                 host,
                 user=None,
                 token=None,
                 pr_history_window=MAX_PR_HISTORY_WINDOW):
        super().__init__(
            gh_org=WPT_GH_ORG,
            gh_repo_name=WPT_GH_REPO_NAME,
            export_pr_label=EXPORT_PR_LABEL,
            provisional_pr_label=PROVISIONAL_PR_LABEL,
            host=host,
            user=user,
            token=token,
            pr_history_window=pr_history_window,
            main_branch=LEGACY_MAIN_BRANCH_NAME,
            min_expected_prs=200,
        )

    @property
    def skipped_revisions(self):
        return [
            # The great blink mv: https://crbug.com/843412#c13
            '77578ccb4082ae20a9326d9e673225f1189ebb63',
        ]


class JSONResponse(object):
    """An HTTP response containing JSON data."""

    def __init__(self, raw_response):
        """Initializes a JSONResponse instance.

        Args:
            raw_response: a response object returned by requests.
        """
        self._raw_response = raw_response
        self.status_code = raw_response.status_code
        try:
            self.data = raw_response.json()
        except ValueError:
            self.data = None

    def getheader(self, header):
        """Gets the value of the header with the given name.

        Delegates to request.Response.headers, which is case-insensitive."""
        return self._raw_response.headers.get(header)


class GitHubError(Exception):
    """Raised when GitHub returns a non-OK response status for a request."""

    def __init__(self, expected, received, action, extra_data=None):
        message = 'Expected {}, but received {} from GitHub when attempting to {}'.format(
            expected, received, action)
        if extra_data:
            message += '\n' + str(extra_data)
        super(GitHubError, self).__init__(message)


class MergeError(GitHubError):
    """An error specifically for when a PR cannot be merged.

    This should only be thrown when GitHub returns status code 405,
    indicating that the PR could not be merged.
    """

    def __init__(self, pr_number):
        super(MergeError, self).__init__(200, 405, 'merge PR %d' % pr_number)


PullRequest = namedtuple(
    'PullRequest', ['title', 'number', 'body', 'state', 'node_id', 'labels'])
