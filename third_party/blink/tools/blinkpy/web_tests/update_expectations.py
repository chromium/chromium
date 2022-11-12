# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates TestExpectations based on results in builder bots.

Scans the TestExpectations file and uses results from actual builder bots runs
to remove tests that are marked as flaky or failing but don't fail in the
specified way.

--type=flake updates only lines that incude a 'Pass' expectation plus at
least one other expectation.

--type=fail updates lines that include only 'Failure', 'Timeout', or
'Crash' expectations.

E.g. If a test has this expectation:
    bug(test) fast/test.html [ Failure Pass ]

And all the runs on builders have passed the line will be removed.

Additionally, the runs don't all have to be Passing to remove the line;
as long as the non-Passing results are of a type not specified in the
expectation this line will be removed. For example, if this is the
expectation:

    bug(test) fast/test.html [ Crash Pass ]

But the results on the builders show only Passes and Timeouts, the line
will be removed since there's no Crash results.
"""

import argparse
import logging
import webbrowser

from blinkpy.tool.commands.flaky_tests import FlakyTests
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)

CHROMIUM_BUG_PREFIX = 'crbug.com/'


def main(host, bot_test_expectations_factory, argv):
    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        '--type',
        choices=['flake', 'fail', 'all'],
        default='all',
        help='type of expectations to update (default: %(default)s)')
    parser.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        default=False,
        help='enable more verbose logging')
    parser.add_argument(
        '--remove-missing',
        action='store_true',
        default=False,
        help='also remove lines if there were no results, '
        'e.g. Android-only expectations for tests '
        'that are not in SmokeTests')
    # TODO(crbug.com/1077883): Including cq results might introduce false
    # negatives. An in-review change that caused a test with fail test
    # expectation but no longer fails, to fail again, will result in
    # the test expectation not being removed even if the change
    # does not ultimately land.
    parser.add_argument(
        '--include-cq-results',
        action='store_true',
        default=False,
        help='include results from cq.')
    parser.add_argument(
        '--show-results',
        '-s',
        action='store_true',
        default=False,
        help='Open results dashboard for all removed lines')
    args = parser.parse_args(argv)

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(levelname)s: %(message)s')

    port = host.port_factory.get()
    expectations_file = port.path_to_generic_test_expectations_file()
    if not host.filesystem.isfile(expectations_file):
        _log.warning("Didn't find generic expectations file at: " +
                     expectations_file)
        return 1

    remover = ExpectationsRemover(host, port, bot_test_expectations_factory,
                                  webbrowser, args.type, args.remove_missing,
                                  args.include_cq_results)

    test_expectations = remover.get_updated_test_expectations()
    if args.show_results:
        remover.show_removed_results()

    host.filesystem.write_text_file(expectations_file, test_expectations)
    remover.print_suggested_commit_description()
    return 0


class ExpectationsRemover(object):
    def __init__(self,
                 host,
                 port,
                 bot_test_expectations_factory,
                 browser,
                 type_flag='all',
                 remove_missing=False,
                 include_cq_results=False):
        self._host = host
        self._port = port
        self._expectations_factory = bot_test_expectations_factory
        self._browser = browser
        self._expectations_to_remove_list = None
        self._type = type_flag
        self._bug_numbers = set()
        self._remove_missing = remove_missing
        self._include_cq_results = include_cq_results
        self._builder_results_by_path = self._get_builder_results_by_path()
        self._removed_test_names = set()
        self._version_to_os = {}

    def _can_delete_line(self, expectation):
        """Returns whether a given line in the expectations can be removed.

        Uses results from builder bots to determine if a given line is stale and
        can safely be removed from the TestExpectations file. (i.e. remove if
        the bots show that it's not flaky.) There are also some rules about when
        not to remove lines (e.g. never remove lines with Rebaseline
        expectations, don't remove non-flaky expectations, etc.)

        Args:
            test_expectation_line (TestExpectationLine): A line in the test
                expectation file to test for possible removal.

        Returns:
            True if the line can be removed, False otherwise.
        """
        expected_results = expectation.results

        if not expected_results:
            return False

        # Don't check lines that have expectations like Skip or Slow.
        if ResultType.Skip in expected_results or expectation.is_slow_test:
            return False

        # Don't check consistent passes.
        if len(expected_results) == 1 and ResultType.Pass in expected_results:
            return False

        # Don't check flakes in fail mode.
        if self._type == 'fail' and ResultType.Pass in expected_results:
            return False

        # Don't check failures in flake mode.
        if self._type == 'flake' and ResultType.Pass not in expected_results:
            return False

        # Initialize OS version to OS dictionary.
        if not self._version_to_os:
            for os, os_versions in \
                self._port.configuration_specifier_macros().items():
                for version in os_versions:
                    self._version_to_os[version.lower()] = os.lower()

        # The line can be deleted if none of the expectations appear in the
        # actual results or only a PASS expectation appears in the actual
        # results.
        builders_checked = []
        builders = []
        for config in self._port.all_test_configurations():
            if set(expectation.tags).issubset(
                    set([
                        self._version_to_os[config.version.lower()],
                        config.version.lower(),
                        config.build_type.lower()
                    ])):
                try_server_configs = [False]
                if self._include_cq_results:
                    try_server_configs.append(True)
                for is_try_server in try_server_configs:
                    builder_name = self._host.builders.builder_name_for_specifiers(
                            config.version, config.build_type, is_try_server)
                    if not builder_name:
                        _log.debug('No builder with config %s for %s',
                                   config, 'CQ' if is_try_server else 'CI')
                        # For many configurations, there is no matching builder in
                        # blinkpy/common/config/builders.json. We ignore these
                        # configurations and make decisions based only on configurations
                        # with actual builders.
                        continue
                    builders.append(builder_name)

        for builder_name in builders:

            builders_checked.append(builder_name)

            if builder_name not in self._builder_results_by_path.keys():
                _log.error('Failed to find results for builder "%s"',
                           builder_name)
                return False

            results_by_path = self._builder_results_by_path[builder_name]

            # No results means the tests were all skipped, or all results are passing.
            if expectation.test not in results_by_path.keys():
                if self._remove_missing:
                    continue
                return False

            results_for_single_test = set(results_by_path[expectation.test])
            expectations_met = expected_results & results_for_single_test
            if (expectations_met != set([ResultType.Pass])
                    and expectations_met != set([])):
                return False
        if builders_checked:
            _log.debug('Checked builders:\n  %s',
                       '\n  '.join(builders_checked))
        else:
            _log.warning('No matching builders for line, deleting line.')
        return True

    def _get_builder_results_by_path(self):
        """Returns a dictionary of results for each builder.

        Returns a dictionary where each key is a builder and value is a dictionary containing
        the distinct results for each test. E.g.

        {
            'WebKit Linux Precise': {
                  'test1.html': ['PASS', 'IMAGE'],
                  'test2.html': ['PASS'],
            },
            'WebKit Mac10.10': {
                  'test1.html': ['PASS', 'IMAGE'],
                  'test2.html': ['PASS', 'TEXT'],
            }
        }
        """
        builder_results_by_path = {}
        builders = []
        if self._include_cq_results:
            builders = self._host.builders.all_builder_names()
        else:
            builders = self._host.builders.all_continuous_builder_names()

        for builder_name in builders:
            expectations_for_builder = (
                self._expectations_factory.expectations_for_builder(builder_name)
            )

            if not expectations_for_builder:
                # This is not fatal since we may not need to check these
                # results. If we do need these results we'll log an error later
                # when trying to check against them.
                _log.warning(
                    'Downloaded results are missing results for builder "%s"',
                    builder_name)
                continue

            builder_results_by_path[builder_name] = (
                expectations_for_builder.all_results_by_path())
        return builder_results_by_path

    def get_updated_test_expectations(self):
        """Filters out passing lines from TestExpectations file.

        Reads the current TestExpectations file and, using results from the
        build bots, removes lines that are passing. That is, removes lines that
        were not needed to keep the bots green.

        Returns:
            A TestExpectations object with the passing lines filtered out.
        """
        generic_exp_path = self._port.path_to_generic_test_expectations_file()
        raw_test_expectations = self._host.filesystem.read_text_file(
            generic_exp_path)
        expectations_dict = {generic_exp_path: raw_test_expectations}
        test_expectations = TestExpectations(
            port=self._port, expectations_dict=expectations_dict)
        removed_exps = []
        lines = []

        for exp in test_expectations.get_updated_lines(generic_exp_path):
            # only get expectations objects for non glob patterns
            if not exp.test or exp.is_glob:
                continue

            if self._can_delete_line(exp):
                reason = exp.reason or ''
                self._bug_numbers.update([
                    reason[len(CHROMIUM_BUG_PREFIX):]
                    for reason in reason.split()
                    if reason.startswith(CHROMIUM_BUG_PREFIX)
                ])
                self._removed_test_names.add(exp.test)
                removed_exps.append(exp)
                _log.info('Deleting line "%s"' % exp.to_string().strip())

        if removed_exps:
            test_expectations.remove_expectations(generic_exp_path,
                                                  removed_exps)

        return '\n'.join([
            e.to_string()
            for e in test_expectations.get_updated_lines(generic_exp_path)
        ])

    def show_removed_results(self):
        """Opens a browser showing the removed lines in the results dashboard.

        Opens the results dashboard in the browser, showing all the tests for
        lines removed from the TestExpectations file, allowing the user to
        manually confirm the results.
        """
        url = self._flakiness_dashboard_url()
        _log.info('Opening results dashboard: ' + url)
        self._browser.open(url)

    def print_suggested_commit_description(self):
        """Prints the body of a suggested CL description after removing some lines."""

        expectation_type = ''
        if self._type != 'all':
            expectation_type = self._type + ' '
        dashboard_url = self._flakiness_dashboard_url()
        bugs = ', '.join(sorted(self._bug_numbers))
        message = (
            'Remove %sTestExpectations which are not failing in the specified way.\n\n'
            'This change was made by the update_expectations.py script.\n\n'
            'Recent test results history:\n%s\n\n'
            'Bug: %s') % (expectation_type, dashboard_url, bugs)
        _log.info('Suggested commit description:\n' + message)

    def _flakiness_dashboard_url(self):
        removed_test_names = ','.join(sorted(self._removed_test_names))
        return FlakyTests.FLAKINESS_DASHBOARD_URL % removed_test_names
