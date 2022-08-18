# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update WPT metadata from builder results."""

from concurrent.futures import Executor, ThreadPoolExecutor
import contextlib
import json
import logging
import optparse
import re
from typing import Iterator, List, Optional

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

_log = logging.getLogger(__name__)


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
                metavar='<builder>[:<buildnum>],...',
                action='callback',
                callback=_parse_build_specifiers,
                type='string',
                help=('Comma-separated list of builds to download results for '
                      '(e.g., "Linux Tests:100,linux-rel"). '
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
                help=('Disable tests that have inconsistent results '
                      'with the given reason.')),
            optparse.make_option('--keep-statuses',
                                 action='store_true',
                                 help='Keep all existing statuses.'),
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
        # This tool's performance bottleneck is the network I/O to ResultDB,
        # which downloads something in the range of:
        #   10 try builders * (30 shards / builder) * (10 MB / shard) = 3 GB
        # Informational messages (e.g., stack traces) and subtest results make
        # up the bulk of this data.
        #
        # A process pool would allow for physical parallelism (i.e., avoid
        # Python's Global Interpreter Lock) but incur the cost of IPC.
        #
        # TODO(crbug.com/1299650): Upload minimal wptreports on the builder side
        # to reduce download overhead. Expected results should be filtered out
        # (similar to '{full,failing}_results.json') and messages removed.
        self._io_pool = ThreadPoolExecutor(max_workers=max_io_workers)
        self.git_cl = git_cl or GitCL(self._tool)

    def execute(self, options: optparse.Values, args: List[str],
                _tool: Host) -> Optional[int]:
        build_resolver = BuildResolver(
            self._tool.builders,
            self.git_cl,
            can_trigger_jobs=(options.trigger_jobs and not options.dry_run))
        try:
            build_statuses = build_resolver.resolve_builds(
                self._select_builds(options), options.patchset)
            with contextlib.ExitStack() as stack:
                stack.enter_context(self._trace('Updated metadata'))
                stack.enter_context(self._io_pool)
                for report in self.gather_reports(build_statuses,
                                                  options.reports or []):
                    results, run_info = report['results'], report['run_info']
                    _log.info(
                        '%s (product: %s, os: %s, os_version: %s, '
                        'cpu: %s-%s, flag_specific: %s)',
                        grammar.pluralize('test', len(results)),
                        run_info.get('product', '?'), run_info.get('os', '?'),
                        run_info.get('version', '?'),
                        run_info.get('processor', '?'),
                        str(run_info.get('bits', '?')),
                        run_info.get('flag_specific', '-'))
        except RPCError as error:
            _log.error('%s', error)
            _log.error('Request payload: %s',
                       json.dumps(error.request_body, indent=2))
            return 1
        except json.JSONDecodeError as error:
            _log.error('Unable to parse wptreport: %s', str(error))
            return 1
        except (UnresolvedBuildException, OSError) as error:
            _log.error('%s', error)
            return 1

    def _select_builds(self, options: optparse.Values) -> List[Build]:
        if options.builds:
            return options.builds
        if options.reports:
            return []
        # Only default to try builders if neither builds nor local reports are
        # explicitly specified.
        builders = self._tool.builders.all_try_builder_names()
        return [Build(builder) for builder in builders]

    def gather_reports(self, build_statuses: BuildStatuses,
                       report_paths: List[str]):
        """Lazily fetches and parses wptreports.

        Arguments:
            build_statuses: Builds to fetch wptreport artifacts from. Builds
                without resolved IDs or non-failing builds are ignored.
            report_paths: Paths to wptreport files on disk.

        Yields:
            JSON objects corresponding to wptrunner suite runs. The objects are
            not ordered in any particular way.

        Raises:
            OSError: If a local wptreport is not readable.
            json.JSONDecodeError: If any wptreport line is not valid JSON.
        """
        build_ids = [
            build.build_id for build, (_, status) in build_statuses.items()
            if build.build_id and status == 'FAILURE'
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
            yield from _parse_report_contents(contents)

    def _fetch_report_contents(self, urls: List[str],
                               report_paths: List[str]) -> Iterator[str]:
        for path in report_paths:
            yield self._tool.filesystem.read_text_file(path)
            _log.debug('Read report from %r', path)
        responses = self._io_pool.map(self._tool.web.get_binary, urls)
        for url, response in zip(urls, responses):
            _log.debug('Fetched report from %r (size: %d bytes)', url,
                       len(response))
            yield response.decode()

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
        fs = self._tool.filesystem
        path = fs.expanduser(value)
        if fs.isfile(path):
            reports.append(path)
        elif fs.isdir(path):
            for filename in fs.listdir(path):
                child_path = fs.join(path, filename)
                if fs.isfile(child_path):
                    reports.append(child_path)
        else:
            raise optparse.OptionValueError(
                '%r is neither a regular file nor a directory' % value)
        setattr(parser.values, option.dest, reports)


def _parse_report_contents(contents: str):
    try:
        yield from map(json.loads, contents.splitlines())
    except json.JSONDecodeError:
        # Allow a single object written across multiple lines.
        yield json.loads(contents)


def _parse_build_specifiers(option: optparse.Option, _opt_str: str, value: str,
                            parser: optparse.OptionParser):
    builds = getattr(parser.values, option.dest, None) or []
    for build_specifier in value.split(','):
        builder, sep, maybe_num = build_specifier.partition(':')
        try:
            build_num = int(maybe_num) if sep else None
            builds.append(Build(builder, build_num))
        except ValueError:
            raise optparse.OptionValueError('invalid build number for %r' %
                                            builder)
    setattr(parser.values, option.dest, builds)


def _coerce_bug_number(option: optparse.Option, _opt_str: str, value: str,
                       parser: optparse.OptionParser):
    bug_match = re.fullmatch(r'(crbug(\.com)?/)?(?P<bug>\d+)', value)
    if not bug_match:
        raise optparse.OptionValueError('invalid bug number or URL %r' % value)
    setattr(parser.values, option.dest, int(bug_match['bug']))
