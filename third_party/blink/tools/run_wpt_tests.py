#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests for Chromium-related products."""

import argparse
import contextlib
import functools
import glob
import json
import logging
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import warnings
from typing import List, Optional, Tuple

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor
from blinkpy.web_tests.port.android import (
    ANDROID_WEBVIEW,
    CHROME_ANDROID,
)
from blinkpy.web_tests.port.base import ARTIFACTS_SUB_DIR, Port

path_finder.add_testing_dir_to_sys_path()
path_finder.add_build_android_to_sys_path()
path_finder.add_build_ios_to_sys_path()
path_finder.bootstrap_wpt_imports()

import mozlog
from scripts import common
from wptrunner import wptcommandline, wptlogging

logger = logging.getLogger('run_wpt_tests')

UPSTREAM_GIT_URL = 'https://github.com/web-platform-tests/wpt.git'

try:
    # This import adds `devil` to `sys.path`.
    import devil_chromium
    from devil import devil_env
    from devil.utils.parallelizer import SyncParallelizer
    from devil.android import apk_helper
    from devil.android import device_utils
    from devil.android.device_errors import CommandFailedError
    from devil.android.tools import webview_app
    from pylib.local.emulator import avd
    _ANDROID_ENABLED = True
except ImportError:
    _ANDROID_ENABLED = False

try:
    import xcode_util as xcode
    _IOS_ENABLED = True
except ImportError:
    _IOS_ENABLED = False


def _make_log_enabled_grouping_formatter():
    # Make a grouping log formatter that shows regular log messages:
    #   WARNING Unsupported test type wdspec for product content_shell
    #
    # Activating logs dynamically with:
    #   StructuredLogger.send_message('show_logs', 'on')
    # appears buggy. This factory exists as a workaround.
    grouping_formatter = mozlog.formatters.GroupingFormatter()
    grouping_formatter.message_handler.handle_message('show_logs', 'on')
    return grouping_formatter


mozlog.commandline.log_formatters['grouped'] = (
    _make_log_enabled_grouping_formatter,
    mozlog.commandline.log_formatters['grouped'][1],
)


class StructuredLogAdapter(logging.Handler):
    def __init__(self, logger, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logger
        self._fallback_handler = logging.StreamHandler()
        self._fallback_handler.setFormatter(
            logging.Formatter('%(name)s %(levelname)s %(message)s'))

    def emit(self, record):
        log = getattr(self._logger, record.levelname.lower(),
                      self._logger.debug)
        try:
            log(record.getMessage(), component=record.name)
        except mozlog.structuredlog.LoggerShutdownError:
            self._fallback_handler.emit(record)


PARAMETER_DENYLIST = {
    # Parameters specific to non-Chromium vendors.
    '--prefs-root',
    '--preload-browser',
    '--no-preload-browser',
    '--disable-e10s',
    '--enable-fission',
    '--no-enable-fission',
    '--stackfix-dir',
    '--specialpowers-path',
    '--setpref',
    '--leak-check',
    '--no-leak-check',
    '--stylo-threads',
    '--reftest-screenshot',
    '--reftest-internal',
    '--reftest-external',
    '--chaos',
    '--user-stylesheet',
    '--sauce-browser',
    '--sauce-platform',
    '--sauce-version',
    '--sauce-build',
    '--sauce-tags',
    '--sauce-tunnel-id',
    '--sauce-user',
    '--sauce-key',
    '--sauce-connect-binary',
    '--sauce-init-timeout',
    '--sauce-connect-arg',
    '--github-checks-text-file',
    '--webkit-port',
    '--kill-safari',
}
if not _ANDROID_ENABLED:
    PARAMETER_DENYLIST.update({
        '--adb-binary',
        '--package-name',
        '--keep-app-data-directory',
        '--device-serial',
    })


class WPTAdapter:
    def __init__(self):
        self.host = Host()
        self.fs = self.host.filesystem
        self.path_finder = PathFinder(self.fs)
        self.port = self.host.port_factory.get()
        self.port.set_option_default('use_xvfb', True)
        self._shard_index = _parse_environ_int('GTEST_SHARD_INDEX')
        self._total_shards = _parse_environ_int('GTEST_TOTAL_SHARDS')

    def parse_arguments(
            self,
            argv: Optional[List[str]] = None,
    ) -> argparse.Namespace:
        wptrunner_parser = wptcommandline.create_parser(
            product_choices=sorted(_product_registry, key=len))
        # Not ideal, but this creates a wptrunner-compliant CLI without showing
        # many irrelevant parameters.
        for group in wptrunner_parser._action_groups:
            group.conflict_handler = 'resolve'
        for action in wptrunner_parser._actions:
            if frozenset(action.option_strings) & PARAMETER_DENYLIST:
                action.help = argparse.SUPPRESS
        parser = argparse.ArgumentParser(
            description=__doc__,
            parents=[
                wptrunner_parser,
                # Put the overridden parameters in a separate parent instead of on
                # this parser so that arguments of groups with the same title are
                # merged together.
                self._make_override_parser(),
            ],
            conflict_handler='resolve',
            epilog=('All unrecognized arguments are passed through to the '
                    'browser binary.'),
        )
        options, unknown_args = parser.parse_known_intermixed_args(argv)
        try:
            self._check_and_update_options(options)
        except ValueError as error:
            parser.error(error)
        options.binary_args.extend(unknown_args)
        return options

    def _make_override_parser(self) -> argparse.ArgumentParser:
        """Create a parser that overrides existing wptrunner options.

        `argument.ArgumentParser` can extend other parsers and override their
        options, with the caveat that the child parser only inherits options
        that the parent had at the time of the child's initialization.

        See Also:
            https://docs.python.org/3/library/argparse.html#parents
        """
        parser = argparse.ArgumentParser()
        # Absorb options that are part of the isolated script convention, but
        # should have no effect.
        parser.add_argument('--isolated-outdir', help=argparse.SUPPRESS)
        parser.add_argument('--isolated-script-test-also-run-disabled-tests',
                            action='store_true',
                            help=argparse.SUPPRESS)
        parser.add_argument('--isolated-script-test-chartjson-output',
                            help=argparse.SUPPRESS)
        parser.add_argument('--isolated-script-test-perf-output',
                            help=argparse.SUPPRESS)
        parser.add_argument('--script-type', help=argparse.SUPPRESS)
        # `Port.setup_test_run` will always start Xvfb on Linux.
        parser.add_argument('--xvfb',
                            action='store_true',
                            default=True,
                            help=argparse.SUPPRESS)
        parser.add_argument(
            '-j',
            '--processes',
            '--child-processes',
            type=lambda processes: max(0, int(processes)),
            help=('Number of drivers to start in parallel. (For Android, '
                  'this number is the number of emulators started.) '
                  'The actual number of devices tested may be greater '
                  'if physical devices are available.)'))
        self.add_output_arguments(parser)
        self.add_binary_arguments(parser)
        self.add_test_arguments(parser)
        self.add_debugging_arguments(parser)
        self.add_configuration_arguments(parser)
        if _ANDROID_ENABLED:
            self.add_android_arguments(parser)
        else:
            warnings.warn('Android tools not found')
        if _IOS_ENABLED:
            self.add_ios_arguments(parser)
        else:
            warnings.warn('iOS tools not found')
        # Nightly installation is not supported, so just add defaults.
        parser.set_defaults(
            prompt=False,
            install_browser=False,
            install_webdriver=False,
            channel='nightly',
            affected=None,
        )
        return parser

    def _check_and_update_options(self, options):
        """Postprocess options, some of which can depend on each other."""
        # Set up logging as early as possible.
        self._check_and_update_output_options(options)
        self._check_and_update_upstream_options(options)
        self._check_and_update_config_options(options)
        self._check_and_update_sharding_options(options)
        # TODO(crbug/1316055): Enable tombstone with '--stackwalk-binary' and
        # '--symbols-path'.
        options.exclude = options.exclude or []
        options.exclude.extend([
            # Exclude webdriver tests for now. The CI runs them separately.
            'webdriver',
            'infrastructure/webdriver',
        ])
        options.pause_after_test = False
        options.no_capture_stdio = True
        options.manifest_download = False

    def _check_and_update_output_options(self, options):
        if options.verbose >= 1:
            options.log_mach = '-'
            options.log_mach_level = 'info'
            options.log_mach_verbose = True
        if options.verbose >= 2:
            options.log_mach_level = 'debug'
        if options.verbose >= 3:
            options.webdriver_args.extend([
                '--verbose',
                '--log-path=-',
            ])

        output_dir = self.path_from_output_dir(options.target)
        if not self.fs.isdir(output_dir):
            raise ValueError("'--target' must be a directory under //out")
        self.port.set_option_default('target', options.target)
        if options.log_chromium == '' or options.show_results:
            options.log_chromium = self.fs.join(output_dir, 'results.json')
        if options.log_wptreport == '':
            if self._shard_index is None:
                filename = 'wpt_reports_%s.json' % options.product
            else:
                filename = 'wpt_reports_%s_%02d.json' % (options.product,
                                                         self._shard_index)
            options.log_wptreport = self.fs.join(output_dir, filename)
        for log_type in ('chromium', 'wptreport'):
            dest = 'log_%s' % log_type
            filename = getattr(options, dest)
            if filename:
                filename = self.fs.abspath(filename)
                setattr(options, dest, [mozlog.commandline.log_file(filename)])
        options.log = wptlogging.setup(dict(vars(options)),
                                       {'grouped': sys.stdout})
        logging.root.handlers.clear()
        logging.root.addHandler(StructuredLogAdapter(options.log))

    def _check_and_update_config_options(self, options: argparse.Namespace):
        options.webdriver_args.extend([
            '--enable-chrome-logs',
        ])
        options.binary_args.extend([
            '--host-resolver-rules='
            'MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1',
            '--enable-experimental-web-platform-features',
            '--enable-blink-features=MojoJS,MojoJSTest',
            '--enable-blink-test-features',
            '--disable-field-trial-config',
            '--enable-features='
            'DownloadService<DownloadServiceStudy',
            '--force-fieldtrials=DownloadServiceStudy/Enabled',
            '--force-fieldtrial-params='
            'DownloadServiceStudy.Enabled:start_up_delay_ms/0',
        ])
        if options.retry_unexpected is None:
            if _has_explicit_tests(options):
                options.retry_unexpected = 0
                logger.warning('Tests explicitly specified; disabling retries')
            else:
                options.retry_unexpected = 3
                logger.warning(
                    'Tests not explicitly specified; '
                    'using %d retries', options.retry_unexpected)
        if not options.mojojs_path:
            options.mojojs_path = self.path_from_output_dir(
                options.target, 'gen')
        if not options.config and options.run_wpt_internal:
            options.config = self.path_finder.path_from_web_tests(
                'wptrunner.blink.ini')
        if options.flag_specific:
            # Enable adding smoke tests later.
            self.port.set_option_default('flag_specific',
                                         options.flag_specific)
            configs = self.port.flag_specific_configs()
            args, _ = configs[options.flag_specific]
            logger.info('Running with flag-specific arguments: "%s"',
                        ' '.join(args))
            options.binary_args.extend(args)

        if self.port.default_smoke_test_only():
            smoke_file_short_path = self.fs.relpath(
                self.port.path_to_smoke_tests_file(),
                self.port.web_tests_dir())
            if not _has_explicit_tests(options):
                self._load_smoke_tests(options)
                logger.info(
                    'Tests not explicitly specified; '
                    'running tests from %s', smoke_file_short_path)
            else:
                logger.warning(
                    'Tests explicitly specified; '
                    'not running tests from %s', smoke_file_short_path)

    def _load_smoke_tests(self, options: argparse.Namespace):
        """Read the smoke tests file and append its tests to the test list.

        This method handles smoke test files inherited from `run_web_tests.py`
        differently from the native `wpt run --include-file` parameter.
        Specifically, tests are assumed to be relative to `web_tests/`, so a
        line without a recognized `external/wpt/` or `wpt_internal/` prefix is
        assumed to be a legacy layout test that is excluded.
        """
        smoke_file_path = self.port.path_to_smoke_tests_file()
        options.include = options.include or []
        with self.fs.open_text_file_for_reading(smoke_file_path) as smoke_file:
            for line in smoke_file:
                test, _, _ = line.partition('#')
                test = test.strip()
                for wpt_dir, url_prefix in Port.WPT_DIRS.items():
                    if not wpt_dir.endswith('/'):
                        wpt_dir += '/'
                    if test.startswith(wpt_dir):
                        options.include.append(
                            test.replace(wpt_dir, url_prefix, 1))

    def _check_and_update_upstream_options(self, options: argparse.Namespace):
        if options.use_upstream_wpt:
            # when running with upstream, the goal is to get wpt report that can
            # be uploaded to wpt.fyi. We do not really cares if tests failed or
            # not. Add '--no-fail-on-unexpected' so that the overall result is
            # success. Add '--no-restart-on-unexpected' to speed up the test. On
            # Android side, we are always running with one emulator or worker,
            # so do not add '--run-by-dir=0'
            options.retry_unexpected = 0
            options.fail_on_unexpected = False
            options.restart_on_unexpected = False
            options.run_wpt_internal = False
        else:
            # By default, wpt will treat unexpected passes as errors, so we
            # disable that to be consistent with Chromium CI. Add
            # '--run-by-dir=0' so that tests can be more evenly distributed
            # among workers.
            options.fail_on_unexpected_pass = False
            options.restart_on_new_group = False
            options.run_by_dir = 0

    def _check_and_update_sharding_options(self, options):
        if self._shard_index is not None:
            # wptrunner uses a 1-based index, whereas LUCI uses 0-based.
            options.this_chunk = self._shard_index + 1
        if self._total_shards is not None:
            options.total_chunks = self._total_shards
        logger.info('Selecting tests for shard %d/%d', options.this_chunk,
                    options.total_chunks)
        # The default sharding strategy is to shard by directory. But
        # we want to hash each test to determine which shard runs it.
        # This allows running individual directories that have few
        # tests across many shards.
        options.chunk_type = options.chunk_type or 'hash'

    def path_from_output_dir(self, *parts):
        return self.path_finder.path_from_chromium_base('out', *parts)

    def run_tests(self, options: argparse.Namespace) -> int:
        with contextlib.ExitStack() as stack:
            tmp_dir = stack.enter_context(self.fs.mkdtemp())
            # Manually remove the temporary directory's contents recursively
            # after the tests complete. Otherwise, `mkdtemp()` raise an error.
            stack.callback(self.fs.rmtree, tmp_dir)
            product = self._make_product(options)
            stack.enter_context(product.test_env())

            if options.use_upstream_wpt:
                tests_root = tools_root = self.fs.join(tmp_dir, 'upstream-wpt')
                logger.info('Using upstream wpt, cloning to %s ...',
                            tests_root)
                if self.fs.isdir(tests_root):
                    shutil.rmtree(tests_root, ignore_errors=True)
                self.host.executive.run_command([
                    'git', 'clone', UPSTREAM_GIT_URL, tests_root, '--depth=25'
                ])
                self._checkout_3h_epoch_commit(tools_root)
            else:
                tests_root = self.path_finder.path_from_wpt_tests()
                tools_root = path_finder.get_wpt_tools_wpt_dir()

            options.tests_root = options.tests_root or tests_root
            options.metadata_root = options.metadata_root or tests_root
            options.run_info = options.run_info or tmp_dir
            logger.debug('Using WPT tests (external) from %s', tests_root)
            logger.debug('Using WPT tools from %s', tools_root)
            self._create_extra_run_info(options)

            self.port.setup_test_run()  # Start Xvfb, if necessary.
            stack.callback(self.port.clean_up_test_run)
            self.fs.chdir(self.path_finder.web_tests_dir())
            run = _load_entry_point(tools_root)
            stack.enter_context(self.process_and_upload_results(options))
            exit_code = run(**vars(options))
            return exit_code

    def _make_product(self, options: argparse.Namespace) -> 'Product':
        product_cls = _product_registry[options.product]
        return product_cls(self.host, options, self.port.python3_command())

    def _checkout_3h_epoch_commit(self, tools_root: str):
        wpt_executable = self.fs.join(tools_root, 'wpt')
        output = self.host.executive.run_command(
            [wpt_executable, 'rev-list', '--epoch', '3h'])
        commit = output.splitlines()[0]
        logger.info('Running against upstream wpt@%s', commit)
        self.host.executive.run_command(['git', 'checkout', commit],
                                        cwd=tools_root)

    def _create_extra_run_info(self, options: argparse.Namespace):
        run_info = {
            # This property should always be a string so that the metadata
            # updater works, even when wptrunner is not running a flag-specific
            # suite.
            'os': self.port.operating_system(),
            'port': self.port.version(),
            'debug': self.port.get_option('configuration') == 'Debug',
            'flag_specific': options.flag_specific or '',
            'used_upstream': options.use_upstream_wpt,
            'sanitizer_enabled': options.enable_sanitizer,
        }
        if options.use_upstream_wpt:
            # `run_wpt_tests` does not run in the upstream checkout's git
            # context, so wptrunner cannot infer the latest revision. Manually
            # add the revision here.
            run_info['revision'] = self.host.git(
                path=options.tests_root).current_revision()
        # The filename must be `mozinfo.json` for wptrunner to read it from the
        # `--run-info` directory.
        run_info_path = self.fs.join(options.run_info, 'mozinfo.json')
        with self.fs.open_text_file_for_writing(run_info_path) as file_handle:
            json.dump(run_info, file_handle)

    @contextlib.contextmanager
    def process_and_upload_results(
            self,
            options,
            layout_test_results_subdir: str = ARTIFACTS_SUB_DIR,
    ):
        if options.log_chromium:
            artifacts_dir = self.fs.join(
                self.fs.dirname(options.log_chromium[0].name),
                layout_test_results_subdir)
        else:
            artifacts_dir = self.path_from_output_dir(
                options.target, layout_test_results_subdir)
        processor = WPTResultsProcessor(self.host.filesystem,
                                        self.port,
                                        artifacts_dir=artifacts_dir)
        with processor.stream_results() as events:
            options.log.add_handler(events.put)
            yield
        if options.log_wptreport:
            processor.process_wpt_report(options.log_wptreport[0].name)
        if options.log_chromium:
            processor.process_results_json(options.log_chromium[0].name)
        if options.show_results and processor.has_regressions:
            self.port.show_results_html_file(
                self.fs.join(artifacts_dir, 'results.html'))

    def add_configuration_arguments(self, parser: argparse.ArgumentParser):
        group = parser.add_argument_group('Configuration')
        group.add_argument('-t',
                           '--target',
                           default='Release',
                           help='Target build subdirectory under //out')
        group.add_argument(
            '-p',
            '--product',
            default='content_shell',
            choices=sorted(_product_registry, key=len),
            help='Product (browser or browser component) to test.')
        group.add_argument('--headless',
                           action='store_true',
                           default=True,
                           help=argparse.SUPPRESS)
        group.add_argument('--webdriver-binary',
                           type=os.path.abspath,
                           help=('Path of the webdriver binary.'
                                 'It needs to have the same major version '
                                 'as the browser binary or APK.'))
        group.add_argument(
            '--use-upstream-wpt',
            action='store_true',
            help=('Use tests and tools from the main branch of the WPT GitHub '
                  'repo instead of chromium/src. The repo will be cloned to '
                  'a temporary directory.'))
        group.add_argument(
            '--flag-specific',
            choices=sorted(self.port.flag_specific_configs()),
            metavar='CONFIG',
            help=('The name of a flag-specific suite to run. '
                  'See "web_tests/FlagSpecificConfig" for the full list.'))
        return group

    def add_debugging_arguments(self, parser: argparse.ArgumentParser):
        group = parser.add_argument_group('Debugging')
        group.add_argument('--repeat',
                           '--gtest_repeat',
                           '--isolated-script-test-repeat',
                           type=lambda value: max(1, int(value)),
                           default=1,
                           help=('Number of times to run the tests, '
                                 'restarting between each run.'))
        group.add_argument(
            '--retry-unexpected',
            '--test-launcher-retry-limit',
            '--isolated-script-test-launcher-retry-limit',
            metavar='RETRIES',
            type=lambda value: max(0, int(value)),
            default=None,
            help=(
                'Maximum number of times to rerun unexpectedly failed tests. '
                'Defaults to 3 unless given an explicit list of tests to run.'
            ))
        group.add_argument('--no-show-results',
                           dest='show_results',
                           action='store_false',
                           default=self.host.platform.interactive,
                           help=("Don't launch a browser with results after"
                                 "the tests are done"))
        group.add_argument(
            '--enable-sanitizer',
            action='store_true',
            help='Only report sanitizer-related errors and crashes.')
        group.add_argument('--enable-leak-detection',
                           action='append_const',
                           dest='binary_args',
                           const='--enable-leak-detection',
                           help='Enable the leak detection of DOM objects.')
        return group

    def add_binary_arguments(self, parser):
        group = parser.add_argument_group(
            'Chrome-specific',
            'Options for configuring the binary under test.')
        group.add_argument(
            '--enable-features',
            metavar='FEATURES',
            action='append',
            dest='binary_args',
            type=lambda feature: '--enable-features=%s' % feature,
            help='Chromium features to enable during testing.')
        group.add_argument(
            '--disable-features',
            metavar='FEATURES',
            action='append',
            dest='binary_args',
            type=lambda feature: '--disable-features=%s' % feature,
            help='Chromium features to disable during testing.')
        group.add_argument(
            '--force-fieldtrials',
            metavar='TRIALS',
            action='append',
            dest='binary_args',
            type=lambda feature: '--force-fieldtrials=%s' % feature,
            help='Force trials for Chromium features.')
        group.add_argument(
            '--force-fieldtrial-params',
            metavar='TRIAL_PARAMS',
            action='append',
            dest='binary_args',
            type=lambda feature: '--force-fieldtrial-params=%s' % feature,
            help='Force trial params for Chromium features.')
        return group

    def add_test_arguments(self, parser):
        group = parser.add_argument_group(
            'Test Selection', 'Options for selecting tests to run.')
        # `--gtest_filter` and `--isolated-script-test-filter` have slightly
        # different formats and behavior, so keep them as separate options.
        # See: crbug/1316164#c4
        group.add_argument(
            '--test-filter',
            '--gtest_filter',
            metavar='<test1>:...',
            dest='include',
            action='extend',
            type=self._parse_gtest_filter,
            help='Colon-separated list of test names or directories.')
        group.add_argument(
            '--isolated-script-test-filter',
            metavar='<glob1>::...',
            action=functools.partial(IsolatedScriptTestFilterAction,
                                     finder=self.path_finder),
            help=('An isolated script-style pattern for selecting tests. '
                  'The pattern consists of globs separated by double-colons '
                  "'::'. A glob prefixed by '-' will exclude tests that match "
                  'instead of including them.'))
        group.add_argument('--no-wpt-internal',
                           action='store_false',
                           dest='run_wpt_internal',
                           help='Do not run internal WPTs.')
        return group

    def _parse_gtest_filter(self, value: str) -> List[str]:
        return [
            self.path_finder.strip_wpt_path(test_id)
            for test_id in value.split(':')
        ]

    def add_output_arguments(self, parser):
        group = parser.add_argument_group(
            'Output Logging', 'Options for controlling logging behavior.')
        # For the overridden '--log-*' options, the value will be `None` if no
        # report should be logged, or the empty string if a default filename
        # should be derived.
        group.add_argument(
            '--log-chromium',
            '--isolated-script-test-output',
            nargs='?',
            const='',
            help=('Log results in the legacy Chromium JSON results format. '
                  'See https://chromium.googlesource.com/chromium/src/+/HEAD/'
                  'docs/testing/json_test_results_format.md'))
        group.add_argument(
            '--log-wptreport',
            nargs='?',
            const='',
            help=('Log a wptreport as newline-delimited JSON objects '
                  '(default: //out/<target>/'
                  'wpt_reports_<product>_<shard-index>.json)'))
        group.add_argument('-v',
                           '--verbose',
                           action='count',
                           default=0,
                           help='Increase verbosity')
        return group

    def add_android_arguments(self, parser):
        group = parser.add_argument_group(
            'Android specific arguments',
            'Options for configuring Android devices and tooling.')
        common.add_emulator_args(group)
        group.add_argument(
            '--browser-apk',
            # Aliases for backwards compatibility.
            '--chrome-apk',
            '--system-webview-shell',
            type=os.path.abspath,
            help=('Path to the browser APK to install and run. '
                  '(For WebView, this value is the shell. '
                  'Defaults to an on-device APK if not provided.)'))
        group.add_argument('--webview-provider',
                           type=os.path.abspath,
                           help=('Path to a WebView provider APK to install. '
                                 '(WebView only.)'))
        group.add_argument(
            '--additional-apk',
            type=os.path.abspath,
            action='append',
            default=[],
            help='Paths to additional APKs to install.')
        group.add_argument(
            '--release-channel',
            help='Install WebView from release channel. (WebView only.)')
        return group

    def add_ios_arguments(self, parser):
        group = parser.add_argument_group(
            'iOS specific arguments', 'Options for configuring iOS tooling.')
        group.add_argument(
            '--xcode-build-version',
            help='Xcode build version to install. Use chrome_ios'
            ' product to enable this',
            metavar='build_id')
        return group


class IsolatedScriptTestFilterAction(argparse.Action):
    def __init__(self, finder, *args, **kwargs):
        self._finder = finder
        super().__init__(*args, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        include = getattr(namespace, 'include') or []
        exclude = getattr(namespace, 'exclude') or []
        if isinstance(values, str):
            values = [values]
        if isinstance(values, list):
            for test_filter in values:
                extra_include, extra_exclude = self._resolve_tests(test_filter)
                include.extend(extra_include)
                exclude.extend(extra_exclude)
        namespace.include, namespace.exclude = include, exclude

    def _resolve_tests(self, test_filter: str) -> Tuple[List[str], List[str]]:
        """Resolve an isolated script-style filter string into lists of tests.

        Arguments:
            test_filter: Glob patterns delimited by double colons ('::'). The
                glob is prefixed with '-' to indicate that tests matching the
                pattern should not run. Assume a valid wpt name cannot start
                with '-'.

        Returns:
            Tests to include and exclude, respectively.
        """
        included_tests, excluded_tests = [], []
        for pattern in test_filter.split('::'):
            test_group = included_tests
            if pattern.startswith('-'):
                test_group, pattern = excluded_tests, pattern[1:]
            if self._finder.is_wpt_internal_path(pattern):
                pattern_on_disk = self._finder.path_from_web_tests(pattern)
            else:
                pattern_on_disk = self._finder.path_from_wpt_tests(pattern)
            test_group.extend(glob.glob(pattern_on_disk))
        return included_tests, excluded_tests


def _has_explicit_tests(options: argparse.Namespace) -> bool:
    return (options.include or options.exclude or options.include_file
            or options.test_list)


def _load_entry_point(tools_root: str):
    """Import and return a callable that runs wptrunner.

    Arguments:
        tests_root: Path to a directory whose structure corresponds to the WPT
            repository. This will use the tools under `tools/`.

    Returns:
        Callable whose keyword arguments are the namespace corresponding to
        command line options.
    """
    if tools_root not in sys.path:
        sys.path.insert(0, tools_root)
    # Remove current cached modules to force a reload.
    module_pattern = re.compile(r'\bwpt(runner|serve)?\b')
    for name in list(sys.modules):
        if module_pattern.search(name):
            del sys.modules[name]
    from tools import localpaths
    from tools.wpt import run
    from tools.wpt.virtualenv import Virtualenv
    import wptrunner
    import wptserve
    for module in (run, wptrunner, wptserve):
        assert module.__file__.startswith(tools_root), module.__file__

    # vpython, not virtualenv, vends third-party packages in chromium/src.
    dummy_venv = Virtualenv(path_finder.get_source_dir(),
                            skip_virtualenv_setup=True)
    return functools.partial(run.run, dummy_venv)


class Product:
    """A product (browser or browser component) that can run web platform tests.

    Attributes:
        name (str): The official wpt-accepted name of this product.
        aliases (list[str]): Human-friendly aliases for the official name.
    """
    name = ''
    aliases = []

    def __init__(self, host, options, python_executable=None):
        self._host = host
        self._path_finder = PathFinder(self._host.filesystem)
        self._options = options
        self._python_executable = python_executable
        self._tasks = contextlib.ExitStack()
        self._validate_options()

    def _path_from_target(self, *components):
        return self._path_finder.path_from_chromium_base(
            'out', self._options.target, *components)

    def _validate_options(self):
        """Validate product-specific command-line options.

        The validity of some options may depend on the product. We check these
        options here instead of at parse time because the product itself is an
        option and the parser cannot handle that dependency.

        The test environment will not be set up at this point, so checks should
        not depend on external resources.

        Raises:
            ValueError: When the given options are invalid for this product.
                The user will see the error's message (formatted with
                `argparse`, not a traceback) and the program will exit early,
                which avoids wasted runtime.
        """

    @contextlib.contextmanager
    def test_env(self):
        """Set up and clean up the test environment."""
        with self._tasks:
            self.update_options()
            yield

    def update_options(self):
        """Override product-specific wptrunner parameters."""
        version = self.get_version()  # pylint: disable=assignment-from-none
        if version:
            self._options.browser_version = version
        webdriver = self.webdriver_binary
        if webdriver and self._host.filesystem.exists(webdriver):
            self._options.webdriver_binary = webdriver

    def get_version(self):
        """Get the product version, if available."""
        return None

    @property
    def webdriver_binary(self):
        """Optional[str]: Path to the webdriver binary, if available."""
        return self._options.webdriver_binary


class DesktopBase(Product):
    @property
    def binary(self):
        raise NotImplementedError

    def update_options(self):
        super().update_options()
        self._options.binary = self.binary
        port = self._host.port_factory.get()
        self._options.processes = (self._options.processes
                                   or port.default_child_processes())


class Chrome(DesktopBase):
    name = 'chrome'

    @property
    def binary(self):
        binary_path = 'chrome'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Chromium.app',
                                                     'Contents', 'MacOS',
                                                     'Chromium')
        return self._path_from_target(binary_path)

    @property
    def webdriver_binary(self):
        default_binary = 'chromedriver'
        if self._host.platform.is_win():
            default_binary += '.exe'
        return (super().webdriver_binary
                or self._path_from_target(default_binary))


class ContentShell(DesktopBase):
    name = 'content_shell'

    @property
    def binary(self):
        binary_path = 'content_shell'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Content Shell.app',
                                                     'Contents', 'MacOS',
                                                     'Content Shell')
        return self._path_from_target(binary_path)


class ChromeiOS(Product):
    name = 'chrome_ios'

    @property
    def webdriver_binary(self) -> Optional[str]:
        return os.path.realpath(
            os.path.join(
                os.path.dirname(__file__),
                '../../../ios/chrome/test/wpt/tools/'
                'run_cwt_chromedriver_wrapper.py'))

    @contextlib.contextmanager
    def test_env(self):
        with super().test_env():
            self.update_options()
            if self._options.xcode_build_version:
                try:
                    runtime_cache_folder = xcode.construct_runtime_cache_folder(
                        '../../Runtime-ios-', '16.0')
                    os.makedirs(runtime_cache_folder, exist_ok=True)
                    xcode.install('../../mac_toolchain',
                                  self._options.xcode_build_version,
                                  '../../Xcode.app',
                                  runtime_cache_folder=runtime_cache_folder,
                                  ios_version='16.0')
                    xcode.select('../../Xcode.app')
                except subprocess.CalledProcessError as e:
                    logger.error(
                        'Xcode build version %s failed to install: %s ',
                        self._options.xcode_build_version, e)
                else:
                    logger.info(
                        'Xcode build version %s successfully installed.',
                        self._options.xcode_build_version)
            else:
                logger.warning('Skip the Xcode installation, no '
                               '--xcode-build-version')
            yield


@contextlib.contextmanager
def _install_apk(device, path):
    """Helper context manager for ensuring a device uninstalls an APK."""
    device.Install(path)
    try:
        yield
    finally:
        device.Uninstall(path)


class ChromeAndroidBase(Product):
    def __init__(self, host, options, python_executable=None):
        super().__init__(host, options, python_executable)
        self.devices = {}

    @contextlib.contextmanager
    def test_env(self):
        with super().test_env():
            devil_chromium.Initialize(adb_path=self._options.adb_binary)
            if not self._options.adb_binary:
                self._options.adb_binary = devil_env.config.FetchPath('adb')
            devices = self._tasks.enter_context(get_devices(self._options))
            if not devices:
                raise Exception('No devices attached to this host. '
                                "Make sure to provide '--avd-config' "
                                'if using only emulators.')

            self.provision_devices(devices)
            self.update_options()
            yield

    def update_options(self):
        super().update_options()
        self._options.device_serial.extend(sorted(self.devices))
        self._options.package_name = (self._options.package_name
                                      or self.get_browser_package_name())

    def get_version(self):
        version_provider = self.get_version_provider_package_name()
        if self.devices and version_provider:
            # Assume devices are identically provisioned, so select any.
            device = list(self.devices.values())[0]
            try:
                version = device.GetApplicationVersion(version_provider)
                logger.info('Product version: %s %s (package: %r)', self.name,
                            version, version_provider)
                return version
            except CommandFailedError:
                logger.warning(
                    'Failed to retrieve version of %s (package: %r)',
                    self.name, version_provider)
        return None

    @property
    def webdriver_binary(self):
        default_binary = self._path_from_target('clang_x64', 'chromedriver')
        return super().webdriver_binary or default_binary

    def get_browser_package_name(self):
        """Get the name of the package to run tests against.

        For WebView, this package is the shell.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the runnable package.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L867-L924
        """
        if self._options.package_name:
            return self._options.package_name
        if self._options.browser_apk:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.browser_apk)
        return None

    def get_version_provider_package_name(self):
        """Get the name of the package containing the product version.

        Some Android products are made up of multiple packages with decoupled
        "versionName" fields. This method identifies the package whose
        "versionName" should be consider the product's version.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the version.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/run.py#L810-L816
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L850-L924
        """
        # Assume the product is a single APK.
        return self.get_browser_package_name()

    def provision_devices(self, devices):
        """Provisions a set of Android devices in parallel."""
        contexts = [self._provision_device(device) for device in devices]
        self._tasks.enter_context(SyncParallelizer(contexts))

        for device in devices:
            if device.serial in self.devices:
                raise Exception('duplicate device serial: %s' % device.serial)
            self.devices[device.serial] = device
            self._tasks.callback(self.devices.pop, device.serial, None)

    @contextlib.contextmanager
    def _provision_device(self, device):
        """Provision a single Android device for a test.

        This method will be executed in parallel on all devices, so
        it is crucial that it is thread safe.
        """
        with contextlib.ExitStack() as exit_stack:
            if self._options.browser_apk:
                exit_stack.enter_context(
                    _install_apk(device, self._options.browser_apk))
            for apk in self._options.additional_apk:
                exit_stack.enter_context(_install_apk(device, apk))
            logger.info('Provisioned device (serial: %s)', device.serial)
            yield


class WebView(ChromeAndroidBase):
    name = ANDROID_WEBVIEW
    aliases = ['webview']

    def _install_webview(self, device):
        # Prioritize local builds.
        if self._options.webview_provider:
            return webview_app.UseWebViewProvider(
                device, self._options.webview_provider)
        assert self._options.release_channel, 'no webview install method'
        return self._install_webview_from_release(device)

    def _validate_options(self):
        super()._validate_options()
        if not self._options.webview_provider \
                and not self._options.release_channel:
            raise ValueError("Must provide either '--webview-provider' or "
                             "'--release-channel' to install WebView.")

    def get_browser_package_name(self):
        return (super().get_browser_package_name()
                or 'org.chromium.webview_shell')

    def get_version_provider_package_name(self):
        # Prioritize using the webview provider, not the shell, since the
        # provider is distributed to end users. The shell is developer-facing,
        # so its version is usually not actively updated.
        if self._options.webview_provider:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(
                    self._options.webview_provider)
        return super().get_version_provider_package_name()

    @contextlib.contextmanager
    def _provision_device(self, device):
        with self._install_webview(device), super()._provision_device(device):
            yield

    @contextlib.contextmanager
    def _install_webview_from_release(self, device):
        script_path = self._path_finder.path_from_chromium_base(
            'clank', 'bin', 'install_webview.py')
        command = [
            self._python_executable, script_path, '-s', device.serial,
            '--channel', self._options.release_channel
        ]
        exit_code = common.run_command(command)
        if exit_code != 0:
            raise Exception(
                'failed to install webview from release '
                '(serial: %r, channel: %r, exit code: %d)' %
                (device.serial, self._options.release_channel, exit_code))
        yield


class ChromeAndroid(ChromeAndroidBase):
    name = CHROME_ANDROID
    aliases = ['clank']

    def _validate_options(self):
        super()._validate_options()
        if not self._options.package_name and not self._options.browser_apk:
            raise ValueError(
                "Must provide either '--package-name' or '--browser-apk' "
                'for %r.' % self.name)


def _make_product_registry():
    """Create a mapping from all product names (including aliases) to their
    respective classes.
    """
    product_registry = {}
    product_classes = [Chrome, ContentShell, ChromeiOS]
    if _ANDROID_ENABLED:
        product_classes.extend([ChromeAndroid, WebView])
    for product_cls in product_classes:
        names = [product_cls.name] + product_cls.aliases
        product_registry.update((name, product_cls) for name in names)
    return product_registry


_product_registry = _make_product_registry()


@contextlib.contextmanager
def get_device(args):
    with get_devices(args) as devices:
        yield None if not devices else devices[0]


@contextlib.contextmanager
def get_devices(args):
    if not _ANDROID_ENABLED:
        raise Exception('Android is not available')
    instances = []
    try:
        if args.avd_config:
            avd_config = avd.AvdConfig(args.avd_config)
            logger.warning('Installing emulator from %s', args.avd_config)
            avd_config.Install()

            for _ in range(max(args.processes or 1, 1)):
                instance = avd_config.CreateInstance()
                instances.append(instance)

            SyncParallelizer(instances).Start(writable_system=True,
                                              window=args.emulator_window,
                                              require_fast_start=True)

        #TODO(weizhong): when choose device, make sure abi matches with target
        yield device_utils.DeviceUtils.HealthyDevices()
    finally:
        SyncParallelizer(instances).Stop()


def _parse_environ_int(name: str) -> Optional[int]:
    value = os.environ.get(name)
    with contextlib.suppress(ValueError):
        if value is not None:
            return int(value)
    return None


def main() -> int:
    # Force log output in utf-8 instead of a locale-dependent encoding. On
    # Windows, this can be cp1252. See: crbug.com/1371195.
    if sys.version_info[:2] >= (3, 7):
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    # Also apply utf-8 mode to python subprocesses.
    os.environ['PYTHONUTF8'] = '1'
    try:
        adapter = WPTAdapter()
        options = adapter.parse_arguments()
        return adapter.run_tests(options)
    except KeyboardInterrupt:
        return exit_codes.INTERRUPTED_EXIT_STATUS


if __name__ == '__main__':
    multiprocessing.set_start_method('spawn')
    sys.exit(main())
