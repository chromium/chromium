# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update WPT metadata from builder results."""

import json
import logging
import optparse
import re
from typing import List, Optional

from blinkpy.common.host import Host
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import Command, check_file_option
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL

_log = logging.getLogger(__name__)


class UpdateMetadata(Command):
    name = 'update-metadata'
    show_in_main_help = False  # TODO(crbug.com/1299650): To be switched on.
    help_text = 'Update WPT metadata from builder results.'
    argument_names = '[<test>,...]'
    long_help = __doc__

    def __init__(self, tool: Host, git_cl: Optional[GitCL] = None):
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
                '--run-log',
                dest='run_logs',
                action='callback',
                callback=self._append_run_logs,
                type='string',
                help=('Path to a wptrunner log file (or directory of '
                      'log files) to use in the update. Logs may use either '
                      'the raw mozlog format or the wptreport format. '
                      'May specify multiple times.')),
            RebaselineCL.patchset_option,
            RebaselineCL.test_name_file_option,
            RebaselineCL.only_changed_tests_option,
            RebaselineCL.no_trigger_jobs_option,
            RebaselineCL.dry_run_option,
        ])
        self._tool = tool
        self.git_cl = git_cl or GitCL(self._tool)

    def execute(self, options: optparse.Values, args: List[str],
                _tool: Host) -> Optional[int]:
        if not options.builds:
            builders = self._tool.builders.all_try_builder_names()
            options.builds = [Build(builder) for builder in builders]
        build_resolver = BuildResolver(
            self._tool.builders,
            self.git_cl,
            can_trigger_jobs=(options.trigger_jobs and not options.dry_run))
        try:
            build_statuses = build_resolver.resolve_builds(
                options.builds, options.patchset)
        except RPCError as error:
            _log.error('%s', error)
            _log.error('Request payload: %s',
                       json.dumps(error.request_body, indent=2))
            return 1
        except UnresolvedBuildException as error:
            _log.error('%s', error)
            return 1

    def _append_run_logs(self, option: optparse.Option, _opt_str: str,
                         value: str, parser: optparse.OptionParser):
        run_logs = getattr(parser.values, option.dest, None) or []
        fs = self._tool.filesystem
        path = fs.expanduser(value)
        if fs.isfile(path):
            run_logs.append(path)
        elif fs.isdir(path):
            for filename in fs.listdir(path):
                child_path = fs.join(path, filename)
                if fs.isfile(child_path):
                    run_logs.append(child_path)
        else:
            raise optparse.OptionValueError(
                '%r is neither a regular file nor a directory' % value)
        setattr(parser.values, option.dest, run_logs)


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
