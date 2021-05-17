#!/usr/bin/env vpython

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
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.android import (
    PRODUCTS, PRODUCTS_TO_STEPNAMES)

CSV_HEADING = ('Test name, Test Result, Baseline Result, '
               'Result Comparison, Test Flaky Results, '
               'Baseline Flaky Results, Unreliable Comparison\n')
YES = 'Yes'
NO = 'No'
_log = logging.getLogger(os.path.basename(__file__))
STEP_NAME_VARIANTS = {
    'chrome_public_wpt': ['chrome_public_wpt on Ubuntu-16.04 or Ubuntu-18.04'],
    'weblayer_shell_wpt': ['weblayer_shell_wpt on Ubuntu-16.04 or Ubuntu-18.04']
}

def map_tests_to_results(output_mp, input_mp, path=''):
    if 'actual' in input_mp:
        output_mp[path[1:]] = input_mp
    else:
        for k, v in input_mp.items():
            map_tests_to_results(output_mp, v, path + '/' + k)


class WPTResultsDiffer(object):

    def __init__(self, args, host, actual_results_map,
                 baseline_results_map, csv_output):
        self._args = args
        self._host = host
        self._actual_results_map = actual_results_map
        self._baseline_results_map = baseline_results_map
        self._csv_output = csv_output
        self._test_flaky_results = self._get_flaky_test_results(
            args.product_to_compare)
        self._baseline_flaky_results = self._get_flaky_test_results(
            args.baseline_product)

    def _get_flaky_test_results(self, product):
        return self._get_bot_expectations(product).flakes_by_path(
            False, ignore_bot_expected_results=True,
            consider_only_flaky_runs=False)

    def _get_bot_expectations(self, product):
        specifiers = [product]
        builders = self._host.builders.filter_builders(
            include_specifiers=specifiers)
        assert len(builders) == 1, (
            'Multiple builders match the specifiers %s' % specifiers)

        builder_name = builders[0]
        bot_expectations_factory = BotTestExpectationsFactory(
            self._host.builders, PRODUCTS_TO_STEPNAMES[product])

        return bot_expectations_factory.expectations_for_builder(builder_name)

    def flaky_results(self, test_name, flaky_dict):
        return (flaky_dict.get(test_name, set()) or
                flaky_dict.get(test_name.replace('external/wpt/', ''), set()))

    def create_csv(self):
        super_set = (set(self._actual_results_map.keys()) |
                     set(self._baseline_results_map.keys()))
        file_output = CSV_HEADING

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
                line.append('MISSING RESULTS')
            else:
                line.append('DIFFERENT RESULTS')

            if line[-1] != 'MISSING RESULTS':
                test_flaky_results = self.flaky_results(
                    test, self._test_flaky_results)

                baseline_flaky_results = self.flaky_results(
                    test, self._baseline_flaky_results)

                test_flaky_results.update([line[1]])
                baseline_flaky_results.update([line[2]])
                is_flaky = (len(test_flaky_results) > 1 or
                            len(baseline_flaky_results) > 1)
                line.extend(['"{%s}"' % ', '.join(test_flaky_results),
                             '"{%s}"' % ', '.join(baseline_flaky_results)])

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
def _get_product_test_results(host, product, results_path=None):
    if results_path:
        json_results_obj = open(results_path, 'r')
    else:
        _log.info(('Retrieving test results for '
                   'product %s using the bb command'), product)
        specifiers = [product]
        builders = host.builders.filter_builders(
            include_specifiers=specifiers)
        assert len(builders) == 1

        builder_name = builders[0]
        latest_build = host.bb_agent.get_latest_finished_build(
            builder_name)
        _log.debug('The latest build for %s is %d',
                   builder_name, latest_build.build_number)

        build_results = _get_build_test_results(host, product, latest_build)
        json_results_obj = tempfile.TemporaryFile()
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
                        help='Name of the baseline WPT product')
    parser.add_argument('--test-results-to-compare', required=False,
                        help='Path to actual test results JSON file')
    parser.add_argument('--product-to-compare', required=True, action='store',
                        choices=PRODUCTS,
                        help='Name of the WPT product being compared')
    parser.add_argument('--csv-output', required=True,
                        help='Path to CSV output file')
    args = parser.parse_args()

    # TODO(rmhasan): Set the log level using a command line argument
    logging.basicConfig(level=logging.INFO)

    assert args.product_to_compare != args.baseline_product, (
        'Product to compare and the baseline product cannot be the same')

    host = Host()
    actual_results_getter = _get_product_test_results(
        host, args.product_to_compare, args.test_results_to_compare)
    baseline_results_getter = _get_product_test_results(
        host, args.baseline_product, args.baseline_test_results)

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
        map_tests_to_results(tests_to_actual_results,
                             actual_results_json['tests'])
        map_tests_to_results(tests_to_baseline_results,
                             baseline_results_json['tests'])

        # Create a CSV file which compares tests results to baseline results
        WPTResultsDiffer(args, host, tests_to_actual_results,
                         tests_to_baseline_results, csv_output).create_csv()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
