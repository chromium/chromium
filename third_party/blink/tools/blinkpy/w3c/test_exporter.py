# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Exports Chromium changes to web-platform-tests."""

import argparse
import collections
import enum
import logging
from typing import MutableMapping, NamedTuple, Optional, Set, TextIO

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.common import (
    CHANGE_ID_FOOTER,
    read_credentials,
)
from blinkpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from blinkpy.w3c.export_notifier import ExportNotifier, ExportNotifierError
from blinkpy.w3c.gerrit import GerritAPI, GerritCL, GerritError
from blinkpy.w3c.graphql import GraphQL
from blinkpy.w3c.pr_cleanup_tool import PrCleanupTool
from blinkpy.w3c.wpt_github import MergeError

_log = logging.getLogger(__name__)


class PREventType(enum.Enum):
    CREATED = enum.auto()
    UPDATED = enum.auto()
    BLOCKED = enum.auto()
    MARKED_READY = enum.auto()
    MERGED = enum.auto()


class PREvent(NamedTuple):
    number: int
    event_type: PREventType


PREventsByType = MutableMapping[PREventType, Set[int]]


class TestExporter:

    def __init__(self, host):
        self.host = host
        self.project_config = host.project_config
        self.pr_cleaner = PrCleanupTool(self.host)
        self.github = None
        self.graphql = None
        self.gerrit = None
        self.dry_run = False
        self.local_repo = None
        self.surface_failures_to_gerrit = False
        self.create_draft_pr = (
            self.project_config.gerrit_project == 'chromium/src')

    def main(self, argv=None):
        """Creates PRs for in-flight CLs and merges changes that land on main.

        Returns:
            A boolean: True if success, False if there were any patch failures.
        """
        options = self.parse_args(argv)

        self.dry_run = options.dry_run
        self.surface_failures_to_gerrit = options.surface_failures_to_gerrit
        log_level = logging.DEBUG if options.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        # Having the full output when executive.run_command fails is useful when
        # investigating a failed export, as all we have are logs.
        self.host.executive.error_output_limit = None

        credentials = read_credentials(self.host, options.credentials_json)
        if not (credentials.get('GH_USER') and credentials.get('GH_TOKEN')):
            _log.error('You must provide your GitHub credentials for this '
                       'script to work.')
            _log.error('See https://chromium.googlesource.com/chromium/src'
                       '/+/main/docs/testing/web_platform_tests.md'
                       '#GitHub-credentials for instructions on how to set '
                       'your credentials up.')
            return False
        self.github = self.github or self.project_config.github_factory(
            host=self.host,
            user=credentials['GH_USER'],
            token=credentials['GH_TOKEN'])
        self.gerrit = self.gerrit or GerritAPI.from_credentials(
            self.host, credentials)
        self.local_repo = self.local_repo or self.project_config.local_repo_factory(
            host=self.host, gh_token=credentials['GH_TOKEN'])

        if self.create_draft_pr:
            self.graphql = GraphQL(credentials['GH_TOKEN'])

        if not self.dry_run:
            self.pr_cleaner.run(self.github, self.gerrit)

        self.local_repo.fetch()

        pr_events = collections.defaultdict(set)

        gerrit_error = self.export_in_flight_changes(pr_events)

        _log.info('Searching for exportable Chromium commits.')
        exportable_commits, git_errors = self.get_exportable_commits()
        self.process_chromium_commits(exportable_commits, pr_events)
        if git_errors:
            _log.info(
                'Attention: The following errors have prevented some commits from being '
                'exported:')
            for error in git_errors:
                _log.error(error)

        try:
            export_error = gerrit_error or git_errors
            if export_error:
                return not export_error

            _log.info('Automatic export process has finished successfully.')

            if self.surface_failures_to_gerrit:
                _log.info(
                    'Starting surfacing cross-browser failures to Gerrit.')
                notifier = ExportNotifier(self.host, self.github, self.gerrit,
                                          self.dry_run)
                prs_by_change_id = notifier.main()
                pr_events[PREventType.BLOCKED].update(
                    pr_status.pr_number
                    for pr_status in prs_by_change_id.values())

            return True
        except ExportNotifierError as error:
            _log.exception(f'Failed to surface upstream failures: {error}')
            return False
        finally:
            if options.summary_markdown:
                with self.host.filesystem.open_text_file_for_writing(
                        options.summary_markdown) as summary_file:
                    self.summarize(summary_file, pr_events)

    def summarize(self, summary_file: TextIO, pr_events: PREventsByType):
        if not pr_events:
            summary_file.write('No pull requests modified.\n')
            return
        descriptions = {
            PREventType.CREATED: 'Pull requests created',
            PREventType.UPDATED: 'Pull requests updated to a new revision',
            PREventType.BLOCKED: 'Pull requests that failed to merge',
            PREventType.MARKED_READY:
            'Pull requests marked as ready for review',
            PREventType.MERGED: 'Pull requests merged',
        }
        for event_type in PREventType:
            pr_numbers = pr_events.get(event_type, set())
            if not pr_numbers:
                continue
            summary_file.write(f'{descriptions[event_type]}:\n')
            for pr_number in sorted(pr_numbers):
                summary_file.write(f'* {self.github.url}pull/{pr_number}\n')
            summary_file.write('\n')

    def parse_args(self, argv):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='See what would be done without actually creating or merging '
            'any pull requests.')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with an object containing zero or more of the '
            'following keys: GH_USER, GH_TOKEN, GERRIT_USER, GERRIT_TOKEN')
        parser.add_argument(
            '--surface-failures-to-gerrit',
            action='store_true',
            help='Indicates whether to run the service that surfaces GitHub '
            'faliures to Gerrit through comments.')
        parser.add_argument(
            '--summary-markdown',
            help='Write a summary of PR updates to this markdown file.')
        return parser.parse_args(argv)

    def export_in_flight_changes(self, pr_events: PREventsByType) -> bool:
        """ Search and export in-flight changes from Gerrit.
        Returns:
            A boolean: True if there was an error, False otherwise.
        """
        _log.info('Searching for exportable in-flight CLs.')
        # The Gerrit search API is slow and easy to fail, so we wrap it in a try
        # statement to continue exporting landed commits when it fails.
        try:
            open_gerrit_cls = self.gerrit.query_exportable_cls()
        except GerritError as e:
            _log.info(
                'In-flight CLs cannot be exported due to the following error:')
            _log.error(str(e))
            # TODO(crbug.com/346392205) change this back to True once the bug is fixed
            # We do not need to mark the exporter run as failed due to this. Instead
            # in flight changes can be exported in the next exporter run, or exported
            # after the change has been submitted.
            return False
        else:
            self.process_gerrit_cls(open_gerrit_cls, pr_events)
            return False

    def process_gerrit_cls(self, gerrit_cls, pr_events: PREventType):
        for cl in gerrit_cls:
            maybe_event = self.process_gerrit_cl(cl)
            if maybe_event:
                pr_events[maybe_event.event_type].add(maybe_event.number)

    def process_gerrit_cl(self, cl) -> Optional[PREvent]:
        _log.info('Found Gerrit in-flight CL: "%s" %s', cl.subject, cl.url)

        if not cl.has_review_started:
            _log.info('CL review has not started, skipping.')
            return None

        pull_request = self.github.pr_with_change_id(cl.change_id)
        if pull_request:
            # If CL already has a corresponding PR, see if we need to update it.
            pr_url = f'{self.github.url}pull/{pull_request.number}'
            _log.info('In-flight PR found: %s', pr_url)
            pr_cl_revision = self.github.extract_metadata(
                self.project_config.revision_footer, pull_request.body)
            if cl.current_revision_sha == pr_cl_revision:
                _log.info(
                    'PR revision matches CL revision. Nothing to do here.')
                return None

            _log.info('New revision found, updating PR...')
            return self.create_or_update_pr_from_inflight_cl(cl, pull_request)
        else:
            # Create a new PR for the CL if it does not have one.
            _log.info('No in-flight PR found for CL. Creating...')
            return self.create_or_update_pr_from_inflight_cl(cl)

    def process_chromium_commits(self, exportable_commits,
                                 pr_events: PREventsByType):
        for commit in exportable_commits:
            maybe_event = self.process_chromium_commit(commit)
            if maybe_event:
                pr_events[maybe_event.event_type].add(maybe_event.number)

    def process_chromium_commit(self, commit) -> Optional[PREvent]:
        _log.info('Found exportable Chromium commit: %s %s', commit.subject(),
                  commit.sha)

        pull_request = self.github.pr_for_chromium_commit(commit)
        if pull_request:
            pr_url = f'{self.github.url}pull/{pull_request.number}'
            _log.info('In-flight PR found: %s', pr_url)

            if pull_request.state != 'open':
                _log.info('Pull request is %s. Skipping.', pull_request.state)
                return None

            if self.create_draft_pr:
                pr_response = self.graphql.mark_ready_for_review(
                    pull_request.node_id)
                _log.info(f'Marked PR with node ID {pull_request.node_id!r} '
                          'as ready for review.')

            if self.github.provisional_pr_label in pull_request.labels:
                # If the PR was created from a Gerrit in-flight CL, update the
                # PR with the final checked-in commit in Chromium history.
                # TODO(robertma): Only update the PR when it is not up-to-date
                # to avoid unnecessary Travis runs.
                _log.info('Updating PR with the final checked-in change...')
                self.create_or_update_pr_from_landed_commit(
                    commit, pull_request)
                self.remove_provisional_pr_label(pull_request)
                # Updating the patch triggers Travis, which will block merge.
                # Return early and merge next time.
                return PREvent(pull_request.number, PREventType.MARKED_READY)

            return self.merge_pull_request(pull_request)
        else:
            _log.info('No PR found for Chromium commit. Creating...')
            return self.create_or_update_pr_from_landed_commit(commit)

    def get_exportable_commits(self):
        """Gets exportable commits that can apply cleanly and independently.

        Returns:
            A list of ChromiumCommit for clean exportable commits, and a list
            of error messages for other exportable commits that fail to apply.
        """
        # Exportable commits that cannot apply cleanly are logged, and will be
        # retried next time. A common case is that a commit depends on an
        # earlier commit, and can only be exported after the earlier one.
        return exportable_commits_over_last_n_commits(self.host,
                                                      self.local_repo,
                                                      self.github,
                                                      require_clean=True)

    def remove_provisional_pr_label(self, pull_request):
        if self.dry_run:
            _log.info(
                '[dry_run] Would have attempted to remove the provisional PR label'
            )
            return
        _log.info('Removing provisional label "%s"...',
                  self.github.provisional_pr_label)
        self.github.remove_label(pull_request.number,
                                 self.github.provisional_pr_label)

    def merge_pull_request(self, pull_request) -> Optional[PREvent]:
        if self.dry_run:
            _log.info('[dry_run] Would have attempted to merge PR')
            return None

        _log.info('Attempting to merge...')

        # This is outside of the try block because if there's a problem communicating
        # with the GitHub API, we should hard fail.
        branch = self.github.get_pr_branch(pull_request.number)

        try:
            self.github.merge_pr(pull_request.number)
            change_id = self.github.extract_metadata(CHANGE_ID_FOOTER,
                                                     pull_request.body)
            if change_id:
                cl = GerritCL(data={'change_id': change_id}, api=self.gerrit)
                pr_url = f'{self.github.url}pull/{pull_request.number}'
                cl.post_comment(
                    f'The {self.local_repo.name} PR for this CL has been '
                    f'merged upstream! {pr_url}')
                return PREvent(pull_request.number, PREventType.MERGED)
        except MergeError:
            _log.warn('Could not merge PR.')
            return PREvent(pull_request.number, PREventType.BLOCKED)
        return None

    def create_or_update_pr_from_landed_commit(
        self,
        commit,
        pull_request=None,
    ) -> Optional[PREvent]:
        """Creates or updates a PR from a landed Chromium commit.

        Args:
            commit: A ChromiumCommit object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        if pull_request:
            return self.create_or_update_pr_from_commit(
                commit, provisional=False, pr_number=pull_request.number)
        else:
            branch_name = 'chromium-export-' + commit.short_sha
            return self.create_or_update_pr_from_commit(
                commit, provisional=False, pr_branch_name=branch_name)

    def create_or_update_pr_from_inflight_cl(
        self,
        cl,
        pull_request=None,
    ) -> Optional[PREvent]:
        """Creates or updates a PR from an in-flight Gerrit CL.

        Args:
            cl: A GerritCL object.
            pull_request: Optional, a PullRequest namedtuple.
                If specified, updates the PR instead of creating one.
        """
        commit = cl.fetch_current_revision_commit(self.host)
        patch = commit.format_patch()

        success, error = self.local_repo.test_patch(patch)
        if not success:
            _log.error('Gerrit CL patch did not apply cleanly:')
            _log.error(error)
            _log.debug(
                'First 500 characters of patch: << END_OF_PATCH_EXCERPT')
            _log.debug(patch[0:500])
            _log.debug('END_OF_PATCH_EXCERPT')
            return None

        footer = ''
        # Change-Id can be deleted from the body of an in-flight CL in Chromium
        # (https://crbug.com/gerrit/12244). We need to add it back. And we've
        # asserted that cl.change_id is present in GerritCL.
        if not self.github.extract_metadata(CHANGE_ID_FOOTER,
                                            commit.message()):
            _log.warn('Adding missing Change-Id back to %s', cl.url)
            footer += '{}{}\n'.format(CHANGE_ID_FOOTER, cl.change_id)
        # Reviewed-on footer is not in the git commit message of in-flight CLs,
        # but a link to code review is useful so we add it manually.
        footer += 'Reviewed-on: {}\n'.format(cl.url)
        # WPT_REVISION_FOOTER is used by the exporter to check the CL revision.
        footer += '{}{}'.format(self.project_config.revision_footer,
                                cl.current_revision_sha)

        if pull_request:
            maybe_event = self.create_or_update_pr_from_commit(
                commit,
                provisional=True,
                pr_number=pull_request.number,
                pr_footer=footer)

            # When surface_failures_to_gerrit is enabled, the pull request update comment below
            # is ignored.
            # TODO(jeffcarp): Turn PullRequest into a class with a .url method
            if not self.surface_failures_to_gerrit and maybe_event:
                pr_url = f'{self.github.url}pull/{maybe_event.number}'
                cl.post_comment(
                    self.project_config.pr_updated_comment_template.format(
                        subject=cl.current_revision_description,
                        pr_url=pr_url))
        else:
            branch_name = 'chromium-export-cl-{}'.format(cl.number)
            maybe_event = self.create_or_update_pr_from_commit(
                commit,
                provisional=True,
                pr_footer=footer,
                pr_branch_name=branch_name)
            if maybe_event:
                pr_url = f'{self.github.url}pull/{maybe_event.number}'
                cl.post_comment(
                    self.project_config.inflight_cl_comment_template.format(
                        pr_url=pr_url))

        return maybe_event

    def create_or_update_pr_from_commit(
            self,
            commit,
            provisional,
            pr_number=None,
            pr_footer='',
            pr_branch_name=None) -> Optional[PREvent]:
        """Creates or updates a PR from a Chromium commit.

        The commit can be either landed or in-flight. The exportable portion of
        the patch is extracted and applied to a new branch in the local WPT
        repo, whose name is determined by pr_branch_name (if the branch already
        exists, it will be recreated from main). The branch is then pushed to
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
            An event describing how the updated/created PR changed, or None if
            no change is made.
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
            pr_branch_name = self.github.get_pr_branch(pr_number)

        if self.dry_run:
            action_str = 'updating' if updating else 'creating'
            origin_str = 'CL' if provisional else 'Chromium commit'
            _log.info('[dry_run] Stopping before %s PR from %s', action_str,
                      origin_str)
            _log.info('\n\n[dry_run] message:')
            _log.info(message)
            _log.debug(
                '\n[dry_run] First 500 characters of patch: << END_OF_PATCH_EXCERPT'
            )
            _log.debug(patch[0:500])
            _log.debug('END_OF_PATCH_EXCERPT')
            return None

        self.local_repo.create_branch_with_patch(pr_branch_name,
                                                 message,
                                                 patch,
                                                 author,
                                                 force_push=True)

        if updating:
            self.github.update_pr(pr_number, subject, pr_description)
            return PREvent(pr_number, PREventType.UPDATED)
        else:
            pr_number = self.github.create_pr(pr_branch_name, subject,
                                              pr_description)
            self.github.add_label(pr_number, self.github.export_pr_label)
            if provisional:
                self.github.add_label(pr_number,
                                      self.github.provisional_pr_label)
            return PREvent(pr_number, PREventType.CREATED)
