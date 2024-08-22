# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.web_tests.controllers import web_test_finder
from blinkpy.web_tests.models import test_expectations


class WebTestFinderTests(unittest.TestCase):
    def test_skip_tests_expectations(self):
        """Tests that tests are skipped based on to expectations and options."""
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        all_tests = [
            'fast/css/passes.html',
            'fast/css/fails.html',
            'fast/css/times_out.html',
            'fast/css/skip.html',
        ]

        # Patch port.tests() to return our tests
        port.tests = lambda paths: paths or all_tests

        options = optparse.Values({
            'no_expectations': False,
            'enable_sanitizer': False,
            'skipped': 'default',
            'skip_timeouts': False,
            'skip_failing_tests': False,
        })
        finder = web_test_finder.WebTestFinder(port, options)

        expectations = test_expectations.TestExpectations(port)
        expectations.merge_raw_expectations(
            ('# results: [ Failure Timeout Skip ]'
             '\nfast/css/fails.html [ Failure ]'
             '\nfast/css/times_out.html [ Timeout ]'
             '\nfast/css/skip.html [ Skip ]'))

        # When run with default settings, we only skip the tests marked Skip.
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(tests, set(['fast/css/skip.html']))

        # Specify test on the command line; by default should not skip.
        tests = finder.skip_tests(['fast/css/skip.html'], all_tests,
                                  expectations)
        self.assertEqual(tests, set())

        # Specify test on the command line, but always skip.
        finder._options.skipped = 'always'
        tests = finder.skip_tests(['fast/css/skip.html'], all_tests,
                                  expectations)
        self.assertEqual(tests, set(['fast/css/skip.html']))
        finder._options.skipped = 'default'

        # Only run skip tests, aka skip all non-skipped tests.
        finder._options.skipped = 'only'
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(
            tests,
            set([
                'fast/css/passes.html', 'fast/css/fails.html',
                'fast/css/times_out.html'
            ]))
        finder._options.skipped = 'default'

        # Ignore any skip entries, aka never skip anything.
        finder._options.skipped = 'ignore'
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(tests, set())
        finder._options.skipped = 'default'

        # Skip tests that are marked TIMEOUT.
        finder._options.skip_timeouts = True
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(
            tests, set(['fast/css/times_out.html', 'fast/css/skip.html']))
        finder._options.skip_timeouts = False

        # Skip tests that are marked FAILURE
        finder._options.skip_failing_tests = True
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(tests,
                         set(['fast/css/fails.html', 'fast/css/skip.html']))
        finder._options.skip_failing_tests = False

        # Disable expectations entirely; nothing should be skipped by default.
        finder._options.no_expectations = True
        tests = finder.skip_tests([], all_tests, None)
        self.assertEqual(tests, set())

    def test_skip_tests_idlharness(self):
        """Tests that idlharness tests are skipped on MSAN/ASAN runs.

        See https://crbug.com/856601
        """
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        non_idlharness_test = 'external/wpt/dir1/dir2/foo.html'
        idlharness_test_1 = 'external/wpt/dir1/dir2/idlharness.any.html'
        idlharness_test_2 = 'external/wpt/dir1/dir2/idlharness.any.worker.html'
        all_tests = [
            non_idlharness_test,
            idlharness_test_1,
            idlharness_test_2,
        ]

        # Patch port.tests() to return our tests
        port.tests = lambda paths: paths or all_tests

        options = optparse.Values({
            'no_expectations': False,
            'enable_sanitizer': False,
            'skipped': 'default',
            'skip_timeouts': False,
            'skip_failing_tests': False,
        })
        finder = web_test_finder.WebTestFinder(port, options)

        # Default case; not MSAN/ASAN so should not skip anything.
        expectations = test_expectations.TestExpectations(port)
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(tests, set())

        # MSAN/ASAN, with no paths specified explicitly, so should skip both
        # idlharness tests.
        expectations = test_expectations.TestExpectations(port)
        finder._options.enable_sanitizer = True
        tests = finder.skip_tests([], all_tests, expectations)
        self.assertEqual(tests, set([idlharness_test_1, idlharness_test_2]))

        # Disable expectations entirely; we should still skip the idlharness
        # tests but shouldn't touch the expectations parameter.
        finder._options.no_expectations = True
        tests = finder.skip_tests([], all_tests, None)
        self.assertEqual(tests, set([idlharness_test_1, idlharness_test_2]))

        # MSAN/ASAN, with one of the tests specified explicitly (and
        # --skipped=default), so should skip only the unspecified test.
        expectations = test_expectations.TestExpectations(port)
        tests = finder.skip_tests([idlharness_test_1], all_tests, expectations)
        self.assertEqual(tests, set([idlharness_test_2]))

    def test_find_fastest_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        all_tests = [
            'path/test.html',
            'new/test.html',
            'fast/css/1.html',
            'fast/css/2.html',
            'fast/css/3.html',
            'fast/css/skip1.html',
            'fast/css/skip2.html',
            'fast/css/skip3.html',
            'fast/css/skip4.html',
            'fast/css/skip5.html',
        ]

        port.tests = lambda paths: paths or all_tests

        finder = web_test_finder.WebTestFinder(port, {})
        finder._times_trie = lambda: {
            'fast': {
                'css': {
                    '1.html': 1,
                    '2.html': 2,
                    '3.html': 3,
                    'skip1.html': 0,
                    'skip2.html': 0,
                    'skip3.html': 0,
                    'skip4.html': 0,
                    'skip5.html': 0,
                }
            },
            'path': {
                'test.html': 4,
            }
        }

        tests = finder.find_tests(fastest_percentile=50, args=[])
        self.assertEqual(
            set(tests[1]),
            set(['fast/css/1.html', 'fast/css/2.html', 'new/test.html']))

        tests = finder.find_tests(
            fastest_percentile=50, args=['path/test.html'])
        self.assertEqual(
            set(tests[1]),
            set([
                'fast/css/1.html', 'fast/css/2.html', 'path/test.html',
                'new/test.html'
            ]))

        tests = finder.find_tests(args=[])
        self.assertEqual(tests[1], all_tests)

        tests = finder.find_tests(args=['path/test.html'])
        self.assertEqual(tests[1], ['path/test.html'])

    def test_find_fastest_tests_excludes_deleted_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)

        all_tests = [
            'fast/css/1.html',
            'fast/css/2.html',
        ]

        port.tests = lambda paths: paths or all_tests

        finder = web_test_finder.WebTestFinder(port, {})

        finder._times_trie = lambda: {
            'fast': {
                'css': {
                    '1.html': 1,
                    '2.html': 2,
                    'non-existant.html': 1,
                }
            },
        }

        tests = finder.find_tests(fastest_percentile=90, args=[])
        self.assertEqual(set(tests[1]), set(['fast/css/1.html']))

    def test_split_chunks(self):
        split = web_test_finder.WebTestFinder._split_into_chunks  # pylint: disable=protected-access

        tests = ['1', '2', '3', '4']
        self.assertEqual(['1', '2', '3', '4'], split(tests, 0, 1))

        self.assertEqual(['3', '4'], split(tests, 0, 2))
        self.assertEqual(['1', '2'], split(tests, 1, 2))

        self.assertEqual(['1', '2', '4'], split(tests, 0, 3))
        self.assertEqual([], split(tests, 1, 3))
        self.assertEqual(['3'], split(tests, 2, 3))

        tests = ['1', '2', '3', '4', '5']
        self.assertEqual(['1', '2', '3', '4', '5'], split(tests, 0, 1))

        self.assertEqual(['3', '4'], split(tests, 0, 2))
        self.assertEqual(['1', '2', '5'], split(tests, 1, 2))

        self.assertEqual(['1', '2', '4'], split(tests, 0, 3))
        self.assertEqual(['5'], split(tests, 1, 3))
        self.assertEqual(['3'], split(tests, 2, 3))

        tests = ['1', '2', '3', '4', '5', '6']
        self.assertEqual(['1', '2', '3', '4', '5', '6'], split(tests, 0, 1))

        self.assertEqual(['3', '4'], split(tests, 0, 2))
        self.assertEqual(['1', '2', '5', '6'], split(tests, 1, 2))

        self.assertEqual(['1', '2', '4'], split(tests, 0, 3))
        self.assertEqual(['5', '6'], split(tests, 1, 3))
        self.assertEqual(['3'], split(tests, 2, 3))

    def test_test_list_find_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)
        mock_files = {'test-list.txt': \
            'path/test.html\n'\
            'virtual/path/test.html'}
        host.filesystem = MockFileSystem(files=mock_files)

        port_tests = [
            'path/test.html',
            'not/in/test/list.html',
        ]

        port.tests = lambda paths: paths or port_tests

        finder = web_test_finder.WebTestFinder(port, {})

        tests = finder.find_tests(args=[], test_lists=['test-list.txt'])
        self.assertEqual(
            set(tests[1]),
            set(['path/test.html','virtual/path/test.html',]))

    def test_inverted_test_filter_find_tests(self):
        host = MockHost()
        port = host.port_factory.get('test-win-win7', None)
        mock_files = {
            'test-list.txt': 'path/test.html\nvirtual/path/test.html',
            'inverted-filter.txt': 'path/test.html'
        }
        host.filesystem = MockFileSystem(files=mock_files)

        port_tests = [
            'path/test.html',
            'not/in/test/list.html',
        ]

        port.tests = lambda paths: paths or port_tests

        finder = web_test_finder.WebTestFinder(port, {})

        tests = finder.find_tests(
            args=[],
            test_lists=['test-list.txt'],
            inverted_filter_files=['inverted-filter.txt'])
        self.assertEqual(set(tests[1]), set([
            'virtual/path/test.html',
        ]))

class FilterTestsTests(unittest.TestCase):
    simple_test_filter = ['a/a1.html', 'a/a2.html', 'b/b1.html']

    def check(self, tests, filters, expected_tests):
        self.assertEqual(expected_tests,
                         web_test_finder.filter_tests(tests, filters))

    def test_no_filters(self):
        self.check(self.simple_test_filter, [], self.simple_test_filter)

    def test_empty_glob_is_rejected(self):
        self.assertRaises(ValueError, self.check, self.simple_test_filter,
                          [['']], [])
        self.assertRaises(ValueError, self.check, self.simple_test_filter,
                          [['-']], [])

    def test_one_all_positive_filter(self):
        self.check(self.simple_test_filter, [['a*']],
                   ['a/a1.html', 'a/a2.html'])
        self.check(self.simple_test_filter, [['+a*']],
                   ['a/a1.html', 'a/a2.html'])

        self.check(self.simple_test_filter, [['a*', 'b*']],
                   self.simple_test_filter)

    def test_one_exact_positive_filter(self):
        self.check(self.simple_test_filter, [['a/a1.html']], ['a/a1.html'])
        self.check(self.simple_test_filter, [['+a/a1.html']], ['a/a1.html'])

    def test_one_all_negative_filter(self):
        self.check(self.simple_test_filter, [['-c*']], self.simple_test_filter)

    def test_one_exact_negative_filter(self):
        self.check(self.simple_test_filter, [['-a/a1.html']],
                   ['a/a2.html', 'b/b1.html'])

    def test_one_mixed_filter(self):
        self.check(self.simple_test_filter, [['a*', '-c*']],
                   ['a/a1.html', 'a/a2.html'])

    def test_two_all_positive_filters(self):
        self.check(self.simple_test_filter, [['a*'], ['b*']], [])

    def test_two_all_negative_filters(self):
        self.check(self.simple_test_filter, [['-a*'], ['-b*']], [])

        self.check(self.simple_test_filter, [['-a*'], ['-c*']], ['b/b1.html'])

    def test_two_mixed_filters(self):
        self.check(self.simple_test_filter, [['a*'], ['-b*']],
                   ['a/a1.html', 'a/a2.html'])

    def test_longest_glob_wins(self):
        # These test that if two matching globs are specified as
        # part of the same filter expression, the longest matching
        # glob wins (takes precedence). The order of the two globs
        # must not matter.
        self.check(self.simple_test_filter, [['a/a*', '-a/a2*']],
                   ['a/a1.html'])
        self.check(self.simple_test_filter, [['-a/a*', 'a/a2*']],
                   ['a/a2.html'])

        # In this test, the positive and negative globs are in
        # separate filter expressions, so a2 should be filtered out
        # and nothing should run (tests should only be run if they
        # would be run by every filter individually).
        self.check(self.simple_test_filter, [['-a/a*'], ['a/a2*']], [])

    def test_only_trailing_unescaped_globs_work(self):
        self.check(self.simple_test_filter, [['a*']],
                   ['a/a1.html', 'a/a2.html'])
        # These test that if you have a glob that contains a "*" that isn't
        # at the end, it is rejected; only globs at the end should work.
        self.assertRaises(ValueError, self.check, self.simple_test_filter,
                          [['*1.html']], [])
        self.assertRaises(ValueError, self.check, self.simple_test_filter,
                          [['a*.html']], [])

    def test_escaped_globs_allowed(self):
        self.check(self.simple_test_filter + ['a\\*1'], [['-a\\*1']],
                   self.simple_test_filter)
