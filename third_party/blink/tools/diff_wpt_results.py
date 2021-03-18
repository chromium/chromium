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
import json
import os
import sys


def map_tests_to_results(output_mp, input_mp, path=''):
    if 'actual' in input_mp:
        output_mp[path[1:]] = input_mp
    else:
        for k, v in input_mp.items():
            map_tests_to_results(output_mp, v, path + '/' + k)


def create_csv(actual_results_map, baseline_results_map, csv_output):
    super_set = (set(actual_results_map.keys()) |
                 set(baseline_results_map.keys()))
    file_output = 'Test name, Test Result, Baseline Result, Result Comparison\n'

    for test in sorted(super_set):
        line = [test]
        for result_mp in [actual_results_map, baseline_results_map]:
            line.append(result_mp.get(test, {'actual': 'MISSING'})
                                 .get('actual').split()[-1])

        if line[-1] == line[-2]:
            line.append('SAME RESULTS')
        elif 'MISSING' in (line[-1], line[-2]):
            line.append('MISSING RESULTS')
        else:
            line.append('DIFFERENT RESULTS')
        file_output += ','.join(line) + '\n'

    csv_output.write(file_output)


def main(args):
    parser = argparse.ArgumentParser(prog=os.path.basename(__file__))
    parser.add_argument('--baseline-test-results', required=True,
                        help='Path to baseline test results JSON file')
    parser.add_argument('--test-results-to-compare', required=True,
                        help='Path to actual test results JSON file')
    parser.add_argument('--csv-output', required=True,
                        help='Path to CSV output file')
    args = parser.parse_args()

    with open(args.test_results_to_compare, 'r') as actual_results_content, \
            open(args.baseline_test_results, 'r') as baseline_results_content, \
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
        create_csv(tests_to_actual_results,
                   tests_to_baseline_results, csv_output)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
