# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import posixpath
import sys

import common

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


class BaseWptScriptAdapter(common.BaseIsolatedScriptArgsAdapter):
    """The base class for script adapters that use wptrunner to execute web
    platform tests. This contains any code shared between these scripts, such
    as integrating output with the results viewer. Subclasses contain other
    (usually platform-specific) logic."""

    def __init__(self, host=None):
        super(BaseWptScriptAdapter, self).__init__()
        if not host:
            host = Host()
        self.fs = host.filesystem
        self.path_finder = PathFinder(self.fs)
        self.port = host.port_factory.get()
        # Path to the output of the test run. Comes from the args passed to the
        # run, parsed after this constructor. Can be overwritten by tests.
        self.wpt_output = None
        self.wptreport = None
        self.layout_test_results_subdir = 'layout-test-results'
        default_wpt_binary = os.path.join(
            common.SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")
        self.wpt_binary = os.environ.get("WPT_BINARY", default_wpt_binary)

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
            '--repeat',
            '--gtest_repeat',
            type=int,
            default=1,
            help='Number of times to run the tests')
        # TODO(crbug/1306222): wptrunner currently cannot rerun individual
        # failed tests, so this flag is accepted but not used.
        parser.add_argument(
            '--test-launcher-retry-limit',
            metavar='LIMIT',
            type=int,
            default=0,
            help='Maximum number of times to rerun a failed test')
        parser.add_argument(
            '--default-exclude',
            action='store_true',
            help=('Only run the tests explicitly given in arguments '
                  '(can run no tests, which will exit with code 0)'))
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='Do not upload results to ResultDB')
        # We provide an option to show wptrunner's help here because the 'wpt'
        # executable may not be inaccessible from the user's PATH. The top-level
        # 'wpt' command also needs to have virtualenv disabled.
        parser.add_argument(
            '--wpt-help',
            action='store_true',
            help="Show the wptrunner help message and exit")

        self.output_group = parser.add_argument_group(
            'Output Logging',
            'Options for controlling logging behavior.')
        self.output_group.add_argument(
            '--log-wptreport',
            nargs='?',
            const=self._default_wpt_report(),
            help=('Log a wptreport in JSON to the output directory '
                  '(default filename: %(const)s)'))
        self.output_group.add_argument(
            '-v',
            '--verbose',
            action='count',
            default=0,
            help='Increase verbosity')

        # Parser will format the epilog for us.
        parser.epilog = (parser.epilog or '') + ' ' + (
            'All unrecognized arguments are passed through to wptrunner. '
            "Use '--wpt-help' to see wptrunner's usage."
        )

    def maybe_set_default_isolated_script_test_output(self):
        if self.options.isolated_script_test_output:
            return
        default_value = DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT.format(
            self.options.target)
        print("--isolated-script-test-output not set, defaulting to %s" %
              default_value)
        self.options.isolated_script_test_output = default_value

    def generate_test_output_args(self, output):
        return ['--log-chromium', output]

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
        # Update the output directory to the default if it's not set.
        # We cannot provide a CLI arg default because the path depends on
        # --target.
        self.maybe_set_default_isolated_script_test_output()
        if self.options.log_wptreport:
            wpt_output = self.options.isolated_script_test_output
            self.wptreport = self.fs.join(
                self.fs.dirname(wpt_output),
                self.options.log_wptreport)

    def do_pre_test_run_tasks(self):
        super(BaseWptScriptAdapter, self).do_pre_test_run_tasks()
        if self.options.wpt_help:
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
        wpt_args = super(BaseWptScriptAdapter, self).rest_args

        rest_args = list(self._wpt_run_args)
        rest_args.extend([
            # By default, wpt will treat unexpected passes as errors, so we
            # disable that to be consistent with Chromium CI.
            '--no-fail-on-unexpected-pass',
            self.wpt_product_name(),
            '--no-pause-after-test',
            '--no-capture-stdio',
            '--no-manifest-download',
            '--tests=%s' % self.wpt_root_dir,
            '--mojojs-path=%s' % self.mojo_js_directory,
            '--repeat=%d' % self.options.repeat,
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

        rest_args.extend(wpt_args)
        return rest_args

    def do_post_test_run_tasks(self):
        if not self.wpt_output and self.options:
            self.wpt_output = self.options.isolated_script_test_output

        command = [
            self.select_python_executable(),
            os.path.join(BLINK_TOOLS_DIR, 'wpt_process_results.py'),
            '--verbose',
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
        if self.wptreport:
            command.extend(['--wpt-report', self.wptreport])
        common.run_command(command)

    @classmethod
    def wpt_product_name(cls):
        raise NotImplementedError

    def _default_wpt_report(self):
        product = self.wpt_product_name()
        shard_index = os.environ.get('GTEST_SHARD_INDEX')
        if shard_index is not None:
            return 'wpt_reports_%s_%02d.json' % (product, int(shard_index))
        return 'wpt_reports_%s.json' % product
