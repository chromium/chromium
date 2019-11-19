# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Exports Chromium changes to web-platform-tests."""

import argparse
import logging

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from blinkpy.w3c.common import (
    WPT_GH_URL,
    WPT_REVISION_FOOTER,
    EXPORT_PR_LABEL,
    PROVISIONAL_PR_LABEL,
    read_credentials
)
from blinkpy.w3c.gerrit import GerritAPI, GerritCL, GerritError
from blinkpy.w3c.wpt_github import WPTGitHub, MergeError
from blinkpy.w3c.export_notifier import ExportNotifier

_log = logging.getLogger(__name__)


class TestExporter(object):

    def __init__(self, host):
        self.host = host
        self.wpt_github = None
        self.gerrit = None
        self.dry_run = False
        self.local_wpt = None

    def main(self, argv=None):
        """Creates PRs for in-flight CLs and merges changes that land on master.

        Returns:
            A boolean: True if success, False if there were any patch failures.
        """
        options = self.parse_args(argv)

        self.dry_run = options.dry_run
        log_level = logging.DEBUG if options.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)
        if options.verbose:
            # Print out the full output when executive.run_command fails.
            self.host.executive.error_output_limit = None

        credentials = read_credentials(self.host, options.credentials_json)
        if not (credentials.get('GH_USER') and credentials.get('GH_TOKEN')):
            _log.error('You must provide your GitHub credentials for this '
                       'script to work.')
            _log.error('See https://chromium.googlesource.com/chromium/src'
                       '/+/master/docs/testing/web_platform_tests.md'
                       '#GitHub-credentials for instructions on how to set '
                       'your credentials up.')
            return False

        self.wpt_github = self.wpt_github or WPTGitHub(self.host, credentials['GH_USER'], credentials['GH_TOKEN'])
        self.gerrit = self.gerrit or GerritAPI(self.host, credentials['GERRIT_USER'], credentials['GERRIT_TOKEN'])
        self.local_wpt = self.local_wpt or LocalWPT(self.host, credentials['GH_TOKEN'])
        self.local_wpt.fetch()

        _log.info('Searching for exportable in-flight CLs.')
        # The Gerrit search API is slow and easy to fail, so we wrap it in a try
        # statement to continue exporting landed commits when it fails.
        try:
            open_gerrit_cls = self.gerrit.query_exportable_open_cls()
        except GerritError as e:
            _log.info('In-flight CLs cannot be exported due to the following error:')
            _log.error(str(e))
            gerrit_error = True
        else:
            self.process_gerrit_cls(open_gerrit_cls)
            gerrit_error = False

        _log.info('Searching for exportable Chromium commits.')
        exportable_commits, git_errors = self.get_exportable_commits()
        self.process_chromium_commits(exportable_commits)
        if git_errors:
            _log.info('Attention: The following errors have prevented some commits from being '
                      'exported:')
            for error in git_errors:
                _log.error(error)

        export_error = gerrit_error or git_errors
        if export_error:
            return not export_error

        _log.info('Automatic export process has finished successfully.')

        export_notifier_failure = False
        if options.surface_failures_to_gerrit:
            _log.info('Starting surfacing cross-browser failures to Gerrit.')
            export_notifier_failure = ExportNotifier(
                self.host, self.wpt_github, self.gerrit, self.dry_run).main()

        return not export_notifier_failure

    def parse_args(self, argv):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v', '--verbose', action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--dry-run', action='store_true',
            help='See what would be done without actually creating or merging '
                 'any pull requests.')
        parser.add_argument(
            '--credentials-json', required=True,
            help='A JSON file with an object containing zero or more of the '
                 'following keys: GH_USER, GH_TOKEN, GERRIT_USER, GERRIT_TOKEN')
        parser.add_argument(
            '--surface-failures-to-gerrit', action='store_true',
            help='Indicates whether to run the service that surfaces GitHub '
                 'faliures to Gerrit through comments.')
        return parser.parse_args(argv)

    def process_gerrit_cls(self, gerrit_cls):
        for cl in gerrit_cls:
            self.process_gerrit_cl(cl)

    def process_gerrit_cl(self, cl):
        _log.info('Found Gerrit in-flight CL: "%s" %s', cl.subject, cl.url)

        if not cl.has_review_started:
            _log.info('CL review has not started, skipping.')
            return

        pull_request = self.wpt_github.pr_with_change_id(cl.change_id)
        if pull_request:
            # If CL already has a corresponding PR, see if we need to update it.
            pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
            _log.info('In-flight PR found: %s', pr_url)

            pr_cl_revision = self.wpt_github.extract_metadata(WPT_REVISION_FOOTER + ' ', pull_request.body)
            if cl.current_revision_sha == pr_cl_revision:
                _log.info('PR revision matches CL revision. Nothing to do here.')
                return

            _log.info('New revision found, updating PR...')
            self.create_or_update_pr_from_inflight_cl(cl, pull_request)
        else:
            # Create a new PR for the CL if it does not have one.
            _log.info('No in-flight PR found for CL. Creating...')
            self.create_or_update_pr_from_inflight_cl(cl)

    def process_chromium_commits(self, exportable_commits):
        for commit in exportable_commits:
            self.process_chromium_commit(commit)

    def process_chromium_commit(self, commit):
        _log.info('Found exportable Chromium commit: %s %s', commit.subject(), commit.sha)

        pull_request = self.wpt_github.pr_for_chromium_commit(commit)
        if pull_request:
            pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
            _log.info('In-flight PR found: %s', pr_url)

            if pull_request.state != 'open':
                _log.info('Pull request is %s. Skipping.', pull_request.state)
                return

            if PROVISIONAL_PR_LABEL in pull_request.labels:
                # If the PR was created from a Gerrit in-flight CL, update the
                # PR with the final checked-in commit in Chromium history.
                # TODO(robertma): Only update the PR when it is not up-to-date
                # to avoid unnecessary Travis runs.
                _log.info('Updating PR with the final checked-in change...')
                self.create_or_update_pr_from_landed_commit(commit, pull_request)
                self.remove_provisional_pr_label(pull_request)
                # Updating the patch triggers Travis, which will block merge.
                # Return early and merge next time.
                return

            self.merge_pull_request(pull_request)
        else:
            _log.info('No PR found for Chromium commit. Creating...')
            self.create_or_update_pr_from_landed_commit(commit)

    def get_exportable_commits(self):
        """Gets exportable commits that can apply cleanly and independently.

        Returns:
            A list of ChromiumCommit for clean exportable commits, and a list
            of error messages for other exportable commits that fail to apply.
        """
        # Exportable commits that cannot apply cleanly are logged, and will be
        # retried next time. A common case is that a commit depends on an
        # earlier commit, and can only be exported after the earlier one.
        return exportable_commits_over_last_n_commits(
            self.host, self.local_wpt, self.wpt_github, require_clean=True)

    def remove_provisional_pr_label(self, pull_request):
        if self.dry_run:
            _log.info('[dry_run] Would have attempted to remove the provisional PR label')
            return

        _log.info('Removing provisional label "%s"...', PROVISIONAL_PR_LABEL)
        self.wpt_github.remove_label(pull_request.number, PROVISIONAL_PR_LABEL)

    def merge_pull_request(self, pull_request):
        if self.dry_run:
            _log.info('[dry_run] Would have attempted to merge PR')
            return

        _log.info('Attempting to merge...')

        # This is outside of the try block because if there's a problem communicating
        # with the GitHub API, we should hard fail.
        branch = self.wpt_github.get_pr_branch(pull_request.number)

        try:
            self.wpt_github.merge_pr(pull_request.number)

            # This is in the try block because if a PR can't be merged, we shouldn't
            # delete its branch.
            _log.info('Deleting remote branch %s...', branch)
            self.wpt_github.delete_remote_branch(branch)

            change_id = self.wpt_github.extract_metadata('Change-Id: ', pull_request.body)
            if change_id:
                cl = GerritCL(data={'change_id': change_id}, api=self.gerrit)
                pr_url = '{}pull/{}'.format(WPT_GH_URL, pull_request.number)
                cl.post_comment((
                    'The WPT PR for this CL has been merged upstream! {pr_url}'
                ).format(
                    pr_url=pr_url
                ))

        except MergeError:
            _log.warn('Could not merge PR.')

    def create_or_update_pr_from_landed_commit(self, commit, pull_request=None):
        """Creates or updates a PR from a landed Chromium commit.

        Args:
            commit: A ChromiumCommit object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        if pull_request:
            self.create_or_update_pr_from_commit(commit, provisional=False, pr_number=pull_request.number)
        else:
            branch_name = 'chromium-export-' + commit.short_sha
            self.create_or_update_pr_from_commit(commit, provisional=False, pr_branch_name=branch_name)

    def create_or_update_pr_from_inflight_cl(self, cl, pull_request=None):
        """Creates or updates a PR from an in-flight Gerrit CL.

        Args:
            cl: A GerritCL object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        commit = cl.fetch_current_revision_commit(self.host)
        patch = commit.format_patch()

        success, error = self.local_wpt.test_patch(patch)
        if not success:
            _log.error('Gerrit CL patch did not apply cleanly:')
            _log.error(error)
            _log.debug('First 500 characters of patch: << END_OF_PATCH_EXCERPT')
            _log.debug(patch[0:500])
            _log.debug('END_OF_PATCH_EXCERPT')
            return

        # Reviewed-on footer is not in the git commit message of in-flight CLs,
        # but a link to code review is useful so we add it manually.
        footer = 'Reviewed-on: {}\n'.format(cl.url)
        # WPT_REVISION_FOOTER is used by the exporter to check the CL revision.
        footer += '{} {}'.format(WPT_REVISION_FOOTER, cl.current_revision_sha)

        if pull_request:
            pr_number = self.create_or_update_pr_from_commit(
                commit, provisional=True, pr_number=pull_request.number, pr_footer=footer)
            if pr_number is None:
                return

            # TODO(jeffcarp): Turn PullRequest into a class with a .url method
            cl.post_comment((
                'Successfully updated WPT GitHub pull request with '
                'new revision "{subject}": {pr_url}'
            ).format(
                subject=cl.current_revision_description,
                pr_url='%spull/%d' % (WPT_GH_URL, pull_request.number),
            ))
        else:
            branch_name = 'chromium-export-cl-{}'.format(cl.number)
            pr_number = self.create_or_update_pr_from_commit(
                commit, provisional=True, pr_footer=footer, pr_branch_name=branch_name)
            if pr_number is None:
                return

            cl.post_comment((
                'Exportable changes to web-platform-tests were detected in this CL '
                'and a pull request in the upstream repo has been made: {pr_url}.\n\n'
                'When this CL lands, the bot will automatically merge the PR '
                'on GitHub if the required GitHub checks pass; otherwise, '
                'ecosystem-infra@ team will triage the failures and may contact you.\n\n'
                'WPT Export docs:\n'
                'https://chromium.googlesource.com/chromium/src/+/master'
                '/docs/testing/web_platform_tests.md#Automatic-export-process'
            ).format(
                pr_url='%spull/%d' % (WPT_GH_URL, pr_number)
            ))

    def create_or_update_pr_from_commit(self, commit, provisional, pr_number=None, pr_footer='', pr_branch_name=None):
        """Creates or updates a PR from a Chromium commit.

        The commit can be either landed or in-flight. The exportable portion of
        the patch is extracted and applied to a new branch in the local WPT
        repo, whose name is determined by pr_branch_name (if the branch already
        exists, it will be recreated from master). The branch is then pushed to
        WPT on GitHub, from which a PR is created or updated.

        Args:
            commit: A ChromiumCommit object.
            provisional: True if the commit is from a Gerrit in-flight CL,
                False if the commit has landed.
            pr_number: Optional, a PR issue number.
                If specified, updates the PR instead of creating one.
            pr_footer: Optional, additional text to be appended to PR
                description after the commit message.
            pr_branch_name: Optional, the name of the head branch of the PR.
                If unspecified, the current head branch of the PR will be used.

        Returns:
            The issue number (an int) of the updated/created PR, or None if no
            change is made.
        """
        patch = commit.format_patch()
        message = commit.message()
        subject = commit.subject()
        # Replace '<' with '\<', crbug.com/822278.
        body = commit.body().replace(r'<', r'\<')
        author = commit.author()
        updating = bool(pr_number)
        pr_description = body + pr_footer
        if not pr_branch_name:
            assert pr_number, 'pr_number and pr_branch_name cannot be both absent.'
            pr_branch_name = self.wpt_github.get_pr_branch(pr_number)

        if self.dry_run:
            action_str = 'updating' if updating else 'creating'
            origin_str = 'CL' if provisional else 'Chromium commit'
            _log.info('[dry_run] Stopping before %s PR from %s', action_str, origin_str)
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.debug('\n[dry_run] First 500 characters of patch: << END_OF_PATCH_EXCERPT')
            _log.debug(patch[0:500])
            _log.debug('END_OF_PATCH_EXCERPT')
            return

        self.local_wpt.create_branch_with_patch(pr_branch_name, message, patch, author, force_push=True)

        if updating:
            self.wpt_github.update_pr(pr_number, subject, pr_description)
        else:
            pr_number = self.wpt_github.create_pr(pr_branch_name, subject, pr_description)
            self.wpt_github.add_label(pr_number, EXPORT_PR_LABEL)
            if provisional:
                self.wpt_github.add_label(pr_number, PROVISIONAL_PR_LABEL)

        return pr_number
