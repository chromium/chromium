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

from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.path_finder import PathFinder
from blinkpy.w3c.common import WPT_GH_URL, WPT_GH_RANGE_URL_TEMPLATE
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.w3c.monorail import MonorailAPI, MonorailIssue
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater

_log = logging.getLogger(__name__)

GITHUB_COMMIT_PREFIX = WPT_GH_URL + 'commit/'
SHORT_GERRIT_PREFIX = 'https://crrev.com/c/'

class ImportNotifier(object):
    def __init__(self, host, chromium_git, local_wpt):
        self.host = host
        self.git = chromium_git
        self.local_wpt = local_wpt

        self._monorail_api = MonorailAPI
        self.default_port = host.port_factory.get()
        self.finder = PathFinder(host.filesystem)
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
             service_account_key_json=None):
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
            directory = self.find_owned_directory(test_name)
            if not directory:
                _log.warning('Cannot find OWNERS of %s', test_name)
                continue

            for baseline in changed_baselines:
                if self.more_failures_in_baseline(baseline):
                    self.new_failures_by_directory[directory].append(
                        TestFailure(
                            TestFailure.BASELINE_CHANGE,
                            test_name,
                            baseline_path=baseline,
                            gerrit_url_with_ps=gerrit_url_with_ps))

    def more_failures_in_baseline(self, baseline):
        """Determines if a testharness.js baseline file has new failures.

        The file is assumed to have been modified in the current git checkout,
        and so has a diff we can parse.

        We recognize two types of failures: FAIL lines, which are output for a
        specific subtest failing, and harness errors, which indicate an uncaught
        error in the test. Increasing numbers of either are considered new
        failures - this includes going from FAIL to error or vice-versa.
        """

        diff = self.git.run(['diff', '-U0', 'origin/main', '--', baseline])
        delta_failures = 0
        delta_harness_errors = 0
        for line in diff.splitlines():
            if line.startswith('+FAIL'):
                delta_failures += 1
            if line.startswith('-FAIL'):
                delta_failures -= 1
            if line.startswith('+Harness Error.'):
                delta_harness_errors += 1
            if line.startswith('-Harness Error.'):
                delta_harness_errors -= 1
        return delta_failures > 0 or delta_harness_errors > 0

    def examine_new_test_expectations(self, test_expectations):
        """Examines new test expectations to find new failures.

        Args:
            test_expectations: A dictionary mapping names of tests that cannot
                be rebaselined to a list of new test expectation lines.
        """
        for test_name, expectation_lines in test_expectations.items():
            directory = self.find_owned_directory(test_name)
            if not directory:
                _log.warning('Cannot find OWNERS of %s', test_name)
                continue

            for expectation_line in expectation_lines:
                self.new_failures_by_directory[directory].append(
                    TestFailure(
                        TestFailure.NEW_EXPECTATION,
                        test_name,
                        expectation_line=expectation_line))

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
            metadata_file = self.host.filesystem.join(full_directory,
                                                      'DIR_METADATA')
            is_wpt_notify_enabled = False
            try:
                is_wpt_notify_enabled = self.owners_extractor.is_wpt_notify_enabled(
                    metadata_file)
            except KeyError:
                _log.info('KeyError when parsing %s' % metadata_file)

            if not is_wpt_notify_enabled:
                _log.info("WPT-NOTIFY disabled in %s." % full_directory)
                continue

            owners = self.owners_extractor.extract_owners(owners_file)
            # owners may be empty but not None.
            cc = owners

            component = self.owners_extractor.extract_component(metadata_file)
            # component could be None.
            components = [component] if component else None

            prologue = ('WPT import {} introduced new failures in {}:\n\n'
                        'List of new failures:\n'.format(
                            gerrit_url, directory))
            failure_list = ''
            for failure in failures:
                failure_list += str(failure) + '\n'

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

            description = (prologue + failure_list + expectations_statement +
                           range_statement + commit_list + links_list)

            bug = MonorailIssue.new_chromium_issue(summary,
                                                   description,
                                                   cc,
                                                   components,
                                                   labels=['Test-WebTest'])
            _log.info(bug)
            _log.info("WPT-NOTIFY enabled in %s; adding the bug to the pending list." % full_directory)
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

    def find_owned_directory(self, test_name):
        """Finds the lowest directory that contains the test and has OWNERS.

        Args:
            The name of the test (a path relative to web_tests).

        Returns:
            The path of the found directory relative to web_tests.
        """
        # Always use non-virtual test names when looking up OWNERS.
        if self.default_port.lookup_virtual_test_base(test_name):
            test_name = self.default_port.lookup_virtual_test_base(test_name)
        # find_owners_file takes either a relative path from the *root* of the
        # repository, or an absolute path.
        abs_test_path = self.finder.path_from_web_tests(test_name)
        owners_file = self.owners_extractor.find_owners_file(
            self.host.filesystem.dirname(abs_test_path))
        if not owners_file:
            return None
        owned_directory = self.host.filesystem.dirname(owners_file)
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
        for index, bug in enumerate(bugs, start=1):
            response = api.insert_issue(bug)
            _log.info('[%d] Filed bug: %s', index,
                      MonorailIssue.crbug_link(response['id']))

    def _get_monorail_api(self, service_account_key_json):
        if service_account_key_json:
            return self._monorail_api(
                service_account_key_json=service_account_key_json)
        token = LuciAuth(self.host).get_access_token()
        return self._monorail_api(access_token=token)


class TestFailure(object):
    """A simple abstraction of a new test failure for the notifier."""

    # Failure types:
    BASELINE_CHANGE = 1
    NEW_EXPECTATION = 2

    def __init__(self,
                 failure_type,
                 test_name,
                 expectation_line='',
                 baseline_path='',
                 gerrit_url_with_ps=''):
        if failure_type == self.BASELINE_CHANGE:
            assert baseline_path and gerrit_url_with_ps
        else:
            assert failure_type == self.NEW_EXPECTATION
            assert expectation_line

        self.failure_type = failure_type
        self.test_name = test_name
        self.expectation_line = expectation_line
        self.baseline_path = baseline_path
        self.gerrit_url_with_ps = gerrit_url_with_ps

    def __str__(self):
        if self.failure_type == self.BASELINE_CHANGE:
            return self._format_baseline_change()
        else:
            return self._format_new_expectation()

    def __eq__(self, other):
        return (self.failure_type == other.failure_type
                and self.test_name == other.test_name
                and self.expectation_line == other.expectation_line
                and self.baseline_path == other.baseline_path
                and self.gerrit_url_with_ps == other.gerrit_url_with_ps)

    def _format_baseline_change(self):
        assert self.failure_type == self.BASELINE_CHANGE
        result = ''
        # TODO(robertma): Is there any better way than using regexp?
        platform = re.search(r'/platform/([^/]+)/', self.baseline_path)
        if platform:
            result += '[ {} ] '.format(platform.group(1).capitalize())
        result += '{} new failing tests: {}{}'.format(
            self.test_name, self.gerrit_url_with_ps, self.baseline_path)
        return result

    def _format_new_expectation(self):
        assert self.failure_type == self.NEW_EXPECTATION
        # TODO(robertma): Are there saner ways to remove the link to the umbrella bug?
        line = self.expectation_line
        if line.startswith(WPTExpectationsUpdater.UMBRELLA_BUG):
            line = line[len(WPTExpectationsUpdater.UMBRELLA_BUG):].lstrip()
        return line
