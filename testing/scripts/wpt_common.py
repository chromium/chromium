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
        self.wptreport = None
        self._include_filename = None
        self.layout_test_results_subdir = 'layout-test-results'

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
