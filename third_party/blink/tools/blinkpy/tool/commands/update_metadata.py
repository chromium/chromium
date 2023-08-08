# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update WPT metadata from builder results."""

import collections
import contextlib
import functools
import io
import json
import logging
import pathlib
import optparse
import re
from concurrent.futures import Executor
from typing import (
    Any,
    ClassVar,
    Collection,
    Dict,
    FrozenSet,
    Iterable,
    Iterator,
    List,
    Literal,
    Mapping,
    NamedTuple,
    Optional,
    Set,
    TypedDict,
    Union,
)

from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.net.git_cl import BuildStatuses, GitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.system.user import User
from blinkpy.tool import grammar
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import Command
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL
from blinkpy.w3c.wpt_metadata import BUG_PATTERN, TestConfigurations
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.models.test_expectations import (
    TestExpectations,
    SPECIAL_PREFIXES,
)
from blinkpy.web_tests.models import typ_types

path_finder.bootstrap_wpt_imports()
from manifest import manifest as wptmanifest
from wptrunner import (
    manifestupdate,
    metadata,
    testloader,
    wpttest,
)
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.parser import ParseError

_log = logging.getLogger(__name__)


class TestPaths(TypedDict):
    tests_path: str
    metadata_path: str
    manifest_path: str
    url_base: Optional[str]


ManifestMap = Mapping[wptmanifest.Manifest, TestPaths]


class UpdateMetadata(Command):
    name = 'update-metadata'
    show_in_main_help = False  # TODO(crbug.com/1299650): To be switched on.
    help_text = 'Update WPT metadata from builder results.'
    argument_names = '[<test>,...]'
    long_help = __doc__

    def __init__(self,
                 tool: Host,
                 io_pool: Optional[Executor] = None,
                 git_cl: Optional[GitCL] = None):
        super().__init__(options=[
            optparse.make_option(
                '--build',
                dest='builds',
                metavar='[{ci,try}/]<builder>[:<start>[-<end>]],...',
                action='callback',
                callback=_parse_build_specifiers,
                type='string',
                help=('Comma-separated list of builds or build ranges to '
                      'download results for (e.g., "ci/Linux Tests:100-110"). '
                      'When provided with only the builder name, use the try '
                      'build from the latest patchset. '
                      'May specify multiple times.')),
            optparse.make_option(
                '-b',
                '--bug',
                action='callback',
                callback=_coerce_bug_number,
                type='string',
                help='Bug number to include for updated tests.'),
            optparse.make_option(
                '--overwrite-conditions',
                default='fill',
                metavar='{fill,yes,no}',
                choices=['fill', 'yes', 'no'],
                help=('Specify whether to reformat conditional statuses. '
                      '"fill" will reformat conditions using existing '
                      'expectations for platforms without results. (default: '
                      '"fill")')),
            # TODO(crbug.com/1299650): This reason should be optional, but
            # nargs='?' is only supported by argparse, which we should
            # eventually migrate to.
            optparse.make_option(
                '--disable-intermittent',
                metavar='REASON',
                help=('Disable tests and subtests that have inconsistent '
                      'results instead of updating the status list.')),
            optparse.make_option('--keep-statuses',
                                 action='store_true',
                                 help='Keep all existing statuses.'),
            optparse.make_option(
                '--migrate',
                action='store_true',
                help=('Import comments, bugs, and test disables from '
                      'TestExpectations. Warning: Some manual reformatting '
                      'may be required afterwards.')),
            optparse.make_option('--exclude',
                                 action='append',
                                 help='URL prefix of tests to exclude. '
                                 'May specify multiple times.'),
            # TODO(crbug.com/1299650): Support nargs='*' after migrating to
            # argparse to allow usage with shell glob expansion. Example:
            #   --report out/*/wpt_reports*android*.json
            optparse.make_option(
                '--report',
                dest='reports',
                action='callback',
                callback=self._append_reports,
                type='string',
                help=('Path to a wptreport log file (or directory of '
                      'log files) to use in the update. May specify '
                      'multiple times.')),
            RebaselineCL.patchset_option,
            RebaselineCL.test_name_file_option,
            RebaselineCL.only_changed_tests_option,
            RebaselineCL.no_trigger_jobs_option,
            RebaselineCL.dry_run_option,
        ])
        self._tool = tool
        # This tool's performance bottleneck is the network I/O to ResultDB.
        # Using `ProcessPoolExecutor` here would allow for physical parallelism
        # (i.e., avoid Python's Global Interpreter Lock) but incur the cost of
        # IPC.
        self._io_pool = io_pool
        self._path_finder = path_finder.PathFinder(self._tool.filesystem)
        self.git = self._tool.git(path=self._path_finder.web_tests_dir())
        self.git_cl = git_cl or GitCL(self._tool)

    @property
    def _fs(self):
        return self._tool.filesystem

    @property
    def _map_for_io(self):
        return self._io_pool.map if self._io_pool else map

    def execute(self, options: optparse.Values, args: List[str],
                _tool: Host) -> Optional[int]:
        build_resolver = BuildResolver(
            self._tool.web,
            self.git_cl,
            can_trigger_jobs=(options.trigger_jobs and not options.dry_run))
        manifests = load_and_update_manifests(self._path_finder)
        updater = MetadataUpdater.from_manifests(
            manifests,
            TestConfigurations.generate(self._tool),
            self._tool.port_factory.get(),
            self._explicit_include_patterns(options, args),
            options.exclude,
            overwrite_conditions=options.overwrite_conditions,
            disable_intermittent=options.disable_intermittent,
            keep_statuses=options.keep_statuses,
            migrate=options.migrate,
            bug=options.bug,
            dry_run=options.dry_run)
        try:
            test_files = updater.test_files_to_update()
            if options.only_changed_tests:
                test_files = self._filter_unchanged_test_files(test_files)
            self._check_test_files(test_files)
            build_statuses = build_resolver.resolve_builds(
                self._select_builds(options), options.patchset)
            with contextlib.ExitStack() as stack:
                stack.enter_context(self._trace('Updated metadata'))
                if self._io_pool:
                    stack.enter_context(self._io_pool)
                tests_from_builders = updater.collect_results(
                    self.gather_reports(build_statuses, options.reports or []))
                if not options.only_changed_tests:
                    self._check_for_tests_absent_locally(
                        manifests, tests_from_builders)
                self.remove_orphaned_metadata(manifests,
                                              dry_run=options.dry_run)
                modified_test_files = self.write_updates(updater, test_files)
                if not options.dry_run:
                    self.stage(modified_test_files)
        except RPCError as error:
            _log.error('%s', error)
            _log.error('Request payload: %s',
                       json.dumps(error.request_body, indent=2))
            return 1
        except (UnresolvedBuildException, UpdateAbortError, OSError) as error:
            _log.error('%s', error)
            return 1

    def remove_orphaned_metadata(self,
                                 manifests: ManifestMap,
                                 dry_run: bool = False):
        infrastructure_tests = self._path_finder.path_from_wpt_tests(
            'infrastructure')
        for manifest, paths in manifests.items():
            allowlist = {
                metadata.expected_path(paths['metadata_path'], test_path)
                for _, test_path, _ in manifest
            }
            orphans = []
            glob = self._fs.join(paths['metadata_path'], '**', '*.ini')
            for candidate in self._fs.glob(glob):
                # Directory metadata are not supposed to correspond to any
                # particular test, so do not remove them. Skip infrastructure
                # test metadata as well, since they are managed by the upstream
                # repository.
                if (self._fs.basename(candidate) == '__dir__.ini'
                        or candidate.startswith(infrastructure_tests)):
                    continue
                if candidate not in allowlist:
                    orphans.append(candidate)

            if orphans:
                message = (
                    'Deleting %s:' %
                    grammar.pluralize('orphaned metadata file', len(orphans)))
                self._log_metadata_paths(message, orphans)
                if not dry_run:
                    # Ignore untracked files.
                    self.git.delete_list(orphans, ignore_unmatch=True)

    def write_updates(
            self,
            updater: 'MetadataUpdater',
            test_files: List[metadata.TestFileData],
    ) -> List[metadata.TestFileData]:
        """Write updates to disk.

        Returns:
            The subset of test files that were modified.
        """
        _log.info('Updating expectations for up to %s.',
                  grammar.pluralize('test file', len(test_files)))
        modified_test_files = []
        write = functools.partial(self._log_update_write, updater)
        for test_file, modified in zip(test_files,
                                       self._map_for_io(write, test_files)):
            if modified:
                modified_test_files.append(test_file)
        return modified_test_files

    def _log_update_write(
            self,
            updater: 'MetadataUpdater',
            test_file: metadata.TestFileData,
    ) -> bool:
        try:
            modified = updater.update(test_file)
            test_path = pathlib.Path(test_file.test_path).as_posix()
            if modified:
                _log.info("Updated '%s'", test_path)
            else:
                _log.debug("No change needed for '%s'", test_path)
            return modified
        except ParseError as error:
            path = self._fs.relpath(_metadata_path(test_file),
                                    self._path_finder.path_from_web_tests())
            _log.error("Failed to parse '%s': %s", path, error)
            return False

    def stage(self, test_files: List[metadata.TestFileData]) -> None:
        unstaged_changes = {
            self._path_finder.path_from_chromium_base(path)
            for path in self.git.unstaged_changes()
        }
        # Filter out all-pass metadata files marked as "modified" that
        # already do not exist on disk or in the index. Otherwise, `git add`
        # will fail.
        paths = [
            path for path in map(_metadata_path, test_files)
            if path in unstaged_changes
        ]
        all_pass = len(test_files) - len(paths)
        if all_pass:
            _log.info(
                'Already deleted %s from the index '
                'for all-pass tests.',
                grammar.pluralize('metadata file', all_pass))
        self.git.add_list(paths)
        _log.info('Staged %s.', grammar.pluralize('metadata file', len(paths)))

    def _filter_unchanged_test_files(
            self,
            test_files: List[metadata.TestFileData],
    ) -> List[metadata.TestFileData]:
        files_changed_since_branch = {
            self._path_finder.path_from_chromium_base(path)
            for path in self.git.changed_files(diff_filter='AM')
        }
        return [
            test_file for test_file in test_files
            if self._fs.join(test_file.metadata_path, test_file.test_path) in
            files_changed_since_branch
        ]

    def _check_test_files(self, test_files: List[metadata.TestFileData]):
        if not test_files:
            raise UpdateAbortError('No metadata to update.')
        uncommitted_changes = {
            self._path_finder.path_from_chromium_base(path)
            for path in self.git.uncommitted_changes()
        }
        metadata_paths = set(map(_metadata_path, test_files))
        uncommitted_metadata = uncommitted_changes & metadata_paths
        if uncommitted_metadata:
            self._log_metadata_paths(
                'Aborting: there are uncommitted metadata files:',
                uncommitted_metadata,
                log=_log.error)
            raise UpdateAbortError('Please commit or reset these files '
                                   'to continue.')

    def _check_for_tests_absent_locally(self, manifests: ManifestMap,
                                        tests_from_builders: Set[str]):
        """Warn if some builds contain results for tests absent locally.

        In practice, most users will only update metadata for tests modified in
        the same change. For this reason, avoid aborting or prompting the user
        to continue, which could be too disruptive.
        """
        tests_present_locally = {
            test.id
            for manifest in manifests for test in _tests(manifest)
        }
        tests_absent_locally = tests_from_builders - tests_present_locally
        if tests_absent_locally:
            _log.warning(
                'Some builders have results for tests that are absent '
                'from your local checkout.')
            _log.warning('To update metadata for these tests, '
                         'please rebase-update on tip-of-tree.')
            _log.debug('Absent tests:')
            for test_id in sorted(tests_absent_locally):
                _log.debug('  %s', test_id)

    def _log_metadata_paths(self,
                            message: str,
                            paths: Collection[str],
                            log=_log.warning):
        log(message)
        web_tests_root = self._path_finder.web_tests_dir()
        for path in sorted(paths):
            rel_path = pathlib.Path(path).relative_to(web_tests_root)
            log('  %s', rel_path.as_posix())

    def _select_builds(self, options: optparse.Values) -> List[Build]:
        if options.builds:
            return options.builds
        if options.reports or (options.migrate and not options.trigger_jobs):
            return []
        # Only default to wptrunner try builders if neither builds nor local
        # reports are explicitly specified.
        builders = self._tool.builders.all_try_builder_names()
        return [
            Build(builder) for builder in builders
            if self._tool.builders.uses_wptrunner(builder)
        ]

    def _explicit_include_patterns(self, options: optparse.Values,
                                   args: List[str]) -> List[str]:
        patterns = list(args)
        if options.test_name_file:
            patterns.extend(
                testloader.read_include_from_file(options.test_name_file))
        return patterns

    def gather_reports(self, build_statuses: BuildStatuses,
                       report_paths: List[str]):
        """Lazily fetches wptreports.

        Arguments:
            build_statuses: Builds to fetch wptreport artifacts from. Builds
                without resolved IDs or non-failing builds are ignored.
            report_paths: Paths to wptreport files on disk.

        Yields:
            Seekable text buffers whose format is understood by the wptrunner
            metadata updater (e.g., newline-delimited JSON objects, each
            corresponding to a suite run). The buffers are not yielded in any
            particular order.

        Raises:
            OSError: If a local wptreport is not readable.
            UpdateAbortError: If one or more builds finished with
                `INFRA_FAILURE` and the user chose not to continue.
        """
        if GitCL.filter_infra_failed(build_statuses):
            if not self._tool.user.confirm(default=User.DEFAULT_NO):
                raise UpdateAbortError('Aborting update due to build(s) with '
                                       'infrastructure failures.')
        # TODO(crbug.com/1299650): Filter by failed builds again after the FYI
        # builders are green and no longer experimental.
        build_ids = [
            build.build_id for build, (_, status) in build_statuses.items()
            if build.build_id and (status == 'FAILURE' or self._tool.builders.
                                   has_experimental_steps(build.builder_name))
        ]
        urls = self._tool.results_fetcher.fetch_wpt_report_urls(*build_ids)
        if build_ids and not urls:
            _log.warning('All builds are missing report artifacts.')
        total_reports = len(report_paths) + len(urls)
        if not total_reports:
            _log.warning('No reports to process.')
        for i, contents in enumerate(
                self._fetch_report_contents(urls, report_paths)):
            _log.info('Processing wptrunner report (%d/%d)', i + 1,
                      total_reports)
            yield contents

    def _fetch_report_contents(
            self,
            urls: List[str],
            report_paths: List[str],
    ) -> Iterator[io.TextIOBase]:
        for path in report_paths:
            with self._fs.open_text_file_for_reading(path) as file_handle:
                yield file_handle
            _log.debug('Read report from %r', path)
        responses = self._map_for_io(self._tool.web.get_binary, urls)
        for url, response in zip(urls, responses):
            _log.debug('Fetched report from %r (size: %d bytes)', url,
                       len(response))
            yield io.StringIO(response.decode())

    @contextlib.contextmanager
    def _trace(self, message: str, *args) -> Iterator[None]:
        start = self._tool.time()
        try:
            yield
        finally:
            _log.debug(message + ' (took %.2fs)', *args,
                       self._tool.time() - start)

    def _append_reports(self, option: optparse.Option, _opt_str: str,
                        value: str, parser: optparse.OptionParser):
        reports = getattr(parser.values, option.dest, None) or []
        path = self._fs.expanduser(value)
        if self._fs.isfile(path):
            reports.append(path)
        elif self._fs.isdir(path):
            for filename in self._fs.listdir(path):
                child_path = self._fs.join(path, filename)
                if self._fs.isfile(child_path):
                    reports.append(child_path)
        else:
            raise optparse.OptionValueError(
                '%r is neither a regular file nor a directory' % value)
        setattr(parser.values, option.dest, reports)


class UpdateAbortError(Exception):
    """Exception raised when the update should be aborted."""


class TestInfo(NamedTuple):
    extra_bugs: Set[str]
    extra_comments: List[str]
    slow: bool = False


TestFileMap = Mapping[str, metadata.TestFileData]
# Note: Unlike `TestFileMap`, `TestInfoMap`'s values are set on a per-test ID
# basis, not a per-test file one.
TestInfoMap = Mapping[str, TestInfo]


class MetadataUpdater:
    # When using wptreports from builds, only update expectations for tests
    # that exhaust all retries. Unexpectedly passing tests and occasional
    # flakes/timeouts will not cause an update.
    min_results_for_update: ClassVar[int] = 4

    def __init__(
        self,
        test_files: TestFileMap,
        test_info: TestInfoMap,
        configs: TestConfigurations,
        primary_properties: Optional[List[str]] = None,
        dependent_properties: Optional[Mapping[str, str]] = None,
        overwrite_conditions: Literal['yes', 'no', 'fill'] = 'fill',
        disable_intermittent: Optional[str] = None,
        keep_statuses: bool = False,
        bug: Optional[int] = None,
        dry_run: bool = False,
    ):
        self._configs = configs
        self._test_info = test_info
        self._default_expected = _default_expected_by_type()
        self._primary_properties = primary_properties or [
            'debug',
            'product',
        ]
        self._dependent_properties = dependent_properties or {
            'product': ['os'],
            'os': ['port', 'flag_specific'],
        }
        self._overwrite_conditions = overwrite_conditions
        self._disable_intermittent = disable_intermittent
        self._keep_statuses = keep_statuses
        self._bug = bug
        self._dry_run = dry_run
        self._updater = metadata.ExpectedUpdater(test_files)

    @classmethod
    def from_manifests(cls,
                       manifests: ManifestMap,
                       configs: Dict[metadata.RunInfo, Port],
                       default_port: Port,
                       include: Optional[List[str]] = None,
                       exclude: Optional[List[str]] = None,
                       **options) -> 'MetadataUpdater':
        """Construct a metadata updater from WPT manifests.

        Arguments:
            include: A list of test patterns that are resolved into test IDs to
                update. The resolution works the same way as `wpt run`:
                  * Directories are expanded to include all children (e.g., `a/`
                    includes `a/b.html?c`).
                  * Test files are expanded to include all variants (e.g.,
                    `a.html` includes `a.html?b` and `a.html?c`).
        """
        # TODO(crbug.com/1299650): Validate the include list instead of silently
        # ignoring the bad test pattern.
        test_filter = testloader.TestFilter(manifests,
                                            include=include,
                                            exclude=exclude)
        test_files = {}
        test_info = collections.defaultdict(lambda: TestInfo(set(), []))
        for manifest, paths in manifests.items():
            # Unfortunately, test filtering is tightly coupled to the
            # `testloader.TestLoader` API. Monkey-patching here is the cleanest
            # way to filter tests to be updated without loading more tests than
            # are necessary.
            itertypes = manifest.itertypes
            try:
                manifest.itertypes = _compose(test_filter, manifest.itertypes)
                test_files.update(
                    metadata.create_test_tree(paths['metadata_path'],
                                              manifest))
                for test in _tests(manifest):
                    if getattr(test, 'timeout', None) == 'long':
                        test_info[test.id] = TestInfo(set(), [], slow=True)
            finally:
                manifest.itertypes = itertypes

        if options.pop('migrate', False):
            cls._load_expectations_for_migration(default_port, test_info)
        return cls(test_files, test_info, configs, **options)

    @staticmethod
    def _load_expectations_for_migration(default_port: Port,
                                         test_info: TestInfoMap):
        contents = default_port.all_expectations_dict()
        expectations = TestExpectations(default_port, contents)
        comments_buffer = []
        for path in contents:
            lines = [typ_types.Expectation()]
            lines.extend(expectations.get_updated_lines(path))
            for prev_line, line in zip(lines[:-1], lines[1:]):
                if line.test:
                    for wpt_dir, url_prefix in Port.WPT_DIRS.items():
                        base_test = default_port.lookup_virtual_test_base(
                            line.test) or line.test
                        wpt_dir += '/'
                        if base_test.startswith(wpt_dir):
                            test_id = base_test.replace(wpt_dir, url_prefix, 1)
                            # Ensure that `comment_buffers` is only added once
                            # for a block listing the same test multiple times.
                            if not set(comments_buffer) <= set(
                                    test_info[test_id].extra_comments):
                                test_info[test_id].extra_comments.extend(
                                    comments_buffer)
                            comment = _strip_comment(line.trailing_comments)
                            if comment:
                                test_info[test_id].extra_comments.append(
                                    comment)
                            test_info[test_id].extra_bugs.update(
                                line.reason.strip().split())
                else:
                    comment_or_empty = _strip_comment(line.to_string())
                    if not comment_or_empty or prev_line.test:
                        comments_buffer.clear()
                    if comment_or_empty:
                        comments_buffer.append(comment_or_empty)
                # TODO(crbug.com/1464393):
                #   * Import `[ Skip ]` expectations too.
                #   * Handle glob lines.

    def collect_results(self, reports: Iterable[io.TextIOBase]) -> Set[str]:
        """Parse and record test results."""
        test_ids = set()
        for report in reports:
            report = self._merge_retry_reports(map(json.loads, report))
            test_ids.update(result['test'] for result in report['results'])
            self._updater.update_from_wptreport_log(report)
        return test_ids

    def _merge_retry_reports(self, retry_reports):
        report, results_by_test = {}, collections.defaultdict(list)
        for retry_report in retry_reports:
            retry_report['run_info'] = config = self._reduce_config(
                retry_report['run_info'])
            if config != report.get('run_info', config):
                raise ValueError('run info values should be identical '
                                 'across retries')
            report.update(retry_report)
            for result in retry_report['results']:
                results_by_test[result['test']].append(result)
        report['results'] = []
        for test_id, results in results_by_test.items():
            if len(results) >= self.min_results_for_update:
                report['results'].extend(results)
        return report

    def _reduce_config(self, config: Dict[str, Any]) -> Dict[str, Any]:
        properties = frozenset(self._primary_properties).union(
            *self._dependent_properties.values())
        return {
            prop: value
            for prop, value in config.items() if prop in properties
        }

    def test_files_to_update(self) -> List[metadata.TestFileData]:
        test_files = {
            test_file
            for test_file in self._updater.id_test_map.values()
            if not test_file.test_path.endswith('__dir__')
        }
        return sorted(test_files, key=lambda test_file: test_file.test_path)

    def _fill_missing(self, test_file: metadata.TestFileData):
        """Fill in results for any missing port.

        A filled-in status is derived by evaluating the expectation conditions
        against each port's (i.e., "config's") properties. The statuses are
        then "replayed" as if the results had been from a wptreport.

        The purpose of result replay is to prevent the update backend from
        clobbering expectations for ports without results.

        Missing configs are only detected at the test level so that subtests can
        still be pruned.
        """
        expected = self._make_initialized_expectations(test_file)
        for test in expected.child_map.values():
            updated_configs = self._updated_configs(test_file, test.id)
            # Nothing to update. This commonly occurs when every port runs
            # expectedly. As an optimization, skip this file's update entirely
            # instead of replaying every result.
            if not updated_configs:
                continue
            enabled_configs = self._configs.enabled_configs(
                test, test_file.metadata_path)
            missing_configs = enabled_configs - updated_configs
            for config in missing_configs:
                self._updater.suite_start({'run_info': config.data})
                self._updater.test_start({'test': test.id})
                for subtest_id, subtest in test.subtests.items():
                    expected, *known_intermittent = self._eval_statuses(
                        subtest, config,
                        self._default_expected[test_file.item_type, True])
                    self._updater.test_status({
                        'test':
                        test.id,
                        'subtest':
                        subtest_id,
                        'status':
                        expected,
                        'known_intermittent':
                        known_intermittent,
                    })
                expected, *known_intermittent = self._eval_statuses(
                    test, config,
                    self._default_expected[test_file.item_type, False])
                self._updater.test_end({
                    'test':
                    test.id,
                    'status':
                    expected,
                    'known_intermittent':
                    known_intermittent,
                })

    def _make_initialized_expectations(
        self,
        test_file: metadata.TestFileData,
    ) -> manifestupdate.ExpectedManifest:
        """Make an expectation manifest with nodes for (sub)tests to update.

        The existing metadata file may not contain a (sub)test section if the
        (sub)test is new, or the test was formerly all-pass (i.e., the metadata
        file doesn't exist). This method ensures such (sub)test sections exist
        so that results for passing configurations can be filled in, which will
        correctly generate the necessary conditions.

        See also: crbug.com/1422011
        """
        # Use a `manifestupdate.ExpectedManifest` here instead of a
        # `manifestexpected.ExpectedManifest` because the former is
        # conditionally compiled, meaning keys can be evaluated against
        # different run info without needing to re-read the file.
        expected = test_file.expected(
            (self._primary_properties, self._dependent_properties),
            update_intermittent=(not self._disable_intermittent),
            remove_intermittent=(not self._keep_statuses))
        expected.set('type', test_file.item_type)
        for test_id in test_file.data:
            test = expected.get_test(test_id)
            if not test:
                test = manifestupdate.TestNode.create(test_id)
                expected.append(test)
            for subtest in test_file.data.get(test_id, []):
                if subtest != None:
                    # This creates the subtest node if it doesn't exist.
                    test.get_subtest(subtest)
        return expected

    def _eval_statuses(
            self,
            node: manifestupdate.TestNode,
            config: metadata.RunInfo,
            default_status: str,
    ) -> List[str]:
        statuses: Union[str, List[str]] = default_status
        with contextlib.suppress(KeyError):
            statuses = node.get('expected', config)
        return [statuses] if isinstance(statuses, str) else statuses

    def _updated_configs(
            self,
            test_file: metadata.TestFileData,
            test_id: str,
            subtest_id: Optional[str] = None,
    ) -> FrozenSet[metadata.RunInfo]:
        """Find configurations a (sub)test has results for so far."""
        subtests = test_file.data.get(test_id, {})
        subtest_data = subtests.get(subtest_id, [])
        return frozenset(run_info for _, run_info, _ in subtest_data)

    def update(self, test_file: metadata.TestFileData) -> bool:
        """Update and serialize the AST of a metadata file.

        Returns:
            Whether the test file's metadata was modified.
        """
        test_infos = [self._test_info[test_id] for test_id in test_file.tests]
        needs_migration = any(test_info.extra_bugs or test_info.extra_comments
                              for test_info in test_infos)
        if not test_file.data and not needs_migration:
            # Without test results and no TestExpectations to migrate, skip the
            # general update case, which is slow and unnecessary. See
            # crbug.com/1466002.
            return False
        if self._overwrite_conditions == 'fill':
            self._fill_missing(test_file)
        # Set `requires_update` to check for orphaned test sections.
        test_file.set_requires_update()
        expected = test_file.update(
            self._default_expected,
            (self._primary_properties, self._dependent_properties),
            full_update=(self._overwrite_conditions != 'no'),
            disable_intermittent=self._disable_intermittent,
            # `disable_intermittent` becomes a no-op when `update_intermittent`
            # is set, so always force them to be opposites. See:
            #   https://github.com/web-platform-tests/wpt/blob/merge_pr_35624/tools/wptrunner/wptrunner/manifestupdate.py#L422-L436
            update_intermittent=(not self._disable_intermittent),
            remove_intermittent=(not self._keep_statuses))
        if expected:
            self._disable_slow_timeouts(test_file, expected)
            self._remove_orphaned_tests(expected)
            self._migrate_expectations(expected)
            if self._bug:
                for test in expected.iterchildren():
                    if test.modified:
                        self._add_bug_urls(test, {f'crbug.com/{self._bug}'})

        modified = expected and expected.modified
        if modified:
            sort_metadata_ast(expected.node)
            if not self._dry_run:
                metadata.write_new_expected(test_file.metadata_path, expected)
        return modified

    def _disable_slow_timeouts(
            self,
            test_file: metadata.TestFileData,
            expected: manifestupdate.ExpectedManifest,
            message: str = 'times out even with extended deadline'):
        """Disable tests that are simultaneously slow and consistently time out.

        Such tests provide too little value for the large amount of time/compute
        that they consume.
        """
        for test in expected.iterchildren():
            if not self._test_info[test.id].slow:
                continue
            results = test_file.data.get(test.id, {}).get(None, [])
            statuses_by_config = collections.defaultdict(set)
            for prop, config, value in results:
                if prop == 'status':
                    status, known_intermittent = metadata.unpack_result(value)
                    statuses_by_config[config].add(status)
                    if known_intermittent:
                        statuses_by_config[config].update(known_intermittent)
            # Writing a conditional `disabled` value is complicated, so just
            # disable the test unconditionally if any configuration times out
            # consistently.
            if any(statuses == {'TIMEOUT'}
                   for statuses in statuses_by_config.values()):
                test.set('disabled', message)
                test.modified = True

    def _remove_orphaned_tests(self,
                               expected: manifestupdate.ExpectedManifest):
        # Iterate over a copy, since `test.remove()` mutates `expected`.
        for test in list(expected.iterchildren()):
            if test.id not in self._updater.id_test_map:
                test.remove()
                expected.modified = True

    def _migrate_expectations(self, expected: manifestupdate.ExpectedManifest):
        for test in expected.iterchildren():
            if test.is_empty:
                continue
            test_info = self._test_info[test.id]
            # Clear existing comments so that `--migrate` is idempotent.
            test.node.comments.clear()
            test.node.comments.extend(
                ('comment', comment) for comment in test_info.extra_comments)
            self._add_bug_urls(test, test_info.extra_bugs, overwrite=False)
            expected.modified = True

    def _add_bug_urls(self,
                      test: manifestupdate.TestNode,
                      new_bugs: FrozenSet[str],
                      overwrite: bool = True,
                      key: str = 'bug'):
        bugs = []
        with contextlib.suppress(KeyError):
            bugs = test.get(key)
            if isinstance(bugs, str):
                bugs = [bugs]
        if overwrite:
            bugs.clear()
        bugs = sorted({*bugs, *new_bugs})
        test.clear(key)
        if bugs:
            test.set(key, bugs[0] if len(bugs) == 1 else bugs)


def _strip_comment(maybe_comment: str) -> str:
    maybe_comment = maybe_comment.lstrip()
    if maybe_comment.startswith('#') and not any(
            maybe_comment.startswith(prefix) for prefix in SPECIAL_PREFIXES):
        return maybe_comment[1:]
    return ''


def sort_metadata_ast(node: wptnode.DataNode) -> None:
    """Sort the metadata abstract syntax tree to create a stable rendering.

    Since keys/sections are identified by unique names within their block, their
    ordering within the file do not matter. Sorting avoids creating spurious
    diffs after serialization.

    Note:
        This mutates the given node. Create a copy with `node.copy()` if you
        wish to keep the original.
    """
    assert all(
        isinstance(child, (wptnode.DataNode, wptnode.KeyValueNode))
        for child in node.children), node
    # Put keys first, then child sections. Keys and child sections are sorted
    # alphabetically within their respective groups.
    node.children.sort(key=lambda child: (bool(
        isinstance(child, wptnode.DataNode)), child.data or ''))
    for child in node.children:
        if isinstance(child, wptnode.DataNode):
            sort_metadata_ast(child)


def _compose(f, g):
    return lambda *args, **kwargs: f(g(*args, **kwargs))


def load_and_update_manifests(finder: path_finder.PathFinder) -> ManifestMap:
    """Load and update WPT manifests on disk by scanning the test root.

    Arguments:
        finder: Path finder for constructing test paths (test root, metadata
            root, and manifest path).

    See Also:
        https://github.com/web-platform-tests/wpt/blob/merge_pr_35574/tools/wptrunner/wptrunner/testloader.py#L171-L199
    """
    test_paths = {}
    for rel_path_to_wpt_root, url_base in Port.WPT_DIRS.items():
        wpt_root = finder.path_from_web_tests(rel_path_to_wpt_root)
        test_paths[url_base] = {
            'tests_path':
            wpt_root,
            'metadata_path':
            wpt_root,
            'manifest_path':
            finder.path_from_web_tests(rel_path_to_wpt_root, 'MANIFEST.json'),
        }
    return testloader.ManifestLoader(test_paths,
                                     force_manifest_update=True).load()


def _tests(
        manifest: wptmanifest.Manifest) -> Iterator[wptmanifest.ManifestItem]:
    item_types = frozenset(wptmanifest.item_classes) - {
        'support',
        'manual',
        'conformancechecker',
    }
    for _, _, tests in manifest.itertypes(*item_types):
        yield from tests


def _default_expected_by_type():
    default_expected_by_type = {}
    for test_type, test_cls in wpttest.manifest_test_cls.items():
        if test_cls.result_cls:
            expected = test_cls.result_cls.default_expected
            default_expected_by_type[test_type, False] = expected
        if test_cls.subtest_result_cls:
            expected = test_cls.subtest_result_cls.default_expected
            default_expected_by_type[test_type, True] = expected
    return default_expected_by_type


def _parse_build_specifiers(option: optparse.Option, _opt_str: str, value: str,
                            parser: optparse.OptionParser):
    builds = getattr(parser.values, option.dest, None) or []
    specifier_pattern = re.compile(r'(ci/|try/)?([^:]+)(:\d+(-\d+)?)?')
    for specifier in value.split(','):
        specifier_match = specifier_pattern.fullmatch(specifier)
        if not specifier_match:
            raise optparse.OptionValueError('invalid build specifier %r' %
                                            specifier)
        bucket, builder, build_range, maybe_end = specifier_match.groups()
        if build_range:
            start = int(build_range[1:].split('-')[0])
            end = int(maybe_end[1:]) if maybe_end else start
            build_numbers = range(start, end + 1)
            if not build_numbers:
                raise optparse.OptionValueError(
                    'start build number must precede end for %r' % specifier)
        else:
            build_numbers = [None]
        bucket = bucket[:-1] if bucket else 'try'
        for build_number in build_numbers:
            builds.append(Build(builder, build_number, bucket=bucket))
    setattr(parser.values, option.dest, builds)


def _coerce_bug_number(option: optparse.Option, _opt_str: str, value: str,
                       parser: optparse.OptionParser):
    bug_match = BUG_PATTERN.fullmatch(value)
    if not bug_match:
        raise optparse.OptionValueError('invalid bug number or URL %r' % value)
    setattr(parser.values, option.dest, int(bug_match['bug']))


def _metadata_path(test_file: metadata.TestFileData) -> str:
    return metadata.expected_path(test_file.metadata_path, test_file.test_path)
