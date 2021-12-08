#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_results import TestResult

from rust_main_program import _scrape_test_list
from rust_main_program import _scrape_test_results


class ScrapingTestLists(unittest.TestCase):
    def test_basics(self):
        test_input = """
test_foo: test
test_bar: test
        """.strip()
        actual_results = _scrape_test_list(test_input)
        expected_results = ['test_foo', 'test_bar']
        self.assertEqual(actual_results, expected_results)


class ScrapingTestResults(unittest.TestCase):
    def test_basics(self):
        test_input = """
running 3 tests
test test_foo ... ok
test test_bar ... ok
test test_foobar ... FAILED

failures:

---- test_foobar stdout ----
thread 'test_foobar' panicked at 'assertion failed: `(left == right)`
  left: `7`,
 right: `124`', ../../build/rust/tests/test_rust_source_set/src/lib.rs:29:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace


failures:
    test_foobar

test result: FAILED. 2 passed; 1 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
        """.strip()
        list_of_expected_test_names = ['test_foo', 'test_bar', 'test_foobar']
        actual_results = _scrape_test_results(test_input,
                                              list_of_expected_test_names)
        expected_results = [
            TestResult('test_foo', 'PASS'),
            TestResult('test_bar', 'PASS'),
            TestResult('test_foobar', 'FAILED')
        ]
        self.assertEqual(actual_results, expected_results)


if __name__ == '__main__':
    unittest.main()
