#!/usr/bin/env vpython
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

DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT = os.path.join(OUT_DIR, "results.json")
MOJO_JS_PATH = os.path.join(OUT_DIR, "gen")

TESTS_ROOT_DIR = os.path.join(WEB_TESTS_DIR, "external", "wpt")

WPT_BINARY = os.path.join(SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")

class WPTTestAdapter(wpt_common.BaseWptScriptAdapter):

    @property
    def rest_args(self):
        rest_args = super(WPTTestAdapter, self).rest_args

        # Update the output directory to the default if it's not set.
        self.maybe_set_default_isolated_script_test_output()

        chrome = CHROME_BINARY.format(self.options.target)
        chromedriver = CHROMEDRIVER_BINARY.format(self.options.target)
        if self.port.host.platform.is_win():
            chrome = "%s.exe" % chrome
            chromedriver = "%s.exe" % chromedriver
        elif self.port.host.platform.is_mac():
            chrome = CHROME_BINARY_MAC.format(self.options.target)

        # Here we add all of the arguments required to run WPT tests on Chrome.
        rest_args.extend([
            WPT_BINARY,
            "--venv=" + SRC_DIR,
            "--skip-venv-setup",
            # TODO(crbug.com/1166741): We should be running WPT under Python 3.
            "--py2",
            "run",
            "chrome"
        ] + self.options.test_list + [
            "--binary=" + chrome,
            "--binary-arg=--host-resolver-rules="
                "MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1",
            "--binary-arg=--enable-experimental-web-platform-features",
            "--binary-arg=--enable-blink-test-features",
            "--binary-arg=--enable-blink-features=MojoJS,MojoJSTest",
            "--webdriver-binary=" + chromedriver,
            "--webdriver-arg=--enable-chrome-logs",
            "--headless",
            "--no-capture-stdio",
            "--no-manifest-download",
            "--no-pause-after-test",
            # Exclude webdriver tests for now. They are run separately on the CI
            "--exclude=webdriver",
            "--exclude=infrastructure/webdriver",
            # Setting --no-fail-on-unexpected makes the return code of wpt 0
            # even if there were test failures. The CQ doesn't like this since
            # it uses the exit code to determine which shards to retry (ie:
            # those that had non-zero exit codes).
            #"--no-fail-on-unexpected",
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
            # (crbug.com/1023835) The flags below are temporary to aid debugging
            "--log-mach=-",
            "--log-mach-verbose",
            # See if multi-processing affects timeouts.
            # TODO(lpz): Consider removing --processes and compute automatically
            # from multiprocessing.cpu_count()
            "--processes=" + self.options.child_processes,
            "--mojojs-path=" + MOJO_JS_PATH.format(self.options.target),
            "--tests=" + TESTS_ROOT_DIR,
        ])
        return rest_args

    def add_extra_arguments(self, parser):
        target_help = "Specify the target build subdirectory under src/out/"
        parser.add_argument("-t", "--target", dest="target", default="Release",
                            help=target_help)
        child_processes_help = "Number of drivers to run in parallel"
        parser.add_argument("-j", "--child-processes", dest="child_processes",
                            default="1", help=child_processes_help)
        parser.add_argument("test_list", nargs="*",
                            help="List of tests or test directories to run")

    def maybe_set_default_isolated_script_test_output(self):
        if self.options.isolated_script_test_output:
            return
        default_value = DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT.format(
            self.options.target)
        print("--isolated-script-test-output not set, defaulting to %s" %
              default_value)
        self.options.isolated_script_test_output = default_value

    def do_pre_test_run_tasks(self):
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
            "--no-handle-annotations"
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
