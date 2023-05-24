# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011 Apple Inc. All rights reserved.
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

import argparse
import logging
import optparse
import traceback

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.web_tests.controllers.manager import Manager
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.port import factory
from blinkpy.web_tests.views import printing

_log = logging.getLogger(__name__)


def main(argv, stderr):
    options, args = parse_args(argv)

    if options.platform and 'test' in options.platform and not 'browser_test' in options.platform:
        # It's a bit lame to import mocks into real code, but this allows the user
        # to run tests against the test platform interactively, which is useful for
        # debugging test failures.
        from blinkpy.common.host_mock import MockHost
        host = MockHost()
    else:
        host = Host()

    if stderr.isatty():
        stderr.reconfigure(write_through=True)
    printer = printing.Printer(host, options, stderr)

    try:
        port = host.port_factory.get(options.platform, options)
    except (NotImplementedError, ValueError) as error:
        _log.error(error)
        printer.cleanup()
        return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

    try:
        return run(port, options, args, printer).exit_code

    # We need to still handle KeyboardInterrupt, at least for blinkpy unittest cases.
    except KeyboardInterrupt:
        return exit_codes.INTERRUPTED_EXIT_STATUS
    except test_run_results.TestRunException as error:
        _log.error(error.msg)
        return error.code
    except BaseException as error:
        if isinstance(error, Exception):
            _log.error('\n%s raised: %s', error.__class__.__name__, error)
            traceback.print_exc(file=stderr)
        return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS
    finally:
        printer.cleanup()


def parse_args(args):
    parser = argparse.ArgumentParser(
        usage='%(prog)s [options] [tests]',
        description=('Runs Blink web tests as described in '
                     '//docs/testing/web_tests.md'))

    factory.add_platform_options_group(parser)
    factory.add_configuration_options_group(parser)
    printing.add_print_options_group(parser)
    factory.add_wpt_options_group(parser)

    fuchsia_group = parser.add_argument_group('Fuchsia-specific Options')
    fuchsia_group.add_argument(
        '--zircon-logging',
        action='store_true',
        default=True,
        help='Log Zircon debug messages (enabled by default).')
    fuchsia_group.add_argument('--no-zircon-logging',
                               dest='zircon_logging',
                               action='store_false',
                               default=True,
                               help='Do not log Zircon debug messages.')
    fuchsia_group.add_argument(
        '--device',
        choices=['qemu', 'device', 'fvdl'],
        default='fvdl',
        help='Choose device to launch Fuchsia with. Defaults to fvdl.')
    fuchsia_group.add_argument(
        '--fuchsia-target-cpu',
        choices=['x64', 'arm64'],
        default='x64',
        help='cpu architecture of the device. Defaults to x64.')
    fuchsia_group.add_argument('--fuchsia-out-dir',
                               help='Path to Fuchsia build output directory.')
    fuchsia_group.add_argument(
        '--custom-image',
        help='Specify an image used for booting up the emulator.')
    fuchsia_group.add_argument(
        '--fuchsia-ssh-config',
        help=('The path to the SSH configuration used for '
              'connecting to the target device.'))
    fuchsia_group.add_argument(
        '--fuchsia-target-id',
        help='The node-name of the device to boot or deploy to.')
    fuchsia_group.add_argument(
        '--fuchsia-host-ip',
        help=('The IP address of the test host observed by the Fuchsia '
              'device. Required if running on hardware devices.'))
    fuchsia_group.add_argument('--logs-dir',
                               help='Location of diagnostics logs')

    results_group = parser.add_argument_group('Results Options')
    results_group.add_argument(
        '--flag-specific',
        help=('Name of a flag-specific configuration defined in '
              'FlagSpecificConfig. It is like a shortcut of '
              '--additional-driver-flag, --additional-expectations and '
              '--additional-platform-directory. When setting up flag-specific '
              'testing on bots, we should use this option instead of the '
              'discrete options. See crbug.com/1238155 for details.'))
    results_group.add_argument(
        '--additional-driver-flag',
        '--additional-drt-flag',
        dest='additional_driver_flag',
        action='append',
        default=[],
        help=('Additional command line flag to pass to the driver. '
              'Specify multiple times to add multiple flags.'))
    results_group.add_argument(
        '--additional-expectations',
        action='append',
        default=[],
        help=('Path to a test_expectations file that will override previous '
              'expectations. Specify multiple times for multiple sets of '
              'overrides.'))
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
        help=('Additional directory where to look for test baselines (will '
              'take precedence over platform baselines). Specify multiple '
              'times to add multiple search path entries.'))
    results_group.add_argument(
        '--build-directory',
        default='out',
        help=(
            'Path to the directory where build files are kept, not including '
            'configuration. In general this will be "out".'))
    results_group.add_argument(
        '--clobber-old-results',
        action='store_true',
        help='Clobbers test results from previous runs.')
    results_group.add_argument('--compare-port',
                               help="Use the specified port's baselines first")
    results_group.add_argument(
        '--copy-baselines',
        action='store_true',
        help=('If the actual result is different from the current baseline, '
              'copy the current baseline into the *most-specific-platform* '
              'directory, or the flag-specific generic-platform directory if '
              '--additional-driver-flag is specified. See --reset-results.'))
    results_group.add_argument('--driver-name',
                               help='Alternative driver binary to use')
    results_group.add_argument(
        '--json-test-results',  # New name from json_results_generator
        '--write-full-results-to',  # Old argument name
        '--isolated-script-test-output',  # Isolated API
        help='Path to write the JSON test results for *all* tests.')
    results_group.add_argument(
        '--write-run-histories-to',
        help='Path to write the JSON test run histories.')
    results_group.add_argument(
        '--no-show-results',
        dest='show_results',
        action='store_false',
        help="Don't launch a browser with results after the tests are done")
    results_group.add_argument(
        '--reset-results',
        action='store_true',
        help=('Reset baselines to the generated results in their existing '
              'location or the default location if no baseline exists. For '
              'virtual tests, reset the virtual baselines. If '
              '--additional-driver-flag is specified, reset the flag-specific '
              'baselines. If --copy-baselines is specified, the copied '
              'baselines will be reset.'))
    results_group.add_argument('--results-directory',
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

    testing_group = parser.add_argument_group('Testing Options')
    testing_group.add_argument(
        '--additional-env-var',
        action='append',
        default=[],
        help=('Passes that environment variable to the tests '
              '(--additional-env-var=NAME=VALUE)'))
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
    testing_group.add_argument('--child-processes',
                               '--jobs',
                               '-j',
                               help='Number of drivers to run in parallel.')
    testing_group.add_argument(
        '--disable-breakpad',
        action='store_true',
        help="Don't use breakpad to symbolize unexpected crashes.")
    testing_group.add_argument(
        '--driver-logging',
        action='store_true',
        help='Print detailed logging of the driver/content_shell')
    testing_group.add_argument(
        '--enable-leak-detection',
        action='store_true',
        help='Enable the leak detection of DOM objects.')
    testing_group.add_argument(
        '--enable-sanitizer',
        action='store_true',
        help='Only alert on sanitizer-related errors and crashes')
    testing_group.add_argument(
        '--enable-tracing',
        help=('Capture and write a trace file with the specified '
              'categories for each test. Passes appropriate --trace-startup '
              'flags to the driver. If in doubt, use "*".'))
    testing_group.add_argument(
        '--exit-after-n-crashes-or-timeouts',
        type=int,
        default=None,
        help='Exit after the first N crashes instead of running all tests')
    testing_group.add_argument(
        '--exit-after-n-failures',
        type=int,
        default=None,
        help='Exit after the first N failures instead of running all tests')
    testing_group.add_argument(
        '--fuzzy-diff',
        action='store_true',
        help=('When running tests on an actual GPU, variance in pixel output '
              'can lead to image differences causing failed expectations. '
              'Using a fuzzy diff instead accounts for this variance. '
              'See //tools/imagediff/image_diff.cc'))
    testing_group.add_argument(
        '--ignore-builder-category',
        help=('The category of builders to use with the --ignore-flaky-tests '
              "option ('layout' or 'deps')."))
    testing_group.add_argument(
        '--ignore-flaky-tests',
        help=('Control whether tests that are flaky on the bots get ignored. '
              "'very-flaky' == Ignore any tests that flaked more than once on "
              "the bot. 'maybe-flaky' == Ignore any tests that flaked once on "
              "the bot. 'unexpected' == Ignore any tests that had unexpected "
              'results on the bot.'))
    testing_group.add_argument(
        '--iterations',
        '--isolated-script-test-repeat',
        # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
        '--gtest_repeat',
        type=int,
        default=1,
        help='Number of times to run the set of tests (e.g. ABCABCABC)')
    testing_group.add_argument('--layout-tests-directory',
                               help='Path to a custom web tests directory')
    testing_group.add_argument('--max-locked-shards',
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
        help=('Determine the order in which the test cases will be run. '
              "'none' == use the order in which the tests were listed "
              'either in arguments or test list, '
              "'random' == pseudo-random order (default). Seed can be "
              "specified via --seed, otherwise it will default to the current "
              "unix timestamp. 'natural' == use the natural order"))
    testing_group.add_argument('--profile',
                               action='store_true',
                               help='Output per-test profile information.')
    testing_group.add_argument('--profiler',
                               help=('Output per-test profile information, '
                                     'using the specified profiler.'))
    testing_group.add_argument(
        '--restart-shell-between-tests',
        choices=['always', 'never', 'on_retry'],
        default='on_retry',
        help=('Restarting the shell between tests produces more '
              'consistent results, as it prevents state from carrying over '
              'from previous tests. It also increases test run time by at '
              'least 2X. By default, the shell is restarted when tests get '
              'retried, since leaking state between retries can sometimes '
              'mask underlying flakiness, and the whole point of retries is '
              'to look for flakiness.'))
    testing_group.add_argument(
        '--repeat-each',
        type=int,
        default=1,
        help='Number of times to run each test (e.g. AAABBBCCC)')
    testing_group.add_argument(
        '--num-retries',
        '--test-launcher-retry-limit',
        '--isolated-script-test-launcher-retry-limit',
        type=int,
        default=None,
        help=('Number of times to retry failures. Default (when this '
              'flag is not specified) is to retry 3 times, unless an '
              'explicit list of tests is passed to run_web_tests.py. '
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
        type=int,
        help=('Total number of shards being used for this test run. '
              'Must be used with --shard-index. '
              '(The user of this script is responsible for spawning '
              'all of the shards.)'))
    testing_group.add_argument(
        '--shard-index',
        type=int,
        help=('Shard index [0..total_shards) of this test run. '
              'Must be used with --total-shards.'))
    testing_group.add_argument(
        '--seed',
        type=int,
        help=('Seed to use for random test order (default: %(default)s). '
              'Only applicable in combination with --order=random.'))
    testing_group.add_argument(
        '--skipped',
        help=(
            'Control how tests marked SKIP are run. '
            '"default" == Skip tests unless explicitly listed on the command '
            'line, "ignore" == Run them anyway, '
            '"only" == only run the SKIP tests, '
            '"always" == always skip, even if listed on the command line.'))
    testing_group.add_argument(
        '--isolated-script-test-also-run-disabled-tests',
        # TODO(crbug.com/893235): Remove the gtest alias when FindIt no longer uses it.
        '--gtest_also_run_disabled_tests',
        action='store_const',
        const='ignore',
        dest='skipped',
        help=('Equivalent to --skipped=ignore.'))
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
    testing_group.add_argument(
        '--fastest',
        type=float,
        help=('Run the N%% fastest tests as well as any tests listed '
              'on the command line'))
    testing_group.add_argument('--test-list',
                               action='append',
                               metavar='FILE',
                               help='read filters for tests to run')
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
    testing_group.add_argument('--timeout-ms',
                               help='Set the timeout for each test')
    testing_group.add_argument(
        '--initialize-webgpu-adapter-at-startup-timeout-ms',
        type=float,
        help='Initialize WebGPU adapter before running any tests.')
    testing_group.add_argument(
        '--wrapper',
        help=('wrapper command to insert before invocations of the driver; '
              'option is split on whitespace before running. (Example: '
              '--wrapper="valgrind --smc-check=all")'))
    # FIXME: Display the default number of child processes that will run.
    testing_group.add_argument('-f',
                               '--fully-parallel',
                               action='store_true',
                               help='run all tests in parallel')
    testing_group.add_argument(
        '--virtual-parallel',
        action='store_true',
        help=('When running in parallel, include virtual tests. Useful for '
              'running a single virtual test suite, but will be slower '
              'in other cases.'))
    testing_group.add_argument(
        '-i',
        '--ignore-tests',
        action='append',
        default=[],
        help='directories or test to ignore (may specify multiple times)')
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
        '--zero-tests-executed-ok',
        action='store_true',
        help=('If set, exit with a success code when no tests are run. '
              'Used on trybots when web tests are retried without patch.'))
    testing_group.add_argument(
        '--driver-kill-timeout-secs',
        type=float,
        default=1.0,
        help=('Number of seconds to wait before killing a driver, and the '
              'main use case is to leave enough time to allow the process to '
              'finish post-run hooks, such as dumping code coverage data. '
              'Default is 1 second, can be overriden for specific use cases.'))
    testing_group.add_argument(
        '--ignore-testharness-expected-txt',
        action='store_true',
        help=('Ignore *-expected.txt for all testharness tests. All '
              'testharness test failures will be shown, even if the '
              'failures are expected in *-expected.txt.'))

    # FIXME: Move these into json_results_generator.py.
    json_group = parser.add_argument_group('Result JSON Options')
    # TODO(qyearsley): --build-name is unused and should be removed.
    json_group.add_argument('--build-name', help=argparse.SUPPRESS)
    json_group.add_argument(
        '--step-name',
        default='blink_web_tests',
        help='The name of the step in a build running this script.')
    json_group.add_argument(
        '--build-number',
        default='DUMMY_BUILD_NUMBER',
        help='The build number of the builder running this script.')
    json_group.add_argument(
        '--builder-name',
        default='',
        help=('The name of the builder shown on the waterfall running '
              'this script, e.g. "Mac10.13 Tests".'))

    parser.add_argument('tests',
                        nargs='*',
                        help='Paths to test files or directories to run')
    params = vars(parser.parse_args(args))
    args = params.pop('tests')
    options = optparse.Values(params)
    return (options, args)


def _set_up_derived_options(port, options, args):
    """Sets the options values that depend on other options values."""
    # --restart-shell-between-tests is implemented by changing the batch size.
    if options.restart_shell_between_tests == 'always':
        options.derived_batch_size = 1
        options.must_use_derived_batch_size = True
    elif options.restart_shell_between_tests == 'never':
        options.derived_batch_size = 0
        options.must_use_derived_batch_size = True
    else:
        # If 'repeat_each' or 'iterations' has been set, then implicitly set the
        # batch size to 1. If we're already repeating the tests more than once,
        # then we're not particularly concerned with speed. Restarting content
        # shell provides more consistent results.
        if options.repeat_each > 1 or options.iterations > 1:
            options.derived_batch_size = 1
            options.must_use_derived_batch_size = True
        else:
            options.derived_batch_size = port.default_batch_size()
            options.must_use_derived_batch_size = False

    if not options.child_processes:
        options.child_processes = port.host.environ.get(
            'WEBKIT_TEST_CHILD_PROCESSES', str(port.default_child_processes()))
    if not options.max_locked_shards:
        options.max_locked_shards = int(
            port.host.environ.get('WEBKIT_TEST_MAX_LOCKED_SHARDS',
                                  str(port.default_max_locked_shards())))

    if not options.configuration:
        options.configuration = port.get_option('configuration')

    if not options.target:
        options.target = port.get_option('target')

    if not options.timeout_ms:
        options.timeout_ms = str(port.timeout_ms())

    options.slow_timeout_ms = str(5 * int(options.timeout_ms))

    if options.additional_platform_directory:
        additional_platform_directories = []
        for path in options.additional_platform_directory:
            additional_platform_directories.append(
                port.host.filesystem.abspath(path))
        options.additional_platform_directory = additional_platform_directories

    if not args and not options.test_list and options.smoke is None:
        options.smoke = port.default_smoke_test_only()
    if options.smoke:
        if not args and not options.test_list and options.num_retries is None:
            # Retry failures 3 times if we're running a smoke test without
            # additional tests. SmokeTests is an explicit list of tests, so we
            # wouldn't retry by default without this special case.
            options.num_retries = 3

        if not options.test_list:
            options.test_list = []
        options.test_list.append(port.path_to_smoke_tests_file())
        if not options.skipped:
            options.skipped = 'always'

    if not options.skipped:
        options.skipped = 'default'

    if options.gtest_filter:
        args.extend(options.gtest_filter.split(':'))

    if not options.total_shards and 'GTEST_TOTAL_SHARDS' in port.host.environ:
        options.total_shards = int(port.host.environ['GTEST_TOTAL_SHARDS'])
    if not options.shard_index and 'GTEST_SHARD_INDEX' in port.host.environ:
        options.shard_index = int(port.host.environ['GTEST_SHARD_INDEX'])

    if not options.seed:
        options.seed = port.host.time()


def run(port, options, args, printer):
    _set_up_derived_options(port, options, args)
    manager = Manager(port, options, printer)
    printer.print_config(port)
    run_details = manager.run(args)
    _log.debug('')
    _log.debug('Testing completed. Exit status: %d', run_details.exit_code)
    printer.flush()
    return run_details
