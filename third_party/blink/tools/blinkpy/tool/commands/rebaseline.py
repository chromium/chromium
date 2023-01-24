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
from collections import defaultdict
from typing import ClassVar

from blinkpy.common import message_pool
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.memoized import memoized
from blinkpy.common.net.results_fetcher import Build
from blinkpy.tool.commands.command import Command, check_dir_option
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.test_expectations import SystemConfigurationRemover, TestExpectations
from blinkpy.web_tests.port import base, factory

_log = logging.getLogger(__name__)

# For CLI compatibility, we would like a list of baseline extensions without
# the leading dot.
# TODO(robertma): Investigate changing the CLI.
BASELINE_SUFFIX_LIST = tuple(ext[1:] for ext in base.Port.BASELINE_EXTENSIONS)
# When large number of tests need to be optimized, limit the length of the commandline to 128 tests
# to not run into issues with any commandline size limitations with popen. In windows CreateProcess()
# arg length limit is 32768. With 250 chars in test path length, choosing a chunk size of 128.
MAX_TESTS_IN_OPTIMIZE_CMDLINE = 128


class AbstractRebaseliningCommand(Command):
    """Base class for rebaseline-related commands."""
    # pylint: disable=abstract-method; not overriding `execute()`

    # Generic option groups (list of options):
    platform_options = factory.platform_options(use_globs=True)
    wpt_options = factory.wpt_options()

    no_optimize_option = optparse.make_option(
        '--no-optimize',
        dest='optimize',
        action='store_false',
        default=True,
        help=
        ('Do not optimize (de-duplicate) the expectations after rebaselining '
         '(default is to de-dupe automatically). You can use "blink_tool.py '
         'optimize-baselines" to optimize separately.'))
    dry_run_option = optparse.make_option(
        '--dry-run',
        action='store_true',
        default=False,
        help=('Dry run mode. List actions that would be performed '
              'but do not actually write to disk.'))
    results_directory_option = optparse.make_option(
        '--results-directory',
        action='callback',
        callback=check_dir_option,
        type='string',
        help='Local results directory to use.')
    suffixes_option = optparse.make_option(
        '--suffixes',
        default=','.join(BASELINE_SUFFIX_LIST),
        action='store',
        help='Comma-separated-list of file types to rebaseline.')
    builder_option = optparse.make_option(
        '--builder',
        help=('Name of the builder to pull new baselines from, '
              'e.g. "Mac11 Tests".'))
    port_name_option = optparse.make_option(
        '--port-name',
        help=('Fully-qualified name of the port that new baselines belong to, '
              'e.g. "mac-mac11". If not given, this is determined based on '
              '--builder.'))
    test_option = optparse.make_option('--test', help='Test to rebaseline.')
    build_number_option = optparse.make_option(
        '--build-number',
        default=None,
        type='int',
        help='Optional build number; if not given, the latest build is used.')
    step_name_option = optparse.make_option(
        '--step-name',
        help=('Name of the step which ran the actual tests, and which '
              'should be used to retrieve results from.'))
    flag_specific_option = optparse.make_option(
        '--flag-specific',
        # TODO(crbug/1291020): build the list from builders.json
        choices=[
            "disable-site-isolation-trials", "highdpi",
            "skia-vulkan-swiftshader"
        ],
        default=None,
        action='store',
        help=(
            'Name of a flag-specific configuration defined in '
            'FlagSpecificConfig. This option will rebaseline '
            'results for the given FlagSpecificConfig while ignoring results '
            'from other builders.'))
    resultDB_option = optparse.make_option(
        '--resultDB',
        default=False,
        action='store_true',
        help=('Fetch results from resultDB(WIP). '
              'Works with --test-name-file '
              'and positional parameters'))

    fetch_url_option = optparse.make_option(
        '--fetch-url',
        default=None,
        action='store',
        help=('Comma separated list of complete urls to fetch the baseline '
              'artifact from. Developers do not need this option while '
              'using rebaseline-cl. Default is empty. '
              'When this is empty baselines will not be downloaded'))

    def __init__(self, options=None):
        super(AbstractRebaseliningCommand, self).__init__(options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST
        self.expectation_line_changes = ChangeSet()
        self._tool = None
        self._dry_run = False
        self._resultdb_fetcher = False

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

    def __init__(self, host, prefix_mode=True):
        """Args:
            host: A Host object.
            prefix_mode: (Optional, default to True) Whether the collection
                contains test prefixes or specific tests.
        """
        self._host = host
        # Set self._port to None to avoid accidentally calling port.tests when
        # we are not in prefix mode.
        self._port = self._host.port_factory.get() if prefix_mode else None
        self._build_steps = set()
        self._prefix_mode = prefix_mode
        self._test_prefix_map = collections.defaultdict(list)

    def __iter__(self):
        return iter(self._iter_combinations())

    def __bool__(self):
        return bool(self._test_prefix_map)

    def _iter_combinations(self):
        """Iterates through (test, build, step, port) combinations."""
        for test_prefix, build_steps in self._test_prefix_map.items():
            if not self._prefix_mode:
                for build_step in build_steps:
                    yield (test_prefix, ) + build_step
                continue

            for test in self._port.tests([test_prefix]):
                for build_step in build_steps:
                    yield (test, ) + build_step

    def __str__(self):
        if not self._test_prefix_map:
            return '<Empty TestBaselineSet>'
        return '<TestBaselineSet with:\n  %s>' % '\n  '.join(
            '%s: %s, %s, %s' % combo for combo in self._iter_combinations())

    def test_prefixes(self):
        """Returns a sorted list of test prefixes (or tests) added thus far."""
        return sorted(self._test_prefix_map)

    def all_tests(self):
        """Returns a sorted list of all tests without duplicates."""
        tests = set()
        for test_prefix in self._test_prefix_map:
            if self._prefix_mode:
                tests.update(self._port.tests([test_prefix]))
            else:
                tests.add(test_prefix)
        return sorted(tests)

    def build_port_pairs(self, test_prefix):
        # Return a copy in case the caller modifies the returned list.
        return [(build, port)
                for build, _, port in self._test_prefix_map[test_prefix]]

    def add(self, test_prefix, build, step_name=None, port_name=None):
        """Adds an entry for baselines to download for some set of tests.

        Args:
            test_prefix: This can be a full test path; if the instance was
                constructed in prefix mode (the default), this can also be a
                directory of tests or a path with globs.
            build: A Build object. Along with the step name, this specifies
                where to fetch baselines from.
            step_name: The name of the build step this test was run for.
            port_name: This specifies what platform the baseline is for.
        """
        port_name = port_name or self._host.builders.port_name_for_builder_name(
            build.builder_name)
        self._build_steps.add((build.builder_name, step_name))
        build_step = (build, step_name, port_name)
        self._test_prefix_map[test_prefix].append(build_step)

    def all_build_steps(self):
        """Returns all builder name, step name pairs in this collection."""
        return self._build_steps


class AbstractParallelRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline commands that do some tasks in parallel."""
    # pylint: disable=abstract-method; not overriding `execute()`

    MAX_WORKERS: ClassVar[int] = 16

    def __init__(self, options=None):
        super(AbstractParallelRebaselineCommand,
              self).__init__(options=options)

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

    def _filter_baseline_set_builders(self, test_baseline_set):
        build_steps_to_fetch_from = self.build_steps_to_fetch_from(
            test_baseline_set.all_build_steps())
        for test_prefix, build, step_name, port_name in test_baseline_set:
            if (build.builder_name, step_name) in build_steps_to_fetch_from:
                yield (test_prefix, build, step_name, port_name)

    def build_steps_to_fetch_from(self, build_steps_to_check):
        """Returns the subset of builder-step pairs that will cover all of the
        baseline search paths used in the input list.

        In particular, if the input list contains both Release and Debug
        versions of a configuration, we *only* return the Release version
        (since we don't save debug versions of baselines).

        Args:
            build_steps_to_check: A collection of builder name, step name
                tuples.

        Returns:
            A subset of the input list that we should fetch from.
        """
        release_build_steps = set()
        debug_build_steps = set()
        for builder, step in build_steps_to_check:
            port = self._tool.port_factory.get_from_builder_name(builder)
            if port.test_configuration().build_type == 'release':
                release_build_steps.add((builder, step))
            else:
                debug_build_steps.add((builder, step))

        build_steps_to_fallback_paths = defaultdict(dict)
        #TODO: we should make the selection of (builder, step) deterministic
        for builder, step in list(release_build_steps) + list(
                debug_build_steps):
            if not self._tool.builders.uses_wptrunner(builder):
                # Some result db related unit tests set step to None
                is_legacy_step = step is None or 'blink_web_tests' in step
                flag_spec_option = self._tool.builders.flag_specific_option(
                    builder, step)
                port = self._tool.port_factory.get_from_builder_name(builder)
                port.set_option_default('flag_specific', flag_spec_option)
                fallback_path = port.baseline_search_path()
                if fallback_path not in list(
                        build_steps_to_fallback_paths[is_legacy_step].values()):
                    build_steps_to_fallback_paths[
                        is_legacy_step][builder, step] = fallback_path
        return (set(build_steps_to_fallback_paths[True])
                | set(build_steps_to_fallback_paths[False]))

    def _rebaseline_args(self,
                         test,
                         suffixes,
                         port_name=None,
                         flag_specific=None,
                         verbose=False):
        args = []
        if verbose:
            args.append('--verbose')
        args.extend([
            '--test',
            test,
            # Sort suffixes so we can have a deterministic order for comparing
            # commands in unit tests.
            '--suffixes',
            ','.join(sorted(suffixes)),
        ])
        if port_name:
            args.extend(['--port-name', port_name])
        if flag_specific:
            args.extend(['--flag-specific', flag_specific])
        return args

    def _rebaseline_commands(self, test_baseline_set, options):
        path_to_blink_tool = self._tool.path()
        cwd = self._tool.git().checkout_root
        rebaseline_commands = []
        copy_baseline_commands = []
        lines_to_remove = {}

        # A test baseline set is a high-dimensional object, so we try to avoid
        # iterating it.
        baseline_subset = self._filter_baseline_set_builders(test_baseline_set)
        for test, build, step_name, port_name in baseline_subset:
            suffixes = list(
                self._suffixes_for_actual_failures(test, build, step_name))
            if not suffixes:
                # Only try to remove the expectation if the test
                #   1. ran and passed ([ Skip ], [ WontFix ] should be kept)
                #   2. passed unexpectedly (flaky expectations should be kept)
                if self._test_passed_unexpectedly(test, build, port_name,
                                                  step_name):
                    _log.debug(
                        'Test %s passed unexpectedly in %s. '
                        'Will try to remove it from TestExpectations.', test,
                        build)
                    if test not in lines_to_remove:
                        lines_to_remove[test] = []
                    lines_to_remove[test].append(port_name)
                continue

            flag_spec_option = self._tool.builders.flag_specific_option(
                build.builder_name, step_name)

            args = self._rebaseline_args(test, suffixes, port_name,
                                         flag_spec_option, options.verbose)
            args.extend(['--builder', build.builder_name])
            if build.build_number:
                args.extend(['--build-number', str(build.build_number)])
            if options.results_directory:
                args.extend(['--results-directory', options.results_directory])
            if step_name:
                args.extend(['--step-name', step_name])

            if self._resultdb_fetcher:
                args.append('--resultDB')
                raw_result = self._result_for_test(test, build,
                                                   step_name).result_dict()
                # TODO(crbug.com/1282507): Instead of grabbing the first
                # artifact of each type, we should download artifacts across
                # all retries and and check if they're all exactly the same.
                fetch_urls = [
                    artifacts[0]
                    for artifacts in raw_result['artifacts'].values()
                ]
                args.extend(['--fetch-url', ','.join(fetch_urls)])

            rebaseline_command = [
                self._tool.executable, path_to_blink_tool,
                'rebaseline-test-internal'
            ] + args
            rebaseline_commands.append((rebaseline_command, cwd))

            copy_command = [
                self._tool.executable,
                path_to_blink_tool,
                'copy-existing-baselines-internal',
            ]
            copy_command.extend(
                self._rebaseline_args(test, suffixes, port_name,
                                      flag_spec_option, options.verbose))
            copy_baseline_commands.append((copy_command, cwd))

        return copy_baseline_commands, rebaseline_commands, lines_to_remove

    @staticmethod
    def _extract_expectation_line_changes(command_results):
        """Parses the JSON lines from sub-command output and returns the result as a ChangeSet."""
        change_set = ChangeSet()
        for _, stdout, _ in command_results:
            updated = False
            for line in stdout.splitlines():
                if not line:
                    continue
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

    def _optimize_commands(self,
                           test_baseline_set,
                           verbose=False,
                           resultDB=False):
        """Returns a list of commands to run in parallel to de-duplicate baselines."""
        test_set = set()
        baseline_subset = self._filter_baseline_set_builders(test_baseline_set)
        for test, build, step_name, _ in baseline_subset:
            # Use the suffixes information to determine whether to proceed with the optimize
            # step. Suffixes are not passed to the optimizer.
            suffixes = self._suffixes_for_actual_failures(
                test, build, step_name)
            if suffixes:
                test_set.add(test)

        # For real tests we will optimize all the virtual tests derived from
        # that. No need to include a virtual tests if we will also optimize the
        # non virtual version.
        port = self._tool.port_factory.get()
        virtual_tests_to_exclude = set([
            test for test in test_set
            if port.lookup_virtual_test_base(test) in test_set
        ])
        test_set -= virtual_tests_to_exclude

        # Process the test_list so that each list caps at MAX_TESTS_IN_OPTIMIZE_CMDLINE tests
        capped_test_list = []
        test_list = list(test_set)
        for i in range(0, len(test_set), MAX_TESTS_IN_OPTIMIZE_CMDLINE):
            capped_test_list.append(test_list[i:i +
                                              MAX_TESTS_IN_OPTIMIZE_CMDLINE])

        optimize_commands = []
        cwd = self._tool.git().checkout_root
        path_to_blink_tool = self._tool.path()

        # Build one optimize-baselines invocation command for each flag_spec.
        # All the tests in the test list will be optimized iteratively.
        for test_list in capped_test_list:
            command = [
                self._tool.executable,
                path_to_blink_tool,
                'optimize-baselines',
                # FIXME: We should propagate the platform options as well.
                # Prevent multiple baseline optimizer to race updating the manifest.
                # The manifest has already been updated when listing tests.
                '--no-manifest-update',
            ]
            if verbose:
                command.append('--verbose')

            command.extend(test_list)
            optimize_commands.append((command, cwd))

        return optimize_commands

    def _update_expectations_files(self, lines_to_remove):
        tests = list(lines_to_remove.keys())
        to_remove = collections.defaultdict(set)
        all_versions = frozenset([
            config.version.lower() for config in self._tool.port_factory.get().
            all_test_configurations()
        ])
        # This is so we remove lines for builders that skip this test.
        # For example, Android skips most tests and we don't want to leave
        # stray [ Android ] lines in TestExpectations.
        # This is only necessary for "blink_tool.py rebaseline".
        for port_name in self._tool.port_factory.all_port_names():
            port = self._tool.port_factory.get(port_name)
            for test in tests:
                if (port.test_configuration().version.lower() in all_versions
                        and port.skips_test(test)):
                    to_remove[test].add(
                        port.test_configuration().version.lower())

        # Get configurations to remove based on builders for each test
        for test, port_names in lines_to_remove.items():
            for port_name in port_names:
                port = self._tool.port_factory.get(port_name)
                if port.test_configuration().version.lower() in all_versions:
                    to_remove[test].add(
                        port.test_configuration().version.lower())

        if self._dry_run:
            for test, versions in to_remove.items():
                _log.debug('Would have removed expectations for %s: %s', test,
                           ', '.join(sorted(versions)))
            return

        port = self._tool.port_factory.get()
        path = port.path_to_generic_test_expectations_file()
        test_expectations = TestExpectations(
            port,
            expectations_dict={
                path: self._tool.filesystem.read_text_file(path)
            })
        system_remover = SystemConfigurationRemover(self._tool.filesystem, test_expectations)
        for test, versions in to_remove.items():
            system_remover.remove_os_versions(test, versions)
        system_remover.update_expectations()

    def _run_in_parallel(self, commands, resultdb):
        """
        Parallel run the commands using the MessagePool class to make sure that
        each process will have only one request session.
        """
        if not commands:
            return []

        if self._dry_run:
            for command, _ in commands:
                _log.debug('Would have run: "%s"',
                           self._tool.executive.command_for_printing(command))
            return [(0, '', '')] * len(commands)
        results = []
        if resultdb:
            try:
                num_workers = min(self.MAX_WORKERS,
                                  self._tool.executive.cpu_count())
                pool = message_pool.get(self, self._worker_factory,
                                        num_workers, self._tool)
                pool.run(('rebaseline', command) for command, cwd in commands)
            except Exception as error:
                _log.debug('%s("%s") raised, exiting',
                           error.__class__.__name__, error)
                raise
        else:
            results = self._tool.executive.run_in_parallel(commands)

        for _, _, stderr in results:
            if stderr:
                lines = stderr.decode("utf-8", "ignore").splitlines()
                for line in lines:
                    print(line)
        return results

    def _worker_factory(self, worker_connection):
        return Worker(self._tool.git().checkout_root)

    def rebaseline(self, options, test_baseline_set):
        """Fetches new baselines and removes related test expectation lines.

        Args:
            options: An object with the command line options.
            test_baseline_set: A TestBaselineSet instance, which represents
                a set of tests/platform combinations to rebaseline.
        """
        if not self._dry_run and self._tool.git(
        ).has_working_directory_changes(pathspec=self._web_tests_dir()):
            _log.error(
                'There are uncommitted changes in the web tests directory; aborting.'
            )
            return

        for test in test_baseline_set.all_tests():
            _log.info('Rebaselining %s', test)

        # extra_lines_to_remove are unexpected passes, while lines_to_remove are
        # failing tests that have been rebaselined.
        copy_baseline_commands, rebaseline_commands, extra_lines_to_remove = self._rebaseline_commands(
            test_baseline_set, options)
        lines_to_remove = {}
        # TODO(crbug/1213998): Make both copy and rebaseline command use the
        # message pool, currently the second time when run the message pool,
        # it will halt there.
        self._run_in_parallel(copy_baseline_commands, False)
        command_results = self._run_in_parallel(
            rebaseline_commands, getattr(options, 'resultDB', False))
        change_set = self._extract_expectation_line_changes(command_results)
        lines_to_remove = change_set.lines_to_remove
        for test in extra_lines_to_remove:
            if test in lines_to_remove:
                lines_to_remove[test] = (
                    lines_to_remove[test] + extra_lines_to_remove[test])
            else:
                lines_to_remove[test] = extra_lines_to_remove[test]

        if lines_to_remove:
            self._update_expectations_files(lines_to_remove)

        if options.optimize:
            # No point in optimizing during a dry run where no files were
            # downloaded.
            if self._dry_run:
                _log.info('Skipping optimization during dry run.')
            else:
                optimize_commands = self._optimize_commands(
                    test_baseline_set, options.verbose, options.resultDB)
                for (cmd, cwd) in optimize_commands:
                    output = self._tool.executive.run_command(cmd, cwd)
                    print(output)

        if not self._dry_run:
            self._tool.git().add_list(self.unstaged_baselines())

    def unstaged_baselines(self):
        """Returns absolute paths for unstaged (including untracked) baselines."""
        baseline_re = re.compile(r'.*[\\/]' + WEB_TESTS_LAST_COMPONENT +
                                 r'[\\/].*-expected\.(txt|png|wav)$')
        unstaged_changes = self._tool.git().unstaged_changes()
        return sorted(self._tool.git().absolute_path(path)
                      for path in unstaged_changes
                      if re.match(baseline_re, path))

    def _generic_baseline_paths(self, test_baseline_set):
        """Returns absolute paths for generic baselines for the given tests.

        Even when a test does not have a generic baseline, the path where it
        would be is still included in the return value.
        """
        filesystem = self._tool.filesystem
        baseline_paths = []
        for test in test_baseline_set.all_tests():
            filenames = [
                self._file_name_for_expected_result(test, suffix)
                for suffix in BASELINE_SUFFIX_LIST
            ]
            baseline_paths += [
                filesystem.join(self._web_tests_dir(), filename)
                for filename in filenames
            ]
        baseline_paths.sort()
        return baseline_paths

    def _web_tests_dir(self):
        return self._tool.port_factory.get().web_tests_dir()

    def _suffixes_for_actual_failures(self, test, build, step_name=None):
        """Gets the baseline suffixes for actual mismatch failures in some results.

        Args:
            test: A full test path string.
            build: A Build object.
            step_name: The name of the build step the test ran in.

        Returns:
            A set of file suffix strings.
        """
        test_result = self._result_for_test(test, build, step_name)
        if not test_result:
            return set()
        return test_result.suffixes_for_test_result()

    def _test_passed_unexpectedly(self, test, build, port_name,
                                  step_name=None):
        """Determines if a test passed unexpectedly in a build.

        The routine also takes into account the port that is being rebaselined.
        It is possible to use builds from a different port to rebaseline the
        current port, e.g. rebaseline-cl --fill-missing, in which case the test
        will not be considered passing regardless of the result.

        Args:
            test: A full test path string.
            build: A Build object.
            port_name: The name of port currently being rebaselined.
            step_name: The name of the build step the test ran in.

        Returns:
            A boolean.
        """
        if self._tool.builders.port_name_for_builder_name(
                build.builder_name) != port_name:
            return False
        test_result = self._result_for_test(test, build, step_name)
        if not test_result:
            return False
        return test_result.did_pass() and not test_result.did_run_as_expected()

    @memoized
    def _result_for_test(self, test, build, step_name):
        if self._resultdb_fetcher:
            results = self._tool.results_fetcher.gather_results(
                build, step_name)
        else:
            # We need full results to know if a test passed or was skipped.
            # TODO(robertma): Make memoized support kwargs, and use full=True
            # here.
            results = self._tool.results_fetcher.fetch_results(
                build, True, step_name)
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
            self.dry_run_option,
            # FIXME: should we support the platform options in addition to (or instead of) --builders?
            self.results_directory_option,
            optparse.make_option(
                '--builders',
                default=None,
                action='append',
                help=
                ('Comma-separated-list of builders to pull new baselines from '
                 '(can also be provided multiple times).')),
        ])

    def _builders_to_pull_from(self):
        return self._tool.user.prompt_with_list(
            'Which builder to pull results from:',
            self._release_builders(),
            can_choose_multiple=True)

    def execute(self, options, args, tool):
        self._tool = tool
        self._dry_run = options.dry_run
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
            build = Build(builder)
            step_names = self._tool.results_fetcher.get_layout_test_step_names(
                build)
            for step_name in step_names:
                for test_prefix in args:
                    test_baseline_set.add(test_prefix, build, step_name)

        _log.debug('Rebaselining: %s', test_baseline_set)

        self.rebaseline(options, test_baseline_set)


class Worker:
    """A worker delegate for running Blink tool commands.

    The purpose of this worker is to persist HTTP(S) connections to ResultDB
    across command invocations.

    See Also:
        crbug.com/1213998#c50
    """

    def __init__(self, cwd: str):
        self._cwd = cwd

    def start(self):
        # Dynamically import `BlinkTool` to avoid a circular import.
        from blinkpy.tool.blink_tool import BlinkTool
        # `BlinkTool` cannot be serialized, so construct one here in the worker
        # process instead of in the constructor, which runs in the managing
        # process. See crbug.com/1386267.
        self._tool = BlinkTool(self._cwd)

    def handle(self, name, source, command):
        assert name == 'rebaseline'
        self._tool.main(command[1:])
