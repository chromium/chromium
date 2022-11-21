# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import logging
import os
import sys

# Add src/testing/ into sys.path for importing common without pylint errors.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
from scripts import common

BLINK_TOOLS_DIR = os.path.join(common.SRC_DIR, 'third_party', 'blink', 'tools')
CATAPULT_DIR = os.path.join(common.SRC_DIR, 'third_party', 'catapult')
OUT_DIR = os.path.join(common.SRC_DIR, "out", "{}")
DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT = os.path.join(OUT_DIR, "results.json")
TYP_DIR = os.path.join(CATAPULT_DIR, 'third_party', 'typ')
WEB_TESTS_DIR = os.path.normpath(
    os.path.join(BLINK_TOOLS_DIR, os.pardir, 'web_tests'))

if BLINK_TOOLS_DIR not in sys.path:
    sys.path.append(BLINK_TOOLS_DIR)

if TYP_DIR not in sys.path:
    sys.path.append(TYP_DIR)

from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder

logger = logging.getLogger(__name__)


# pylint: disable=super-with-arguments
class BaseWptScriptAdapter(common.BaseIsolatedScriptArgsAdapter):
    """The base class for script adapters that use wptrunner to execute web
    platform tests. This contains any code shared between these scripts, such
    as integrating output with the results viewer. Subclasses contain other
    (usually platform-specific) logic."""

    def __init__(self, host=None):
        self.host = host or Host()
        self.fs = self.host.filesystem
        self.path_finder = PathFinder(self.fs)
        self.port = self.host.port_factory.get()
        super(BaseWptScriptAdapter, self).__init__()
        self._parser = self._override_options(self._parser)
        self.wptreport = None
        self._include_filename = None
        self.layout_test_results_subdir = 'layout-test-results'

    @property
    def wpt_binary(self):
        default_wpt_binary = os.path.join(
            common.SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")
        return os.environ.get("WPT_BINARY", default_wpt_binary)

    @property
    def wpt_root_dir(self):
        return self.path_finder.path_from_web_tests(
            self.path_finder.wpt_prefix())

    @property
    def output_directory(self):
        return self.path_finder.path_from_chromium_base('out',
                                                        self.options.target)

    @property
    def mojo_js_directory(self):
        return self.fs.join(self.output_directory, 'gen')

    def add_extra_arguments(self, parser):
        parser.add_argument(
            '-t',
            '--target',
            default='Release',
            help='Target build subdirectory under //out')
        parser.add_argument(
            '--default-exclude',
            action='store_true',
            help=('Only run the tests explicitly given in arguments '
                  '(can run no tests, which will exit with code 0)'))
        self.add_mode_arguments(parser)
        self.add_output_arguments(parser)

    def add_mode_arguments(self, parser):
        group = parser.add_argument_group(
            'Mode',
            'Options for wptrunner modes other than running tests.')
        # We provide an option to show wptrunner's help here because the 'wpt'
        # executable may be inaccessible from the user's PATH. The top-level
        # 'wpt' command also needs to have virtualenv disabled.
        group.add_argument(
            '--wpt-help',
            action='store_true',
            help='Show the wptrunner help message and exit')
        return group

    def add_output_arguments(self, parser):
        group = parser.add_argument_group(
            'Output Logging',
            'Options for controlling logging behavior.')
        group.add_argument(
            '--log-wptreport',
            nargs='?',
            # We cannot provide a default, since the default filename depends on
            # the product, so we use this placeholder instead.
            const='',
            help=('Log a wptreport in JSON to the output directory '
                  '(default filename: '
                  'wpt_reports_<product>_<shard-index>.json)'))
        group.add_argument(
            '-v',
            '--verbose',
            action='count',
            default=0,
            help='Increase verbosity')
        return group

    def _override_options(self, base_parser):
        """Create a parser that overrides existing options.

        `argument.ArgumentParser` can extend other parsers and override their
        options, with the caveat that the child parser only inherits options
        that the parent had at the time of the child's initialization. There is
        not a clean way to add option overrides in `add_extra_arguments`, where
        the provided parser is only passed up the inheritance chain, so we add
        overridden options here at the very end.

        See Also:
            https://docs.python.org/3/library/argparse.html#parents
        """
        parser = argparse.ArgumentParser(
            parents=[base_parser],
            # Allow overriding existing options in the parent parser.
            conflict_handler='resolve',
            epilog=('All unrecognized arguments are passed through '
                    "to wptrunner. Use '--wpt-help' to see wptrunner's usage."),
        )
        parser.add_argument(
            '--isolated-script-test-repeat',
            '--repeat',
            '--gtest_repeat',
            metavar='REPEAT',
            type=int,
            default=1,
            help='Number of times to run the tests')
        parser.add_argument(
            '--isolated-script-test-launcher-retry-limit',
            '--test-launcher-retry-limit',
            '--retry-unexpected',
            metavar='RETRIES',
            type=int,
            help=(
                'Maximum number of times to rerun unexpectedly failed tests. '
                'Defaults to 3 unless given an explicit list of tests to run.'))
        # `--gtest_filter` and `--isolated-script-test-filter` have slightly
        # different formats and behavior, so keep them as separate options.
        # See: crbug/1316164#c4

        # TODO(crbug.com/1356318): This is a temporary hack to hide the
        # inherited '--xvfb' option and force Xvfb to run always.
        parser.add_argument('--xvfb', action='store_true', default=True,
                            help=argparse.SUPPRESS)
        return parser

    def maybe_set_default_isolated_script_test_output(self):
        if self.options.isolated_script_test_output:
            return
        default_value = DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT.format(
            self.options.target)
        print("--isolated-script-test-output not set, defaulting to %s" %
              default_value)
        self.options.isolated_script_test_output = default_value

    def generate_test_output_args(self, output):
        return ['--log-chromium=%s' % output]

    def _resolve_tests_from_isolate_filter(self, test_filter):
        """Resolve an isolated script-style filter string into lists of tests.

        Arguments:
            test_filter (str): Glob patterns delimited by double colons ('::').
                The glob is prefixed with '-' to indicate that tests matching
                the pattern should not run. Assume a valid wpt name cannot start
                with '-'.

        Returns:
            tuple[list[str], list[str]]: Tests to include and exclude,
                respectively.
        """
        included_tests, excluded_tests = [], []
        for pattern in common.extract_filter_list(test_filter):
            test_group = included_tests
            if pattern.startswith('-'):
                test_group, pattern = excluded_tests, pattern[1:]
            if self.path_finder.is_wpt_internal_path(pattern):
                pattern_on_disk = self.path_finder.path_from_web_tests(pattern)
            else:
                pattern_on_disk = self.fs.join(self.wpt_root_dir, pattern)
            test_group.extend(glob.glob(pattern_on_disk))
        return included_tests, excluded_tests

    def generate_test_filter_args(self, test_filter_str):
        included_tests, excluded_tests = \
            self._resolve_tests_from_isolate_filter(test_filter_str)
        include_file, self._include_filename = self.fs.open_text_tempfile()
        with include_file:
            for test in included_tests:
                include_file.write(test)
                include_file.write('\n')
        wpt_args = ['--include-file=%s' % self._include_filename]
        for test in excluded_tests:
            wpt_args.append('--exclude=%s' % test)
        return wpt_args

    def generate_test_repeat_args(self, repeat_count):
        return ['--repeat=%d' % repeat_count]

    @property
    def _has_explicit_tests(self):
        # TODO(crbug.com/1356318): `run_wpt_tests` has multiple ways to
        # explicitly specify tests. Some are inherited from wptrunner, the rest
        # from Chromium infra. After we consolidate `run_wpt_tests` and
        # `wpt_common`, maybe we should build a single explicit test list to
        # simplify this check?
        for test_or_option in super().rest_args:
            if not test_or_option.startswith('-'):
                return True
        return (getattr(self.options, 'include', None) or
                getattr(self.options, 'include_file', None) or
                getattr(self.options, 'gtest_filter', None) or
                self._include_filename)

    def generate_test_launcher_retry_limit_args(self, retry_limit):
        return ['--retry-unexpected=%d' % retry_limit]

    def generate_sharding_args(self, total_shards, shard_index):
        return ['--total-chunks=%d' % total_shards,
                # shard_index is 0-based but WPT's this-chunk to be 1-based
                '--this-chunk=%d' % (shard_index + 1),
                # The default sharding strategy is to shard by directory. But
                # we want to hash each test to determine which shard runs it.
                # This allows running individual directories that have few
                # tests across many shards.
                '--chunk-type=hash']

    def parse_args(self, args=None):
        super(BaseWptScriptAdapter, self).parse_args(args)
        if self.options.wpt_help:
            self._show_wpt_help()
        # Update the output directory and wptreport filename to defaults if not
        # set. We cannot provide CLI option defaults because they depend on
        # other options ('--target' and '--product').
        self.maybe_set_default_isolated_script_test_output()
        report = self.options.log_wptreport
        if report is not None:
            if not report:
                report = self._default_wpt_report()
            self.wptreport = self.fs.join(self.fs.dirname(self.wpt_output),
                                          report)
        if self.options.isolated_script_test_launcher_retry_limit is None:
            self.options.isolated_script_test_launcher_retry_limit = (
                self._default_retry_limit)

    @property
    def _default_retry_limit(self) -> int:
        return 0 if self._has_explicit_tests else 3

    @property
    def wpt_output(self):
        return self.options.isolated_script_test_output

    def _show_wpt_help(self):
        command = [
            self.select_python_executable(),
        ]
        command.extend(self._wpt_run_args)
        command.extend(['--help'])
        exit_code = common.run_command(command)
        self.parser.exit(exit_code)

    @property
    def _wpt_run_args(self):
        """The start of a 'wpt run' command."""
        return [
            self.wpt_binary,
            # Use virtualenv packages installed by vpython, not wpt.
            '--venv=%s' % self.path_finder.chromium_base(),
            '--skip-venv-setup',
            'run',
        ]

    @property
    def rest_args(self):
        unknown_args = super(BaseWptScriptAdapter, self).rest_args

        rest_args = list(self._wpt_run_args)
        rest_args.extend([
            '--no-pause-after-test',
            '--no-capture-stdio',
            '--no-manifest-download',
            '--tests=%s' % self.wpt_root_dir,
            '--metadata=%s' % self.wpt_root_dir,
            '--mojojs-path=%s' % self.mojo_js_directory,
        ])

        if self.options.default_exclude:
            rest_args.extend(['--default-exclude'])

        if self.wptreport:
            rest_args.extend(['--log-wptreport', self.wptreport])

        if self.options.verbose >= 3:
            rest_args.extend([
                '--log-mach=-',
                '--log-mach-level=debug',
                '--log-mach-verbose',
            ])
        if self.options.verbose >= 4:
            rest_args.extend([
                '--webdriver-arg=--verbose',
                '--webdriver-arg="--log-path=-"',
            ])

        rest_args.append(self.wpt_product_name())
        # We pass through unknown args as late as possible so that they can
        # override earlier options. It also allows users to pass test names as
        # positional args, which must not have option strings between them.
        for unknown_arg in unknown_args:
            # crbug/1274933#c14: Some developers had used the end-of-options
            # marker '--' to pass through arguments to wptrunner.
            # crrev.com/c/3573284 makes this no longer necessary.
            if unknown_arg == '--':
                logger.warning(
                    'Unrecognized options will automatically fall through '
                    'to wptrunner.')
                logger.warning(
                    "There is no need to use the end-of-options marker '--'.")
            else:
                rest_args.append(unknown_arg)
        return rest_args

    def process_and_upload_results(self):
        command = [
            self.select_python_executable(),
            os.path.join(BLINK_TOOLS_DIR, 'wpt_process_results.py'),
            '--target',
            self.options.target,
            '--web-tests-dir',
            WEB_TESTS_DIR,
            '--artifacts-dir',
            os.path.join(os.path.dirname(self.wpt_output),
                         self.layout_test_results_subdir),
            '--wpt-results',
            self.wpt_output,
        ]
        if self.options.verbose:
            command.append('--verbose')
        if self.wptreport:
            command.extend(['--wpt-report',
                            self.wptreport])
        return common.run_command(command)

    def clean_up_after_test_run(self):
        if self._include_filename:
            self.fs.remove(self._include_filename)

    def wpt_product_name(self):
        raise NotImplementedError

    def _default_wpt_report(self):
        product = self.wpt_product_name()
        shard_index = os.environ.get('GTEST_SHARD_INDEX')
        if shard_index is not None:
            return 'wpt_reports_%s_%02d.json' % (product, int(shard_index))
        return 'wpt_reports_%s.json' % product
