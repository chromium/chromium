#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from test_results import TestResult

from rust_main_program import _format_test_name
from rust_main_program import _parse_test_name
from rust_main_program import _get_exe_specific_tests
from rust_main_program import _scrape_test_list
from rust_main_program import _scrape_test_results
from rust_main_program import _parse_args
from rust_main_program import _TestExecutableWrapper

# Protected access is allowed for unittests.
# pylint: disable=protected-access

class Tests(fake_filesystem_unittest.TestCase):
    def test_format_test_name(self):
        self.assertEqual('test_exe//test_bar',
                         _format_test_name('test_exe', 'test_bar'))
        self.assertEqual('test_exe//foo/test_foo',
                         _format_test_name('test_exe', 'foo::test_foo'))

    def test_parse_test_name(self):
        self.assertEqual(('test_exe', 'test_bar'),
                         _parse_test_name('test_exe//test_bar'))
        self.assertEqual(('test_exe', 'foo::test_foo'),
                         _parse_test_name('test_exe//foo/test_foo'))

    def test_scrape_test_list(self):
        test_input = """
test_foo: test
test_bar: test
foo::test_in_mod: test
test_benchmark: benchmark
        """.strip()
        actual_results = _scrape_test_list(test_input, 'test_exe_name')
        expected_results = [
            'test_exe_name//test_foo', 'test_exe_name//test_bar',
            'test_exe_name//foo/test_in_mod'
        ]
        self.assertEqual(actual_results, expected_results)

    # https://crbug.com/1281664 meant that Rust executables might
    # incorrectly think that they were invoked with no cmdline args.
    # Back then we didn't realize that out test wrappers broken :-(.
    # The test below tries to ensure this won't happen again.
    def test_scrape_test_list_with_unexpected_lines(self):
        test_input = """
running 1 test
test test_hello ... ok

test result: ok. 1 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; \
finished in 0.00s
        """.strip()
        with self.assertRaises(ValueError):
            _scrape_test_list(test_input, 'test_exe_name')

    def test_scrape_test_results(self):
        test_input = """
running 3 tests
test test_foo ... ok
test test_bar ... ok
test foo::test_in_mod ... ok
test test_foobar ... FAILED

failures:

---- test_foobar stdout ----
thread 'test_foobar' panicked at 'assertion failed: `(left == right)`
  left: `7`,
 right: `124`', ../../build/rust/tests/test_rust_static_library/src/lib.rs:29:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace


failures:
    test_foobar

test result: FAILED. 3 passed; 1 failed; 0 ignored; 0 measured; \
0 filtered out; finished in 0.00s
        """.strip()
        list_of_expected_test_names = [
            'test_foo', 'test_bar', 'foo::test_in_mod', 'test_foobar'
        ]
        actual_results = _scrape_test_results(test_input, 'test_exe_name',
                                              list_of_expected_test_names)
        expected_results = [
            TestResult('test_exe_name//test_foo', 'PASS'),
            TestResult('test_exe_name//test_bar', 'PASS'),
            TestResult('test_exe_name//foo/test_in_mod', 'PASS'),
            TestResult('test_exe_name//test_foobar', 'FAIL')
        ]
        self.assertEqual(actual_results, expected_results)

    def test_parse_args(self):
        args = _parse_args(['--rust-test-executable=foo'])
        self.assertEqual(['foo'], args.rust_test_executables)

        args = _parse_args(
            ['--rust-test-executable=foo', '--rust-test-executable=bar'])
        self.assertEqual(['foo', 'bar'], args.rust_test_executables)

    def test_get_exe_specific_tests(self):
        result = _get_exe_specific_tests(
            'exe_name',
            ['exe_name//foo1', 'exe_name//foo2', 'other_exe//foo3'])
        self.assertEqual(['foo1', 'foo2'], result)

    def test_executable_wrapper_basic_construction(self):
        with tempfile.TemporaryDirectory() as tmpdirname:
            exe_filename = 'foo-bar.exe'
            exe_path = os.path.join(tmpdirname, exe_filename)
            with open(exe_path, 'w'):
                pass
            t = _TestExecutableWrapper(exe_path)
            self.assertEqual('foo-bar', t._name_of_test_executable)

    def test_executable_wrapper_missing_file(self):
        with self.assertRaises(ValueError):
            _TestExecutableWrapper('no-such-file.exe')


if __name__ == '__main__':
    unittest.main()
