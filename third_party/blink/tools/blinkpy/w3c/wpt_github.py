# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import datetime
import json
import logging
import re
import urllib2
from collections import namedtuple

from blinkpy.common.memoized import memoized
from blinkpy.w3c.common import WPT_GH_ORG, WPT_GH_REPO_NAME, EXPORT_PR_LABEL

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
MAX_PER_PAGE = 100
MAX_PR_HISTORY_WINDOW = 1000


class WPTGitHub(object):
    """An interface to GitHub for interacting with the web-platform-tests repo.

    This class contains methods for sending requests to the GitHub API.
    Unless mentioned otherwise, API calls are expected to succeed, and
    GitHubError will be raised if an API call fails.
    """

    def __init__(self, host, user=None, token=None, pr_history_window=MAX_PR_HISTORY_WINDOW):
        if pr_history_window > MAX_PR_HISTORY_WINDOW:
            raise ValueError("GitHub only provides up to %d results per search" % MAX_PR_HISTORY_WINDOW)
        self.host = host
        self.user = user
        self.token = token

        self._pr_history_window = pr_history_window

    def has_credentials(self):
        return self.user and self.token

    def auth_token(self):
        assert self.has_credentials()
        return base64.b64encode('{}:{}'.format(self.user, self.token))

    def request(self, path, method, body=None):
        """Sends a request to GitHub API and deserializes the response.

        Args:
            path: API endpoint without base URL (starting with '/').
            method: HTTP method to be used for this request.
            body: Optional payload in the request body (default=None).

        Returns:
            A JSONResponse instance.
        """
        assert path.startswith('/')

        if body:
            body = json.dumps(body)

        headers = {'Accept': 'application/vnd.github.v3+json'}

        if self.has_credentials():
            headers['Authorization'] = 'Basic {}'.format(self.auth_token())

        response = self.host.web.request(
            method=method,
            url=API_BASE + path,
            data=body,
            headers=headers
        )
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

        path = '/repos/%s/%s/pulls' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
        body = {
            'title': desc_title,
            'body': body,
            'head': remote_branch_name,
            'base': 'master',
        }
        response = self.request(path, method='POST', body=body)

        if response.status_code != 201:
            raise GitHubError(201, response.status_code, 'create PR')

        return response.data['number']

    def update_pr(self, pr_number, desc_title=None, body=None, state=None):
        """Updates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#update-a-pull-request
        """
        path = '/repos/{}/{}/pulls/{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        payload = {}
        if desc_title:
            payload['title'] = desc_title
        if body:
            payload['body'] = body
        if state:
            payload['state'] = state
        response = self.request(path, method='PATCH', body=payload)

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'update PR %d' % pr_number)

    def add_label(self, number, label):
        """Adds a label to a GitHub issue (or PR).

        API doc: https://developer.github.com/v3/issues/labels/#add-labels-to-an-issue
        """
        path = '/repos/%s/%s/issues/%d/labels' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number
        )
        body = [label]
        response = self.request(path, method='POST', body=body)

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'add label %s to issue %d' % (label, number))

    def remove_label(self, number, label):
        """Removes a label from a GitHub issue (or PR).

        API doc: https://developer.github.com/v3/issues/labels/#remove-a-label-from-an-issue
        """
        path = '/repos/%s/%s/issues/%d/labels/%s' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number,
            urllib2.quote(label),
        )
        response = self.request(path, method='DELETE')

        # The GitHub API documentation claims that this endpoint returns a 204
        # on success. However in reality it returns a 200.
        if response.status_code not in (200, 204):
            raise GitHubError((200, 204), response.status_code, 'remove label %s from issue %d' % (label, number))

    def add_comment(self, number, comment_body):
        """Add a comment for an issue (or PR).

        API doc: https://developer.github.com/v3/issues/comments/#create-a-comment
        """
        path = '/repos/%s/%s/issues/%d/comments' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number
        )
        body = {'body': comment_body}
        response = self.request(path, method='POST', body=body)

        if response.status_code != 201:
            raise GitHubError(201, response.status_code, 'add comment %s to issue %d' % (comment_body, number))

    def make_pr_from_item(self, item):
        labels = [label['name'] for label in item['labels']]
        return PullRequest(
            title=item['title'],
            number=item['number'],
            body=item['body'],
            state=item['state'],
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
            '&per_page={}'
        ).format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            EXPORT_PR_LABEL,
            one_month_ago.isoformat(),
            MAX_PER_PAGE
        )

        failing_prs = []
        while path is not None:
            response = self.request(path, method='GET')
            if response.status_code == 200:
                if response.data['incomplete_results']:
                    raise GitHubError('complete results', 'incomplete results',
                                      'fetch failing open chromium exports', path)

                prs = [self.make_pr_from_item(item) for item in response.data['items']]
                failing_prs += prs
            else:
                raise GitHubError(200, response.status_code,
                                  'fetch failing open chromium exports', path)
            path = self.extract_link_next(response.getheader('Link'))

        _log.info('Fetched %d PRs from GitHub.', len(failing_prs))
        return failing_prs

    @memoized
    def all_pull_requests(self):
        """Fetches the most recent (open and closed) PRs with the export label.

        The maximum number of PRs is pr_history_window. Search endpoint is used
        instead of listing PRs, because we need to filter by labels. Note that
        there are already more than MAX_PR_HISTORY_WINDOW export PRs, so we
        can't really find *all* of them; we fetch the most recently updated ones
        because we only check whether recent commits have been exported.

        API doc: https://developer.github.com/v3/search/#search-issues-and-pull-requests

        Returns:
            A list of PullRequest namedtuples.
        """
        path = (
            '/search/issues'
            '?q=repo:{}/{}%20type:pr%20label:{}'
            '&sort=updated'
            '&page=1'
            '&per_page={}'
        ).format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            EXPORT_PR_LABEL,
            min(MAX_PER_PAGE, self._pr_history_window)
        )
        all_prs = []
        while path is not None and len(all_prs) < self._pr_history_window:
            response = self.request(path, method='GET')
            if response.status_code == 200:
                if response.data['incomplete_results']:
                    raise GitHubError('complete results', 'incomplete results', 'fetch all pull requests', path)

                prs = [self.make_pr_from_item(item) for item in response.data['items']]
                all_prs += prs[:self._pr_history_window - len(all_prs)]
            else:
                raise GitHubError(200, response.status_code, 'fetch all pull requests', path)
            path = self.extract_link_next(response.getheader('Link'))

        # There are way more than 1000 exported PRs on GitHub, so we should
        # always get at least pr_history_window PRs. This assertion is added to
        # mitigate transient GitHub API issues (crbug.com/814617).
        if len(all_prs) < self._pr_history_window:
            raise GitHubError('at least %d commits' % self._pr_history_window,
                              len(all_prs), 'fetch all pull requests')

        _log.info('Fetched %d PRs from GitHub.', len(all_prs))
        return all_prs

    def get_branch_statuses(self, branch_name):
        """Gets the status of a PR.

        API doc: https://developer.github.com/v3/repos/statuses/#get-the-combined-status-for-a-specific-ref

        Returns:
            The list of check statuses of the PR.
        """
        path = '/repos/{}/{}/commits/{}/status'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            branch_name
        )
        response = self.request(path, method='GET')

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'get the statuses of PR %d' % branch_name)

        return response.data['statuses']

    def get_pr_branch(self, pr_number):
        """Gets the remote branch name of a PR.

        API doc: https://developer.github.com/v3/pulls/#get-a-single-pull-request

        Returns:
            The remote branch name.
        """
        path = '/repos/{}/{}/pulls/{}'.format(
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        response = self.request(path, method='GET')

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'get the branch of PR %d' % pr_number)

        return response.data['head']['ref']

    def is_pr_merged(self, pr_number):
        """Checks if a PR has been merged.

        API doc: https://developer.github.com/v3/pulls/#get-if-a-pull-request-has-been-merged

        Returns:
            True if merged, False if not.
        """
        path = '/repos/%s/%s/pulls/%d/merge' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        try:
            response = self.request(path, method='GET')
            if response.status_code == 204:
                return True
            else:
                raise GitHubError(204, response.status_code, 'check if PR %d is merged' % pr_number)
        except urllib2.HTTPError as e:
            if e.code == 404:
                return False
            else:
                raise

    def merge_pr(self, pr_number):
        """Merges a PR.

        If merge cannot be performed, MergeError is raised. GitHubError is
        raised when other unknown errors happen.

        API doc: https://developer.github.com/v3/pulls/#merge-a-pull-request-merge-button
        """
        path = '/repos/%s/%s/pulls/%d/merge' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            pr_number
        )
        body = {
            'merge_method': 'rebase',
        }

        try:
            response = self.request(path, method='PUT', body=body)
        except urllib2.HTTPError as e:
            if e.code == 405:
                raise MergeError(pr_number)
            else:
                raise

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'merge PR %d' % pr_number)

    def delete_remote_branch(self, remote_branch_name):
        """Deletes a remote branch.

        API doc: https://developer.github.com/v3/git/refs/#delete-a-reference
        """
        path = '/repos/%s/%s/git/refs/heads/%s' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            remote_branch_name
        )
        response = self.request(path, method='DELETE')

        if response.status_code != 204:
            raise GitHubError(204, response.status_code, 'delete remote branch %s' % remote_branch_name)

    def pr_for_chromium_commit(self, chromium_commit):
        """Returns a PR corresponding to the given ChromiumCommit, or None."""
        # We rely on Change-Id because Gerrit returns ToT+1 as the commit
        # positions for in-flight CLs, whereas Change-Id is permanent.
        return self.pr_with_change_id(chromium_commit.change_id())

    def pr_with_change_id(self, target_change_id):
        for pull_request in self.all_pull_requests():
            # Note: Search all 'Change-Id's so that we can manually put multiple
            # CLs in one PR. (The exporter always creates one PR for each CL.)
            change_ids = self.extract_metadata('Change-Id: ', pull_request.body, all_matches=True)
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


class JSONResponse(object):
    """An HTTP response containing JSON data."""

    def __init__(self, raw_response):
        """Initializes a JSONResponse instance.

        Args:
            raw_response: a response object returned by open methods in urllib2.
        """
        self._raw_response = raw_response
        self.status_code = raw_response.getcode()
        try:
            self.data = json.load(raw_response)
        except ValueError:
            self.data = None

    def getheader(self, header):
        """Gets the value of the header with the given name.

        Delegates to HTTPMessage.getheader(), which is case-insensitive."""
        return self._raw_response.info().getheader(header)


class GitHubError(Exception):
    """Raised when GitHub returns a non-OK response status for a request."""

    def __init__(self, expected, received, action, extra_data=None):
        message = 'Expected {}, but received {} from GitHub when attempting to {}'.format(
            expected, received, action
        )
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


PullRequest = namedtuple('PullRequest', ['title', 'number', 'body', 'state', 'labels'])
