#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from test_results import TestResult

import main_program


class FakeTestExecutableWrapper:
    def __init__(self, hardcoded_test_list, hardcoded_test_results):
        self._hardcoded_test_list = hardcoded_test_list
        self._hardcoded_test_results = hardcoded_test_results

    def list_all_tests(self):
        return self._hardcoded_test_list

    def run_tests(self, list_of_tests_to_run):
        results = []
        for test in self._hardcoded_test_results:
            if test.test_name in list_of_tests_to_run:
                results.append(test)
        return results


class EndToEndTests(fake_filesystem_unittest.TestCase):
    def test_basic_scenario(self):
        with tempfile.TemporaryDirectory() as tmpdirname:
            # Prepare simulated inputs.
            test_list = [
                'test_foo', 'test_bar', 'test_foobar', 'module/test_foo'
            ]
            test_results = [
                TestResult('test_foo', 'PASS'),
                TestResult('test_bar', 'PASS'),
                TestResult('test_foobar', 'FAILED'),
                TestResult('module/test_foo', 'PASS')
            ]
            fake_executable_wrapper = FakeTestExecutableWrapper(
                test_list, test_results)
            parser = argparse.ArgumentParser()
            main_program.add_cmdline_args(parser)
            output_file = os.path.join(tmpdirname, 'test.out')
            args = parser.parse_args(
                args=['--isolated-script-test-output={}'.format(output_file)])
            fake_env = {'GTEST_SHARD_INDEX': 0, 'GTEST_TOTAL_SHARDS': 1}

            # Run code under test.
            main_program.main([fake_executable_wrapper], args, fake_env)

            # Verify results.
            with open(output_file) as f:
                actual_json_output = json.load(f)
                del actual_json_output['seconds_since_epoch']
            # yapf: disable
            expected_json_output = {
                'interrupted': False,
                'path_delimiter': '//',
                #'seconds_since_epoch': 1635974313.8388052,
                'version': 3,
                'tests': {
                    'test_foo': {
                        'expected': 'PASS',
                        'actual': 'PASS'
                    },
                    'test_bar': {
                        'expected': 'PASS',
                        'actual': 'PASS'
                    },
                    'test_foobar': {
                        'expected': 'PASS',
                        'actual': 'FAILED'
                    },
                    'module/test_foo': {
                        'expected': 'PASS',
                        'actual': 'PASS'
                    }},
                'num_failures_by_type': {
                    'PASS': 3,
                    'FAILED': 1
                }
            }
            # yapf: enable
            self.assertEqual(actual_json_output, expected_json_output)


if __name__ == '__main__':
    unittest.main()
