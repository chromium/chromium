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
_WPT_ROOT_FRAGMENT = posixpath.join('external', 'wpt', '')
TESTS_ROOT_DIR = os.path.join(WEB_TESTS_DIR, 'external', 'wpt')

if BLINK_TOOLS_DIR not in sys.path:
    sys.path.append(BLINK_TOOLS_DIR)

if TYP_DIR not in sys.path:
    sys.path.append(TYP_DIR)

from blinkpy.common.host import Host


def strip_wpt_root_prefix(path):
    """Remove a redundant prefix from a WPT path.

    ResultDB identifies WPTs as web tests with the prefix "external/wpt", but
    wptrunner expects paths relative to the WPT root, which already ends in
    "external/wpt". This function transforms a general web test path into a
    WPT path.

    WPT paths are always POSIX-style.
    """
    if path.startswith(_WPT_ROOT_FRAGMENT):
        return posixpath.relpath(path, _WPT_ROOT_FRAGMENT)
    # Path is absolute or does not start with the prefix.
    # Assume the path already points to a valid WPT and pass through.
    return path


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
        self.port = host.port_factory.get()
        # Path to the output of the test run. Comes from the args passed to the
        # run, parsed after this constructor. Can be overwritten by tests.
        self.wpt_output = None
        self.wptreport = None
        self.layout_test_results_subdir = 'layout-test-results'
        default_wpt_binary = os.path.join(
            common.SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")
        self.wpt_binary = os.environ.get("WPT_BINARY") or default_wpt_binary

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

    def do_post_test_run_tasks(self):
        if not self.wpt_output and self.options:
            self.wpt_output = self.options.isolated_script_test_output

        command = [
            sys.executable,
            os.path.join(BLINK_TOOLS_DIR, 'wpt_process_results.py'),
            '--verbose',
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
