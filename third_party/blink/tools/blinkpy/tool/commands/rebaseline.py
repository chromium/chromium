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
import enum
import functools
import logging
import optparse
import re
from dataclasses import dataclass
from typing import (
    ClassVar,
    Collection,
    Dict,
    List,
    NamedTuple,
    Optional,
    Set,
    Tuple,
)
from urllib.parse import urlparse

from blinkpy.common import message_pool
from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.common.host import Host
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.memoized import memoized
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import Artifact, WebTestResult
from blinkpy.tool.commands.command import (
    Command,
    check_dir_option,
    check_file_option,
)
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.test_expectations import SystemConfigurationEditor, TestExpectations
from blinkpy.web_tests.models.typ_types import RESULT_TAGS, ResultType
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
    test_name_file_option = optparse.make_option(
        '--test-name-file',
        action='callback',
        callback=check_file_option,
        type='string',
        help=('Read names of tests to update from this file, '
              'one test per line.'))

    def __init__(self, options=None):
        super(AbstractRebaseliningCommand, self).__init__(options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST
        self.expectation_line_changes = ChangeSet()
        self._tool = None
        self._results_dir = None
        self._dry_run = False

    def baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        return port.baseline_version_dir()

    @functools.cached_property
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


class RebaselineTask(NamedTuple):
    test: str
    build: Build
    step_name: str
    port_name: str


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
                yield RebaselineTask(test, *build_step)

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

    def add(self,
            test: str,
            build: Build,
            step_name: str,
            port_name: Optional[str] = None):
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


class RebaselineFailureReason(enum.Enum):
    TIMEOUT_OR_CRASH = 'May not run to completion'
    REFTEST_IMAGE_FAILURE = 'Reftest image failure'
    FLAKY_OUTPUT = 'Flaky output'
    LOCAL_BASELINE_NOT_FOUND = 'Missing from local results directory'


RebaselineGroup = Dict[RebaselineTask, WebTestResult]
RebaselineFailures = Dict[RebaselineTask, RebaselineFailureReason]


class AbstractParallelRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline commands that do some tasks in parallel."""
    # pylint: disable=abstract-method; not overriding `execute()`

    MAX_WORKERS: ClassVar[int] = 16

    def __init__(self, options=None):
        super(AbstractParallelRebaselineCommand,
              self).__init__(options=options)
        self.baseline_cache_stats = BaselineCacheStatistics()
        self._rebaseline_failures = {}

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
        for task in test_baseline_set:
            test, build, step_name, port_name = task
            if (build.builder_name,
                    step_name) not in build_steps_to_fetch_from:
                continue
            suffixes = self._suffixes_for_actual_failures(
                test, build, step_name)
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
            if not self._tool.builders.uses_wptrunner(builder, step):
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
        with self._message_pool(self._worker_factory) as pool:
            pool.run([('copy_baselines', test, suffix, group)
                      for test, group in groups.items()
                      for suffix in self._suffixes_for_group(group)])

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
        tests = set(test_baseline_set.all_tests())
        for test, build, step_name, port_name in test_baseline_set:
            nonvirtual_test = self._host_port.lookup_virtual_test_base(
                test) or test
            test_for_group = (nonvirtual_test
                              if nonvirtual_test in tests else test)
            groups[test_for_group].add(test, build, step_name, port_name)
        return groups

    def _suffixes_for_group(self,
                            test_baseline_set: TestBaselineSet) -> Set[str]:
        return frozenset().union(
            *(self._suffixes_for_actual_failures(test, build, step_name)
              for test, build, step_name, _ in test_baseline_set))

    def _download_baselines(self, groups: Dict[str, TestBaselineSet]):
        with self._message_pool(self._worker_factory) as pool:
            # The same worker should download all the baselines in a group so
            # that its baseline cache is effective.
            pool.run([('download_baselines', self._group_with_results(group))
                      for group in groups.values()])

    def _group_with_results(
        self,
        test_baseline_set: TestBaselineSet,
    ) -> RebaselineGroup:
        tasks_to_results = {}
        for task in test_baseline_set:
            maybe_result = self._result_for_test(task.test, task.build,
                                                 task.step_name)
            if maybe_result:
                tasks_to_results[task] = maybe_result
        return tasks_to_results

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
            config.version.lower()
            for config in self._host_port.all_test_configurations()
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

        path = port.path_to_generic_test_expectations_file()
        test_expectations = TestExpectations(
            self._host_port,
            expectations_dict={
                path: self._tool.filesystem.read_text_file(path)
            })
        system_remover = SystemConfigurationEditor(test_expectations)
        for test, versions in to_remove.items():
            system_remover.remove_os_versions(test, versions)
        system_remover.update_expectations()

    def _message_pool(self, worker_factory):
        num_workers = min(self.MAX_WORKERS, self._tool.executive.cpu_count())
        return message_pool.get(self, worker_factory, num_workers)

    def _worker_factory(self, worker_connection):
        return Worker(worker_connection,
                      dry_run=self._dry_run)

    def handle(self, name: str, source: str, *args):
        """Handler called when a worker completes a rebaseline task.

        This allows this class to conform to the `message_pool.MessageHandler`
        interface.
        """
        if name == 'report_baseline_cache_stats':
            (stats, ) = args
            self.baseline_cache_stats += stats
        elif name == 'download_baselines':
            (rebaseline_failures, ) = args
            self._rebaseline_failures.update(rebaseline_failures)

    def rebaseline(self, options, test_baseline_set):
        """Fetches new baselines and removes related test expectation lines.

        Args:
            options: An object with the command line options.
            test_baseline_set: A TestBaselineSet instance, which represents
                a set of tests/platform combinations to rebaseline.
        """
        self._rebaseline_failures.clear()
        self._results_dir = options.results_directory
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
        self._copy_baselines(groups)
        self._download_baselines(groups)
        _log.debug('Baseline cache statistics: %s', self.baseline_cache_stats)
        self._warn_about_rebaseline_failures()

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

    def _warn_about_rebaseline_failures(self):
        if not self._rebaseline_failures:
            return
        tasks_by_exp_file = self._group_tasks_by_exp_file(
            self._rebaseline_failures)
        _log.warning('Some test failures should be suppressed in '
                     'TestExpectations instead of being rebaselined.')
        # TODO(crbug.com/1149035): Fully automate writing to TestExpectations
        # files. This should be done by integrating with the existing
        # TestExpectations updater, which has better handling for:
        #   * Conflicting tag removal
        #   * Specifier merge/split
        for exp_file in sorted(tasks_by_exp_file):
            lines = '\n'.join(
                map(self._format_line, tasks_by_exp_file[exp_file]))
            # Make the TestExpectation lines copy-pastable by logging them
            # in a single statement (i.e., not prefixed by the log's
            # formatting).
            _log.warning('Consider adding the following lines to %s:\n%s',
                         exp_file, lines)

    def _group_tasks_by_exp_file(
        self,
        tasks: Collection[RebaselineTask],
    ) -> Dict[str, Set[RebaselineTask]]:
        tasks_by_exp_file = collections.defaultdict(set)
        for task in tasks:
            flag_spec_option = self._tool.builders.flag_specific_option(
                task.build.builder_name, task.step_name)
            if flag_spec_option:
                exp_file = self._host_port.path_to_flag_specific_expectations_file(
                    flag_spec_option)
            else:
                exp_file = (
                    self._host_port.path_to_generic_test_expectations_file())
            tasks_by_exp_file[exp_file].add(task)
        return tasks_by_exp_file

    def _format_line(self, task: RebaselineTask) -> str:
        result = self._result_for_test(task.test, task.build, task.step_name)
        # This assertion holds because we only request unexpected non-PASS
        # results from ResultDB. This precondition ensures `result_tags` is not
        # `[ Pass ]` or empty.
        assert set(result.actual_results()) - {ResultType.Pass}
        specifier = self._tool.builders.version_specifier_for_port_name(
            task.port_name)
        results = sorted(set(result.actual_results()))
        result_tags = ' '.join(RESULT_TAGS[result] for result in results)
        reason = self._rebaseline_failures[task].value
        return f'[ {specifier} ] {task.test} [ {result_tags} ]  # {reason}'

    def unstaged_baselines(self):
        """Returns absolute paths for unstaged (including untracked) baselines."""
        baseline_re = re.compile(r'.*[\\/]' + WEB_TESTS_LAST_COMPONENT +
                                 r'[\\/].*-expected\.(txt|png|wav)$')
        unstaged_changes = self._tool.git().unstaged_changes()
        return sorted(self._tool.git().absolute_path(path)
                      for path in unstaged_changes
                      if re.match(baseline_re, path))

    def _web_tests_dir(self):
        return self._host_port.web_tests_dir()

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
        return set(test_result.baselines_by_suffix())

    @memoized
    def _result_for_test(self, test, build, step_name):
        results = self._tool.results_fetcher.gather_results(build, step_name)
        if not results:
            _log.debug('No results found for build %s', build)
            return None
        test_result = results.result_for_test(test)
        if not test_result:
            _log.info('No test result for test %s in build %s', test, build)
            return None
        if self._results_dir:
            for artifact_name, suffix in [
                ('actual_text', 'txt'),
                ('actual_image', 'png'),
                ('actual_audio', 'wav'),
            ]:
                filename = self._file_name_for_actual_result(test, suffix)
                path = self._tool.filesystem.join(self._results_dir, filename)
                test_result.artifacts[artifact_name] = [Artifact(path)]
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
        tests = self._host_port.tests(args)
        for builder in builders_to_check:
            build = Build(builder)
            step_names = self._tool.builders.step_names_for_builder(
                build.builder_name)
            for step_name in step_names:
                for test in tests:
                    test_baseline_set.add(test, build, step_name)

        _log.debug('Rebaselining: %s', test_baseline_set)
        return self.rebaseline(options, test_baseline_set)


@dataclass
class BaselineCacheStatistics:
    hit_count: int = 0
    hit_bytes: int = 0
    total_count: int = 0
    total_bytes: int = 0

    def __add__(self,
                other: 'BaselineCacheStatistics') -> 'BaselineCacheStatistics':
        return BaselineCacheStatistics(self.hit_count + other.hit_count,
                                       self.hit_bytes + other.hit_bytes,
                                       self.total_count + other.total_count,
                                       self.total_bytes + other.total_bytes)

    def __str__(self) -> str:
        return (
            f'hit rate: {self.hit_count}/{self.total_count} '
            f'({_percentage(self.hit_count, self.total_count):.1f}%), '
            f'bytes served via cache: {self.hit_bytes}/{self.total_bytes}B '
            f'({_percentage(self.hit_bytes, self.total_bytes):.1f}%)')

    def record(self, num_bytes: int, hit: bool = False):
        self.total_bytes += num_bytes
        self.total_count += 1
        if hit:
            self.hit_bytes += num_bytes
            self.hit_count += 1


class RebaselineFailure(Exception):
    """Represents a rebaseline task that could not be executed."""
    def __init__(self, reason: RebaselineFailureReason, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.reason = reason


class BaselineLoader:
    """Selects a usable baseline from a list of artifacts.

    This class encompasses:
     1. In-memory baseline caching, keyed on the content's hash digest.
        Baselines likely to be identical (e.g., for a virtual and its base
        test) are downloaded together. The cache takes advantage of this
        temporal locality. For simplicity, the cache is not bounded. It's the
        caller's responsibility to `clear()` the cache as needed.
     2. Finding a "good" baseline for fuzzy-matched pixel tests according to
        some heuristics. See `choose_valid_baseline(...)` for details.
    """
    def __init__(self, host: Host):
        self._host = host
        self._default_port = host.port_factory.get()
        self._digests_to_contents = {}
        # Image diff statistics are commutative. By canonicalizing the
        # (expected, actual) argument order and caching the result, we can
        # avoid invoking `image_diff` a second time unnecessarily when the
        # arguments are swapped.
        make_cached = functools.lru_cache(maxsize=128)
        self._diff_image = make_cached(self._default_port.diff_image)
        self.stats = BaselineCacheStatistics()

    def _image_diff_stats(self, actual: bytes, expected: bytes):
        # By definition, an image has zero diff with itself.
        if actual == expected:
            return {'maxDifference': 0, 'totalPixels': 0}
        _, stats, _ = self._diff_image(*sorted([actual, expected]))
        return stats

    def _fetch_contents(self, url: str) -> bytes:
        # Assume this is a local file.
        if not urlparse(url).scheme:
            try:
                return bytes(self._host.filesystem.read_binary_file(url))
            except FileNotFoundError:
                raise RebaselineFailure(
                    RebaselineFailureReason.LOCAL_BASELINE_NOT_FOUND)
        # Ensure that this is an immutable bytestring (i.e., not `bytearray`).
        return bytes(self._host.web.get_binary(url))

    def load(self, artifact: Artifact) -> bytes:
        hit = False
        if artifact.digest:
            contents = self._digests_to_contents.get(artifact.digest)
            if contents is None:
                contents = self._fetch_contents(artifact.url)
                self._digests_to_contents[artifact.digest] = contents
            else:
                hit = True
        else:
            contents = self._fetch_contents(artifact.url)
        self.stats.record(len(contents), hit)
        return contents

    def choose_valid_baseline(self, artifacts: List[Artifact], test_name: str,
                              suffix: str) -> bytes:
        """Choose a baseline that would have allowed the observed runs to pass.

        Usually, this means returning the contents of a non-flaky artifact
        (i.e., has identical contents across all retries). However, for flaky
        fuzzy-matched pixel tests, we can return an image that would have
        matched all observed retries under existing fuzzy parameters.

        Raises:
            RebaselineFailure: If a test cannot be rebaselined (e.g., flakiness
                outside fuzzy parameters).
        """
        if suffix == 'png' and self._default_port.reference_files(test_name):
            raise RebaselineFailure(
                RebaselineFailureReason.REFTEST_IMAGE_FAILURE)
        assert artifacts
        contents_by_run = {
            artifact: self.load(artifact)
            for artifact in artifacts
        }
        contents = set(contents_by_run.values())
        if len(contents) > 1:
            if suffix == 'png':
                # Because we only rebaseline tests that only had unexpected
                # failures, a test using fuzzy matching can only reach this
                # point if every retry's image fell outside the acceptable
                # range with the current baseline.
                return self._find_fuzzy_matching_baseline(
                    test_name, contents_by_run)
            raise RebaselineFailure(RebaselineFailureReason.FLAKY_OUTPUT)
        return contents.pop()

    def _find_fuzzy_matching_baseline(
        self,
        test_name: str,
        contents_by_run: Dict[Artifact, bytes],
    ) -> bytes:
        max_diff_range, total_pixels_range = (
            self._default_port.get_wpt_fuzzy_metadata(test_name))
        # No fuzzy parameters present.
        if not max_diff_range or not total_pixels_range:
            raise RebaselineFailure(RebaselineFailureReason.FLAKY_OUTPUT)

        max_diff_min, max_diff_max = max_diff_range
        total_pixels_min, total_pixels_max = total_pixels_range
        # The fuzzy parameters must allow the chosen baseline to match itself.
        # Rebaselining a pixel test with nonzero parameter minimums doesn't make
        # sense because any `actual_image` you select as the new baseline will
        # not match if that actual output is seen again.
        if max_diff_min > 0 or total_pixels_min > 0:
            raise RebaselineFailure(RebaselineFailureReason.FLAKY_OUTPUT)

        match_criteria = {}
        for artifact, contents in contents_by_run.items():
            max_diff_needed, total_pixels_needed = (
                self._find_needed_fuzzy_params(contents, contents_by_run))
            fuzzy_mismatch = (max_diff_needed > max_diff_max
                              or total_pixels_needed > total_pixels_max)
            match_criteria[artifact] = (fuzzy_mismatch, total_pixels_needed,
                                        max_diff_needed)

        chosen_artifact = min(match_criteria, key=match_criteria.get)
        fuzzy_mismatch, _, _ = match_criteria[chosen_artifact]
        if fuzzy_mismatch:
            # TODO(crbug.com/1426622): Here, the existing parameters are
            # insufficient for ensuring the test would have passed consistently.
            # Consider suggesting new fuzzy parameters before suggesting
            # TestExpectations, using the tight bounds in
            # `match_criteria[chosen_artifact]`.
            raise RebaselineFailure(RebaselineFailureReason.FLAKY_OUTPUT)
        return contents_by_run[chosen_artifact]

    def _find_needed_fuzzy_params(
        self,
        candidate: bytes,
        contents_by_run: Dict[Artifact, bytes],
    ) -> Tuple[int, int]:
        """Find parameters needed to fuzzily match every observed image.

        Arguments:
            candidate: A candidate baseline that every other run's image will
                be diffed against.

        Returns:
            The minimum (maxDifference, totalPixels) parameters needed.
        """
        max_diff_max = total_pixels_max = 0
        for artifact, run_contents in contents_by_run.items():
            stats = self._image_diff_stats(candidate, run_contents)
            if not stats:
                continue
            max_diff_max = max(max_diff_max, stats['maxDifference'])
            total_pixels_max = max(total_pixels_max, stats['totalPixels'])
        return max_diff_max, total_pixels_max

    def clear(self):
        self._digests_to_contents.clear()


class Worker:
    """A worker delegate for running rebaseline tasks in parallel.

    The purpose of this worker is to persist resources across tasks:
      * HTTP(S) connections to ResultDB
      * TestExpectations cache
      * Baseline cache

    See Also:
        crbug.com/1213998#c50
    """

    def __init__(self,
                 connection,
                 dry_run: bool = False):
        self._connection = connection
        self._dry_run = dry_run
        self._commands = {
            'copy_baselines': self._copy_baselines,
            'download_baselines': self._download_baselines,
        }

    def start(self):
        self._copier = BaselineCopier(self._connection.host)
        self._host = self._connection.host
        self._fs = self._connection.host.filesystem
        self._baseline_loader = BaselineLoader(self._host)

    def stop(self):
        if hasattr(self, '_baseline_loader'):
            self._connection.post('report_baseline_cache_stats',
                                  self._baseline_loader.stats)

    def handle(self, name: str, source: str, *args):
        response = self._commands[name](*args)
        # Post a message to the managing process to flush this worker's logs.
        if response is not None:
            self._connection.post(name, response)
        else:
            self._connection.post(name)

    def _copy_baselines(self, test_name: str, suffix: str,
                        group: TestBaselineSet):
        copies = list(
            self._copier.find_baselines_to_copy(test_name, suffix, group))
        if self._dry_run:
            for source, dest in sorted(copies, key=lambda copy: copy[1]):
                _log.debug('Would have copied %s -> %s', source
                           or '<all-pass>', dest)
        else:
            self._copier.write_copies(copies)

    def _download_baselines(self,
                            group: RebaselineGroup) -> RebaselineFailures:
        self._baseline_loader.clear()
        rebaseline_failures = {}
        for task, result in group.items():
            for suffix, artifacts in result.baselines_by_suffix().items():
                try:
                    contents = self._baseline_loader.choose_valid_baseline(
                        artifacts, task.test, suffix)
                    self._write_baseline(task, suffix, artifacts[0].url,
                                         contents)
                except RebaselineFailure as error:
                    rebaseline_failures[task] = error.reason
        return rebaseline_failures

    def _write_baseline(self, task: RebaselineTask, suffix: str, source: str,
                        contents: bytes):
        port = self._host.port_factory.get(task.port_name)
        flag_spec_option = self._host.builders.flag_specific_option(
            task.build.builder_name, task.step_name)
        port.set_option_default('flag_specific', flag_spec_option)
        dest = self._fs.join(
            port.baseline_version_dir(),
            port.output_filename(task.test,
                                 test_failures.FILENAME_SUFFIX_EXPECTED,
                                 '.' + suffix))
        _log.debug('Retrieving source %s for target %s.', source, dest)
        if not self._dry_run:
            self._fs.maybe_make_directory(self._fs.dirname(dest))
            self._fs.write_binary_file(dest, contents)


def _percentage(a: float, b: float) -> float:
    return 100 * (a / b) if b > 0 else float('nan')
