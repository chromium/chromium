#!/usr/bin/env vpython3

# Copyright (C) 2021 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import contextlib
import json
import logging
import os
import sys
import tempfile

from blinkpy.common.host import Host
from blinkpy.common.system.executive import ScriptError
from blinkpy.web_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory
from blinkpy.web_tests.port.android import (
    PRODUCTS, PRODUCTS_TO_STEPNAMES)

CSV_HEADING = ('Test name, %s Result, %s Result, '
               'Result Comparison, Test Flaky Results, '
               'Baseline Flaky Results, Unreliable Comparison\n')
YES = 'Yes'
NO = 'No'
_log = logging.getLogger(os.path.basename(__file__))

# Extend this script to compare the results between wptrunner/Chrome
# and rwt/content_shell on Linux
PRODUCTS = PRODUCTS + [
    'android_webview', 'chrome_android', 'chrome_linux', 'content_shell',
    'wpt_content_shell_linux', 'wpt_content_shell_win10',
    'wpt_content_shell_win11', 'wpt_content_shell_mac11',
    'wpt_content_shell_mac12', 'wpt_content_shell_mac12_arm64',
    'wpt_content_shell_mac13', 'wpt_content_shell_mac13_arm64'
]
PRODUCTS_TO_STEPNAMES.update({
    'android_webview': 'system_webview_wpt',
    'chrome_android': 'chrome_public_wpt',
    'chrome_linux': 'wpt_tests_suite',
    'wpt_content_shell_linux': 'wpt_tests_suite_linux',
    'wpt_content_shell_win10': 'wpt_tests_suite_win10',
    'wpt_content_shell_win11': 'wpt_tests_suite_win11',
    'wpt_content_shell_mac11': 'wpt_tests_suite_mac_11',
    'wpt_content_shell_mac12': 'wpt_tests_suite_mac12',
    'wpt_content_shell_mac12_arm64': 'wpt_tests_suite_mac12_arm64',
    'wpt_content_shell_mac13': 'wpt_tests_suite_mac13',
    'wpt_content_shell_mac13_arm64': 'wpt_tests_suite_mac13_arm64',
    'content_shell': 'blink_wpt_tests'
})
PRODUCTS_TO_BUILDER_NAME = {
    'android_webview': 'android-webview-pie-x86-wpt-fyi-rel',
    'chrome_android': 'android-chrome-pie-x86-wpt-fyi-rel',
    'chrome_linux': 'linux-wpt-chromium-rel',
    'wpt_content_shell_linux': 'linux-wpt-content-shell-fyi-rel',
    'wpt_content_shell_win10': 'win10-wpt-content-shell-fyi-rel',
    'wpt_content_shell_win11': 'win11-wpt-content-shell-fyi-rel',
    'wpt_content_shell_mac11': 'mac11-wpt-content-shell-fyi-rel',
    'wpt_content_shell_mac12': 'mac12-wpt-content-shell-fyi-rel',
    'wpt_content_shell_mac12_arm64': 'mac12-arm64-wpt-content-shell-fyi-rel',
    'wpt_content_shell_mac13': 'mac13-wpt-content-shell-fyi-rel',
    'wpt_content_shell_mac13_arm64': 'mac13-arm64-wpt-content-shell-fyi-rel',
    'content_shell': "Linux Tests"
}
STEP_NAME_VARIANTS = {
    'chrome_public_wpt': ['chrome_public_wpt on Ubuntu-16.04 or Ubuntu-18.04'],
    'system_webview_wpt':
    ['system_webview_wpt on Ubuntu-16.04 or Ubuntu-18.04'],
    'wpt_tests_suite_linux':
    ['wpt_tests_suite (experimental) on Ubuntu-18.04'],
    'wpt_tests_suite_win10':
    ['wpt_tests_suite (experimental) on Windows-10-19042'],
    'wpt_tests_suite_win11':
    ['wpt_tests_suite (experimental) on Windows-11-22000'],
    'wpt_tests_suite_mac12': ['wpt_tests_suite (experimental) on Mac-12'],
    'wpt_tests_suite_mac11': ['wpt_tests_suite (experimental) on Mac-11'],
    'wpt_tests_suite_mac12_arm64':
    ['wpt_tests_suite (experimental) on Mac-12'],
    'wpt_tests_suite_mac13': ['wpt_tests_suite (experimental) on Mac-13'],
    'wpt_tests_suite_mac13_arm64':
    ['wpt_tests_suite (experimental) on Mac-13'],
    'blink_wpt_tests': ['blink_wpt_tests on Ubuntu-18.04']
}

def map_tests_to_results(output_mp, input_mp, path=''):
    if 'actual' in input_mp:
        output_mp[path[1:]] = input_mp
    else:
        # TODO: needs to be extended when we support virtual tests
        for k, v in input_mp.items():
            if k == 'wpt_internal':
                map_tests_to_results(output_mp, v, "/wpt_internal")
            else:
                map_tests_to_results(output_mp, v, path + '/' + k)


class WPTResultsDiffer(object):

    def __init__(self, args, host, actual_results_map,
                 baseline_results_map, csv_output, ignore_missing=False):
        self._args = args
        self._host = host
        self._actual_results_map = actual_results_map
        self._baseline_results_map = baseline_results_map
        self._csv_output = csv_output
        self._ignore_missing = ignore_missing
        self._test_flaky_results = None
        self._baseline_flaky_results = None

        try:
            self._test_flaky_results = self._get_flaky_test_results(
                args.product_to_compare)
        except:
            _log.info('Failed to get flaky results for %s' % args.product_to_compare)

        try:
            self._baseline_flaky_results = self._get_flaky_test_results(
                args.baseline_product)
        except:
            _log.info('Failed to get flaky results for %s' % args.baseline_product)

        self._handle_flaky = self._test_flaky_results is not None \
            and self._baseline_flaky_results is not None

    def _get_flaky_test_results(self, product):
        return self._get_bot_expectations(product).flakes_by_path(
            False, ignore_bot_expected_results=True,
            consider_only_flaky_runs=False)

    def _get_bot_expectations(self, product):
        builder_name = PRODUCTS_TO_BUILDER_NAME[product]
        bot_expectations_factory = BotTestExpectationsFactory(
            self._host.builders, PRODUCTS_TO_STEPNAMES[product])

        return bot_expectations_factory.expectations_for_builder(builder_name)

    def flaky_results(self, test_name, flaky_dict):
        return (flaky_dict.get(test_name, set()) or
                flaky_dict.get(test_name.replace('external/wpt/', ''), set()))

    def create_csv(self):
        super_set = (set(self._actual_results_map.keys()) |
                     set(self._baseline_results_map.keys()))
        file_output = CSV_HEADING % (self._args.product_to_compare,
                                     self._args.baseline_product)

        for test in sorted(super_set):
            if ',' in test:
                line = ['"%s"' % test]
            else:
                line = [test]

            for result_mp in [self._actual_results_map,
                              self._baseline_results_map]:
                line.append(result_mp.get(test, {'actual': 'MISSING'})
                                     .get('actual').split()[-1])

            if line[-1] == line[-2]:
                line.append('SAME RESULTS')
            elif 'MISSING' in (line[-1], line[-2]):
                if self._ignore_missing:
                    continue
                line.append('MISSING RESULTS')
            else:
                line.append('DIFFERENT RESULTS')

            if self._handle_flaky and line[-1] != 'MISSING RESULTS':
                test_flaky_results = self.flaky_results(
                    test, self._test_flaky_results)

                baseline_flaky_results = self.flaky_results(
                    test, self._baseline_flaky_results)

                test_flaky_results.update([line[1]])
                baseline_flaky_results.update([line[2]])
                is_flaky = (len(test_flaky_results) > 1 or
                            len(baseline_flaky_results) > 1)
                line.extend(['"{%s}"' % ', '.join(sorted(test_flaky_results)),
                             '"{%s}"' % ', '.join(sorted(baseline_flaky_results))])

                if (is_flaky and line[1] in baseline_flaky_results and
                        line[2] in test_flaky_results):
                    line.append(YES)
                else:
                    line.append(NO)
            else:
                line.extend(['{}', '{}', NO])

            file_output += ','.join(line) + '\n'

        self._csv_output.write(file_output)


def _get_build_test_results(host, product, build):
    step_name = PRODUCTS_TO_STEPNAMES[product]
    for step_name_var in STEP_NAME_VARIANTS[step_name] + [step_name]:
        try:
            build_results = host.bb_agent.get_build_test_results(
                build, step_name_var)
            return build_results
        except ScriptError:
            _log.debug(('%s is not a step name that ran on %s:%d. '
                        'Re-attempting with a different step name.'),
                       step_name_var, build.builder_name, build.build_number)
        except Exception as ex:
            _log.exception(('Exception was raised when attempting to get '
                            'test results for step %s on build %s:%d'),
                           step_name_var, build.builder_name,
                           build.build_number)
            raise ex


@contextlib.contextmanager
def _get_product_test_results(host, product, build_number, results_path=None):
    if results_path:
        json_results_obj = open(results_path, 'r')
    else:
        _log.info(('Retrieving test results for '
                   'product %s using the bb command'), product)
        builder_name = PRODUCTS_TO_BUILDER_NAME[product]
        # TODO: Note the builder name and number in the CSV file
        build = host.bb_agent.get_finished_build(builder_name, build_number)
        _log.debug('Using build %s(%d)', builder_name, build.build_number)

        build_results = _get_build_test_results(host, product, build)
        json_results_obj = tempfile.NamedTemporaryFile(mode='w+t')
        json_results_obj.write(json.dumps(build_results))
        json_results_obj.seek(0)

    try:
        yield json_results_obj
    finally:
        json_results_obj.close()


def main(args):
    parser = argparse.ArgumentParser(prog=os.path.basename(__file__))
    parser.add_argument('--baseline-test-results', required=False,
                        help='Path to baseline test results JSON file')
    parser.add_argument('--baseline-product', required=True, action='store',
                        choices=PRODUCTS,
                        help='Name of the baseline product')
    parser.add_argument(
        '--baseline-build-number',
        type=int,
        default=0,
        help='The baseline builder to fetch results, default to latest.')
    parser.add_argument('--test-results-to-compare', required=False,
                        help='Path to actual test results JSON file')
    parser.add_argument('--product-to-compare', required=True, action='store',
                        choices=PRODUCTS,
                        help='Name of the product being compared')
    parser.add_argument(
        '--compare-build-number',
        type=int,
        default=0,
        help=
        'The product to compare builder to fetch results, default to latest.')
    parser.add_argument('--csv-output', required=True,
                        help='Path to CSV output file')
    parser.add_argument('--verbose', '-v', action='count', default=1,
                        help='Verbosity level')
    parser.add_argument('--ignore-missing', action='store_true',
                        required=False, default=False,
                        help='Ignore tests that are not run for one of the product')
    args = parser.parse_args()

    if args.verbose >= 3:
        log_level = logging.DEBUG
    elif args.verbose == 2:
        log_level = logging.INFO
    else:
        log_level = logging.WARNING

    logging.basicConfig(level=log_level)

    assert args.product_to_compare != args.baseline_product, (
        'Product to compare and the baseline product cannot be the same')

    host = Host()
    actual_results_getter = _get_product_test_results(
        host, args.product_to_compare, args.compare_build_number,
        args.test_results_to_compare)
    baseline_results_getter = _get_product_test_results(
        host, args.baseline_product, args.baseline_build_number,
        args.baseline_test_results)

    with actual_results_getter as actual_results_content,            \
            baseline_results_getter as baseline_results_content,     \
            open(args.csv_output, 'w') as csv_output:

        # Read JSON results files. They must follow the Chromium
        # json test results format
        actual_results_json = json.loads(actual_results_content.read())
        baseline_results_json = json.loads(baseline_results_content.read())

        # Compress the JSON results trie in order to map the test
        # names to their results map
        tests_to_actual_results = {}
        tests_to_baseline_results = {}
        if (args.product_to_compare.startswith('chrome_linux')
                or args.product_to_compare.startswith('wpt_content_shell')):
            path = '/external/wpt'
        else:
            path = ''
        map_tests_to_results(tests_to_actual_results,
                             actual_results_json['tests'],
                             path=path)

        if (args.baseline_product.startswith('chrome_linux')
                or args.baseline_product.startswith('wpt_content_shell')):
            path = '/external/wpt'
        else:
            path = ''
        map_tests_to_results(tests_to_baseline_results,
                             baseline_results_json['tests'],
                             path=path)

        # Create a CSV file which compares tests results to baseline results
        WPTResultsDiffer(args, host, tests_to_actual_results,
                         tests_to_baseline_results, csv_output,
                         args.ignore_missing).create_csv()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
