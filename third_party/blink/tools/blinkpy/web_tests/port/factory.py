# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Factory method to retrieve the appropriate port implementation."""

import argparse
import fnmatch
import optparse
import os
import re
import shlex
import sys
from copy import deepcopy
from typing import List

from blinkpy.common.path_finder import PathFinder


class PortFactory:
    PORT_CLASSES = (
        'android.AndroidPort',
        'fuchsia.FuchsiaPort',
        'ios.IOSPort',
        'linux.LinuxPort',
        'mac.MacPort',
        'mock_drt.MockDRTPort',
        'test.TestPort',
        'webview.WebviewPort',
        'win.WinPort',
    )

    def __init__(self, host):
        self._host = host

    def _default_port(self):
        platform = self._host.platform
        if platform.is_linux() or platform.is_freebsd():
            return 'linux'
        elif platform.is_mac():
            return 'mac'
        elif platform.is_win():
            return 'win'
        raise NotImplementedError('unknown platform: %s' % platform)

    def get(self, port_name=None, options=None, **kwargs):
        """Returns an object implementing the Port interface.

        If port_name is None, this routine attempts to guess at the most
        appropriate port on this platform.
        """
        port_name = port_name or self._default_port()
        port_options = deepcopy(options) or optparse.Values()

        _update_configuration_and_target(self._host.filesystem, port_options)

        port_class, class_name = self.get_port_class(port_name)
        if port_class is None:
            raise NotImplementedError('unsupported platform: "%s"' % port_name)

        full_port_name = port_class.determine_full_port_name(
            self._host, port_options,
            class_name if 'browser_test' in port_name else port_name)
        return port_class(self._host,
                          full_port_name,
                          options=port_options,
                          **kwargs)

    @classmethod
    def get_port_class(cls, port_name):
        """Returns a Port subclass and its name for the given port_name."""
        if 'browser_test' in port_name:
            module_name, class_name = port_name.rsplit('.', 1)
            try:
                module = __import__(module_name, globals(), locals(), [], -1)
            except ValueError:
                # Python3 doesn't allow the level param to be -1. Setting it to
                # 1 searches for modules in 1 parent directory.
                module = __import__(module_name, globals(), locals(), [], 1)
            port_class_name = module.get_port_class_name(class_name)
            if port_class_name is not None:
                return module.__dict__[port_class_name], class_name
        else:
            for port_class in cls.PORT_CLASSES:
                module_name, class_name = port_class.rsplit('.', 1)
                try:
                    module = __import__(module_name, globals(), locals(), [],
                                        -1)
                except ValueError:
                    # Python3 doesn't allow the level param to be -1. Setting it
                    # to 1 searches for modules in 1 parent directory.
                    module = __import__(module_name, globals(), locals(), [],
                                        1)
                port_class = module.__dict__[class_name]
                if port_name.startswith(port_class.port_name):
                    return port_class, class_name
        return None, None

    def all_port_names(self, platform=None):
        """Returns a list of all valid, fully-specified, "real" port names.

        This is the list of directories that are used as actual baseline_paths()
        by real ports. This does not include any "fake" names like "test"
        or "mock-mac", and it does not include any directories that are not
        port names.

        If platform is not specified, all known port names will be returned.
        """
        platform = platform or '*'
        return fnmatch.filter(self._host.builders.all_port_names(), platform)

    def get_from_builder_name(self, builder_name):
        port_name = self._host.builders.port_name_for_builder_name(
            builder_name)
        assert port_name, 'unrecognized builder name: "%s"' % builder_name
        return self.get(port_name, options=_builder_options(builder_name))


def platform_options(use_globs=False):
    return [
        optparse.make_option(
            '--platform',
            action='store',
            help=('Glob-style list of platform/ports to use (e.g., "mac*")'
                  if use_globs else 'Platform to use (e.g., "mac-lion")')),
    ]


def configuration_options():
    return [
        optparse.make_option('--debug',
                             action='store_const',
                             const='Debug',
                             dest='configuration',
                             help='Set the configuration to Debug'),
        optparse.make_option(
            '-t',
            '--target',
            dest='target',
            help='Specify the target build subdirectory under //out/'),
        optparse.make_option('--release',
                             action='store_const',
                             const='Release',
                             dest='configuration',
                             help='Set the configuration to Release'),
        optparse.make_option('--no-xvfb',
                             action='store_false',
                             dest='use_xvfb',
                             default=True,
                             help='Do not run tests with Xvfb'),
    ]


def wpt_options():
    return [
        optparse.make_option(
            '--no-manifest-update',
            dest='manifest_update',
            action='store_false',
            default=True,
            help=('Do not update the web-platform-tests '
                  'MANIFEST.json unless it does not exist.')),
    ]


# TODO(crbug.com/1431070): Remove the `*_options` methods above once all tools
# use `argparse`.
def add_platform_options_group(parser: argparse.ArgumentParser):
    group = parser.add_argument_group('Platform Options')
    group.add_argument('--platform', help='Platform to use (e.g., "mac-lion")')


def add_common_wpt_options(parser: argparse.ArgumentParser):
    parser.add_argument('--no-manifest-update',
                        dest='manifest_update',
                        action='store_false',
                        help=('Do not update the web-platform-tests '
                              'MANIFEST.json unless it does not exist.'))


def add_configuration_options_group(parser: argparse.ArgumentParser,
                                    rwt: bool = True,
                                    product_choices: list = None):
    group = parser.add_argument_group('Configuration Options')
    group.add_argument(
        '-t',
        '--target',
        help='Specify the target build subdirectory under //out')
    group.add_argument('--debug',
                       action='store_const',
                       const='Debug',
                       dest='configuration',
                       help='Set the configuration to Debug')
    group.add_argument('--release',
                       action='store_const',
                       const='Release',
                       dest='configuration',
                       help='Set the configuration to Release')
    group.add_argument('--chrome-branded',
                       action='store_true',
                       help='Set the configuration as chrome_branded.')
    group.add_argument('--no-xvfb',
                       action='store_false',
                       dest='use_xvfb',
                       help='Do not run tests with Xvfb')
    group.add_argument('--coverage-dir', type=str, help=argparse.SUPPRESS)
    add_common_wpt_options(group)
    if not rwt:
        group.add_argument(
            '-p',
            '--product',
            default='headless_shell',
            choices=(product_choices or []),
            metavar='PRODUCT',
            help='Product (browser or browser component) to test.')
        group.add_argument('--no-headless',
                           action='store_false',
                           dest='headless',
                           help=('Do not run browser in headless mode.'))
        group.add_argument('--webdriver-binary',
                           metavar='PATH',
                           type=str,
                           help='Alternate path of the webdriver binary.')
        group.add_argument(
            '--use-upstream-wpt',
            action='store_true',
            help=
            ('CI only parameter. Use tests and tools from upstream WPT GitHub repo. '
             'Used to create wpt reports for uploading to wpt.fyi.'))


def add_results_options_group(parser: argparse.ArgumentParser,
                              rwt: bool = True):
    results_group = parser.add_argument_group('Results Options')
    results_group.add_argument(
        '--flag-specific',
        help=('Name of a flag-specific configuration defined in '
              'FlagSpecificConfig.'))
    results_group.add_argument(
        '--additional-driver-flag',
        '--additional-drt-flag',
        metavar='FLAG',
        dest='additional_driver_flag',
        action='append',
        default=[],
        help=('Additional command line flag to pass to the driver. '
              'Specify multiple times to add multiple flags.'))
    results_group.add_argument(
        '--build-directory',
        metavar='PATH',
        default='out',
        help=(
            'Path to the directory where build files are kept, not including '
            'configuration. In general this will be "out".'))
    results_group.add_argument(
        '--clobber-old-results',
        action='store_true',
        help='Clobbers test results from previous runs.')
    results_group.add_argument(
        '--json-test-results',  # New name from json_results_generator
        '--write-full-results-to',  # Old argument name
        '--isolated-script-test-output',  # Isolated API
        metavar='PATH',
        help='Path to write the JSON test results for *all* tests.')
    results_group.add_argument(
        '--write-run-histories-to',
        metavar='PATH',
        help='Path to write the JSON test run histories.')
    results_group.add_argument(
        '--no-show-results',
        dest='show_results',
        action='store_false',
        help="Don't launch a browser with results after the tests are done")
    results_group.add_argument('--results-directory',
                               metavar='PATH',
                               help='Location of test results')
    results_group.add_argument('--smoke',
                               action='store_true',
                               default=None,
                               help='Run just the SmokeTests')
    results_group.add_argument('--no-smoke',
                               dest='smoke',
                               action='store_false',
                               default=None,
                               help='Do not run just the SmokeTests')
    results_group.add_argument(
        '--additional-expectations',
        action='append',
        default=[],
        help=('Path to a test_expectations file that will override previous '
              'expectations. Specify multiple times for multiple sets of '
              'overrides.'))
    results_group.add_argument('--driver-name',
                               help='Alternative driver binary to use')
    if rwt:
        results_group.add_argument(
            '--ignore-default-expectations',
            action='store_true',
            help='Do not use the default set of TestExpectations files.')
        results_group.add_argument(
            '--no-expectations',
            action='store_true',
            help=('Do not use TestExpectations, only run the tests without '
                  'reporting any results. Useful for generating code '
                  'coverage reports.'))
        results_group.add_argument(
            '--additional-platform-directory',
            action='append',
            default=[],
            help=(
                'Additional directory where to look for test baselines (will '
                'take precedence over platform baselines). Specify multiple '
                'times to add multiple search path entries.'))
        results_group.add_argument(
            '--compare-port', help="Use the specified port's baselines first")
        results_group.add_argument(
            '--copy-baselines',
            action='store_true',
            help=(
                'If the actual result is different from the current baseline, '
                'copy the current baseline into the *most-specific-platform* '
                'directory, or the flag-specific generic-platform directory if '
                '--additional-driver-flag is specified. See --reset-results.'))
        results_group.add_argument(
            '--reset-results',
            action='store_true',
            help=
            ('Reset baselines to the generated results in their existing '
             'location or the default location if no baseline exists. For '
             'virtual tests, reset the virtual baselines. If '
             '--additional-driver-flag is specified, reset the flag-specific '
             'baselines. If --copy-baselines is specified, the copied '
             'baselines will be reset.'))
    else:
        results_group.add_argument(
            '--reset-results',
            action='store_true',
            help=('Reset expectations in test metadata to the generated '
                  'results. Without existing platform-specific expectations, '
                  'extend local results to all platforms. If `--product` or '
                  '`--flag-specific` is specified, only reset expectations '
                  'for that product or flag. Virtual expectations are always '
                  'updated per-suite.'))


def add_testing_options_group(parser: argparse.ArgumentParser,
                              rwt: bool = True):
    testing_group = parser.add_argument_group('Testing Options')
    testing_group.add_argument(
        '--additional-env-var',
        metavar='NAME=VALUE',
        action='append',
        default=[],
        help='Pass an environment variable NAME to the test driver.')
    testing_group.add_argument('--child-processes',
                               '--jobs',
                               '-j',
                               metavar='N',
                               type=int,
                               help='Number of drivers to run in parallel.')
    testing_group.add_argument(
        '--enable-leak-detection',
        action='store_true',
        help='Enable the leak detection of DOM objects.')
    testing_group.add_argument(
        '--enable-sanitizer',
        action='store_true',
        help='Only alert on sanitizer-related errors and crashes')
    testing_group.add_argument(
        '--exit-after-n-crashes-or-timeouts',
        metavar='N',
        type=int,
        default=None,
        help='Exit after the first N crashes instead of running all tests')
    testing_group.add_argument(
        '--exit-after-n-failures',
        metavar='N',
        type=int,
        default=None,
        help='Exit after the first N failures instead of running all tests')
    testing_group.add_argument(
        '--iterations',
        '--isolated-script-test-repeat',
        # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
        '--gtest_repeat',
        metavar='N',
        type=int,
        default=1,
        help='Number of times to run the set of tests (e.g. ABCABCABC)')
    testing_group.add_argument(
        '--repeat-each',
        metavar='N',
        type=int,
        default=1,
        help='Number of times to run each test (e.g. AAABBBCCC)')
    testing_group.add_argument(
        '--num-retries',
        '--test-launcher-retry-limit',
        '--isolated-script-test-launcher-retry-limit',
        metavar='N',
        type=int,
        default=None,
        help=('Number of times to retry failures. Default (when this '
              'flag is not specified) is to retry 3 times, unless an '
              'explicit list of tests is given. '
              'If a non-zero value is given explicitly, failures are '
              'retried regardless.'))
    testing_group.add_argument(
        '--no-retry-failures',
        dest='num_retries',
        action='store_const',
        const=0,
        help="Don't retry any failures (equivalent to --num-retries=0).")
    testing_group.add_argument(
        '--total-shards',
        metavar='SHARDS',
        type=int,
        help=('Total number of shards being used for this test run. '
              'Must be used with --shard-index. '
              '(The user of this script is responsible for spawning '
              'all of the shards.)'))
    testing_group.add_argument(
        '--shard-index',
        metavar='INDEX',
        type=int,
        help=('Shard index [0..total_shards) of this test run. '
              'Must be used with --total-shards.'))
    testing_group.add_argument('--test-list',
                               action='append',
                               metavar='FILE',
                               help='read filters for tests to run')
    testing_group.add_argument(
        '--inverted-test-launcher-filter-file',
        action='append',
        metavar='FILE',
        help=('Filters in the file will be inverted before applied.'))
    testing_group.add_argument(
        '--isolated-script-test-filter-file',
        '--test-launcher-filter-file',
        action='append',
        metavar='FILE',
        help=('read filters for tests to not run as if '
              'they were specified on the command line'))
    testing_group.add_argument(
        '--isolated-script-test-filter',
        action='append',
        help=('A list of test globs to run or skip, separated by TWO colons, '
              'e.g. fast::css/test.html; prefix the glob with "-" to skip it'))
    # TODO(crbug.com/893235): Remove gtest_filter when FindIt no longer uses it.
    testing_group.add_argument(
        '--gtest_filter',
        help=('A colon-separated list of tests to run. Wildcards are '
              'NOT supported. It is the same as listing the tests as '
              'positional arguments.'))
    testing_group.add_argument(
        '-i',
        '--ignore-tests',
        action='append',
        default=[],
        help='directories or test to ignore (may specify multiple times)')
    testing_group.add_argument(
        '--zero-tests-executed-ok',
        action='store_true',
        help=('If set, exit with a success code when no tests are run. '
              'Used on trybots when web tests are retried without patch.'))
    testing_group.add_argument(
        '--wrapper',
        type=command_wrapper,
        default=[],
        help=('Wrapper command to insert before invocations of the driver; '
              'option is split on whitespace before running. (Example: '
              '--wrapper="valgrind --smc-check=all")'))
    testing_group.add_argument('-f',
                               '--fully-parallel',
                               action='store_true',
                               help='run all tests in parallel')
    testing_group.add_argument(
        '--skipped',
        help=('Control how tests marked SKIP are run. '
              '"default" == Skip tests unless explicitly listed on the '
              'command line, "ignore" == Run them anyway, '
              '"only" == only run the SKIP tests, '
              '"always" == always skip, even if listed on the command line.'))
    testing_group.add_argument(
        '--skip-failing-tests',
        action='store_true',
        help=('Skip tests that are expected to fail. Note: When using this '
              'option, you might miss new crashes in these tests.'))
    testing_group.add_argument(
        '--skip-timeouts',
        action='store_true',
        help=('Skip tests marked TIMEOUT. Use it to speed up running the '
              'entire test suite.'))
    if rwt:
        testing_group.add_argument(
            '--build',
            action='store_true',
            default=True,
            help=('Check to ensure the build is up to date (default).'))
        testing_group.add_argument(
            '--no-build',
            dest='build',
            action='store_false',
            help="Don't check to see if the build is up to date.")
        testing_group.add_argument('--wpt-only',
                                   action='store_true',
                                   help='Run web platform tests only.')
        testing_group.add_argument(
            '--disable-breakpad',
            action='store_true',
            help="Don't use breakpad to symbolize unexpected crashes.")
        testing_group.add_argument(
            '--driver-logging',
            action='store_true',
            help='Print detailed logging of the driver/content_shell')
        testing_group.add_argument(
            '--enable-tracing',
            help=(
                'Capture and write a trace file with the specified '
                'categories for each test. Passes appropriate --trace-startup '
                'flags to the driver. If in doubt, use "*". '
                'This implies --restart-shell-between-tests=always.'))
        testing_group.add_argument(
            '--enable-per-test-tracing',
            action='store_true',
            help=(
                'Capture and write a trace file with all tracing '
                'categories enabled for each test. Unlike --enable-tracing, '
                'this excludes driver startup/shutdown in the trace, and does '
                'not imply --restart-shell-between-tests=always.'))
        testing_group.add_argument(
            '--fuzzy-diff',
            action='store_true',
            help=(
                'When running tests on an actual GPU, variance in pixel output '
                'can lead to image differences causing failed expectations. '
                'Using a fuzzy diff instead accounts for this variance. '
                'See //tools/imagediff/image_diff.cc'))
        testing_group.add_argument(
            '--ignore-builder-category',
            help=(
                'The category of builders to use with the --ignore-flaky-tests '
                "option ('layout' or 'deps')."))
        testing_group.add_argument(
            '--ignore-flaky-tests',
            help=
            ('Control whether tests that are flaky on the bots get ignored. '
             "'very-flaky' == Ignore any tests that flaked more than once on "
             "the bot. 'maybe-flaky' == Ignore any tests that flaked once on "
             "the bot. 'unexpected' == Ignore any tests that had unexpected "
             'results on the bot.'))
        testing_group.add_argument('--layout-tests-directory',
                                   help='Path to a custom web tests directory')
        testing_group.add_argument(
            '--max-locked-shards',
            type=int,
            default=0,
            help='Set the maximum number of locked shards')
        testing_group.add_argument(
            '--nocheck-sys-deps',
            action='store_true',
            help="Don't check the system dependencies (themes)")
        testing_group.add_argument(
            '--order',
            choices=['none', 'random', 'natural'],
            default='random',
            help=
            ('Determine the order in which the test cases will be run. '
             "'none' == use the order in which the tests were listed "
             'either in arguments or test list, '
             "'random' == pseudo-random order (default). Seed can be "
             "specified via --seed, otherwise it will default to the current "
             "unix timestamp. 'natural' == use the natural order"))
        testing_group.add_argument('--profile',
                                   action='store_true',
                                   help='Output per-test profile information.')
        testing_group.add_argument(
            '--profiler',
            help=('Output per-test profile information, '
                  'using the specified profiler.'))
        testing_group.add_argument(
            '--restart-shell-between-tests',
            choices=['always', 'never', 'on_retry'],
            default='on_retry',
            help=(
                'Restarting the shell between tests produces more '
                'consistent results, as it prevents state from carrying over '
                'from previous tests. It also increases test run time by at '
                'least 2X. By default, the shell is restarted when tests get '
                'retried, since leaking state between retries can sometimes '
                'mask underlying flakiness, and the whole point of retries is '
                'to look for flakiness.'))
        testing_group.add_argument(
            '--seed',
            type=int,
            help=('Seed to use for random test order (default: %(default)s). '
                  'Only applicable in combination with --order=random.'))
        testing_group.add_argument(
            '--isolated-script-test-also-run-disabled-tests',
            # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
            '--gtest_also_run_disabled_tests',
            action='store_const',
            const='ignore',
            dest='skipped',
            help=('Equivalent to --skipped=ignore.'))
        testing_group.add_argument('--timeout-ms',
                                   type=float,
                                   help='Set the timeout for each test')
        testing_group.add_argument(
            '--fastest',
            type=float,
            help=('Run the N%% fastest tests as well as any tests listed '
                  'on the command line'))
        testing_group.add_argument(
            '--initialize-webgpu-adapter-at-startup-timeout-ms',
            type=float,
            help='Initialize WebGPU adapter before running any tests.')
        testing_group.add_argument(
            '--virtual-parallel',
            action='store_true',
            help=(
                'When running in parallel, include virtual tests. Useful for '
                'running a single virtual test suite, but will be slower '
                'in other cases.'))
        testing_group.add_argument(
            '-n',
            '--dry-run',
            action='store_true',
            help='Do everything but actually run the tests or upload results.')
        testing_group.add_argument(
            '-w',
            '--watch',
            action='store_true',
            help='Re-run tests quickly (e.g. avoid restarting the server)')
        testing_group.add_argument(
            '--driver-kill-timeout-secs',
            type=float,
            default=1.0,
            help=(
                'Number of seconds to wait before killing a driver, and the '
                'main use case is to leave enough time to allow the process to '
                'finish post-run hooks, such as dumping code coverage data. '
                'Default is 1 second, can be overriden for specific use cases.'
            ))
        testing_group.add_argument(
            '--kill-driver-with-sigterm',
            action='store_true',
            help=(
                'Send SIGTERM to the driver process; useful in conjunction '
                'with "--wrapper", for wrapper executables (such as rr) that '
                'require SIGTERM to finish cleanly.'))
        testing_group.add_argument(
            '--ignore-testharness-expected-txt',
            action='store_true',
            help=('Ignore *-expected.txt for all testharness tests. All '
                  'testharness test failures will be shown, even if the '
                  'failures are expected in *-expected.txt.'))
    else:
        test_types = [
            'testharness',
            'reftest',
            'wdspec',
            'crashtest',
            'print-reftest',
            'manual',
        ]
        testing_group.add_argument(
            '--timeout-multiplier',
            type=float,
            help='Multiplier relative to standard test timeouts to use')
        testing_group.add_argument(
            '--test-types',
            nargs='*',
            choices=test_types,
            default=[
                'testharness', 'reftest', 'crashtest', 'print-reftest',
                'wdspec'
            ],
            metavar='TYPE',
            help=f'Test types to run (choices: {", ".join(test_types)})')
        testing_group.add_argument('--no-virtual-tests',
                                   action='store_true',
                                   default=None,
                                   help=('Do not run virtual tests.'))
        testing_group.add_argument('--no-wpt-internal',
                                   action='store_false',
                                   dest='run_wpt_internal',
                                   help='Do not run internal WPTs.')


# for run_wpt_tests.py only
def add_android_options_group(parser: argparse.ArgumentParser):
    group = parser.add_argument_group(
        'Android Options',
        'Options for configuring Android devices and tooling. '
        'Ignored for non-Android products.')
    group.add_argument(
        '--avd-config',
        help=('Path to the avd config. Used by the test runner to launch '
              'Android emulators. Required when there is no android '
              'emulators running, or no devices connected to the system. '
              '(See //tools/android/avd/proto for message definition '
              'and existing *.textpb files.)'))
    group.add_argument('--emulator-window',
                       action='store_true',
                       help='Enable graphical window display on the emulator.')
    group.add_argument(
        '--browser-apk',
        metavar='APK',
        help=(
            'Specify path under //out/ of the browser APK to install and run. '
            'For WebView, this should point to the shell. '
            'The default value is apks/ChromePublic.apk for Chrome Android, '
            'and apks/SystemWebViewShell.apk for WebView.'))
    group.add_argument(
        '--webview-provider',
        metavar='PATH',
        help=(
            'Specify path under //out/ of the WebView provider APK to install. '
            'The default value is apks/SystemWebView.apk.'))
    group.add_argument('--additional-apk',
                       metavar='APK',
                       type=os.path.abspath,
                       action='append',
                       default=[],
                       help='Path to additional APKs to install')
    group.add_argument('--no-install',
                       action='store_true',
                       help=('Do not install packages to devices. '
                             'Use the packages preinstalled.'))
    return group


def add_ios_options_group(parser: argparse.ArgumentParser):
    group = parser.add_argument_group(
        'iOS Options', 'Options for configuring iOS devices and tooling. '
        'Ignored for non-`chrome_ios` products.')
    group.add_argument('--xcode-build-version',
                       help='Xcode build version to install.',
                       metavar='VERSION')
    return group


def add_logging_options_group(parser: argparse.ArgumentParser):
    group = parser.add_argument_group('Logging Options')
    group.add_argument(
        '-v',
        '--verbose',
        action='count',
        default=0,
        help=('Increase verbosity (may provide multiple times). '
              'Providing at least once will dump browser logs.')),
    # TODO: when using run_wpt_tests.py on swarming, we should run
    # that inside run_isolated_script_test.py so that we can remove
    # the workaround below
    group.add_argument('--isolated-outdir', help=argparse.SUPPRESS),
    group.add_argument('--isolated-script-test-also-run-disabled-tests',
                       action='store_true',
                       help=argparse.SUPPRESS),
    group.add_argument('--isolated-script-test-chartjson-output',
                       help=argparse.SUPPRESS),
    group.add_argument('--isolated-script-test-perf-output',
                       help=argparse.SUPPRESS),
    group.add_argument('--script-type', help=argparse.SUPPRESS)


def _builder_options(builder_name):
    return optparse.Values({
        'builder_name':
        builder_name,
        'configuration':
        'Debug' if re.search(r'[d|D](ebu|b)g', builder_name) else 'Release',
        'target':
        None,
    })


def _update_configuration_and_target(host, options):
    """Updates options.configuration and options.target based on a best guess."""
    if not getattr(options, 'target', None):
        options.target = getattr(options, 'configuration', None) or 'Release'

    gn_configuration = _read_configuration_from_gn(host, options)
    if gn_configuration:
        expected_configuration = getattr(options, 'configuration', None)
        if expected_configuration not in (None, gn_configuration):
            raise ValueError('Configuration does not match the GN build args. '
                             'Expected "%s" but got "%s".' %
                             (expected_configuration, gn_configuration))
        options.configuration = gn_configuration
        return

    if getattr(options, 'configuration', None):
        return

    if options.target in ('Debug', 'Debug_x64'):
        options.configuration = 'Debug'
    elif options.target in ('Release', 'Release_x64'):
        options.configuration = 'Release'
    else:
        raise ValueError(
            'Could not determine build configuration type.\n'
            'Either switch to one of the default target directories,\n'
            'use args.gn, or specify --debug or --release explicitly.\n'
            'If the directory is out/<dir>, then pass -t <dir>.')


def _read_configuration_from_gn(fs, options):
    """Returns the configuration to used based on args.gn, if possible."""
    build_directory = getattr(options, 'build_directory', 'out')
    target = options.target
    finder = PathFinder(fs)
    path = fs.join(finder.chromium_base(), build_directory, target, 'args.gn')
    if not fs.exists(path):
        path = fs.join(finder.chromium_base(), build_directory, target,
                       'toolchain.ninja')
        if not fs.exists(path):
            # This does not appear to be a GN-based build directory, so we don't know
            # how to interpret it.
            return None

        # toolchain.ninja exists, but args.gn does not; this can happen when
        # `gn gen` is run with no --args.
        return 'Debug'

    args = fs.read_text_file(path)
    for line in args.splitlines():
        if re.match(r'^\s*is_debug\s*=\s*false(\s*$|\s*#.*$)', line):
            return 'Release'

    # If is_debug is not set, the default is based on if is_official_build
    # is set to true.
    for line in args.splitlines():
        if re.match(r'^\s*is_official_build\s*=\s*true(\s*$|\s*#.*$)', line):
            return 'Release'

    # If is_debug is set to anything other than false, or if it
    # does not exist at all, we should use the default value (True).
    return 'Debug'


def command_wrapper(wrapper: str) -> List[str]:
    return shlex.split(wrapper)
