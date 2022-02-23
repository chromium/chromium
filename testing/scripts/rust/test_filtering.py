# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a library for handling of --isolated-script-test-filter and
--isolated-script-test-filter-file cmdline arguments, as specified by
//docs/testing/test_executable_api.md

Typical usage:
    import argparse
    import test_filtering

    cmdline_parser = argparse.ArgumentParser()
    test_filtering.add_cmdline_args(cmdline_parser)
    ... adding other cmdline parameter definitions ...
    parsed_cmdline_args = cmdline_parser.parse_args()

    list_of_all_test_names = ... queried from the wrapped test executable ...
    list_of_test_names_to_run = test_filtering.filter_tests(
        parsed_cmdline_args, list_of_all_test_names)
"""

import argparse
import os
import re
import sys


class _TestFilter:
    """_TestFilter represents a single test filter pattern like foo (including
    'foo' test in the test run), bar* (including all tests with a name starting
    with 'bar'), or -baz (excluding 'baz' test from the test run).
    """

    def __init__(self, filter_text):
        assert '::' not in filter_text
        if '*' in filter_text[:-1]:
            raise ValueError('* is only allowed at the end (as documented ' \
                             'in //docs/testing/test_executable_api.md).')

        if filter_text.startswith('-'):
            self._is_exclusion_filter = True
            filter_text = filter_text[1:]
        else:
            self._is_exclusion_filter = False

        if filter_text.endswith('*'):
            self._is_prefix_match = True
            filter_text = filter_text[:-1]
        else:
            self._is_prefix_match = False

        self._filter_text = filter_text

    def is_match(self, test_name):
        """Returns whether the test filter should apply to `test_name`.
        """
        if self._is_prefix_match:
            return test_name.startswith(self._filter_text)
        else:
            return test_name == self._filter_text

    def is_exclusion_filter(self):
        """Rreturns whether this filter excludes (rather than includes) matching
        test names.
        """
        return self._is_exclusion_filter

    def get_specificity_key(self):
        """Returns a key that can be used to sort the TestFilter objects by
        their specificity.  From //docs/testing/test_executable_api.md:
        If multiple filters [...] match a given test name, the longest match
        takes priority (longest match wins). [...] It is an error to have
        multiple expressions of the same length that conflict (e.g., a*::-a*).
        """
        return (len(self._filter_text), self._filter_text)

    def __str__(self):
        result = self._filter_text
        if self._is_exclusion_filter:
            result = "-" + result
        if self._is_prefix_match:
            result += "*"
        return result


class _TestFiltersGroup:
    """_TestFiltersGroup represents an individual group of test filters
    (corresponding to a single --isolated-script-test-filter or
    --isolated-script-test-filter-file cmdline argument).
    """

    def __init__(self, list_of_test_filters):
        """Internal implementation detail - please use from_string and/or
        from_filter_file static methods instead."""
        self._list_of_test_filters = sorted(
            list_of_test_filters,
            key=lambda x: x.get_specificity_key(),
            reverse=True)

        if all(f.is_exclusion_filter() for f in self._list_of_test_filters):
            self._list_of_test_filters.append(_TestFilter('*'))
        assert len(list_of_test_filters)

        for i in range(len(self._list_of_test_filters) - 1):
            prev = self._list_of_test_filters[i]
            curr = self._list_of_test_filters[i + 1]
            if prev.get_specificity_key() == curr.get_specificity_key():
                raise ValueError(
                    'It is an error to have multiple test filters of the ' \
                    'same length that conflict (e.g., a*::-a*).  Conflicting ' \
                    'filters: {} and {}'.format(prev, curr))

    @staticmethod
    def from_string(cmdline_arg):
        """Constructs a _TestFiltersGroup from a string that follows the format
        of --isolated-script-test-filter cmdline argument as described in
        Chromium's //docs/testing/test_executable_api.md
        """
        list_of_test_filters = []
        for filter_text in cmdline_arg.split('::'):
            list_of_test_filters.append(_TestFilter(filter_text))
        return _TestFiltersGroup(list_of_test_filters)

    @staticmethod
    def from_filter_file(filepath):
        """Constructs a _TestFiltersGroup from an input file that can be passed
        to the --isolated-script-test-filter-file cmdline argument as described
        Chromium's //docs/testing/test_executable_api.md.  The file format is
        described in bit.ly/chromium-test-list-format (aka go/test-list-format).
        """
        list_of_test_filters = []
        regex = r'  \[ [^]]* \]'  # [ foo ]
        regex += r'| Bug \( [^)]* \)'  # Bug(12345)
        regex += r'| crbug.com/\S*'  # crbug.com/12345
        regex += r'| skbug.com/\S*'  # skbug.com/12345
        regex += r'| webkit.org/\S*'  # webkit.org/12345
        compiled_regex = re.compile(regex, re.VERBOSE)
        with open(filepath, mode='r', encoding='utf-8') as f:
            for line in f.readlines():
                filter_text = line.split('#')[0]
                filter_text = compiled_regex.sub('', filter_text)
                filter_text = filter_text.strip()
                if filter_text:
                    list_of_test_filters.append(_TestFilter(filter_text))
        return _TestFiltersGroup(list_of_test_filters)

    def is_test_name_included(self, test_name):
        for test_filter in self._list_of_test_filters:
            if test_filter.is_match(test_name):
                return not test_filter.is_exclusion_filter()
        return False


class _SetOfTestFiltersGroups:
    def __init__(self, list_of_test_filter_groups):
        """Constructs _SetOfTestFiltersGroups from `list_of_test_filter_groups`.

        Args:
            list_of_test_filter_groups: A list of _TestFiltersGroup objects.
        """
        self._test_filters_groups = list_of_test_filter_groups

    def filter_test_names(self, list_of_test_names):
        return [
            t for t in list_of_test_names if self._is_test_name_included(t)
        ]

    def _is_test_name_included(self, test_name):
        for test_filters_group in self._test_filters_groups:
            if not test_filters_group.is_test_name_included(test_name):
                return False
        return True


def _shard_tests(list_of_test_names, env):
    # Defaulting to 0 for `shard_index` and to 1 for `total_shards`.
    shard_index = int(env.get('GTEST_SHARD_INDEX', 0))
    total_shards = int(env.get('GTEST_TOTAL_SHARDS', 1))
    assert shard_index < total_shards

    result = []
    for i in range(len(list_of_test_names)):
        if (i % total_shards) == shard_index:
            result.append(list_of_test_names[i])

    return result


def _filter_test_names(list_of_test_names, argparse_parsed_args):
    inline_filter_groups = [
        _TestFiltersGroup.from_string(s)
        for s in argparse_parsed_args.test_filters
    ]
    filter_file_groups = [
        _TestFiltersGroup.from_filter_file(f)
        for f in argparse_parsed_args.test_filter_files
    ]
    set_of_filter_groups = _SetOfTestFiltersGroups(inline_filter_groups +
                                                   filter_file_groups)
    return set_of_filter_groups.filter_test_names(list_of_test_names)


def add_cmdline_args(argparse_parser):
    """Adds test-filtering-specific cmdline parameter definitions to
    `argparse_parser`.

    Args:
        argparse_parser: An object of argparse.ArgumentParser type.
    """
    filter_help = 'A double-colon-separated list of strings, where each ' \
                  'string either uniquely identifies a full test name or is ' \
                  'a prefix plus a "*" on the end (to form a glob). If the ' \
                  'string has a "-" at the front, the test (or glob of ' \
                  'tests) will be skipped, not run.'
    argparse_parser.add_argument('--test-filter',
                                 '--isolated-script-test-filter',
                                 action='append',
                                 default=[],
                                 dest='test_filters',
                                 help=filter_help,
                                 metavar='TEST-NAME-PATTERNS')
    file_help = 'Path to a file with test filters in Chromium Test List ' \
                'Format. See also //testing/buildbot/filters/README.md and ' \
                'bit.ly/chromium-test-list-format'
    argparse_parser.add_argument('--test-filter-file',
                                 '--isolated-script-test-filter-file',
                                 action='append',
                                 default=[],
                                 dest='test_filter_files',
                                 help=file_help,
                                 metavar='FILEPATH')


def filter_tests(argparse_parsed_args, env, list_of_test_names):
    """Filters `list_of_test_names` as requested by the cmdline arguments
    and sharding-related environment variables.

    Args:
        argparse_parsed_arg: A result of an earlier call to
          argparse_parser.parse_args() call (where `argparse_parser` has been
          populated via an even earlier call to add_cmdline_args).
        env: a dictionary-like object (typically from `os.environ`).
        list_of_test_name: A list of strings (a list of test names).
    """
    filtered_names = _filter_test_names(list_of_test_names,
                                        argparse_parsed_args)
    sharded_names = _shard_tests(filtered_names, env)
    return sharded_names
