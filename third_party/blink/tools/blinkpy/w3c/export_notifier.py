# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sends notifications after automatic exports.

Automatically comments on a Gerrit CL when its corresponding PR fails the Taskcluster check. In
other words, surfaces cross-browser WPT regressions from Github to Gerrit.


Design doc: https://docs.google.com/document/d/1MtdbUcWBDZyvmV0FOdsTWw_Jv16YtE6KW5BnnCVYX4c

"""

import logging
from typing import Mapping

from blinkpy.w3c.common import WPT_REVISION_FOOTER, WPT_GH_URL
from blinkpy.w3c.gerrit import GerritError
from blinkpy.w3c.wpt_github import GitHubError

_log = logging.getLogger(__name__)
RELEVANT_TASKCLUSTER_CHECKS = [
    'wpt-chrome-dev-stability', 'wpt-firefox-nightly-stability', 'lint',
    'infrastructure/ tests'
]


class ExportNotifier(object):
    def __init__(self, host, wpt_github, gerrit, dry_run=True):
        self.host = host
        self.wpt_github = wpt_github
        self.gerrit = gerrit
        self.dry_run = dry_run

    def main(self) -> Mapping[str, 'PRStatusInfo']:
        """Surfaces relevant Taskcluster check failures to Gerrit through comments.

        Returns:
            A map from change IDs to statuses for failed PRs.

        Raises:
            ExportNotifierError: If the export notification somehow failed.
        """
        prs_by_change_id = {}

        try:
            _log.info('Searching for recent failing chromium exports.')
            prs = self.wpt_github.recent_failing_chromium_exports()
        except GitHubError as e:
            raise ExportNotifierError('Surfacing Taskcluster failures '
                                      f'could not be completed: {e}') from e

        if len(prs) > 100:
            raise ExportNotifierError(
                f'Too many open failing PRs: {len(prs)}; abort.')

        _log.info('Found %d failing PRs.', len(prs))
        for pr in prs:
            check_runs = self.get_check_runs(pr.number)
            if not check_runs:
                continue

            checks_results = self.get_relevant_failed_taskcluster_checks(check_runs)
            if not checks_results:
                continue

            gerrit_id = self.wpt_github.extract_metadata(
                'Change-Id: ', pr.body)
            if not gerrit_id:
                _log.error('Can not retrieve Change-Id for %s.', pr.number)
                continue

            gerrit_sha = self.wpt_github.extract_metadata(
                WPT_REVISION_FOOTER, pr.body)
            prs_by_change_id[gerrit_id] = PRStatusInfo(checks_results,
                                                       pr.number, gerrit_sha)

        self.process_failing_prs(prs_by_change_id)
        return prs_by_change_id

    def get_check_runs(self, number):
        """Retrieves check runs through a PR number.

        Returns:
            A JSON array representing the check runs for the HEAD of this PR.
        """
        try:
            branch = self.wpt_github.get_pr_branch(number)
            check_runs = self.wpt_github.get_branch_check_runs(branch)
        except GitHubError as e:
            _log.error(str(e))
            return None

        return check_runs

    def process_failing_prs(self, prs_by_change_id):
        """Processes and comments on CLs with failed Tackcluster checks."""
        _log.info('Processing %d CLs with failed Taskcluster checks.',
                  len(prs_by_change_id))
        for change_id, pr_status_info in prs_by_change_id.items():
            _log.info('Change-Id: %s', change_id)
            try:
                cl = self.gerrit.query_cl_comments_and_revisions(change_id)
                has_commented = self.has_latest_taskcluster_status_commented(
                    cl.messages, pr_status_info)
                if has_commented:
                    _log.info('Comment is up-to-date. Nothing to do here.')
                    continue

                revision = cl.revisions.get(pr_status_info.gerrit_sha)
                if revision:
                    cl_comment = pr_status_info.to_gerrit_comment(
                        revision['_number'])
                else:
                    cl_comment = pr_status_info.to_gerrit_comment()

                if self.dry_run:
                    _log.info('[dry_run] Would have commented on CL %s\n',
                              change_id)
                    _log.debug('Comments are:\n%s\n', cl_comment)
                else:
                    _log.info('Commenting on CL %s\n', change_id)
                    cl.post_comment(cl_comment)
            except GerritError as e:
                _log.error('Could not process Gerrit CL %s: %s', change_id,
                           str(e))
                continue

    def has_latest_taskcluster_status_commented(self, messages,
                                                pr_status_info):
        """Determines if the Taskcluster status has already been commented on the messages of a CL.

        Args:
            messages: messagese of a CL in JSON Array format, in chronological order.
            pr_status_info: PRStatusInfo object.
        """
        for message in reversed(messages):
            cl_gerrit_sha = PRStatusInfo.get_gerrit_sha_from_comment(
                message['message'])
            if cl_gerrit_sha:
                _log.debug('Found latest comment: %s', message['message'])
                return cl_gerrit_sha == pr_status_info.gerrit_sha

        return False

    def get_relevant_failed_taskcluster_checks(self, check_runs):
        """Filters relevant failed Taskcluster checks from check_runs.

        Args:
            check_runs: A JSON array; e.g. "check_runs" in
                https://developer.github.com/v3/checks/runs/#response-3

        Returns:
            A dictionary where keys are names of the Taskcluster checks and values
            are URLs to the Taskcluster checks' results.
        """
        checks_results = {}
        for check in check_runs:
            if (check['conclusion'] == 'failure') and (
                    check['name'] in RELEVANT_TASKCLUSTER_CHECKS):
                result_url = '{}runs/{}'.format(WPT_GH_URL, check['id'])
                checks_results[check['name']] = result_url

        return checks_results


class ExportNotifierError(Exception):
    """Represents an unsuccessful notification attempt."""


class PRStatusInfo(object):
    CL_SHA_TAG = 'Gerrit CL SHA: '
    PATCHSET_TAG = 'Patchset Number: '

    def __init__(self, checks_results, pr_number, gerrit_sha=None):
        self._checks_results = checks_results
        self.pr_number = pr_number
        if gerrit_sha:
            self._gerrit_sha = gerrit_sha
        else:
            self._gerrit_sha = 'Latest'

    @property
    def gerrit_sha(self):
        return self._gerrit_sha

    @staticmethod
    def get_gerrit_sha_from_comment(comment):
        for line in comment.splitlines():
            if line.startswith(PRStatusInfo.CL_SHA_TAG):
                return line[len(PRStatusInfo.CL_SHA_TAG):]

        return None

    def _checks_results_as_comment(self):
        comment = ''
        for check, url in self._checks_results.items():
            comment += '\n%s (%s)' % (check, url)

        return comment

    def to_gerrit_comment(self, patchset=None):
        comment = (
            'The exported PR, {}, has failed the following check(s) '
            'on GitHub:\n{}\n\nThese failures will block the export. '
            'They may represent new or existing problems; please take '
            'a look at the output and see if it can be fixed. '
            'Unresolved failures will be looked at by the Ecosystem-Infra '
            'sheriff after this CL has been landed in Chromium; if you '
            'need earlier help please contact blink-dev@chromium.org.\n\n'
            'Any suggestions to improve this service are welcome; '
            'crbug.com/1027618.').format(
                '%spull/%d' % (WPT_GH_URL, self.pr_number),
                self._checks_results_as_comment())

        comment += ('\n\n{}{}').format(PRStatusInfo.CL_SHA_TAG,
                                       self._gerrit_sha)
        if patchset is not None:
            comment += ('\n{}{}').format(PRStatusInfo.PATCHSET_TAG, patchset)

        return comment
