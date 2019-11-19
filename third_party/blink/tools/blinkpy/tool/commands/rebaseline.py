# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR/ OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import collections
import json
import logging
import optparse
import re

from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.memoized import memoized
from blinkpy.common.net.results_fetcher import Build
from blinkpy.tool.commands.command import Command
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port import base, factory

_log = logging.getLogger(__name__)

# For CLI compatibility, we would like a list of baseline extensions without
# the leading dot.
# TODO(robertma): Investigate changing the CLI.
BASELINE_SUFFIX_LIST = tuple(ext[1:] for ext in base.Port.BASELINE_EXTENSIONS)


class AbstractRebaseliningCommand(Command):
    """Base class for rebaseline-related commands."""
    # Not overriding execute() - pylint: disable=abstract-method

    # Generic option groups (list of options):
    platform_options = factory.platform_options(use_globs=True)
    wpt_options = factory.wpt_options()

    no_optimize_option = optparse.make_option(
        '--no-optimize', dest='optimize', action='store_false', default=True,
        help=('Do not optimize (de-duplicate) the expectations after rebaselining '
              '(default is to de-dupe automatically). You can use "blink_tool.py '
              'optimize-baselines" to optimize separately.'))
    results_directory_option = optparse.make_option(
        '--results-directory', help='Local results directory to use.')
    suffixes_option = optparse.make_option(
        '--suffixes', default=','.join(BASELINE_SUFFIX_LIST), action='store',
        help='Comma-separated-list of file types to rebaseline.')
    builder_option = optparse.make_option(
        '--builder',
        help=('Name of the builder to pull new baselines from, '
              'e.g. "WebKit Mac10.12".'))
    port_name_option = optparse.make_option(
        '--port-name',
        help=('Fully-qualified name of the port that new baselines belong to, '
              'e.g. "mac-mac10.12". If not given, this is determined based on '
              '--builder.'))
    test_option = optparse.make_option('--test', help='Test to rebaseline.')
    build_number_option = optparse.make_option(
        '--build-number', default=None, type='int',
        help='Optional build number; if not given, the latest build is used.')
    step_name_option = optparse.make_option(
        '--step-name',
        help=('Name of the step which ran the actual tests, and which '
              'should be used to retrieve results from.'))

    def __init__(self, options=None):
        super(AbstractRebaseliningCommand, self).__init__(options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST
        self.expectation_line_changes = ChangeSet()
        self._tool = None

    def baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        return port.baseline_version_dir()

    @property
    def _host_port(self):
        return self._tool.port_factory.get()

    def _file_name_for_actual_result(self, test_name, suffix):
        # output_filename takes extensions starting with '.'.
        return self._host_port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_ACTUAL, '.' + suffix)

    def _file_name_for_expected_result(self, test_name, suffix):
        # output_filename takes extensions starting with '.'.
        return self._host_port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_EXPECTED, '.' + suffix)


class ChangeSet(object):
    """A record of TestExpectation lines to remove.

    Note: This class is probably more complicated than necessary; it is mainly
    used to track the list of lines that we want to remove from TestExpectations.
    """

    def __init__(self, lines_to_remove=None):
        self.lines_to_remove = lines_to_remove or {}

    def remove_line(self, test, port_name):
        if test not in self.lines_to_remove:
            self.lines_to_remove[test] = []
        self.lines_to_remove[test].append(port_name)

    def to_dict(self):
        remove_lines = []
        for test in self.lines_to_remove:
            for port_name in self.lines_to_remove[test]:
                remove_lines.append({'test': test, 'port_name': port_name})
        return {'remove-lines': remove_lines}

    @staticmethod
    def from_dict(change_dict):
        lines_to_remove = {}
        if 'remove-lines' in change_dict:
            for line_to_remove in change_dict['remove-lines']:
                test = line_to_remove['test']
                port_name = line_to_remove['port_name']
                if test not in lines_to_remove:
                    lines_to_remove[test] = []
                lines_to_remove[test].append(port_name)
        return ChangeSet(lines_to_remove=lines_to_remove)

    def update(self, other):
        assert isinstance(other, ChangeSet)
        assert isinstance(other.lines_to_remove, dict)
        for test in other.lines_to_remove:
            if test not in self.lines_to_remove:
                self.lines_to_remove[test] = []
            self.lines_to_remove[test].extend(other.lines_to_remove[test])


class TestBaselineSet(object):
    """Represents a collection of tests and platforms that can be rebaselined.

    A TestBaselineSet specifies tests to rebaseline along with information
    about where to fetch the baselines from.
    """

    def __init__(self, host):
        self._host = host
        self._port = self._host.port_factory.get()
        self._builder_names = set()
        self._test_prefix_map = collections.defaultdict(list)

    def __iter__(self):
        return iter(self._iter_combinations())

    def __bool__(self):
        return bool(self._test_prefix_map)

    def _iter_combinations(self):
        """Iterates through (test, build, port) combinations."""
        for test_prefix, build_port_pairs in self._test_prefix_map.iteritems():
            for test in self._port.tests([test_prefix]):
                for build, port_name in build_port_pairs:
                    yield (test, build, port_name)

    def __str__(self):
        if not self._test_prefix_map:
            return '<Empty TestBaselineSet>'
        return ('<TestBaselineSet with:\n  ' +
                '\n  '.join('%s: %s, %s' % triple for triple in self._iter_combinations()) +
                '>')

    def test_prefixes(self):
        """Returns a sorted list of test prefixes."""
        return sorted(self._test_prefix_map)

    def all_tests(self):
        """Returns a sorted list of all tests without duplicates."""
        tests = set()
        for test_prefix in self._test_prefix_map:
            tests.update(self._port.tests([test_prefix]))
        return sorted(tests)

    def build_port_pairs(self, test_prefix):
        # Return a copy in case the caller modifies the returned list.
        return list(self._test_prefix_map[test_prefix])

    def add(self, test_prefix, build, port_name=None):
        """Adds an entry for baselines to download for some set of tests.

        Args:
            test_prefix: This can be a full test path, or directory of tests, or a path with globs.
            build: A Build object. This specifies where to fetch baselines from.
            port_name: This specifies what platform the baseline is for.
        """
        port_name = port_name or self._host.builders.port_name_for_builder_name(build.builder_name)
        self._builder_names.add(build.builder_name)
        self._test_prefix_map[test_prefix].append((build, port_name))

    def all_builders(self):
        """Returns all builder names in in this collection."""
        return self._builder_names


class AbstractParallelRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline commands that do some tasks in parallel."""
    # Not overriding execute() - pylint: disable=abstract-method

    def __init__(self, options=None):
        super(AbstractParallelRebaselineCommand, self).__init__(options=options)

    def _release_builders(self):
        """Returns a list of builder names for continuous release builders.

        The release builders cycle much faster than the debug ones and cover all the platforms.
        """
        release_builders = []
        for builder_name in self._tool.builders.all_continuous_builder_names():
            port = self._tool.port_factory.get_from_builder_name(builder_name)
            if port.test_configuration().build_type == 'release':
                release_builders.append(builder_name)
        return release_builders

    def _builders_to_fetch_from(self, builders_to_check):
        """Returns the subset of builders that will cover all of the baseline
        search paths used in the input list.

        In particular, if the input list contains both Release and Debug
        versions of a configuration, we *only* return the Release version
        (since we don't save debug versions of baselines).

        Args:
            builders_to_check: A collection of builder names.

        Returns:
            A set of builders that we may fetch from, which is a subset
            of the input list.
        """
        release_builders = set()
        debug_builders = set()
        for builder in builders_to_check:
            port = self._tool.port_factory.get_from_builder_name(builder)
            if port.test_configuration().build_type == 'release':
                release_builders.add(builder)
            else:
                debug_builders.add(builder)

        builders_to_fallback_paths = {}
        for builder in list(release_builders) + list(debug_builders):
            port = self._tool.port_factory.get_from_builder_name(builder)
            fallback_path = port.baseline_search_path()
            if fallback_path not in builders_to_fallback_paths.values():
                builders_to_fallback_paths[builder] = fallback_path

        return set(builders_to_fallback_paths)

    def _rebaseline_commands(self, test_baseline_set, options):
        path_to_blink_tool = self._tool.path()
        cwd = self._tool.git().checkout_root
        copy_baseline_commands = []
        rebaseline_commands = []
        lines_to_remove = {}

        builders_to_fetch_from = self._builders_to_fetch_from(test_baseline_set.all_builders())
        for test, build, port_name in test_baseline_set:
            if build.builder_name not in builders_to_fetch_from:
                continue

            suffixes = self._suffixes_for_actual_failures(test, build)
            if not suffixes:
                # Only try to remove the expectation if the test
                #   1. ran and passed ([ Skip ], [ WontFix ] should be kept)
                #   2. passed unexpectedly (flaky expectations should be kept)
                if self._test_passed_unexpectedly(test, build, port_name):
                    _log.debug('Test %s passed unexpectedly in %s. '
                               'Will try to remove it from TestExpectations.', test, build)
                    if test not in lines_to_remove:
                        lines_to_remove[test] = []
                    lines_to_remove[test].append(port_name)
                continue

            args = []
            if options.verbose:
                args.append('--verbose')
            args.extend([
                '--test', test,
                '--suffixes', ','.join(suffixes),
                '--port-name', port_name,
            ])

            copy_command = [self._tool.executable, path_to_blink_tool, 'copy-existing-baselines-internal'] + args
            copy_baseline_commands.append(tuple([copy_command, cwd]))

            args.extend(['--builder', build.builder_name])
            if build.build_number:
                args.extend(['--build-number', str(build.build_number)])
            if options.results_directory:
                args.extend(['--results-directory', options.results_directory])

            step_name = self._tool.results_fetcher.get_layout_test_step_name(build)
            if step_name:
                args.extend(['--step-name', step_name])

            rebaseline_command = [self._tool.executable, path_to_blink_tool, 'rebaseline-test-internal'] + args
            rebaseline_commands.append(tuple([rebaseline_command, cwd]))

        return copy_baseline_commands, rebaseline_commands, lines_to_remove

    @staticmethod
    def _extract_expectation_line_changes(command_results):
        """Parses the JSON lines from sub-command output and returns the result as a ChangeSet."""
        change_set = ChangeSet()
        for _, stdout, _ in command_results:
            updated = False
            for line in filter(None, stdout.splitlines()):
                try:
                    parsed_line = json.loads(line)
                    change_set.update(ChangeSet.from_dict(parsed_line))
                    updated = True
                except ValueError:
                    _log.debug('"%s" is not a JSON object, ignoring', line)
            if not updated:
                # TODO(crbug.com/649412): This could be made into an error.
                _log.debug('Could not add file based off output "%s"', stdout)
        return change_set

    def _optimize_baselines(self, test_baseline_set, verbose=False):
        """Returns a list of commands to run in parallel to de-duplicate baselines."""
        tests_to_suffixes = collections.defaultdict(set)
        builders_to_fetch_from = self._builders_to_fetch_from(test_baseline_set.all_builders())
        for test, build, _ in test_baseline_set:
            if build.builder_name not in builders_to_fetch_from:
                continue
            tests_to_suffixes[test].update(self._suffixes_for_actual_failures(test, build))

        optimize_commands = []
        for test, suffixes in tests_to_suffixes.iteritems():
            # No need to optimize baselines for a test with no failures.
            if not suffixes:
                continue
            # FIXME: We should propagate the platform options as well.
            # Prevent multiple baseline optimizer to race updating the manifest.
            # The manifest has already been updated when listing tests.
            args = ['--no-manifest-update']
            if verbose:
                args.append('--verbose')
            args.extend(['--suffixes', ','.join(suffixes), test])
            path_to_blink_tool = self._tool.path()
            cwd = self._tool.git().checkout_root
            command = [self._tool.executable, path_to_blink_tool, 'optimize-baselines'] + args
            optimize_commands.append(tuple([command, cwd]))

        return optimize_commands

    def _update_expectations_files(self, lines_to_remove):
        tests = lines_to_remove.keys()
        to_remove = []

        # This is so we remove lines for builders that skip this test.
        # For example, Android skips most tests and we don't want to leave
        # stray [ Android ] lines in TestExpectations.
        # This is only necessary for "blink_tool.py rebaseline".
        for port_name in self._tool.port_factory.all_port_names():
            port = self._tool.port_factory.get(port_name)
            for test in tests:
                if port.skips_test(test):
                    for test_configuration in port.all_test_configurations():
                        if test_configuration.version == port.test_configuration().version:
                            to_remove.append((test, test_configuration))

        for test in lines_to_remove:
            for port_name in lines_to_remove[test]:
                port = self._tool.port_factory.get(port_name)
                for test_configuration in port.all_test_configurations():
                    if test_configuration.version == port.test_configuration().version:
                        to_remove.append((test, test_configuration))

        port = self._tool.port_factory.get()
        expectations = TestExpectations(port, include_overrides=False)
        expectations_string = expectations.remove_configurations(to_remove)
        path = port.path_to_generic_test_expectations_file()
        self._tool.filesystem.write_text_file(path, expectations_string)

    def _run_in_parallel(self, commands):
        if not commands:
            return {}

        command_results = self._tool.executive.run_in_parallel(commands)
        for _, _, stderr in command_results:
            if stderr:
                _log.error(stderr)

        change_set = self._extract_expectation_line_changes(command_results)

        return change_set.lines_to_remove

    def rebaseline(self, options, test_baseline_set):
        """Fetches new baselines and removes related test expectation lines.

        Args:
            options: An object with the command line options.
            test_baseline_set: A TestBaselineSet instance, which represents
                a set of tests/platform combinations to rebaseline.
        """
        if self._tool.git().has_working_directory_changes(pathspec=self._web_tests_dir()):
            _log.error('There are uncommitted changes in the web tests directory; aborting.')
            return

        for test in sorted({t for t, _, _ in test_baseline_set}):
            _log.info('Rebaselining %s', test)

        # extra_lines_to_remove are unexpected passes, while lines_to_remove are
        # failing tests that have been rebaselined.
        copy_baseline_commands, rebaseline_commands, extra_lines_to_remove = self._rebaseline_commands(
            test_baseline_set, options)
        lines_to_remove = {}

        self._run_in_parallel(copy_baseline_commands)
        lines_to_remove = self._run_in_parallel(rebaseline_commands)

        for test in extra_lines_to_remove:
            if test in lines_to_remove:
                lines_to_remove[test] = lines_to_remove[test] + extra_lines_to_remove[test]
            else:
                lines_to_remove[test] = extra_lines_to_remove[test]

        if lines_to_remove:
            self._update_expectations_files(lines_to_remove)

        if options.optimize:
            self._run_in_parallel(self._optimize_baselines(test_baseline_set, options.verbose))

        self._tool.git().add_list(self.unstaged_baselines())

    def unstaged_baselines(self):
        """Returns absolute paths for unstaged (including untracked) baselines."""
        baseline_re = re.compile(r'.*[\\/]' + WEB_TESTS_LAST_COMPONENT + r'[\\/].*-expected\.(txt|png|wav)$')
        unstaged_changes = self._tool.git().unstaged_changes()
        return sorted(self._tool.git().absolute_path(path) for path in unstaged_changes if re.match(baseline_re, path))

    def _generic_baseline_paths(self, test_baseline_set):
        """Returns absolute paths for generic baselines for the given tests.

        Even when a test does not have a generic baseline, the path where it
        would be is still included in the return value.
        """
        filesystem = self._tool.filesystem
        baseline_paths = []
        for test in test_baseline_set.all_tests():
            filenames = [self._file_name_for_expected_result(test, suffix) for suffix in BASELINE_SUFFIX_LIST]
            baseline_paths += [filesystem.join(self._web_tests_dir(), filename) for filename in filenames]
        baseline_paths.sort()
        return baseline_paths

    def _web_tests_dir(self):
        return self._tool.port_factory.get().web_tests_dir()

    def _suffixes_for_actual_failures(self, test, build):
        """Gets the baseline suffixes for actual mismatch failures in some results.

        Args:
            test: A full test path string.
            build: A Build object.

        Returns:
            A set of file suffix strings.
        """
        test_result = self._result_for_test(test, build)
        if not test_result:
            return set()
        return test_result.suffixes_for_test_result()

    def _test_passed_unexpectedly(self, test, build, port_name):
        """Determines if a test passed unexpectedly in a build.

        The routine also takes into account the port that is being rebaselined.
        It is possible to use builds from a different port to rebaseline the
        current port, e.g. rebaseline-cl --fill-missing, in which case the test
        will not be considered passing regardless of the result.

        Args:
            test: A full test path string.
            build: A Build object.
            port_name: The name of port currently being rebaselined.

        Returns:
            A boolean.
        """
        if self._tool.builders.port_name_for_builder_name(build.builder_name) != port_name:
            return False
        test_result = self._result_for_test(test, build)
        if not test_result:
            return False
        return test_result.did_pass() and not test_result.did_run_as_expected()

    @memoized
    def _result_for_test(self, test, build):
        # We need full results to know if a test passed or was skipped.
        # TODO(robertma): Make memoized support kwargs, and use full=True here.
        results = self._tool.results_fetcher.fetch_results(build, True)
        if not results:
            _log.debug('No results found for build %s', build)
            return None
        test_result = results.result_for_test(test)
        if not test_result:
            _log.info('No test result for test %s in build %s', test, build)
            return None
        return test_result


class Rebaseline(AbstractParallelRebaselineCommand):
    name = 'rebaseline'
    help_text = 'Rebaseline tests with results from the continuous builders.'
    show_in_main_help = True
    argument_names = '[TEST_NAMES]'

    def __init__(self):
        super(Rebaseline, self).__init__(options=[
            self.no_optimize_option,
            # FIXME: should we support the platform options in addition to (or instead of) --builders?
            self.results_directory_option,
            optparse.make_option('--builders', default=None, action='append',
                                 help=('Comma-separated-list of builders to pull new baselines from '
                                       '(can also be provided multiple times).')),
        ])

    def _builders_to_pull_from(self):
        return self._tool.user.prompt_with_list(
            'Which builder to pull results from:', self._release_builders(), can_choose_multiple=True)

    def execute(self, options, args, tool):
        self._tool = tool
        if not args:
            _log.error('Must list tests to rebaseline.')
            return

        if options.builders:
            builders_to_check = []
            for builder_names in options.builders:
                builders_to_check += builder_names.split(',')
        else:
            builders_to_check = self._builders_to_pull_from()

        test_baseline_set = TestBaselineSet(tool)

        for builder in builders_to_check:
            for test_prefix in args:
                test_baseline_set.add(test_prefix, Build(builder))

        _log.debug('Rebaselining: %s', test_baseline_set)

        self.rebaseline(options, test_baseline_set)
