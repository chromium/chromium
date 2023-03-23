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
import functools
import logging
import optparse
import re
from typing import (
    ClassVar,
    Collection,
    Dict,
    List,
    Set,
)

from blinkpy.common import message_pool
from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.memoized import memoized
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.system.user import User
from blinkpy.tool.commands.command import Command, check_dir_option
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.test_expectations import SystemConfigurationRemover, TestExpectations
from blinkpy.web_tests.port import base, factory

_log = logging.getLogger(__name__)

# For CLI compatibility, we would like a list of baseline extensions without
# the leading dot.
# TODO(robertma): Investigate changing the CLI.
BASELINE_SUFFIX_LIST = tuple(ext[1:] for ext in base.Port.BASELINE_EXTENSIONS)


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


class TestBaselineSet(collections.abc.Set):
    """Represents a collection of tests and platforms that can be rebaselined.

    A TestBaselineSet specifies tests to rebaseline along with information
    about where to fetch the baselines from.
    """

    def __init__(self, builders):
        self._builders = builders
        self._build_steps = set()
        self._test_map = collections.defaultdict(list)

    def __contains__(self, rebaseline_task):
        test, *build_info = rebaseline_task
        return tuple(build_info) in self._test_map.get(test, [])

    def __iter__(self):
        return iter(self._iter_combinations())

    def __len__(self):
        return sum(map(len, self._test_map.values()))

    def __bool__(self):
        return bool(self._test_map)

    def _iter_combinations(self):
        """Iterates through (test, build, step, port) combinations."""
        for test, build_steps in self._test_map.items():
            for build_step in build_steps:
                yield (test, ) + build_step

    def __str__(self):
        if not self._test_map:
            return '<Empty TestBaselineSet>'
        return '<TestBaselineSet with:\n  %s>' % '\n  '.join(
            '%s: %s, %s, %s' % combo for combo in self._iter_combinations())

    def all_tests(self):
        """Returns a sorted list of all tests without duplicates."""
        return sorted(self._test_map)

    def build_port_pairs(self, test):
        # Return a copy in case the caller modifies the returned list.
        return [(build, port) for build, _, port in self._test_map[test]]

    def runs_for_test(self, test: str):
        return list(self._test_map[test])

    def add(self, test, build, step_name=None, port_name=None):
        """Adds an entry for baselines to download for some set of tests.

        Args:
            test: A full test path.
            build: A Build object. Along with the step name, this specifies
                where to fetch baselines from.
            step_name: The name of the build step this test was run for.
            port_name: This specifies what platform the baseline is for.
        """
        port_name = port_name or self._builders.port_name_for_builder_name(
            build.builder_name)
        self._build_steps.add((build.builder_name, step_name))
        build_step = (build, step_name, port_name)
        self._test_map[test].append(build_step)

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
        self._baselines_to_copy = []

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

    def _filter_baseline_set(
        self,
        test_baseline_set: TestBaselineSet,
    ) -> TestBaselineSet:
        build_steps_to_fetch_from = self.build_steps_to_fetch_from(
            test_baseline_set.all_build_steps())
        rebaselinable_set = TestBaselineSet(self._tool.builders)
        port = self._tool.port_factory.get()
        for test, build, step_name, port_name in test_baseline_set:
            if (build.builder_name,
                    step_name) not in build_steps_to_fetch_from:
                continue
            if port.reference_files(test):
                # TODO(crbug.com/1149035): Add a `[ Failure ]` line here
                # instead.
                continue
            suffixes = list(
                self._suffixes_for_actual_failures(test, build, step_name))
            if suffixes:
                rebaselinable_set.add(test, build, step_name, port_name)
        return rebaselinable_set

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

        build_steps_to_fallback_paths = collections.defaultdict(dict)
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

    def _copy_baselines(self, groups: Dict[str, TestBaselineSet]) -> None:
        self._baselines_to_copy.clear()
        with self._message_pool(self._worker_factory) as pool:
            pool.run([('find_baselines_to_copy', test, suffix, group)
                      for test, group in groups.items()
                      for suffix in self._suffixes_for_group(group)])
        implicit_all_pass = [
            dest for source, dest in self._baselines_to_copy if not source
        ]
        if implicit_all_pass:
            _log.warning(
                'The following nonexistent paths will not be rebaselined '
                'because of explicitly provided tests or builders:')
            for baseline in sorted(implicit_all_pass):
                _log.warning('  %s', baseline)
            _log.warning('These baselines risk being clobbered because they '
                         'fall back to others that will be replaced.')
            _log.warning(
                'If results are expected to vary by platform or virtual suite, '
                'consider rerunning `rebaseline-cl` without any arguments to '
                'rebaseline the paths listed above too.')
            _log.warning('See crbug.com/1324638 for details.')
            if not self._tool.user.confirm(default=User.DEFAULT_NO):
                raise RebaselineCancellation
        with self._message_pool(self._worker_factory) as pool:
            pool.run([('write_copy', source, dest)
                      for source, dest in self._baselines_to_copy if source])

    def _group_tests_by_base(
        self,
        test_baseline_set: TestBaselineSet,
    ) -> Dict[str, TestBaselineSet]:
        """Partition a given baseline set into per-base test baseline sets.

        The baseline copier/optimizer handles all virtual tests derived from a
        nonvirtual (base) test. When both a virtual test and its base test need
        to be rebaselined, this method coalesces information about where they
        failed into a shared `TestBaselineSet` that the copier/optimizer can
        easily ingest.
        """
        groups = collections.defaultdict(
            functools.partial(TestBaselineSet, self._tool.builders))
        port = self._tool.port_factory.get()
        tests = set(test_baseline_set.all_tests())
        for test, build, step_name, port_name in test_baseline_set:
            nonvirtual_test = port.lookup_virtual_test_base(test) or test
            test_for_group = (nonvirtual_test
                              if nonvirtual_test in tests else test)
            groups[test_for_group].add(test, build, step_name, port_name)
        return groups

    def _suffixes_for_group(self,
                            test_baseline_set: TestBaselineSet) -> Set[str]:
        return frozenset().union(
            *(self._suffixes_for_actual_failures(test, build, step_name)
              for test, build, step_name, _ in test_baseline_set))

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
        rebaseline_commands = []
        for test, build, step_name, port_name in test_baseline_set:
            suffixes = list(
                self._suffixes_for_actual_failures(test, build, step_name))
            assert suffixes, '(%s, %s, %s) should not be rebaselined' % (
                test, build, step_name)
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
                path_to_blink_tool, 'rebaseline-test-internal'
            ] + args
            rebaseline_commands.append(rebaseline_command)

        return rebaseline_commands

    def _optimize_command(self,
                          tests: Collection[str],
                          verbose: bool = False) -> List[str]:
        """Return a command to de-duplicate baselines."""
        assert tests, 'should not generate a command to optimize no tests'
        command = [
            self._tool.path(),
            'optimize-baselines',
            # The manifest has already been updated when listing tests.
            '--no-manifest-update',
        ]
        if verbose:
            command.append('--verbose')
        command.extend(sorted(tests))
        return command

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

    def _message_pool(self, worker_factory):
        num_workers = min(self.MAX_WORKERS, self._tool.executive.cpu_count())
        return message_pool.get(self, worker_factory, num_workers)

    def _worker_factory(self, worker_connection):
        return Worker(worker_connection,
                      self._tool.git().checkout_root,
                      dry_run=self._dry_run)

    def handle(self, name: str, source: str, *args):
        """Handler called when a worker completes a rebaseline task.

        This allows this class to conform to the `message_pool.MessageHandler`
        interface.
        """
        if name == 'find_baselines_to_copy':
            (baselines_to_copy, ) = args
            self._baselines_to_copy.extend(baselines_to_copy)

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
            return 1

        for test in test_baseline_set.all_tests():
            _log.info('Rebaselining %s', test)

        rebaselinable_set = self._filter_baseline_set(test_baseline_set)
        groups = self._group_tests_by_base(rebaselinable_set)
        try:
            self._copy_baselines(groups)
        except RebaselineCancellation:
            _log.warning('Cancelling rebaseline attempt.')
            return 1

        rebaseline_commands = self._rebaseline_commands(
            rebaselinable_set, options)
        with self._message_pool(self._worker_factory) as pool:
            pool.run([('download_baseline', command)
                      for command in rebaseline_commands])

        exit_code = 0
        if options.optimize and groups:
            # No point in optimizing during a dry run where no files were
            # downloaded.
            if self._dry_run:
                _log.info('Skipping optimization during dry run.')
            else:
                optimize_command = self._optimize_command(
                    groups, options.verbose)
                exit_code = exit_code or self._tool.main(optimize_command)

        if not self._dry_run:
            self._tool.git().add_list(self.unstaged_baselines())
        return exit_code

    def unstaged_baselines(self):
        """Returns absolute paths for unstaged (including untracked) baselines."""
        baseline_re = re.compile(r'.*[\\/]' + WEB_TESTS_LAST_COMPONENT +
                                 r'[\\/].*-expected\.(txt|png|wav)$')
        unstaged_changes = self._tool.git().unstaged_changes()
        return sorted(self._tool.git().absolute_path(path)
                      for path in unstaged_changes
                      if re.match(baseline_re, path))

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

        test_baseline_set = TestBaselineSet(tool.builders)
        tests = self._tool.port_factory.get().tests(args)
        for builder in builders_to_check:
            build = Build(builder)
            step_names = self._tool.results_fetcher.get_layout_test_step_names(
                build)
            for step_name in step_names:
                for test in tests:
                    test_baseline_set.add(test, build, step_name)

        _log.debug('Rebaselining: %s', test_baseline_set)
        self.rebaseline(options, test_baseline_set)


class RebaselineCancellation(Exception):
    """Represents a cancelled rebaseline attempt."""


class Worker:
    """A worker delegate for running rebaseline tasks in parallel.

    The purpose of this worker is to persist resources across tasks:
      * HTTP(S) connections to ResultDB
      * TestExpectations caches

    See Also:
        crbug.com/1213998#c50
    """

    def __init__(self, connection, cwd: str, dry_run: bool = False):
        self._connection = connection
        self._cwd = cwd
        self._dry_run = dry_run
        self._commands = {
            'find_baselines_to_copy': self._find_baselines_to_copy,
            'write_copy': self._write_copy,
            'download_baseline': self._download_baseline,
        }

    def start(self):
        # Dynamically import `BlinkTool` to avoid a circular import.
        from blinkpy.tool.blink_tool import BlinkTool
        # `BlinkTool` cannot be serialized, so construct one here in the worker
        # process instead of in the constructor, which runs in the managing
        # process. See crbug.com/1386267.
        self._tool = BlinkTool(self._cwd)
        self._copier = BaselineCopier(self._connection.host)

    def handle(self, name: str, source: str, *args):
        response = self._commands[name](*args)
        # Post a message to the managing process to flush this worker's logs.
        if response is not None:
            self._connection.post(name, response)
        else:
            self._connection.post(name)

    def _find_baselines_to_copy(self, test_name: str, suffix: str,
                                group: TestBaselineSet):
        return list(
            self._copier.find_baselines_to_copy(test_name, suffix, group))

    def _write_copy(self, source: str, dest: str):
        if self._dry_run:
            _log.info('Would have copied %s -> %s', source, dest)
        else:
            self._copier.write_copies([(source, dest)])

    def _download_baseline(self, command: List[str]):
        if self._dry_run:
            _log.debug('Would have run: %s',
                       self._tool.executive.command_for_printing(command))
        else:
            self._tool.main(command)
