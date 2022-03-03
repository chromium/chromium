#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
from pyfakefs import fake_filesystem_unittest
import tempfile
import unittest

import test_filtering
from test_filtering import _TestFilter
from test_filtering import _TestFiltersGroup
from test_filtering import _SetOfTestFiltersGroups


class FilterTests(fake_filesystem_unittest.TestCase):
    def test_exact_match(self):
        t = _TestFilter('foo')
        self.assertTrue(t.is_match('foo'))
        self.assertFalse(t.is_match('foobar'))
        self.assertFalse(t.is_match('foo/bar'))
        self.assertFalse(t.is_match('fo'))
        self.assertFalse(t.is_exclusion_filter())

    def test_prefix_match(self):
        t = _TestFilter('foo*')
        self.assertTrue(t.is_match('foo'))
        self.assertTrue(t.is_match('foobar'))
        self.assertTrue(t.is_match('foo/bar'))
        self.assertFalse(t.is_match('fo'))
        self.assertFalse(t.is_exclusion_filter())

    def test_exclusion_match(self):
        t = _TestFilter('-foo')
        self.assertTrue(t.is_match('foo'))
        self.assertFalse(t.is_match('foobar'))
        self.assertFalse(t.is_match('foo/bar'))
        self.assertFalse(t.is_match('fo'))
        self.assertTrue(t.is_exclusion_filter())

    def test_exclusion_prefix_match(self):
        t = _TestFilter('-foo*')
        self.assertTrue(t.is_match('foo'))
        self.assertTrue(t.is_match('foobar'))
        self.assertTrue(t.is_match('foo/bar'))
        self.assertFalse(t.is_match('fo'))
        self.assertTrue(t.is_exclusion_filter())

    def test_error_conditions(self):
        # '*' is only supported at the end
        with self.assertRaises(ValueError):
            _TestFilter('*.bar')


def _create_group_from_pseudo_file(file_contents):
    # pylint: disable=unexpected-keyword-arg
    with tempfile.NamedTemporaryFile(delete=False, mode='w',
                                     encoding='utf-8') as f:
        filepath = f.name
        f.write(file_contents)
    try:
        return _TestFiltersGroup.from_filter_file(filepath)
    finally:
        os.remove(filepath)


class FiltersGroupTests(fake_filesystem_unittest.TestCase):
    def test_single_positive_filter(self):
        g = _TestFiltersGroup.from_string('foo*')
        self.assertTrue(g.is_test_name_included('foo'))
        self.assertTrue(g.is_test_name_included('foobar'))
        self.assertFalse(g.is_test_name_included('baz'))
        self.assertFalse(g.is_test_name_included('fo'))

    def test_single_negative_filter(self):
        g = _TestFiltersGroup.from_string('-foo*')
        self.assertFalse(g.is_test_name_included('foo'))
        self.assertFalse(g.is_test_name_included('foobar'))
        self.assertTrue(g.is_test_name_included('baz'))
        self.assertTrue(g.is_test_name_included('fo'))

    def test_specificity_ordering(self):
        # From test_executable_api.md#filtering-which-tests-to-run:
        #
        #     If multiple filters in a flag match a given test name, the longest
        #     match takes priority (longest match wins). I.e.,. if you had
        #     --isolated-script-test-filter='a*::-ab*', then ace.html would run
        #     but abd.html would not. The order of the filters should not
        #     matter.
        g1 = _TestFiltersGroup.from_string('a*::-ab*')  # order #1
        g2 = _TestFiltersGroup.from_string('-ab*::a*')  # order #2
        self.assertTrue(g1.is_test_name_included('ace'))
        self.assertTrue(g2.is_test_name_included('ace'))
        self.assertFalse(g1.is_test_name_included('abd'))
        self.assertFalse(g2.is_test_name_included('abd'))

    def test_specificity_conflicts(self):
        # Docs give this specific example: It is an error to have multiple
        # expressions of the same length that conflict (e.g., a*::-a*).
        with self.assertRaises(ValueError):
            _TestFiltersGroup.from_string('a*::-a*')
        # Other similar conflict:
        with self.assertRaises(ValueError):
            _TestFiltersGroup.from_string('a::-a')
        # It doesn't really make sense to support `foo.bar` and `foo.bar*` and
        # have the latter take precedence over the former.
        with self.assertRaises(ValueError):
            _TestFiltersGroup.from_string('foo.bar::foo.bar*')
        # In the same spirit, identical duplicates are also treated as
        # conflicts.
        with self.assertRaises(ValueError):
            _TestFiltersGroup.from_string('foo.bar::foo.bar')

        # Ok - no conflicts:
        _TestFiltersGroup.from_string('a*::-b*')  # Different filter text

    def test_simple_list(self):
        # 'simple test list' format from bit.ly/chromium-test-list-format
        # (aka go/test-list-format)
        file_content = """
# Comment

aaa
bbb # End-of-line comment
Bug(intentional) ccc [ Crash ] # Comment
crbug.com/12345 [ Debug] ddd
skbug.com/12345 [ Debug] eee
webkit.org/12345 [ Debug] fff
ggg*
-ggghhh
""".strip()
        g = _create_group_from_pseudo_file(file_content)
        self.assertTrue(g.is_test_name_included('aaa'))
        self.assertTrue(g.is_test_name_included('bbb'))
        self.assertTrue(g.is_test_name_included('ccc'))
        self.assertTrue(g.is_test_name_included('ddd'))

        self.assertFalse(g.is_test_name_included('aa'))
        self.assertFalse(g.is_test_name_included('aaax'))

        self.assertTrue(g.is_test_name_included('ggg'))
        self.assertTrue(g.is_test_name_included('gggg'))
        self.assertFalse(g.is_test_name_included('gg'))
        self.assertFalse(g.is_test_name_included('ggghhh'))

        self.assertFalse(g.is_test_name_included('zzz'))

    def test_tagged_list(self):
        # tagged list format from bit.ly/chromium-test-list-format
        # (aka go/test-list-format)
        file_content = """
# Comment

abc
foo* # End-of-line comment
-foobar*
""".strip()
        g = _create_group_from_pseudo_file(file_content)
        self.assertTrue(g.is_test_name_included('abc'))
        self.assertFalse(g.is_test_name_included('abcd'))
        self.assertTrue(g.is_test_name_included('foo'))
        self.assertTrue(g.is_test_name_included('food'))
        self.assertFalse(g.is_test_name_included('foobar'))
        self.assertFalse(g.is_test_name_included('foobarbaz'))


class SetOfFilterGroupsTests(fake_filesystem_unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # `_filter1`, `_filter2`, and `_tests` are based on the setup described
        # in test_executable_api.md#examples
        cls._filter1 = _TestFiltersGroup.from_string(
            'Foo.Bar.*::-Foo.Bar.bar3')
        cls._filter2 = _TestFiltersGroup.from_string('Foo.Bar.bar2')
        cls._tests = [
            'Foo.Bar.bar1',
            'Foo.Bar.bar2',
            'Foo.Bar.bar3',
            'Foo.Baz.baz',  # TODO: Fix typo in test_executable_api.md
            'Foo.Quux.quux'
        ]

    def test_basics(self):
        # This test corresponds to
        # test_executable_api.md#filtering-tests-on-the-command-line
        # and
        # test_executable_api.md#using-a-filter-file
        s = _SetOfTestFiltersGroups([self._filter1])
        self.assertEqual(['Foo.Bar.bar1', 'Foo.Bar.bar2'],
                         s.filter_test_names(self._tests))

    def test_combining_multiple_filters1(self):
        # This test corresponds to the first test under
        # test_executable_api.md#combining-multiple-filters
        s = _SetOfTestFiltersGroups([
            _TestFiltersGroup.from_string('Foo.Bar.*'),
            _TestFiltersGroup.from_string('Foo.Bar.bar2')
        ])
        self.assertEqual(['Foo.Bar.bar2'], s.filter_test_names(self._tests))

    def test_combining_multiple_filters2(self):
        # This test corresponds to the second test under
        # test_executable_api.md#combining-multiple-filters
        # TODO(lukasza@chromium.org): Figure out if the 3rd test example from
        # the docs has correct inputs+outputs (or if there are some typos).
        s = _SetOfTestFiltersGroups([
            _TestFiltersGroup.from_string('Foo.Bar.*'),
            _TestFiltersGroup.from_string('Foo.Baz.baz')
        ])
        self.assertEqual([], s.filter_test_names(self._tests))


class PublicApiTests(fake_filesystem_unittest.TestCase):
    def test_filter_cmdline_arg(self):
        parser = argparse.ArgumentParser()
        test_filtering.add_cmdline_args(parser)
        args = parser.parse_args(args=[
            '--isolated-script-test-filter=-barbaz',
            '--isolated-script-test-filter=foo*::bar*'
        ])
        self.assertEqual(
            ['foo1', 'foo2', 'bar1', 'bar2'],
            test_filtering.filter_tests(
                args, {}, ['foo1', 'foo2', 'bar1', 'bar2', 'barbaz', 'zzz']))

    def test_filter_file_cmdline_arg(self):
        # pylint: disable=unexpected-keyword-arg
        f = tempfile.NamedTemporaryFile(delete=False,
                                        mode='w',
                                        encoding='utf-8')
        try:
            filepath = f.name
            f.write('foo*')
            f.close()

            parser = argparse.ArgumentParser()
            test_filtering.add_cmdline_args(parser)
            args = parser.parse_args(args=[
                '--isolated-script-test-filter-file={0:s}'.format(filepath)
            ])
            self.assertEqual(['foo1', 'foo2'],
                             test_filtering.filter_tests(
                                 args, {}, ['foo1', 'foo2', 'bar1', 'bar2']))
        finally:
            os.remove(filepath)


def _shard_tests(input_test_list_string, input_env):
    input_test_list = input_test_list_string.split(',')
    output_test_list = test_filtering._shard_tests(input_test_list, input_env)
    return ','.join(output_test_list)


class ShardingTest(unittest.TestCase):
    def test_empty_environment(self):
        self.assertEqual('a,b,c', _shard_tests('a,b,c', {}))

    def test_basic_sharding(self):
        self.assertEqual(
            'a,c,e',
            _shard_tests('a,b,c,d,e', {
                'GTEST_SHARD_INDEX': '0',
                'GTEST_TOTAL_SHARDS': '2'
            }))
        self.assertEqual(
            'b,d',
            _shard_tests('a,b,c,d,e', {
                'GTEST_SHARD_INDEX': '1',
                'GTEST_TOTAL_SHARDS': '2'
            }))

    def test_error_conditions(self):
        # shard index > total shards
        with self.assertRaises(Exception):
            _shard_tests('', {
                'GTEST_SHARD_INDEX': '2',
                'GTEST_TOTAL_SHARDS': '2'
            })

        # non-integer shard index
        with self.assertRaises(Exception):
            _shard_tests('', {
                'GTEST_SHARD_INDEX': 'a',
                'GTEST_TOTAL_SHARDS': '2'
            })

        # non-integer total shards
        with self.assertRaises(Exception):
            _shard_tests('', {
                'GTEST_SHARD_INDEX': '0',
                'GTEST_TOTAL_SHARDS': 'b'
            })


if __name__ == '__main__':
    unittest.main()
