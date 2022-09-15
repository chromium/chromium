# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a library for printing test result as specified by
//docs/testing/test_executable_api.md (e.g. --isolated-script-test-output)
and //docs/testing/json_test_results_format.md

Typical usage:
    import argparse
    import test_results

    cmdline_parser = argparse.ArgumentParser()
    test_results.add_cmdline_args(cmdline_parser)
    ... adding other cmdline parameter definitions ...
    parsed_cmdline_args = cmdline_parser.parse_args()

    test_results = []
    test_results.append(test_results.TestResult(
        'test-suite/test-name', 'PASS'))
    ...

    test_results.print_test_results(parsed_cmdline_args, test_results)
"""

import argparse
import json
import os


class TestResult:
    """TestResult represents a result of executing a single test once.
    """

    def __init__(self,
                 test_name,
                 actual_test_result,
                 expected_test_result='PASS'):
        self.test_name = test_name
        self.actual_test_result = actual_test_result
        self.expected_test_result = expected_test_result

    def __eq__(self, other):
        self_tuple = tuple(sorted(self.__dict__.items()))
        other_tuple = tuple(sorted(other.__dict__.items()))
        return self_tuple == other_tuple

    def __hash__(self):
        return hash(tuple(sorted(self.__dict__.items())))

    def __repr__(self):
        result = 'TestResult[{}: {}'.format(self.test_name,
                                            self.actual_test_result)
        if self.expected_test_result != 'PASS':
            result += ' (expecting: {})]'.format(self.expected_test_result)
        else:
            result += ']'
        return result


def _validate_output_directory(outdir):
    if not os.path.isdir(outdir):
        raise argparse.ArgumentTypeError('No such directory: ' + outdir)
    return outdir


def add_cmdline_args(argparse_parser):
    """Adds test-result-specific cmdline parameter definitions to
    `argparse_parser`.

    Args:
        argparse_parser: An object of argparse.ArgumentParser type.
    """
    outdir_help = 'Directory where test results will be written into.'
    argparse_parser.add_argument('--isolated-outdir',
                                 dest='outdir',
                                 help=outdir_help,
                                 metavar='DIRPATH',
                                 type=_validate_output_directory)

    outfile_help = 'If this argument is provided, then test results in the ' \
                   'JSON Test Results Format will be written here. See also ' \
                   '//docs/testing/json_test_results_format.md'
    argparse_parser.add_argument('--isolated-script-test-output',
                                 dest='test_output',
                                 default=None,
                                 help=outfile_help,
                                 metavar='FILENAME')

    argparse_parser.add_argument('--isolated-script-test-perf-output',
                                 dest='ignored_perf_output',
                                 default=None,
                                 help='Deprecated and ignored.',
                                 metavar='IGNORED')


def _build_json_data(list_of_test_results, seconds_since_epoch):
    num_failures_by_type = {}
    tests = {}
    for res in list_of_test_results:
        old_count = num_failures_by_type.get(res.actual_test_result, 0)
        num_failures_by_type[res.actual_test_result] = old_count + 1

        path = res.test_name.split('//')
        group = tests
        for group_name in path[:-1]:
            if not group_name in group:
                group[group_name] = {}
            group = group[group_name]

        group[path[-1]] = {
            'expected': res.expected_test_result,
            'actual': res.actual_test_result,
        }

    return {
        'interrupted': False,
        'path_delimiter': '//',
        'seconds_since_epoch': seconds_since_epoch,
        'version': 3,
        'tests': tests,
        'num_failures_by_type': num_failures_by_type,
    }


def print_test_results(argparse_parsed_args, list_of_test_results,
                       testing_start_time_as_seconds_since_epoch):
    """Prints `list_of_test_results` to a file specified on the cmdline.

    Args:
        argparse_parsed_arg: A result of an earlier call to
          argparse_parser.parse_args() call (where `argparse_parser` has been
          populated via an even earlier call to add_cmdline_args).
        list_of_test_results: A list of TestResult objects.
        testing_start_time_as_seconds_since_epoch: A number from an earlier
          `time.time()` call.
    """
    if argparse_parsed_args.test_output is None:
        return

    json_data = _build_json_data(list_of_test_results,
                                 testing_start_time_as_seconds_since_epoch)

    filepath = argparse_parsed_args.test_output
    with open(filepath, mode='w', encoding='utf-8') as f:
        json.dump(json_data, f, indent=2)
