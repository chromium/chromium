# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import fnmatch
import hashlib
import json
import logging
import re

from blinkpy.web_tests.layout_package.json_results_generator import convert_times_trie_to_flat_paths
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.base import Port
from collections import OrderedDict

_log = logging.getLogger(__name__)


class WebTestFinder(object):
    def __init__(self, port, options):
        self._port = port
        self._options = options
        self._filesystem = self._port.host.filesystem
        self.WEB_TESTS_DIRECTORIES = ('src', 'third_party', 'blink',
                                      'web_tests')

    def find_tests(
        self,
        args,
        test_lists=None,
        filter_files=None,
        inverted_filter_files=None,
        fastest_percentile=None,
        filters=None,
    ):
        filters = filters or []
        paths = self._strip_test_dir_prefixes(args)
        if test_lists:
            new_paths = self._read_test_list_files(
                test_lists, self._port.TEST_PATH_SEPARATOR)
            new_paths = [
                self._strip_test_dir_prefix(new_path) for new_path in new_paths
            ]
            paths += new_paths

        all_tests = []
        if not paths or fastest_percentile:
            all_tests = self._port.tests(None)

        path_tests = []
        if paths:
            path_tests = self._port.tests(paths)

        test_files = None
        running_all_tests = False
        if fastest_percentile:
            times_trie = self._times_trie()
            if times_trie:
                fastest_tests = self._fastest_tests(times_trie, all_tests,
                                                    fastest_percentile)
                test_files = list(set(fastest_tests).union(path_tests))
            else:
                _log.warning(
                    'Running all the tests the first time to generate timing data.'
                )
                test_files = all_tests
                running_all_tests = True
        elif paths:
            test_files = path_tests
        else:
            test_files = all_tests
            running_all_tests = True

        all_filters = []
        if filters:
            all_filters = [f.split('::') for f in filters]
        if filter_files:
            file_filters = self._read_filter_files(
                filter_files, self._port.TEST_PATH_SEPARATOR)
            all_filters = all_filters + file_filters
        if inverted_filter_files:
            file_filters = self._read_filter_files(
                inverted_filter_files, self._port.TEST_PATH_SEPARATOR)
            all_filters = all_filters + self._invert_filters(file_filters)

        test_files = filter_tests(test_files, all_filters)

        # de-dupe the test list and paths here before running them.
        test_files = list(OrderedDict.fromkeys(test_files))
        paths = list(OrderedDict.fromkeys(paths))
        return (paths, test_files, running_all_tests)

    def _times_trie(self):
        times_ms_path = self._port.bot_test_times_path()
        if self._filesystem.exists(times_ms_path):
            return json.loads(self._filesystem.read_text_file(times_ms_path))
        else:
            return {}

    # The following line should run the fastest 50% of tests *and*
    # the css3/flexbox tests. It should *not* run the fastest 50%
    # of the css3/flexbox tests.
    #
    # run_web_tests.py --fastest=50 css3/flexbox
    def _fastest_tests(self, times_trie, all_tests, fastest_percentile):
        times = convert_times_trie_to_flat_paths(times_trie)

        # Ignore tests with a time==0 because those are skipped tests.
        sorted_times = sorted([test for (test, time) in times.items() if time],
                              key=lambda t: (times[t], t))
        clamped_percentile = max(0, min(100, fastest_percentile))
        number_of_tests_to_return = int(
            len(sorted_times) * clamped_percentile / 100)
        fastest_tests = set(sorted_times[:number_of_tests_to_return])

        # Don't try to run tests in the times_trie that no longer exist,
        fastest_tests = fastest_tests.intersection(all_tests)

        # For fastest tests, include any tests not in the times_ms.json so that
        # new tests get run in the fast set.
        unaccounted_tests = set(all_tests) - set(times.keys())

        # Using a set to dedupe here means that --order=None won't work, but that's
        # ok because --fastest already runs in an arbitrary order.
        return list(fastest_tests.union(unaccounted_tests))

    def _strip_test_dir_prefixes(self, paths):
        return [self._strip_test_dir_prefix(path) for path in paths if path]

    def _strip_test_dir_prefix(self, path):
        # Remove src/third_party/blink/web_tests/ from the front of the test path,
        # or any subset of these.
        for i in range(len(self.WEB_TESTS_DIRECTORIES)):
            # Handle both "web_tests/foo/bar.html" and "web_tests\foo\bar.html" if
            # the filesystem uses '\\' as a directory separator
            for separator in (self._port.TEST_PATH_SEPARATOR,
                              self._filesystem.sep):
                directory_prefix = separator.join(
                    self.WEB_TESTS_DIRECTORIES[i:]) + separator
                if path.startswith(directory_prefix):
                    return path[len(directory_prefix):]
        return path

    def _read_filter_files(self, filenames, test_path_separator):
        fs = self._filesystem
        filters = []
        for filename in filenames:
            file_lines = []
            try:
                if test_path_separator != fs.sep:
                    filename = filename.replace(test_path_separator, fs.sep)
                file_contents = fs.read_text_file(filename).split('\n')
                for line in file_contents:
                    line = self._strip_comments(line)
                    if not line:
                        continue
                    file_lines.append(line)
                filters.append(file_lines)
            except IOError as error:
                if error.errno == errno.ENOENT:
                    _log.critical('')
                    _log.critical('--test-launcher-filter-file "%s" not found',
                                  filename)
                raise
        return filters

    def _invert_filters(self, filters):
        inverted_filters = []
        for terms in filters:
            inverted_filter = []
            for term in terms:
                if term.startswith('-'):
                    inverted_filter.append(term[1:])
                elif term.startswith('+'):
                    inverted_filter.append('-' + term[1:])
                else:
                    inverted_filter.append('-' + term)
            inverted_filters.append(inverted_filter)
        return inverted_filters

    def _read_test_list_files(self, filenames, test_path_separator):
        fs = self._filesystem
        positive_matches = []
        for filename in filenames:
            try:
                if test_path_separator != fs.sep:
                    filename = filename.replace(test_path_separator, fs.sep)
                file_contents = fs.read_text_file(filename).split('\n')
                for line in file_contents:
                    line = self._strip_comments(line)
                    if not line:
                        continue
                    if line[0] == '-':
                        _log.debug(
                            'test-list %s contains a negative filter %s' %
                            (filename, line))
                    positive_matches.append(line)
            except IOError as error:
                if error.errno == errno.ENOENT:
                    _log.critical('')
                    _log.critical('--test-list file "%s" not found', filename)
                raise
        return positive_matches

    @staticmethod
    def _strip_comments(line):
        commentIndex = line.find('#')
        if commentIndex == -1:
            commentIndex = len(line)

        line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
        if line == '':
            return None
        else:
            return line

    def skip_tests(self, paths, all_tests_list, expectations):
        """Given a list of tests, returns the ones that should be skipped.

        A test may be skipped for many reasons, depending on the expectation
        files and options selected. The most obvious is SKIP entries in
        TestExpectations, but we also e.g. skip idlharness tests on MSAN/ASAN
        due to https://crbug.com/856601. Note that for programmatically added
        SKIPs, this function will modify the input expectations to include the
        SKIP expectation (but not write it to disk)

        Args:
            paths: the paths passed on the command-line to run_web_tests.py
            all_tests_list: all tests that we are considering running
            expectations: parsed TestExpectations data

        Returns: a set of tests that should be skipped (not run).
        """
        all_tests = set(all_tests_list)
        tests_to_skip = set()
        tests_always_skipped = set()
        for test in all_tests:
            # Manual tests and virtual tests skipped by platform config are
            # always skipped and not affected by the --skip parameter
            if (self._port.skipped_due_to_manual_test(test)
                    or self._port.virtual_test_skipped_due_to_platform_config(
                        test)
                    or self._port.skipped_due_to_exclusive_virtual_tests(test)
                    or self._port.skipped_due_to_skip_base_tests(test)):
                tests_always_skipped.update({test})
                continue

            # We always skip idlharness tests for MSAN/ASAN, even when running
            # with --no-expectations (https://crbug.com/856601). Note we will
            # run the test anyway if it is explicitly specified on the command
            # line; paths are removed from the skip list after this loop.
            if self._options.enable_sanitizer and Port.is_wpt_idlharness_test(
                    test):
                tests_to_skip.update({test})
                continue

            if self._options.no_expectations:
                # do not skip anything from TestExpectations
                continue

            expected_results = expectations.get_expectations(test).results
            if ResultType.Skip in expected_results:
                tests_to_skip.update({test})
            if self._options.skip_timeouts and ResultType.Timeout in expected_results:
                tests_to_skip.update({test})
            if self._options.skip_failing_tests and ResultType.Failure in expected_results:
                tests_to_skip.update({test})

        if self._options.skipped == 'only':
            tests_to_skip = all_tests - tests_to_skip
        elif self._options.skipped == 'ignore':
            tests_to_skip = set()
        elif self._options.skipped != 'always':
            # make sure we're explicitly running any tests passed on the command line; equivalent to 'default'.
            tests_to_skip -= set(paths)

        tests_to_skip.update(tests_always_skipped)

        return tests_to_skip

    def split_into_chunks(self, test_names):
        """split into a list to run and a set to skip, based on --shard_index and --total_shards."""
        if self._options.shard_index is None and self._options.total_shards is None:
            return test_names

        if self._options.shard_index is None:
            raise ValueError(
                'Must provide --shard-index or GTEST_SHARD_INDEX when sharding.'
            )
        if self._options.total_shards is None:
            raise ValueError(
                'Must provide --total-shards or GTEST_TOTAL_SHARDS when sharding.'
            )
        if self._options.shard_index >= self._options.total_shards:
            raise ValueError(
                'Shard index (%d) should be less than total shards (%d)!' %
                (self._options.shard_index, self._options.total_shards))

        return self._split_into_chunks(test_names, self._options.shard_index,
                                       self._options.total_shards)

    @staticmethod
    def _split_into_chunks(test_names, index, count):
        tests_and_indices = [
            (test_name,
             int(hashlib.sha256(test_name.encode('utf-8')).hexdigest(), 16) %
             count) for test_name in test_names
        ]

        tests_to_run = [
            test_name for test_name, test_index in tests_and_indices
            if test_index == index
        ]

        _log.debug('chunk %d of %d contains %d tests of %d', index, count,
                   len(tests_to_run), len(test_names))

        return tests_to_run


def filter_tests(tests, filters):
    """Returns a filtered list of tests to run.

    The test-filtering semantics are documented in
    https://bit.ly/chromium-test-runner-api and
    https://bit.ly/chromium-test-list-format, but are as follows:

    A test should be run only if it would be run when every flag (ie filter) is
    evaluated individually. A test should be skipped if it would be skipped if
    any flag (filter) was evaluated individually.

    Each filter is a list of potentially glob expressions, with each expression
    optionally prefixed by a "-" or "+". If the glob starts with a "-", it is a
    negative term, otherwise it is a positive term.

    If multiple filter terms in a flag (filter) match a given test name, the
    longest match takes priority (longest match wins). The order of the filters
    should not matter. It is an error to have multiple expressions of the same
    length that conflict.

    Globbing is fairly limited; "?" is not allowed, and unescaped "*" must only
    appear at the end of the glob. If multiple globs match a test, the longest
    match wins. If both globs are the same length, an error is raised.

    A test will be run only if it passes every filter.
    """

    def glob_sort_key(k):
        if k and k[0] == '-':
            return (len(k[1:]), k[1:])
        elif k and k[0] == '+':
            return (len(k[1:]), k[1:])
        else:
            return (len(k), k)

    for terms in filters:
        # Validate the filter
        for term in terms:
            if (term.startswith('-') and not term[1:]) or not term:
                raise ValueError('Empty filter entry "%s"' % (term, ))
            for i, c in enumerate(term):
                if i == len(term) - 1:
                    continue
                if c == '*' and (i == 0 or term[i - 1] != '\\'):
                    raise ValueError('Bad test filter "%s" specified; '
                                     'unescaped wildcards are only allowed at '
                                     'the end' % (term, ))
            if term.startswith('-') and term[1:] in terms:
                raise ValueError('Both "%s" and "%s" specified in test '
                                 'filter' % (term, term[1:]))

        # Separate the negative/positive globless terms and glob terms
        include_by_default = all(term.startswith('-') for term in terms)
        exact_neg_terms, exact_pos_terms, glob_terms = _extract_terms(terms)

        filtered_tests = []
        for test in tests:
            # Check for the exact test name
            if test in exact_neg_terms:
                continue
            if test in exact_pos_terms:
                filtered_tests.append(test)
                continue

            # Check globs, this could be done with a trie to avoid this loop
            # but glob terms should be lower volume than exact terms
            include = include_by_default
            for glob in sorted(glob_terms, key=glob_sort_key):
                if glob.startswith('-'):
                    include = include and not fnmatch.fnmatch(test, glob[1:])
                else:
                    include = include or fnmatch.fnmatch(
                        test, glob[1:] if glob[0] == '+' else glob)
            if include:
                filtered_tests.append(test)
        tests = filtered_tests

    return tests


def _extract_terms(filter):
    """Extract terms of the filter into exact +/- terms and glob terms.

    The +/- prefix char for exact terms will also be stripped as they are no
    longer needed to identify the term as a positive or negative filter. Any
    prefix in globbed terms (terms with a *) will preserve the sign.
    """
    exact_negative_terms = set()
    exact_positive_terms = set()
    glob_terms = set()
    for term in filter:
        is_glob = False
        for i, c in enumerate(term):
            if c == '*' and (i == 0 or term[i - 1] != '\\'):
                glob_terms.add(term)
                is_glob = True
                break
        if is_glob:
            continue
        elif term[0] == '-':
            exact_negative_terms.add(term[1:])
        elif term[0] == '+':
            exact_positive_terms.add(term[1:])
        else:
            exact_positive_terms.add(term)
    return exact_negative_terms, exact_positive_terms, list(glob_terms)
