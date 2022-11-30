# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# Copyright (c) 2012 Intel Corporation. All rights reserved.
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

from __future__ import print_function

import fnmatch
import re

from optparse import make_option

from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.system.crash_logs import CrashLogs
from blinkpy.tool.commands.command import Command
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory import platform_options


class CrashLog(Command):
    name = 'crash-log'
    help_text = 'Print the newest crash log for the given process'
    show_in_main_help = True
    long_help = """Finds the newest crash log matching the given process name
and PID and prints it to stdout."""
    argument_names = 'PROCESS_NAME [PID]'

    def execute(self, options, args, tool):
        crash_logs = CrashLogs(tool)
        pid = None
        if len(args) > 1:
            pid = int(args[1])
        print(crash_logs.find_newest_log(args[0], pid))


class PrintExpectations(Command):
    name = 'print-expectations'
    help_text = 'Print the expected result for the given test(s) on the given port(s)'
    show_in_main_help = True

    def __init__(self):
        options = [
            make_option(
                '--all',
                action='store_true',
                default=False,
                help='display the expectations for *all* tests'),
            make_option(
                '-x',
                '--exclude-keyword',
                action='append',
                default=[],
                help=
                'limit to tests not matching the given keyword (for example, '
                '"skip", "slow", or "crash". May specify multiple times'),
            make_option(
                '-i',
                '--include-keyword',
                action='append',
                default=[],
                help=
                'limit to tests with the given keyword (for example, "skip", '
                '"slow", or "crash". May specify multiple times'),
            make_option(
                '--csv',
                action='store_true',
                default=False,
                help=
                'Print a CSV-style report that includes the port name, bugs, '
                'specifiers, tests, and expectations'),
            make_option(
                '--paths',
                action='store_true',
                default=False,
                help='display the paths for all applicable expectation files'),
        ] + platform_options(use_globs=True)

        super(PrintExpectations, self).__init__(options=options)
        self._expectation_models = {}

    def execute(self, options, args, tool):
        if not options.paths and not args and not options.all:
            print('You must either specify one or more test paths or --all.')
            return

        if options.platform:
            port_names = fnmatch.filter(tool.port_factory.all_port_names(),
                                        options.platform)
            if not port_names:
                default_port = tool.port_factory.get(options.platform)
                if default_port:
                    port_names = [default_port.name()]
                else:
                    print("No port names match '%s'" % options.platform)
                    return
            else:
                default_port = tool.port_factory.get(port_names[0])
        else:
            default_port = tool.port_factory.get(options=options)
            port_names = [default_port.name()]

        if options.paths:
            files = default_port.default_expectations_files()
            web_tests_dir = default_port._filesystem.normpath(
                default_port.web_tests_dir())
            for file in files:
                if file.startswith(web_tests_dir):
                    file = file.replace(web_tests_dir,
                                        WEB_TESTS_LAST_COMPONENT)
                print(file)
            return

        tests = set(default_port.tests(args))
        for port_name in port_names:
            port = tool.port_factory.get(port_name, options)
            test_expectations = TestExpectations(port)
            tests_to_print = self._filter_tests(options, test_expectations,
                                                tests)
            lines = [
                test_expectations.get_expectations(test)
                for test in sorted(tests_to_print)
            ]
            if port_name != port_names[0]:
                print('')
            print('\n'.join(self._format_lines(options, port_name, lines)))

    @staticmethod
    def _test_set_for_keyword(keyword, test_expectations, tests):
        filtered_tests = []
        for test in tests:
            if (keyword == 'SLOW'
                    and test_expectations.get_expectations(test).is_slow_test):
                filtered_tests.append(test)
            elif keyword in test_expectations.get_expectations(test).results:
                filtered_tests.append(test)
        return filtered_tests

    def _filter_tests(self, options, test_expectations, tests):
        filtered_tests = set()
        if options.include_keyword:
            for keyword in options.include_keyword:
                filtered_tests.update(
                    self._test_set_for_keyword(keyword.upper(),
                                               test_expectations, tests))
        else:
            filtered_tests = tests

        for keyword in options.exclude_keyword:
            filtered_tests.difference_update(
                self._test_set_for_keyword(keyword.upper(), test_expectations,
                                           tests))
        return filtered_tests

    @staticmethod
    def _to_csv(expectation):
        return '%s,%s,%s,%s' % (expectation.test, expectation.reason, ' '.join(
            expectation.tags), ' '.join(expectation.results))

    def _format_lines(self, options, port_name, lines):
        output = []
        if options.csv:
            for line in lines:
                output.append('%s,%s' % (port_name, self._to_csv(line)))
        elif lines:
            output.append('// For %s' % port_name)
            for line in lines:
                output.append('%s' % line.to_string())
        return output


class PrintBaselines(Command):
    name = 'print-baselines'
    help_text = 'Prints the baseline locations for given test(s) on the given port(s)'
    show_in_main_help = True

    def __init__(self):
        options = [
            make_option(
                '--all',
                action='store_true',
                default=False,
                help='display the baselines for *all* tests'),
            make_option(
                '--csv',
                action='store_true',
                default=False,
                help=
                'Print a CSV-style report that includes the port name, test_name, '
                'test platform, baseline type, baseline location, and baseline platform'
            ),
            make_option(
                '--include-virtual-tests',
                action='store_true',
                help='Include virtual tests'),
        ] + platform_options(use_globs=True)
        super(PrintBaselines, self).__init__(options=options)
        self._platform_regexp = re.compile(r'platform/([^\/]+)/(.+)')

    def execute(self, options, args, tool):
        if not args and not options.all:
            print('You must either specify one or more test paths or --all.')
            return

        default_port = tool.port_factory.get()
        if options.platform:
            port_names = fnmatch.filter(tool.port_factory.all_port_names(),
                                        options.platform)
            if not port_names:
                print("No port names match '%s'" % options.platform)
        else:
            port_names = [default_port.name()]

        if options.include_virtual_tests:
            tests = sorted(default_port.tests(args))
        else:
            tests = sorted(default_port.real_tests(args))

        for port_name in port_names:
            if port_name != port_names[0]:
                print('')
            if not options.csv:
                print('// For %s' % port_name)
            port = tool.port_factory.get(port_name)
            for test_name in tests:
                self._print_baselines(
                    options, port_name, test_name,
                    port.expected_baselines_by_extension(test_name))

    def _print_baselines(self, options, port_name, test_name, baselines):
        for extension in sorted(baselines.keys()):
            baseline_location = baselines[extension]
            if baseline_location:
                if options.csv:
                    print('%s,%s,%s,%s,%s,%s' %
                          (port_name, test_name,
                           self._platform_for_path(test_name), extension[1:],
                           baseline_location,
                           self._platform_for_path(baseline_location)))
                else:
                    print(baseline_location)

    def _platform_for_path(self, relpath):
        platform_matchobj = self._platform_regexp.match(relpath)
        if platform_matchobj:
            return platform_matchobj.group(1)
        return None
