# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Triggers and processes results from flag try jobs.

For more information, see: http://bit.ly/flag-try-jobs
"""
from __future__ import print_function

import argparse
import sys

from blinkpy.common.host import Host
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models.test_configuration import TestConfiguration
from blinkpy.web_tests.models.typ_types import Expectation, TestExpectations, ResultType

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
        self._expectations = []
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
        path = self._flag_expectations_path()
        content = self._filesystem.read_text_file(path)
        test_expectations = TestExpectations()
        test_expectations.parse_tagged_list(content)
        return {
            test_name
            for test_name in test_expectations.individual_exps.keys()
        }

    def trigger(self):
        self._force_flag_for_test_runner()
        if self._args.regenerate:
            self._clear_expectations()
        self._git_cl.run([
            'upload', '--bypass-hooks', '-f', '-m',
            'Flag try job for %s.' % self._args.flag
        ])
        for builder in sorted(BUILDER_BUCKETS):
            bucket = BUILDER_BUCKETS[builder]
            self._git_cl.trigger_try_jobs([builder], bucket)

    def _create_expectation_line(self, result, test_configuration):
        expected_results = set([res for res in result.actual_results()])
        tag = test_configuration.version
        reason = ''
        if self._args.bug:
            reason = 'crbug.com/' + self._args.bug
        return Expectation(
            test=result.test_name(),
            results=expected_results,
            tags=set([tag]),
            reason=reason)

    def _process_result(self, build, result):
        if not result.did_run_as_expected():
            self._expectations.append(
                self._create_expectation_line(
                    result, BUILDER_CONFIGS[build.builder_name]))

    def update(self):
        self._host.print_('Fetching results...')
        # TODO: Get jobs from the _tryflag branch. Current branch for now.
        jobs = self._git_cl.latest_try_jobs(
            builder_names=BUILDER_CONFIGS.keys())
        results_fetcher = self._host.results_fetcher
        for build in sorted(jobs):
            step_names = self._host.builders.step_names_for_builder(
                build.builder_name)
            generic_steps = [
                step_name for step_name in step_names
                if not self._host.builders.flag_specific_option(
                    build.builder_name, step_name)
            ]
            if len(generic_steps) != 1:
                self._host.print_('Expected exactly one non-flag-specific '
                                  'web test step for build %r, but found %d.' %
                                  (build, len(generic_steps)))
                continue
            step_name = generic_steps[0]
            results = results_fetcher.gather_results(build, step_name, False,
                                                     False)
            for result in results:
                self._process_result(build, result)

        # TODO: Write to flag expectations file. For now, stdout. :)
        unexpected_failures = []
        unexpected_passes = []
        tests_in_flag_expectations = self._tests_in_flag_expectations()
        for exp in self._expectations:
            if ResultType.Pass not in exp.results:
                unexpected_failures.append(exp)
            elif exp.test in tests_in_flag_expectations:
                unexpected_passes.append(exp)
        unexpected_passes = sorted(unexpected_passes, key=lambda e: e.test)
        unexpected_failures = sorted(unexpected_failures, key=lambda e: e.test)
        self._print_all(unexpected_passes, 'unexpected passes')
        self._print_all(unexpected_failures, 'unexpected failures')

    def _print_all(self, exps, description):
        self._host.print_('\n### %s %s:\n' % (len(exps), description))
        for exp in exps:
            self._host.print_(exp.to_string())

    def run(self):
        action = self._args.action
        if action == 'trigger':
            self.trigger()
        elif action == 'update':
            self.update()
        else:
            print('specify "trigger" or "update"', file=self._host.stderr)
            return 1
        return 0


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('action', help='"trigger" or "update"')
    parser.add_argument('--bug', help='crbug number for expectation lines')
    parser.add_argument(
        '--flag',
        required=True,
        help='flag to force-enable in run_web_tests.py')
    parser.add_argument(
        '--regenerate',
        action='store_true',
        help='clear the flag expectations before triggering')
    return parser.parse_args(argv)


def main():
    host = Host()
    return TryFlag(sys.argv[1:], host, GitCL(host)).run()
