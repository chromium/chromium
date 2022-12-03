# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update WPT metadata from builder results."""

from concurrent.futures import ThreadPoolExecutor
import contextlib
import io
import json
import logging
import pathlib
import optparse
import re
from typing import (
    Collection,
    Iterable,
    Iterator,
    List,
    Literal,
    Mapping,
    Optional,
    Set,
    TypedDict,
)

from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.net.git_cl import BuildStatuses, GitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.tool import grammar
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import Command
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL
from blinkpy.web_tests.port.base import Port

path_finder.bootstrap_wpt_imports()
from manifest import manifest as wptmanifest
from wptrunner import metadata, testloader, wpttest
from wptrunner.wptmanifest.backends import conditional

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
                 git_cl: Optional[GitCL] = None,
                 max_io_workers: int = 4):
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
                metavar='{yes,no,auto,fill}',
                choices=['yes', 'no', 'auto', 'fill'],
                help=(
                    'Specify whether to reformat conditional statuses. '
                    '"auto" will reformat conditions if every platform '
                    'a test is enabled for has results. '
                    '"fill" will reformat conditions using existing statuses '
                    'for platforms without results. (default: "fill")')),
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
        self._io_pool = ThreadPoolExecutor(max_workers=max_io_workers)
        self._path_finder = path_finder.PathFinder(self._tool.filesystem)
        self.git = self._tool.git(path=self._path_finder.web_tests_dir())
        self.git_cl = git_cl or GitCL(self._tool)

    @property
    def _fs(self):
        return self._tool.filesystem

    def execute(self, options: optparse.Values, args: List[str],
                _tool: Host) -> Optional[int]:
        build_resolver = BuildResolver(
            self.git_cl,
            can_trigger_jobs=(options.trigger_jobs and not options.dry_run))
        manifests = load_and_update_manifests(self._path_finder)
        updater = MetadataUpdater.from_manifests(
            manifests,
            self._explicit_include_patterns(options, args),
            options.exclude,
            overwrite_conditions=options.overwrite_conditions,
            disable_intermittent=options.disable_intermittent,
            keep_statuses=options.keep_statuses,
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
                stack.enter_context(self._io_pool)
                tests_from_builders = updater.collect_results(
                    self.gather_reports(build_statuses, options.reports or []))
                if not options.only_changed_tests:
                    self._check_for_tests_absent_locally(
                        manifests, tests_from_builders)
                self.remove_orphaned_metadata(manifests,
                                              dry_run=options.dry_run)
                self.update_and_stage(updater,
                                      test_files,
                                      dry_run=options.dry_run)
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

    def update_and_stage(self,
                         updater: 'MetadataUpdater',
                         test_files: List[metadata.TestFileData],
                         dry_run: bool = False):
        test_files_to_stage = []
        update_results = zip(test_files,
                             self._io_pool.map(updater.update, test_files))
        _log.info('Updating expectations for up to %s.',
                  grammar.pluralize('test file', len(test_files)))
        for i, (test_file, modified) in enumerate(update_results):
            test_path = pathlib.Path(test_file.test_path).as_posix()
            if modified:
                _log.info("Updated '%s'", test_path)
                test_files_to_stage.append(test_file)
            else:
                _log.debug("No change needed for '%s'", test_path)

        if not dry_run:
            unstaged_changes = {
                self._path_finder.path_from_chromium_base(path)
                for path in self.git.unstaged_changes()
            }
            # Filter out all-pass metadata files marked as "modified" that
            # already do not exist on disk or in the index. Otherwise, `git add`
            # will fail.
            paths = [
                path for path in self._metadata_paths(test_files_to_stage)
                if path in unstaged_changes
            ]
            all_pass = len(test_files_to_stage) - len(paths)
            if all_pass:
                _log.info(
                    'Already deleted %s from the index '
                    'for all-pass tests.',
                    grammar.pluralize('metadata file', all_pass))
            self.git.add_list(paths)
            _log.info('Staged %s.',
                      grammar.pluralize('metadata file', len(paths)))

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
        metadata_paths = set(self._metadata_paths(test_files))
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
        tests_present_locally = set()
        item_types = [
            item_type for item_type in wptmanifest.item_classes
            if item_type != 'support'
        ]
        for manifest in manifests:
            tests_present_locally.update(
                test.id for _, _, tests in manifest.itertypes(*item_types)
                for test in tests)
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

    def _metadata_paths(
            self,
            test_files: List[metadata.TestFileData],
    ) -> List[str]:
        return [
            metadata.expected_path(test_file.metadata_path,
                                   test_file.test_path)
            for test_file in test_files
        ]

    def _select_builds(self, options: optparse.Values) -> List[Build]:
        if options.builds:
            return options.builds
        if options.reports:
            return []
        # Only default to wptrunner try builders if neither builds nor local
        # reports are explicitly specified.
        builders = self._tool.builders.all_try_builder_names()
        return [
            Build(builder) for builder in builders
            if self._tool.builders.is_wpt_builder(builder)
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
        """
        # TODO(crbug.com/1299650): Filter by failed builds again after the FYI
        # builders are green and no longer experimental.
        build_ids = [
            build.build_id for build, (_, status) in build_statuses.items()
            if build.build_id and (status == 'FAILURE' or status == 'SUCCESS')
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
        responses = self._io_pool.map(self._tool.web.get_binary, urls)
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


TestFileMap = Mapping[str, metadata.TestFileData]


class MetadataUpdater:
    def __init__(
            self,
            test_files: TestFileMap,
            primary_properties: Optional[List[str]] = None,
            dependent_properties: Optional[Mapping[str, str]] = None,
            overwrite_conditions: Literal['yes', 'no', 'fill',
                                          'auto'] = 'fill',
            disable_intermittent: Optional[str] = None,
            keep_statuses: bool = False,
            bug: Optional[int] = None,
            dry_run: bool = False,
    ):
        self._test_files = test_files
        self._default_expected = _default_expected_by_type()
        self._primary_properties = primary_properties or [
            'debug',
            'os',
            'processor',
            'product',
            'flag_specific',
        ]
        self._dependent_properties = dependent_properties or {
            'os': ['version'],
            'processor': ['bits'],
        }
        self._overwrite_conditions = overwrite_conditions
        self._disable_intermittent = disable_intermittent
        self._keep_statuses = keep_statuses
        self._bug = bug
        self._dry_run = dry_run

    @classmethod
    def from_manifests(cls,
                       manifests: ManifestMap,
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
            finally:
                manifest.itertypes = itertypes
        return cls(test_files, **options)

    def collect_results(self, reports: Iterable[io.TextIOBase]) -> Set[str]:
        """Parse and record test results."""
        updater = metadata.ExpectedUpdater(self._test_files)
        test_ids = set()
        for report in reports:
            for retry_report in map(json.loads, report):
                test_ids.update(result['test']
                                for result in retry_report['results']
                                if 'test' in result)
                updater.update_from_wptreport_log(retry_report)
        return test_ids

    def test_files_to_update(self) -> List[metadata.TestFileData]:
        test_files = {
            test_file
            for test_file in self._test_files.values()
            if not test_file.test_path.endswith('__dir__')
        }
        return sorted(test_files, key=lambda test_file: test_file.test_path)

    def _determine_if_full_update(self,
                                  test_file: metadata.TestFileData) -> bool:
        if self._overwrite_conditions == 'fill':
            # TODO(crbug.com/1299650): To be implemented.
            return True
        elif self._overwrite_conditions == 'auto':
            # TODO(crbug.com/1299650): To be implemented.
            return False
        elif self._overwrite_conditions == 'yes':
            return True
        else:
            return False

    def update(self, test_file: metadata.TestFileData) -> bool:
        """Update and serialize the AST of a metadata file.

        Returns:
            Whether the test file's metadata was modified.
        """
        expected = test_file.update(
            self._default_expected,
            (self._primary_properties, self._dependent_properties),
            full_update=self._determine_if_full_update(test_file),
            disable_intermittent=self._disable_intermittent,
            # `disable_intermittent` becomes a no-op when `update_intermittent`
            # is set, so always force them to be opposites. See:
            #   https://github.com/web-platform-tests/wpt/blob/merge_pr_35624/tools/wptrunner/wptrunner/manifestupdate.py#L422-L436
            update_intermittent=(not self._disable_intermittent),
            remove_intermittent=(not self._keep_statuses))

        modified = expected and expected.modified
        if modified:
            if self._bug:
                self._add_bug_url(expected)
            if not self._dry_run:
                metadata.write_new_expected(test_file.metadata_path, expected)
        return modified

    def _add_bug_url(self, expected: conditional.ManifestItem):
        for test_id_section in expected.iterchildren():
            if test_id_section.modified:
                test_id_section.set('bug', 'crbug.com/%d' % self._bug)


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
    bug_match = re.fullmatch(r'(crbug(\.com)?/)?(?P<bug>\d+)', value)
    if not bug_match:
        raise optparse.OptionValueError('invalid bug number or URL %r' % value)
    setattr(parser.values, option.dest, int(bug_match['bug']))
