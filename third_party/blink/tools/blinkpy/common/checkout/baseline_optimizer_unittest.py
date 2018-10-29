# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer, ResultDigest
from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.web_tests.builder_list import BuilderList

ALL_PASS_TESTHARNESS_RESULT = """This is a testharness.js-based test.
PASS woohoo
Harness: the test ran to completion.
"""

ALL_PASS_TESTHARNESS_RESULT2 = """This is a testharness.js-based test.
PASS woohoo
PASS yahoo
Harness: the test ran to completion.
"""

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS


class BaselineOptimizerTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.fs = MockFileSystem()
        self.host.filesystem = self.fs
        # TODO(robertma): Even though we have mocked the builder list (and hence
        # all_port_names), we are still relying on the knowledge of currently
        # configured ports and their fallback order. Ideally, we should improve
        # MockPortFactory and use it.
        self.host.builders = BuilderList({
            'Fake Test Win10': {
                'port_name': 'win-win10',
                'specifiers': ['Win10', 'Release']
            },
            'Fake Test Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'Fake Test Mac10.13': {
                'port_name': 'mac-mac10.13',
                'specifiers': ['Mac10.13', 'Release']
            },
            'Fake Test Mac10.12': {
                'port_name': 'mac-mac10.12',
                'specifiers': ['Mac10.12', 'Release']
            },
            'Fake Test Mac10.11': {
                'port_name': 'mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release']
            },
            'Fake Test Mac10.10': {
                'port_name': 'mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release']
            },
        })
        # Note: this is a pre-assumption of the tests in this file. If this
        # assertion fails, port configurations are likely changed, and the
        # tests need to be adjusted accordingly.
        self.assertEqual(sorted(self.host.port_factory.all_port_names()),
                         ['linux-trusty', 'mac-mac10.10', 'mac-mac10.11', 'mac-mac10.12', 'mac-mac10.13', 'win-win10'])

    def _assert_optimization(self, results_by_directory, directory_to_new_results, baseline_dirname=''):
        layout_tests_dir = PathFinder(self.fs).layout_tests_dir()
        test_name = 'mock-test.html'
        baseline_name = 'mock-test-expected.txt'
        self.fs.write_text_file(
            self.fs.join(layout_tests_dir, 'VirtualTestSuites'),
            '[{"prefix": "gpu", "base": "fast/canvas", "args": ["--foo"]}]')

        for dirname, contents in results_by_directory.items():
            self.fs.write_binary_file(self.fs.join(layout_tests_dir, dirname, baseline_name), contents)

        baseline_optimizer = BaselineOptimizer(self.host, self.host.port_factory.get(), self.host.port_factory.all_port_names())
        self.assertTrue(baseline_optimizer.optimize(
            self.fs.join(baseline_dirname, test_name), 'txt'))

        for dirname, contents in directory_to_new_results.items():
            path = self.fs.join(layout_tests_dir, dirname, baseline_name)
            if contents is None:
                # Check files that are explicitly marked as absent.
                self.assertFalse(self.fs.exists(path), '%s should not exist after optimization' % path)
            else:
                self.assertEqual(self.fs.read_binary_file(path), contents, 'Content of %s != "%s"' % (path, contents))

        for dirname in results_by_directory:
            path = self.fs.join(layout_tests_dir, dirname, baseline_name)
            if dirname not in directory_to_new_results or directory_to_new_results[dirname] is None:
                self.assertFalse(self.fs.exists(path), '%s should not exist after optimization' % path)

    def test_linux_redundant_with_win(self):
        self._assert_optimization(
            {
                'platform/win': '1',
                'platform/linux': '1',
            },
            {
                'platform/win': '1',
            })

    def test_covers_mac_win_linux(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '1',
                'platform/linux': '1',
            },
            {
                '': '1',
            })

    def test_overwrites_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '1',
                'platform/linux': '1',
                '': '2',
            },
            {
                '': '1',
            })

    def test_no_new_common_directory(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                '': '2',
            },
            {
                'platform/mac': '1',
                'platform/linux': '1',
                '': '2',
            })

    def test_local_optimization(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.11': '1',
            },
            {
                'platform/mac': '1',
                'platform/linux': '1',
            })

    def test_local_optimization_skipping_a_port_in_the_middle(self):
        # mac-mac10.10 -> mac-mac10.11 -> mac
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.10': '1',
            },
            {
                'platform/mac': '1',
                'platform/linux': '1',
            })

    def test_baseline_redundant_with_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '2',
            },
            {
                'platform/mac': '1',
                '': '2',
            })

    def test_root_baseline_unused(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '3',
            },
            {
                'platform/mac': '1',
                'platform/win': '2',
            })

    def test_root_baseline_unused_and_non_existant(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
            },
            {
                'platform/mac': '1',
                'platform/win': '2',
            })

    def test_virtual_baseline_redundant_with_non_virtual(self):
        self._assert_optimization(
            {
                'platform/win/virtual/gpu/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            },
            {
                'platform/win/fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_redundant_with_non_virtual_fallback(self):
        # virtual linux -> virtual win -> virtual root -> linux -> win
        self._assert_optimization(
            {
                'platform/linux/virtual/gpu/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            },
            {
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/win/fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'platform/win/virtual/gpu/fast/canvas': '2',
                'fast/canvas': '2',
            },
            {
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'fast/canvas': '2',
            },
            {
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            },
            {
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_covers_mac_win_linux(self):
        self._assert_optimization(
            {
                'platform/mac/virtual/gpu/fast/canvas': '1',
                'platform/win/virtual/gpu/fast/canvas': '1',
                'platform/linux/virtual/gpu/fast/canvas': '1',
            },
            {
                'virtual/gpu/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_at_root(self):
        self._assert_optimization(
            {'': ALL_PASS_TESTHARNESS_RESULT},
            {'': None})

    def test_all_pass_testharness_at_linux(self):
        self._assert_optimization(
            {'platform/linux': ALL_PASS_TESTHARNESS_RESULT},
            {'platform/linux': None})

    def test_all_pass_testharness_at_linux_and_win(self):
        # https://crbug.com/805008
        self._assert_optimization(
            {'platform/linux': ALL_PASS_TESTHARNESS_RESULT,
             'platform/win': ALL_PASS_TESTHARNESS_RESULT},
            {'platform/linux': None,
             'platform/win': None})

    def test_all_pass_testharness_at_virtual_root(self):
        self._assert_optimization(
            {'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT},
            {'virtual/gpu/fast/canvas': None},
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_at_virtual_linux(self):
        self._assert_optimization(
            {'platform/linux/virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT},
            {'platform/linux/virtual/gpu/fast/canvas': None},
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_can_be_updated(self):
        # https://crbug.com/866802
        self._assert_optimization(
            {
                'fast/canvas': 'failure',
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/win/virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT2,
                'platform/mac/virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT2,
            },
            {
                'fast/canvas': 'failure',
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT2,
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_falls_back_to_non_pass(self):
        # The all-PASS baseline needs to be preserved in this case.
        self._assert_optimization(
            {
                'platform/linux': ALL_PASS_TESTHARNESS_RESULT,
                '': '1'
            },
            {
                'platform/linux': ALL_PASS_TESTHARNESS_RESULT,
                '': '1'
            })

    def test_virtual_all_pass_testharness_falls_back_to_base(self):
        # The all-PASS baseline needs to be preserved in this case.
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/fast/canvas': '1',
            },
            {
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    # Tests for protected methods - pylint: disable=protected-access

    def test_move_baselines(self):
        self.fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/win/another/test-expected.txt', 'result A')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/mac/another/test-expected.txt', 'result A')
        self.fs.write_binary_file(MOCK_WEB_TESTS + 'another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(), self.host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt',
            {
                MOCK_WEB_TESTS + 'platform/win': 'aaa',
                MOCK_WEB_TESTS + 'platform/mac': 'aaa',
                MOCK_WEB_TESTS[:-1]: 'bbb',
            },
            {
                MOCK_WEB_TESTS[:-1]: 'aaa',
            })
        self.assertEqual(
            self.fs.read_binary_file(
                MOCK_WEB_TESTS + 'another/test-expected.txt'),
            'result A')

    def test_move_baselines_skip_git_commands(self):
        self.fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/win/another/test-expected.txt', 'result A')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/mac/another/test-expected.txt', 'result A')
        self.fs.write_binary_file(MOCK_WEB_TESTS + 'another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(), self.host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt',
            {
                MOCK_WEB_TESTS + 'platform/win': 'aaa',
                MOCK_WEB_TESTS + 'platform/mac': 'aaa',
                MOCK_WEB_TESTS[:-1]: 'bbb',
            },
            {
                MOCK_WEB_TESTS + 'platform/linux': 'bbb',
                MOCK_WEB_TESTS[:-1]: 'aaa',
            })
        self.assertEqual(
            self.fs.read_binary_file(
                MOCK_WEB_TESTS + 'another/test-expected.txt'),
            'result A')


class ResultDigestTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.fs = MockFileSystem()
        self.host.filesystem = self.fs
        self.fs.write_text_file('/all-pass/foo-expected.txt', ALL_PASS_TESTHARNESS_RESULT)
        self.fs.write_text_file('/all-pass/bar-expected.txt', ALL_PASS_TESTHARNESS_RESULT2)
        self.fs.write_text_file('/failures/baz-expected.txt', 'failure')

    def test_test_all_pass_testharness_result(self):
        self.assertTrue(ResultDigest.test_all_pass_testharness_result(
            self.fs, '/all-pass/foo-expected.txt'))
        self.assertTrue(ResultDigest.test_all_pass_testharness_result(
            self.fs, '/all-pass/bar-expected.txt'))
        self.assertFalse(ResultDigest.test_all_pass_testharness_result(
            self.fs, '/failures/baz-expected.txt'))
        self.assertFalse(ResultDigest.test_all_pass_testharness_result(
            self.fs, '/others/something-expected.png'))

    def test_implicit_all_pass(self):
        # Implicit all-PASS should equal to any all-PASS but not failures.
        implicit = ResultDigest(None, None)
        self.assertTrue(implicit == ResultDigest(self.fs, '/all-pass/foo-expected.txt'))
        self.assertTrue(implicit == ResultDigest(self.fs, '/all-pass/bar-expected.txt'))
        self.assertFalse(implicit == ResultDigest(self.fs, '/failures/baz-expected.txt'))

    def test_different_all_pass_results(self):
        x = ResultDigest(self.fs, '/all-pass/foo-expected.txt')
        y = ResultDigest(self.fs, '/all-pass/bar-expected.txt')
        self.assertTrue(x != y)
        self.assertFalse(x == y)
