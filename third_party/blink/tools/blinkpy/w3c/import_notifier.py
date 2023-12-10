# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sends notifications after automatic imports from web-platform-tests (WPT).

Automatically file bugs for new failures caused by WPT imports for opted-in
directories.

Design doc: https://docs.google.com/document/d/1W3V81l94slAC_rPcTKWXgv3YxRxtlSIAxi3yj6NsbBw/edit?usp=sharing
"""

from collections import defaultdict
import logging
import re
import typing
from typing import NamedTuple, Optional

from blinkpy.common import path_finder
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.system.executive import ScriptError
from blinkpy.web_tests.models.testharness_results import (
    LineType,
    Status,
    parse_testharness_baseline,
)
from blinkpy.w3c.common import WPT_GH_URL, WPT_GH_RANGE_URL_TEMPLATE
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.w3c.monorail import MonorailAPI, MonorailIssue
from blinkpy.w3c.buganizer import BuganizerClient
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.wpt_results_processor import TestType

_log = logging.getLogger(__name__)

GITHUB_COMMIT_PREFIX = WPT_GH_URL + 'commit/'
SHORT_GERRIT_PREFIX = 'https://crrev.com/c/'

USE_BUGANIZER = False
BUGANIZER_WPT_COMPONENT = "1415957"


class ImportNotifier:
    def __init__(self, host, chromium_git, local_wpt):
        self.host = host
        self.git = chromium_git
        self.local_wpt = local_wpt

        self._monorail_api = MonorailAPI
        self._buganizer_api = BuganizerClient
        self.default_port = host.port_factory.get()
        self.default_port.set_option_default('test_types',
                                             typing.get_args(TestType))
        self.finder = path_finder.PathFinder(host.filesystem)
        self.owners_extractor = DirectoryOwnersExtractor(host)
        self.new_failures_by_directory = defaultdict(list)

    def main(self,
             wpt_revision_start,
             wpt_revision_end,
             rebaselined_tests,
             test_expectations,
             issue,
             patchset,
             dry_run=True,
             service_account_key_json=None,
             sheriff_email=None):
        """Files bug reports for new failures.

        Args:
            wpt_revision_start: The start of the imported WPT revision range
                (exclusive), i.e. the last imported revision.
            wpt_revision_end: The end of the imported WPT revision range
                (inclusive), i.e. the current imported revision.
            rebaselined_tests: A list of test names that have been rebaselined.
            test_expectations: A dictionary mapping names of tests that cannot
                be rebaselined to a list of new test expectation lines.
            issue: The issue number of the import CL (a string).
            patchset: The patchset number of the import CL (a string).
            dry_run: If True, no bugs will be actually filed to crbug.com.
            service_account_key_json: The path to a JSON private key of a
                service account for accessing Monorail. If None, try to get an
                access token from luci-auth.

        Note: "test names" are paths of the tests relative to web_tests.
        """
        gerrit_url = SHORT_GERRIT_PREFIX + issue
        gerrit_url_with_ps = gerrit_url + '/' + patchset + '/'

        self.sheriff_email = sheriff_email

        changed_test_baselines = self.find_changed_baselines_of_tests(
            rebaselined_tests)
        self.examine_baseline_changes(changed_test_baselines,
                                      gerrit_url_with_ps)
        self.examine_new_test_expectations(test_expectations)

        bugs = self.create_bugs_from_new_failures(wpt_revision_start,
                                                  wpt_revision_end, gerrit_url)
        self.file_bugs(bugs, dry_run, service_account_key_json)

    def find_changed_baselines_of_tests(self, rebaselined_tests):
        """Finds the corresponding changed baselines of each test.

        Args:
            rebaselined_tests: A list of test names that have been rebaselined.

        Returns:
            A dictionary mapping test names to paths of their baselines changed
            in this import CL (paths relative to the root of Chromium repo).
        """
        test_baselines = {}
        changed_files = self.git.changed_files()
        for test_name in rebaselined_tests:
            test_without_ext, _ = self.host.filesystem.splitext(test_name)
            changed_baselines = []
            # TODO(robertma): Refactor this into web_tests.port.base.
            baseline_name = test_without_ext + '-expected.txt'
            for changed_file in changed_files:
                if changed_file.endswith(baseline_name):
                    changed_baselines.append(changed_file)
            if changed_baselines:
                test_baselines[test_name] = changed_baselines
        return test_baselines

    def examine_baseline_changes(self, changed_test_baselines,
                                 gerrit_url_with_ps):
        """Examines all changed baselines to find new failures.

        Args:
            changed_test_baselines: A dictionary mapping test names to paths of
                changed baselines.
            gerrit_url_with_ps: Gerrit URL of this CL with the patchset number.
        """
        for test_name, changed_baselines in changed_test_baselines.items():
            directory = self.find_directory_for_bug(test_name)
            if not directory:
                continue

            for baseline in changed_baselines:
                if self.more_failures_in_baseline(baseline):
                    self.new_failures_by_directory[directory].append(
                        TestFailure.from_file(test_name, baseline,
                                              gerrit_url_with_ps))

    def more_failures_in_baseline(self, baseline: str) -> bool:
        """Determines if a testharness.js baseline file has new failures.

        The file is assumed to have been modified in the current git checkout,
        and so has a diff we can parse.

        We recognize two types of failures: FAIL lines, which are output for a
        specific subtest failing, and harness errors, which indicate an uncaught
        error in the test. Increasing numbers of either are considered new
        failures - this includes going from FAIL to error or vice-versa.
        """
        try:
            old_contents = self.git.show_blob(baseline).decode(
                errors='replace')
            old_lines = parse_testharness_baseline(old_contents)
        except ScriptError:
            old_lines = []
        try:
            new_contents = self.host.filesystem.read_text_file(
                self.finder.path_from_chromium_base(baseline))
            new_lines = parse_testharness_baseline(new_contents)
        except FileNotFoundError:
            new_lines = []

        failure_statuses = set(Status) - {Status.PASS, Status.NOTRUN}
        old_failures = [
            line for line in old_lines if line.statuses & failure_statuses
        ]
        new_failures = [
            line for line in new_lines if line.statuses & failure_statuses
        ]

        is_error = lambda line: line.line_type is LineType.HARNESS_ERROR
        if sum(map(is_error, new_failures)) > sum(map(is_error, old_failures)):
            return True
        is_subtest = lambda line: line.line_type is LineType.SUBTEST
        return sum(map(is_subtest, new_failures)) > sum(
            map(is_subtest, old_failures))

    def examine_new_test_expectations(self, test_expectations):
        """Examines new test expectations to find new failures.

        Args:
            test_expectations: A dictionary mapping names of tests that cannot
                be rebaselined to a list of new test expectation lines.
        """
        for test_name, expectation_lines in test_expectations.items():
            directory = self.find_directory_for_bug(test_name)
            if not directory:
                continue

            for expectation_line in expectation_lines:
                self.new_failures_by_directory[directory].append(
                    TestFailure.from_expectation_line(test_name,
                                                      expectation_line))

    def create_bugs_from_new_failures(self, wpt_revision_start,
                                      wpt_revision_end, gerrit_url):
        """Files bug reports for new failures.

        Args:
            wpt_revision_start: The start of the imported WPT revision range
                (exclusive), i.e. the last imported revision.
            wpt_revision_end: The end of the imported WPT revision range
                (inclusive), i.e. the current imported revision.
            gerrit_url: Gerrit URL of the CL.

        Return:
            A list of MonorailIssue objects that should be filed.
        """
        imported_commits = self.local_wpt.commits_in_range(
            wpt_revision_start, wpt_revision_end)
        bugs = []
        for directory, failures in self.new_failures_by_directory.items():
            summary = '[WPT] New failures introduced in {} by import {}'.format(
                directory, gerrit_url)

            full_directory = self.host.filesystem.join(
                self.finder.web_tests_dir(), directory)
            owners_file = self.host.filesystem.join(full_directory, 'OWNERS')
            metadata = self.owners_extractor.read_dir_metadata(full_directory)
            if not metadata or not metadata.should_notify:
                _log.info("WPT-NOTIFY disabled in %s." % full_directory)
                continue

            cc = []
            if metadata.team_email:
                cc.append(metadata.team_email)
            try:
                cc.extend(self.owners_extractor.extract_owners(owners_file))
            except FileNotFoundError:
                _log.warning(f'{owners_file!r} does not exist and '
                             'was not added to the CC list.')

            # component could be None.
            components = [metadata.monorail_component
                          ] if metadata.monorail_component else None
            buganizer_public_components = [
                metadata.buganizer_public_component
            ] if metadata.buganizer_public_component else None

            prologue = ('WPT import {} introduced new failures in {}:\n\n'
                        'List of new failures:\n'.format(
                            gerrit_url, directory))
            failure_list = ''.join(f'{failure.message}\n'
                                   for failure in failures)

            expectations_statement = (
                '\nExpectations or baseline files [0] have been automatically '
                'added for the failing results to keep the bots green. Please '
                'investigate the new failures and triage as appropriate.\n')

            range_statement = '\nUpstream changes imported:\n'
            range_statement += WPT_GH_RANGE_URL_TEMPLATE.format(
                wpt_revision_start, wpt_revision_end) + '\n'
            commit_list = self.format_commit_list(imported_commits,
                                                  full_directory)

            links_list = '\n[0]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_test_expectations.md\n'

            dir_metadata_path = self.host.filesystem.join(
                directory, "DIR_METADATA")
            epilogue = (
                '\nThis bug was filed automatically due to a new WPT test '
                'failure for which you are marked an OWNER. '
                'If you do not want to receive these reports, please add '
                '"wpt { notify: NO }"  to the relevant DIR_METADATA file.')

            description = (prologue + failure_list + expectations_statement +
                           range_statement + commit_list + links_list +
                           epilogue)

            bug = MonorailIssue.new_chromium_issue(summary,
                                                   description,
                                                   cc,
                                                   components,
                                                   labels=['Test-WebTest'])
            _log.info(bug)
            _log.info("WPT-NOTIFY enabled in %s; adding the bug to the pending list." % full_directory)

            # TODO(crbug.com/1487196): refactor this so we use a common issue which is converted later to
            # buganizer or monorail specific issue.
            bug.buganizer_public_components = buganizer_public_components
            bugs.append(bug)
        return bugs

    def format_commit_list(self, imported_commits, directory):
        """Formats the list of imported WPT commits.

        Imports affecting the given directory will be highlighted.

        Args:
            imported_commits: A list of (SHA, commit subject) pairs.
            directory: An absolute path of a directory in the Chromium repo, for
                which the list is formatted.

        Returns:
            A multi-line string.
        """
        path_from_wpt = self.host.filesystem.relpath(
            directory, self.finder.path_from_web_tests('external', 'wpt'))
        commit_list = ''
        for sha, subject in imported_commits:
            # subject is a Unicode string and can contain non-ASCII characters.
            line = u'{}: {}'.format(subject, GITHUB_COMMIT_PREFIX + sha)
            if self.local_wpt.is_commit_affecting_directory(
                    sha, path_from_wpt):
                line += ' [affecting this directory]'
            commit_list += line + '\n'
        return commit_list

    def find_directory_for_bug(self, test_name: str) -> Optional[str]:
        """Find the lowest directory with `DIR_METADATA` containing the test.

        Args:
            test_name: The name of the test (a path relative to web_tests).

        Returns:
            The path of the found directory relative to web_tests, if found.
        """
        # Always use non-virtual test names when looking up `DIR_METADATA`.
        if self.default_port.lookup_virtual_test_base(test_name):
            test_name = self.default_port.lookup_virtual_test_base(test_name)
        # `find_dir_metadata_file` takes either a relative path from the *root*
        # of the repository, or an absolute path.
        abs_test_path = self.finder.path_from_web_tests(test_name)
        metadata_file = self.owners_extractor.find_dir_metadata_file(
            self.host.filesystem.dirname(abs_test_path))
        if not metadata_file:
            _log.warning('Cannot find DIR_METADATA for %s.', test_name)
            return None
        owned_directory = self.host.filesystem.dirname(metadata_file)
        short_directory = self.host.filesystem.relpath(
            owned_directory, self.finder.web_tests_dir())
        return short_directory

    def file_bugs(self, bugs, dry_run, service_account_key_json=None):
        """Files a list of bugs to Monorail.

        Args:
            bugs: A list of MonorailIssue objects.
            dry_run: A boolean, whether we are in dry run mode.
            service_account_key_json: Optional, see docs for main().
        """
        # TODO(robertma): Better error handling in this method.
        if dry_run:
            _log.info(
                '[dry_run] Would have filed the %d bugs in the pending list.',
                len(bugs))
            return

        _log.info('Filing %d bugs in the pending list to Monorail', len(bugs))
        api = self._get_monorail_api(service_account_key_json)
        buganizer_api = None
        try:
            buganizer_api = self._get_buganizer_api()
        except Exception as e:
            _log.warning('buganizer instantiation failed')
            _log.warning(e)

        for index, bug in enumerate(bugs, start=1):
            buganizer_component_id = BUGANIZER_WPT_COMPONENT
            issue_link = None
            if buganizer_api and USE_BUGANIZER:
                if 'summary' not in bug.body:
                    _log.warning('failed to file bug')
                    _log.warning('summary missing from bug:')
                    _log.warning(bug)
                    continue
                if 'description' not in bug.body:
                    _log.warning('failed to file bug')
                    _log.warning('description missing from bug:')
                    _log.warning(bug)
                    continue
                title = bug.body['summary']
                description = bug.body['description']
                cc = bug.body.get('cc', []) + [self.sheriff_email]
                if bug.buganizer_public_components:
                    buganizer_component_id = bug.buganizer_public_components[0]
                try:
                    buganizer_res = buganizer_api.NewIssue(
                        title=title,
                        description=description,
                        cc=cc,
                        status="New",
                        componentId=buganizer_component_id)
                    issue_link = f'b/{buganizer_res["issue_id"]}'
                except Exception as e:
                    _log.warning('buganizer api call to new issue failed')
                    _log.warning(e)
            else:
                # using monorail
                response = api.insert_issue(bug)
                issue_link = MonorailIssue.crbug_link(response['id'])
            _log.info('[%d] Filed bug: %s', index, issue_link)

    def _get_buganizer_api(self):
        return self._buganizer_api()

    def _get_monorail_api(self, service_account_key_json):
        if service_account_key_json:
            return self._monorail_api(
                service_account_key_json=service_account_key_json)
        token = LuciAuth(self.host).get_access_token()
        return self._monorail_api(access_token=token)


class TestFailure(NamedTuple):
    """A simple abstraction of a new test failure for the notifier."""
    message: str
    test: str

    @classmethod
    def from_file(cls, test: str, baseline_path: str,
                  gerrit_url_with_ps: str) -> 'TestFailure':
        message = ''
        platform = re.search(r'/platform/([^/]+)/', baseline_path)
        if platform:
            message += '[ {} ] '.format(platform.group(1).capitalize())
        message += f'{test} new failing tests: '
        message += gerrit_url_with_ps + baseline_path
        return cls(message, test)

    @classmethod
    def from_expectation_line(cls, test: str, line: str) -> 'TestFailure':
        if line.startswith(WPTExpectationsUpdater.UMBRELLA_BUG):
            line = line[len(WPTExpectationsUpdater.UMBRELLA_BUG):].lstrip()
        return cls(line, test)
