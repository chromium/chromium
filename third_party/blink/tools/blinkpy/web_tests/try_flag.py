# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Triggers and processes results from flag try jobs.

For more information, see: http://bit.ly/flag-try-jobs
"""

import argparse
import sys

from blinkpy.common.host import Host
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models.test_configuration import TestConfiguration
from blinkpy.web_tests.models.test_configuration import TestConfigurationConverter
from blinkpy.web_tests.models.test_expectations import TestExpectationLine
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.test_expectations import TestExpectationsModel


# TODO(skobes): use blinkpy/config/builders.json instead of hardcoding these.
BUILDER_CONFIGS = {
    'linux-rel': TestConfiguration('Linux', '', 'release'),
    'mac-rel': TestConfiguration('Mac', '', 'release'),
    'win7-rel': TestConfiguration('Win', '', 'release')
}
BUILDER_BUCKETS = {
    'linux-rel': 'luci.chromium.try',
    'mac-rel': 'luci.chromium.try',
    'win7-rel': 'luci.chromium.try',
}
FLAG_FILE = 'additional-driver-flag.setting'


class TryFlag(object):

    def __init__(self, argv, host, git_cl):
        self._args = parse_args(argv)
        self._host = host
        self._git_cl = git_cl
        self._expectations_model = TestExpectationsModel()
        self._test_configuration_converter = TestConfigurationConverter(
            set(BUILDER_CONFIGS.values()))
        self._filesystem = self._host.filesystem
        self._path_finder = PathFinder(self._filesystem)
        self._git = self._host.git()

    def _force_flag_for_test_runner(self):
        flag = self._args.flag
        path = self._path_finder.path_from_web_tests(FLAG_FILE)
        self._filesystem.write_text_file(path, flag + '\n')
        self._git.add_list([path])
        self._git.commit_locally_with_message(
            'Flag try job: force %s for run_web_tests.py.' % flag)

    def _flag_expectations_path(self):
        return self._path_finder.path_from_web_tests(
            'FlagExpectations', self._args.flag.lstrip('-'))

    def _clear_expectations(self):
        path = self._flag_expectations_path()
        self._filesystem.write_text_file(path, '')
        self._git.add_list([path])
        self._git.commit_locally_with_message(
            'Flag try job: clear expectations for %s.' % self._args.flag)

    def _tests_in_flag_expectations(self):
        result = set()
        path = self._flag_expectations_path()
        for line in self._filesystem.read_text_file(path).split('\n'):
            expectation_line = TestExpectationLine.tokenize_line(
                path, line, 0, self._host.port_factory.get())
            test_name = expectation_line.name
            if test_name:
                result.add(test_name)
        return result

    def trigger(self):
        self._force_flag_for_test_runner()
        if self._args.regenerate:
            self._clear_expectations()
        self._git_cl.run(['upload', '--bypass-hooks', '-f',
                          '-m', 'Flag try job for %s.' % self._args.flag])
        for builder in sorted(BUILDER_BUCKETS):
            bucket = BUILDER_BUCKETS[builder]
            self._git_cl.trigger_try_jobs([builder], bucket)

    def _create_expectation_line(self, result, test_configuration):
        test_name = result.test_name()
        line = TestExpectationLine()
        line.name = test_name
        line.path = test_name
        line.matching_tests = [test_name]
        line.filename = ''
        if self._args.bug:
            line.bugs = ['crbug.com/%s' % self._args.bug]
        else:
            line.bugs = ['Bug(none)']
        line.expectations = result.actual_results().split()
        line.parsed_expectations = [
            TestExpectations.expectation_from_string(expectation)
            for expectation in line.expectations]
        line.specifiers = [test_configuration.version]
        line.matching_configurations = set([test_configuration])
        return line

    def _process_result(self, build, result):
        if not result.did_run_as_expected():
            self._expectations_model.add_expectation_line(
                self._create_expectation_line(
                    result,
                    BUILDER_CONFIGS[build.builder_name]),
                model_all_expectations=True)

    def update(self):
        self._host.print_('Fetching results...')
        # TODO: Get jobs from the _tryflag branch. Current branch for now.
        jobs = self._git_cl.latest_try_jobs(builder_names=BUILDER_CONFIGS.keys())
        results_fetcher = self._host.results_fetcher
        for build in sorted(jobs):
            self._host.print_('-- %s: %s/results.html' % (
                BUILDER_CONFIGS[build.builder_name].version,
                results_fetcher.results_url(build.builder_name, build.build_number)))
            results = results_fetcher.fetch_results(build, True)
            results.for_each_test(
                lambda result, b=build: self._process_result(b, result))

        # TODO: Write to flag expectations file. For now, stdout. :)
        unexpected_failures = []
        unexpected_passes = []
        tests_in_flag_expectations = self._tests_in_flag_expectations()
        for line in self._expectations_model.all_lines():
            is_pass = (TestExpectations.EXPECTATIONS['pass'] in
                       line.parsed_expectations)
            if not is_pass:
                unexpected_failures.append(line)
            elif line.name in tests_in_flag_expectations:
                unexpected_passes.append(line)

        self._print_all(unexpected_passes, 'unexpected passes')
        self._print_all(unexpected_failures, 'unexpected failures')

    def _print_all(self, lines, description):
        self._host.print_('\n### %s %s:\n' % (len(lines), description))
        for line in lines:
            self._host.print_(line.to_string(
                self._test_configuration_converter))

    def run(self):
        action = self._args.action
        if action == 'trigger':
            self.trigger()
        elif action == 'update':
            self.update()
        else:
            print >> self._host.stderr, 'specify "trigger" or "update"'
            return 1
        return 0


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('action', help='"trigger" or "update"')
    parser.add_argument('--bug', help='crbug number for expectation lines')
    parser.add_argument('--flag', required=True,
                        help='flag to force-enable in run_web_tests.py')
    parser.add_argument('--regenerate', action='store_true',
                        help='clear the flag expectations before triggering')
    return parser.parse_args(argv)


def main():
    host = Host()
    return TryFlag(sys.argv[1:], host, GitCL(host)).run()
