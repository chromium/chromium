#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs WPT as an isolate bundle.

This script maps flags supported by run_isolate_script_test.py to flags that are
understood by WPT.

Here's the mapping [isolate script flag] : [wpt flag]
--isolated-script-test-output : --log-chromium
--total-shards : --total-chunks
--shard-index : -- this-chunk
"""

import json
import logging
import os
import shutil
import sys

import common
import wpt_common

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
WEB_TESTS_DIR = os.path.join(SRC_DIR, "third_party", "blink", "web_tests")
# OUT_DIR is a format string that will later be substituted into for the
# specific output directory.
OUT_DIR = os.path.join(SRC_DIR, "out", "{}")

# The checked-in manifest is copied to a temporary working directory so it can
# be mutated by wptrunner
WPT_CHECKED_IN_MANIFEST = os.path.join(
    WEB_TESTS_DIR, "external", "WPT_BASE_MANIFEST_8.json")
WPT_WORKING_COPY_MANIFEST = os.path.join(OUT_DIR, "MANIFEST.json")

WPT_CHECKED_IN_METADATA_DIR = os.path.join(WEB_TESTS_DIR, "external", "wpt")
WPT_METADATA_OUTPUT_DIR = os.path.join(OUT_DIR, "wpt_expectations_metadata")
WPT_OVERRIDE_EXPECTATIONS_PATH = os.path.join(
    WEB_TESTS_DIR, "WPTOverrideExpectations")

CHROME_BINARY = os.path.join(OUT_DIR, "chrome")
CHROME_BINARY_MAC = os.path.join(
    OUT_DIR, "Chromium.app", "Contents", "MacOS", "Chromium")
CHROMEDRIVER_BINARY = os.path.join(OUT_DIR, "chromedriver")


class WPTTestAdapter(wpt_common.BaseWptScriptAdapter):

    @property
    def rest_args(self):
        rest_args = super(WPTTestAdapter, self).rest_args

        chrome = CHROME_BINARY.format(self.options.target)
        chromedriver = CHROMEDRIVER_BINARY.format(self.options.target)
        if self.port.host.platform.is_win():
            chrome = "%s.exe" % chrome
            chromedriver = "%s.exe" % chromedriver
        elif self.port.host.platform.is_mac():
            chrome = CHROME_BINARY_MAC.format(self.options.target)

        # Here we add all of the arguments required to run WPT tests on Chrome.
        rest_args.extend([
            "--binary=" + chrome,
            "--binary-arg=--host-resolver-rules="
                "MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1",
            "--binary-arg=--enable-experimental-web-platform-features",
            "--binary-arg=--enable-blink-test-features",
            "--binary-arg=--enable-blink-features=MojoJS,MojoJSTest",
            "--binary-arg=--disable-field-trial-config",
            "--webdriver-binary=" + chromedriver,
            "--webdriver-arg=--enable-chrome-logs",
            "--headless",
            # Exclude webdriver tests for now. They are run separately on the CI
            "--exclude=webdriver",
            "--exclude=infrastructure/webdriver",
            "--metadata", WPT_METADATA_OUTPUT_DIR.format(self.options.target),
            # By specifying metadata above, WPT will try to find manifest in the
            # metadata directory. So here we point it back to the correct path
            # for the manifest.
            # TODO(lpz): Allowing WPT to rebuild its own manifest temporarily to
            # gauge performance impact. Issue with specifying the base manifest
            # below is that it can get stale if tests are renamed, and requires
            # a lengthy import/export cycle to refresh. So we allow WPT to
            # update the manifest in cast it's stale.
            #"--no-manifest-update",
            "--manifest", WPT_WORKING_COPY_MANIFEST.format(self.options.target),
            # See if multi-processing affects timeouts.
            # TODO(lpz): Consider removing --processes and compute automatically
            # from multiprocessing.cpu_count()
            "--processes=" + self.options.child_processes,
        ])

        tests = list(self.options.test_list)
        if self.options.test_filter:
          tests.extend(self.options.test_filter.split(":"))
        for maybe_test_prefix in tests:
          if maybe_test_prefix.startswith("-"):
            # TODO(crbug/1274933#c14): Here, we pass through args that appear to
            # be "wpt run" options, so as not to break existing usage. Options
            # must be formatted as a single arg ('--<option>=<value>', not
            # '--<option> <value>'), since there's no way to tell how many args
            # '<option>' consumes.
            rest_args.append(maybe_test_prefix)
            logging.warning(
                "Are you trying to pass options to 'wpt run'? "
                "Try removing the end-of-options marker ('--') before '%s'.",
                maybe_test_prefix)
          else:
            rest_args.extend([
              "--include",
              self.path_finder.strip_wpt_path(maybe_test_prefix),
            ])

        return rest_args

    def add_extra_arguments(self, parser):
        super(WPTTestAdapter, self).add_extra_arguments(parser)
        child_processes_help = "Number of drivers to run in parallel"
        parser.add_argument("-j", "--child-processes", dest="child_processes",
                            default="1", help=child_processes_help)
        parser.add_argument("--test-filter", "--gtest_filter",
                            help="Colon-separated list of test names "
                                 "(URL prefixes)")
        parser.add_argument("test_list", nargs="*",
                            help="List of tests or test directories to run")

    @classmethod
    def wpt_product_name(cls):
        return 'chrome'

    def do_pre_test_run_tasks(self):
        super(WPTTestAdapter, self).do_pre_test_run_tasks()
        # Copy the checked-in manifest to the temporary working directory
        shutil.copy(WPT_CHECKED_IN_MANIFEST,
                    WPT_WORKING_COPY_MANIFEST.format(self.options.target))

        # Generate WPT metadata files.
        common.run_command([
            sys.executable,
            os.path.join(wpt_common.BLINK_TOOLS_DIR, 'build_wpt_metadata.py'),
            "--metadata-output-dir",
            WPT_METADATA_OUTPUT_DIR.format(self.options.target),
            "--additional-expectations",
            WPT_OVERRIDE_EXPECTATIONS_PATH,
            "--checked-in-metadata-dir",
            WPT_CHECKED_IN_METADATA_DIR,
            "--no-process-baselines",
        ])


def main():
    adapter = WPTTestAdapter()
    return adapter.run_test()


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())
