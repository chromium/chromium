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
from typing import List, NamedTuple, Optional

from blinkpy.common import path_finder
from blinkpy.common.checkout.git import CommitRange
from blinkpy.common.net.git_cl import Changelist
from blinkpy.common.system.executive import ScriptError
from blinkpy.web_tests.models import typ_types
from blinkpy.web_tests.models.test_expectations import (
    ExpectationsChange,
    TestExpectations,
)
from blinkpy.web_tests.models.testharness_results import (
    LineType,
    Status,
    parse_testharness_baseline,
)
from blinkpy.w3c.buganizer import (
    BuganizerClient,
    BuganizerError,
    BuganizerIssue,
)
from blinkpy.w3c.common import WPT_GH_URL, WPT_GH_RANGE_URL_TEMPLATE
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.wpt_results_processor import TestType

_log = logging.getLogger(__name__)

GITHUB_COMMIT_PREFIX = WPT_GH_URL + 'commit/'
SHORT_GERRIT_PREFIX = 'https://crrev.com/c/'
CHECKS_URL_TEMPLATE = 'https://chromium-review.googlesource.com/c/chromium/src/+/{}/{}?checksPatchset=1&tab=checks'

BUGANIZER_WPT_COMPONENT = '1456176'


class ImportNotifier:
    def __init__(self,
                 host,
                 chromium_git,
                 local_wpt,
                 buganizer_client: Optional[BuganizerClient] = None):
        self.host = host
        self.git = chromium_git
        self.local_wpt = local_wpt
        self._buganizer_client = buganizer_client or BuganizerClient()

        self.default_port = host.port_factory.get()
        self.default_port.set_option_default('test_types',
                                             typing.get_args(TestType))
        self.finder = path_finder.PathFinder(host.filesystem)
        self.owners_extractor = DirectoryOwnersExtractor(host)
        self.new_failures_by_directory = defaultdict(list)

    def main(self,
             import_range: CommitRange,
             wpt_revision_start,
             wpt_revision_end,
             rebaselined_tests,
             issue,
             patchset,
             dry_run=True):
        """Files bug reports for new failures.

        Arguments:
            import_range: The commits before (exclusive) and after (inclusive)
                the imported CL.
            wpt_revision_start: The start of the imported WPT revision range
                (exclusive), i.e. the last imported revision.
            wpt_revision_end: The end of the imported WPT revision range
                (inclusive), i.e. the current imported revision.
            rebaselined_tests: A list of test names that have been rebaselined.
            issue: The issue number of the import CL (a string).
            patchset: The patchset number of the import CL (a string).
            dry_run: If True, no bugs will be actually filed to crbug.com.

        Note: "test names" are paths of the tests relative to web_tests.
        """
        gerrit_url = str(Changelist(issue))
        gerrit_url_with_ps = str(Changelist(issue, patchset)) + '/'

        changed_test_baselines = self.find_changed_baselines_of_tests(
            rebaselined_tests)
        self.examine_baseline_changes(changed_test_baselines,
                                      gerrit_url_with_ps)
        self.examine_new_test_expectations(import_range)

        bugs = self.create_bugs_from_new_failures(wpt_revision_start,
                                                  wpt_revision_end, issue)
        self.file_bugs(bugs, dry_run)

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

    def examine_new_test_expectations(self, import_range: CommitRange):
        """Examines new test expectations to find new failures.

        Arguments:
            import_range: The commits before (exclusive) and after (inclusive)
                the imported CL.
        """
        exp_files = {
            *self.default_port.all_expectations_dict(),
            self.finder.path_from_web_tests('ChromeTestExpectations'),
            self.finder.path_from_web_tests('MobileTestExpectations'),
        }
        for changed_file in self.git.changed_files(import_range):
            abs_changed_file = self.finder.path_from_chromium_base(
                changed_file)
            if abs_changed_file not in exp_files:
                continue
            lines_before = self._read_lines(changed_file, import_range.start)
            lines_after = self._read_lines(changed_file, import_range.end)
            change = ExpectationsChange(lines_added=lines_after)
            change += ExpectationsChange(lines_removed=lines_before)

            for line in change.lines_added:
                directory = self.find_directory_for_bug(line.test)
                if directory:
                    failure = TestFailure.from_expectation_line(
                        line.test, line.to_string())
                    self.new_failures_by_directory[directory].append(failure)

    def _read_lines(self, path: str, ref: str) -> List[typ_types.Expectation]:
        abs_path = self.finder.path_from_chromium_base(path)
        expectations = TestExpectations(
            self.default_port,
            {abs_path: self.git.show_blob(path, ref).decode()})
        return expectations.get_updated_lines(abs_path)

    def create_bugs_from_new_failures(self, wpt_revision_start: str,
                                      wpt_revision_end: str,
                                      issue: str) -> List[BuganizerIssue]:
        """Files bug reports for new failures.

        Args:
            wpt_revision_start: The start of the imported WPT revision range
                (exclusive), i.e. the last imported revision.
            wpt_revision_end: The end of the imported WPT revision range
                (inclusive), i.e. the current imported revision.
            issue: Issue number of the CL.

        Return:
            A list of issues that should be filed.
        """
        gerrit_url = SHORT_GERRIT_PREFIX + issue
        patchset1_checks_url = CHECKS_URL_TEMPLATE.format(issue, '1')
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

            prologue = ('WPT import {} introduced new failures in {}:\n\n'
                        'List of new failures:\n'.format(
                            gerrit_url, directory))
            failure_list = ''.join(f'{failure.message}\n'
                                   for failure in failures)
            checks = '\nSee {} for details.\n'.format(patchset1_checks_url)

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

            description = (prologue + failure_list + checks +
                           expectations_statement + range_statement +
                           commit_list + links_list + epilogue)

            bug = BuganizerIssue(
                title=summary,
                description=description,
                component_id=(metadata.buganizer_public_component
                              or BUGANIZER_WPT_COMPONENT),
                cc=cc)
            _log.info("WPT-NOTIFY enabled in %s; adding the bug to the pending list." % full_directory)
            _log.info(f'{bug}')
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

    def file_bugs(self, bugs: List[BuganizerIssue], dry_run: bool = False):
        """Files a list of bugs to Buganizer.

        Args:
            bugs: A list of bugs to file.
            dry_run: A boolean, whether we are in dry run mode.
        """
        # TODO(robertma): Better error handling in this method.
        if dry_run:
            _log.info(
                '[dry_run] Would have filed the %d bugs in the pending list.',
                len(bugs))
            return

        _log.info('Filing %d bugs in the pending list to Buganizer', len(bugs))
        for index, bug in enumerate(bugs, start=1):
            try:
                bug = self._buganizer_client.NewIssue(bug)
                _log.info(f'[{index}] Filed bug: {bug.link}')
            except BuganizerError as error:
                _log.exception('Failed to file bug', exc_info=error)


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
