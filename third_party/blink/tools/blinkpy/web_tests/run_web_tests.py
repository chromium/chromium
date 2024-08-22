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
from blinkpy.common.system import command_line
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
    parser = command_line.ArgumentParser(
        usage='%(prog)s [options] [tests]',
        description=('Runs Blink web tests as described in '
                     '//docs/testing/web_tests.md'))

    factory.add_platform_options_group(parser)
    factory.add_configuration_options_group(parser)
    printing.add_print_options_group(parser)

    fuchsia_group = parser.add_argument_group('Fuchsia-specific Options')
    fuchsia_group.add_argument('--fuchsia-out-dir',
                               help='Path to Fuchsia build output directory.')
    fuchsia_group.add_argument(
        '--custom-image',
        help='Specify an image used for booting up the emulator.')
    fuchsia_group.add_argument(
        '--fuchsia-target-id',
        help='The node-name of the device to boot or deploy to.')
    fuchsia_group.add_argument('--logs-dir',
                               help='Location of diagnostics logs')

    factory.add_results_options_group(parser)
    factory.add_testing_options_group(parser)

    # FIXME: Move these into json_results_generator.py.
    json_group = parser.add_argument_group('Result JSON Options')
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
        options.child_processes = int(
            port.host.environ.get('WEBKIT_TEST_CHILD_PROCESSES',
                                  port.default_child_processes()))
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
