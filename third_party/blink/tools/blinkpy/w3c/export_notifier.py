# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sends notifications after automatic exports.

Automatically comments on a Gerrit CL when its corresponding PR fails the Taskcluster check. In
other words, surfaces cross-browser WPT regressions from Github to Gerrit.


Design doc: https://docs.google.com/document/d/1MtdbUcWBDZyvmV0FOdsTWw_Jv16YtE6KW5BnnCVYX4c

"""

import logging

from blinkpy.w3c.common import WPT_REVISION_FOOTER
from blinkpy.w3c.gerrit import GerritError
from blinkpy.w3c.wpt_github import GitHubError

_log = logging.getLogger(__name__)


class ExportNotifier(object):

    def __init__(self, host, wpt_github, gerrit, dry_run=True):
        self.host = host
        self.wpt_github = wpt_github
        self.gerrit = gerrit
        self.dry_run = dry_run

    def main(self):
        """Surfaces stability check failures to Gerrit through comments."""
        gerrit_dict = {}

        try:
            _log.info('Searching for recent failiing chromium exports.')
            prs = self.wpt_github.recent_failing_chromium_exports()
        except GitHubError as e:
            _log.info(
                'Surfacing Taskcluster failures cannot be completed due to the following error:')
            _log.error(str(e))
            return True

        if len(prs) > 100:
            _log.error(
                'Too many open failing PRs: %s; abort.', len(prs))
            return True

        _log.info('Found %d failing PRs.', len(prs))
        for pr in prs:
            statuses = self.get_taskcluster_statuses(pr.number)
            if not statuses:
                continue

            taskcluster_status = self.get_failure_taskcluster_status(
                statuses, pr.number)
            if not taskcluster_status:
                continue

            gerrit_id = self.wpt_github.extract_metadata(
                'Change-Id: ', pr.body)
            if not gerrit_id:
                _log.error(
                    'Can not retrieve Change-Id for %s.', pr.number)
                continue

            gerrit_sha = self.wpt_github.extract_metadata(
                WPT_REVISION_FOOTER + ' ', pr.body)
            gerrit_dict[gerrit_id] = PRStatusInfo(
                taskcluster_status['node_id'],
                taskcluster_status['target_url'],
                gerrit_sha)

        self.process_failing_prs(gerrit_dict)
        return False

    def get_taskcluster_statuses(self, number):
        """Retrieves Taskcluster status through PR number.

        Returns:
            A JSON object representing all Taskcluster statuses for this PR.
        """
        try:
            branch = self.wpt_github.get_pr_branch(number)
            statuses = self.wpt_github.get_branch_statuses(branch)
        except GitHubError as e:
            _log.error(str(e))
            return None

        return statuses

    def process_failing_prs(self, gerrit_dict):
        """Processes and comments on CLs with failed TackCluster status."""
        _log.info('Processing %d CLs with failed Taskcluster status.',
                  len(gerrit_dict))
        for change_id, pr_status_info in gerrit_dict.items():
            try:
                cl = self.gerrit.query_cl_comments_and_revisions(change_id)
                has_commented = self.has_latest_taskcluster_status_commented(
                    cl.messages, pr_status_info)
                if has_commented:
                    continue

                revision = cl.revisions.get(pr_status_info.gerrit_sha)
                if revision:
                    cl_comment = pr_status_info.to_gerrit_comment(
                        revision['_number'])
                else:
                    cl_comment = pr_status_info.to_gerrit_comment()

                if self.dry_run:
                    _log.info(
                        '[dry_run] Would have commented on CL %s\n', change_id)
                    _log.debug(
                        'Comments are:\n %s\n', cl_comment)
                else:
                    _log.info('Commenting on CL %s\n', change_id)
                    cl.post_comment(cl_comment)
            except GerritError as e:
                _log.error(
                    'Could not process Gerrit CL %s: %s', change_id, str(e))
                continue

    def has_latest_taskcluster_status_commented(self, messages, pr_status_info):
        """Determines if the Taskcluster status has already been commented on the messages of a CL.

        Args:
            messages: messagese of a CL in JSON Array format, in chronological order.
            pr_status_info: PRStatusInfo object.
        """
        for message in reversed(messages):
            existing_status = PRStatusInfo.from_gerrit_comment(
                message['message'])
            if existing_status:
                return existing_status.node_id == pr_status_info.node_id

        return False

    def get_failure_taskcluster_status(self, taskcluster_status, pr_number):
        """Parses Taskcluster status from Taskcluster statuses description field.

        Args:
            taskcluster_status: array; is of following format:
                [
                    {
                    "url": "https://b",
                    "avatar_url": "https://a",
                    "id": 1,
                    "node_id": "A",
                    "state": "failure",
                    "description": "TaskGroup: failure",
                    "target_url": "https://tools.taskcluster.net/task-group-inspector/#/abc",
                    "context": "Community-TC (pull_request)",
                    "created_at": "2019-08-05T22:52:08Z",
                    "updated_at": "2019-08-05T22:52:08Z"
                    }
                ]
                e.g. https://api.github.com/repos/web-platform-tests/wpt/commits/chromium-export-cl-1407433/status
            pr_number: The PR number.

        Returns:
            Taskcluster status dictionary if it has a failure status; None otherwise.
        """
        status = None
        for status_dict in taskcluster_status:
            if status_dict['context'] == 'Community-TC (pull_request)':
                status = status_dict
                break

        if status and status['state'] == 'failure':
            return status

        if status is None:
            return None

        assert status['state'] == 'success'
        _log.debug('Taskcluster status for PR %s is %s', pr_number, status)
        return None


class PRStatusInfo(object):
    NODE_ID_TAG = 'Taskcluster Node ID: '
    LINK_TAG = 'Taskcluster Link: '
    CL_SHA_TAG = 'Gerrit CL SHA: '
    PATCHSET_TAG = 'Patchset Number: '

    def __init__(self, node_id, link, gerrit_sha=None):
        self._node_id = node_id
        self._link = link
        if gerrit_sha:
            self._gerrit_sha = gerrit_sha
        else:
            self._gerrit_sha = 'Latest'

    @property
    def node_id(self):
        return self._node_id

    @property
    def link(self):
        return self._link

    @property
    def gerrit_sha(self):
        return self._gerrit_sha

    @staticmethod
    def from_gerrit_comment(comment):
        tags = [PRStatusInfo.NODE_ID_TAG,
                PRStatusInfo.LINK_TAG,
                PRStatusInfo.CL_SHA_TAG]
        values = ['', '', '']

        for line in comment.splitlines():
            for index, tag in enumerate(tags):
                if line.startswith(tag):
                    values[index] = line[len(tag):]

        for val in values:
            if not val:
                return None

        return PRStatusInfo(*values)

    def to_gerrit_comment(self, patchset=None):
        status_line = ('The exported PR for the current patch failed Taskcluster check(s) '
                       'on GitHub, which could indict cross-broswer failures on the '
                       'exportable changes. Please contact ecosystem-infra@ team for '
                       'more information.')
        node_id_line = ('\n\n{}{}').format(
            PRStatusInfo.NODE_ID_TAG, self.node_id)
        link_line = ('\n{}{}').format(PRStatusInfo.LINK_TAG, self.link)
        sha_line = ('\n{}{}').format(PRStatusInfo.CL_SHA_TAG, self.gerrit_sha)

        comment = status_line + node_id_line + link_line + sha_line
        if patchset is not None:
            comment += ('\n{}{}').format(PRStatusInfo.PATCHSET_TAG, patchset)

        return comment
