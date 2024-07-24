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

import json
import optparse
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer, ResultDigest
from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.models.testharness_results import ABBREVIATED_ALL_PASS

ALL_PASS_TESTHARNESS_RESULT = """This is a testharness.js-based test.
[PASS] woohoo
Harness: the test ran to completion.
"""

ALL_PASS_TESTHARNESS_RESULT2 = """This is a testharness.js-based test.
[PASS] woohoo
[PASS] yahoo
Harness: the test ran to completion.
"""

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS


class BaselineTest(unittest.TestCase):
    # TODO(crbug.com/1376646): Consider expanding `FileSystemTestCase` for this
    # similar use case. Currently, `FileSystemTestCase` can neither write
    # initial baselines nor assert that baselines are removed.

    def setUp(self):
        self.host = MockHost()
        self.fs = self.host.filesystem
        self.finder = PathFinder(self.fs)

    def _write_baselines(self, baseline_name: str, results_by_directory):
        for dirname, contents in results_by_directory.items():
            self.fs.write_text_file(
                self.finder.path_from_web_tests(dirname, baseline_name),
                contents)

    def _assert_baselines(self, baseline_name: str, directory_to_new_results):
        for dirname, contents in directory_to_new_results.items():
            path = self.finder.path_from_web_tests(dirname, baseline_name)
            if contents is None:
                # Check files that are explicitly marked as absent.
                self.assertFalse(
                    self.fs.exists(path),
                    '%s should not exist for copy/optimization' % path)
            else:
                self.assertEqual(self.fs.read_text_file(path), contents,
                                 'Content of %s != "%s"' % (path, contents))


@mock.patch(
    'blinkpy.web_tests.port.test.TestPort.FALLBACK_PATHS', {
        'trusty': ['linux', 'win'],
        'mac11': ['mac'],
        'mac10.11': ['mac-mac10.11', 'mac'],
        'mac10.10': ['mac-mac10.10', 'mac-mac10.11', 'mac'],
        'win10': ['win'],
        'win10-arm64': ['win10-arm64', 'win'],
        'win7': ['win7', 'win'],
    })
class BaselineOptimizerTest(BaselineTest):
    def setUp(self):
        super().setUp()
        self.host.builders = BuilderList({
            'Fake Test Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release']
            },
            'Fake Test Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win11', 'Release']
            },
            'Fake Test Win10-arm64': {
                'port_name': 'test-win-win10-arm64',
                'specifiers': ['Win10-arm64', 'Release']
            },
            'Fake Test Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'Fake Test Linux HighDPI': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'steps': {
                    'high_dpi_blink_web_tests': {
                        'flag_specific': 'highdpi',
                    },
                },
            },
            'Fake Test Mac11': {
                'port_name': 'test-mac-mac11',
                'specifiers': ['Mac11', 'Release'],
            },
            'Fake Test Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release']
            },
            'Fake Test Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release']
            },
        })

    def _assert_optimization(self,
                             results_by_directory,
                             directory_to_new_results,
                             baseline_dirname='',
                             suffix='txt',
                             options=None,
                             virtual_suites=None):
        test_name = 'mock-test.html'
        baseline_name = 'mock-test-expected.' + suffix
        virtual_suites = virtual_suites or [{
            'prefix':
            'gpu',
            'platforms': ['Linux', 'Mac', 'Win'],
            'bases': [
                'webexposed',
                'fast/canvas',
                'slow/canvas/mock-test.html',
                'virtual/virtual_empty_bases/',
            ],
            'args': ['--foo'],
        }, {
            'prefix':
            'virtual_empty_bases',
            'platforms': ['Linux', 'Mac', 'Win'],
            'bases': [],
            'args': ['--foo'],
        }, {
            'prefix':
            'stable',
            'platforms': ['Linux', 'Mac', 'Win'],
            'bases': ['webexposed'],
            'args': ['--stable-release-mode'],
        }]
        self.fs.write_text_file(
            self.finder.path_from_web_tests('VirtualTestSuites'),
            json.dumps(virtual_suites))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('FlagSpecificConfig'),
            '[{"name": "highdpi", "args": ["--force-device-scale-factor=1.5"]}]'
        )
        self.fs.write_text_file(
            self.finder.path_from_web_tests('NeverFixTests'),
            '# tags: [ Linux Mac Mac10.10 Mac10.11 Mac11 Win Win7 Win10 Win10-arm64 ]\n'
            '# results: [ Skip Pass ]\n'
            '[ Win7 ] virtual/gpu/fast/canvas/mock-test.html [ Skip ] \n')
        self._write_baselines(baseline_name, results_by_directory)
        if options:
            options = optparse.Values(options)
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(options=options),
            self.host.port_factory.all_port_names())
        self.assertTrue(
            baseline_optimizer.optimize(
                self.fs.join(baseline_dirname, test_name), suffix))

        self._assert_baselines(baseline_name, directory_to_new_results)
        for dirname in results_by_directory:
            path = self.finder.path_from_web_tests(dirname, baseline_name)
            if (dirname not in directory_to_new_results
                    or directory_to_new_results[dirname] is None):
                self.assertFalse(
                    self.fs.exists(path),
                    '%s should not exist after optimization' % path)

    def _assert_reftest_optimization(self,
                                     results_by_directory,
                                     directory_to_new_results,
                                     test_path='',
                                     baseline_dirname=''):
        self.fs.write_text_file(
            self.finder.path_from_web_tests(test_path,
                                            'mock-test-expected.html'), 'ref')
        self._assert_optimization(
            results_by_directory,
            directory_to_new_results,
            baseline_dirname,
            suffix='png')

    def test_linux_redundant_with_win(self):
        self._assert_optimization(
            {
                'platform/win': '1',
                'platform/linux': '1',
            }, {
                'platform/win': '1',
                'platform/linux': None,
            })

    def test_covers_mac_win(self):
        self._assert_optimization({
            'platform/mac': '1',
            'platform/win': '1',
        }, {
            '': '1',
            'platform/mac': None,
            'platform/win': None,
        })

    def test_overwrites_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '1',
                'platform/linux': '1',
                '': '2',
            }, {
                '': '1',
                'platform/mac': None,
                'platform/win': None,
                'platform/linux': None,
            })

    def test_no_new_common_directory(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                '': '2',
            }, {
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
            }, {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.11': None,
            })

    def test_local_optimization_skipping_a_port_in_the_middle(self):
        # mac-mac10.10 -> mac-mac10.11 -> mac
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.10': '1',
            }, {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.10': None,
            })

    def test_baseline_redundant_with_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '2',
            }, {
                'platform/mac': '1',
                'platform/win': None,
                '': '2',
            })

    def test_root_baseline_unused(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '3',
            }, {
                'platform/mac': '1',
                'platform/win': '2',
            })

    def test_root_baseline_unused_and_non_existant(self):
        self._assert_optimization({
            'platform/mac': '1',
            'platform/win': '2',
        }, {
            'platform/mac': '1',
            'platform/win': '2',
        })

    def test_virtual_baseline_redundant_with_non_virtual(self):
        self._assert_optimization(
            {
                'platform/win/virtual/gpu/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            }, {
                'platform/win/fast/canvas': '2',
                'platform/win/virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_redundant_with_non_virtual_fallback(self):
        # virtual linux -> virtual win -> virtual root -> linux -> win
        self._assert_optimization(
            {
                'platform/linux/virtual/gpu/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            }, {
                'platform/linux/virtual/gpu/fast/canvas': None,
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/win/fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'platform/win/virtual/gpu/fast/canvas': '2',
                'fast/canvas': '2',
            }, {
                'fast/canvas': '2',
                'platform/win/virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_fallback_to_same_baseline_after_optimization_1(self):
        self._assert_optimization(
            {
                'platform/win/fast/canvas': '1',
                'platform/win7/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/mac/virtual/gpu/fast/canvas': '1',
            }, {
                'platform/win/fast/canvas': '1',
                'platform/win7/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'virtual/gpu/fast/canvas': None,
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/win7/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_fallback_to_same_baseline_after_optimization_2(self):
        self._assert_optimization(
            {
                'platform/mac-mac10.10/virtual/gpu/fast/canvas': '3',
                'platform/mac-mac10.11/fast/canvas': '1',
                'fast/canvas': '2',
            }, {
                'platform/mac-mac10.10/virtual/gpu/fast/canvas': '3',
                'platform/mac-mac10.11/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': None,
                'virtual/gpu/fast/canvas': None,
                'platform/mac-mac10.11/fast/canvas': '1',
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_fallback_to_same_baseline_after_optimization_3(self):
        self._assert_optimization(
            {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/linux/virtual/gpu/fast/canvas': '1',
            }, {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/linux/virtual/gpu/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_redundant_with_nonvirtual_successor(self):
        self._assert_optimization(
            {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/linux/virtual/gpu/fast/canvas': '2',
            }, {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/linux/virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_not_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'platform/mac-mac10.10/virtual/gpu/fast/canvas': '1',
                'platform/mac-mac10.10/fast/canvas': '1',
                'fast/canvas': '2',
            }, {
                'platform/mac-mac10.10/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': None,
                'virtual/gpu/fast/canvas': None,
                'platform/mac-mac10.10/fast/canvas': '1',
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'fast/canvas': '2',
            }, {
                'fast/canvas': '2',
                'virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            }, {
                'fast/canvas': '2',
                'virtual/gpu/fast/canvas': None,
                'platform/mac/fast/canvas': None,
                'platform/win/fast/canvas': None,
            },
            baseline_dirname='fast/canvas')

    def test_virtual_root_redundant_with_ancestors_exclude_skipped(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
                'platform/win7/fast/canvas': '1',
            },
            {
                'virtual/gpu/fast/canvas': None,
                'fast/canvas': '2',
                # win7 skips the virtual test, so it is not deleted.
                'platform/win7/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_some_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/mac-mac10.10/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/mac-mac10.10/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_platform_not_redundant_with_some_ancestors(self):
        self._assert_optimization(
            {
                'platform/mac-mac10.11/virtual/gpu/fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': '1',
                'platform/mac-mac10.11/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            }, {
                'platform/mac-mac10.11/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': '1',
                'platform/mac-mac10.11/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_flag_specific_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
                'flag-specific/highdpi/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': None,
                'platform/win/fast/canvas': None,
                'fast/canvas': '2',
                'flag-specific/highdpi/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_covers_mac_win(self):
        self._assert_optimization(
            {
                'platform/mac/virtual/gpu/fast/canvas': '1',
                'platform/win/virtual/gpu/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': None,
                'platform/win/virtual/gpu/fast/canvas': None,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_exclusive_virtual_roots(self):
        virtual_suites = [{
            'prefix': 'gpu',
            'platforms': ['Linux', 'Mac', 'Win'],
            'bases': ['fast/canvas'],
            'exclusive_tests': ['fast/canvas'],
            'args': ['--foo'],
        }, {
            'prefix': 'not-gpu',
            'platforms': ['Linux', 'Mac', 'Win'],
            'bases': ['fast/canvas'],
            'exclusive_tests': 'ALL',
            'args': ['--bar'],
        }]
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': '1',
                'platform/win/virtual/gpu/fast/canvas': '1',
                'platform/mac/virtual/not-gpu/fast/canvas': '1',
                'platform/win/virtual/not-gpu/fast/canvas': '1',
            }, {
                'fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': None,
                'platform/win/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/not-gpu/fast/canvas': None,
                'platform/win/virtual/not-gpu/fast/canvas': None,
            },
            baseline_dirname='fast/canvas',
            virtual_suites=virtual_suites)

    def test_all_pass_testharness_at_root(self):
        self._assert_optimization({'': ALL_PASS_TESTHARNESS_RESULT},
                                  {'': None})

    def test_all_pass_testharness_at_linux(self):
        self._assert_optimization({
            'platform/linux': ALL_PASS_TESTHARNESS_RESULT
        }, {'platform/linux': None})

    def test_all_pass_testharness_at_linux_and_win(self):
        # https://crbug.com/805008
        self._assert_optimization(
            {
                'platform/linux': ALL_PASS_TESTHARNESS_RESULT,
                'platform/win': ALL_PASS_TESTHARNESS_RESULT
            }, {
                'platform/linux': None,
                'platform/win': None
            })

    def test_all_pass_testharness_abbreviated_with_full(self):
        self._assert_optimization(
            {
                'platform/mac-mac10.10': ALL_PASS_TESTHARNESS_RESULT,
                'platform/mac-mac10.11': ABBREVIATED_ALL_PASS,
                'platform/mac': ALL_PASS_TESTHARNESS_RESULT2,
            }, {
                'platform/mac-mac10.10': None,
                'platform/mac-mac10.11': None,
                'platform/mac': None,
            })

    def test_all_pass_testharness_at_win_and_mac_not_redundant(self):
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT,
                'platform/win/virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT
            }, {
                'fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': None,
                'platform/win/virtual/gpu/fast/canvas': None,
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_at_virtual_root(self):
        self._assert_optimization(
            {'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT},
            {'virtual/gpu/fast/canvas': None},
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_at_virtual_linux(self):
        self._assert_optimization(
            {
                'platform/linux/virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT
            }, {'platform/linux/virtual/gpu/fast/canvas': None},
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_all_pass_testharness_can_be_updated(self):
        # https://crbug.com/866802
        self._assert_optimization(
            {
                'fast/canvas':
                'failure',
                'virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT,
                'platform/win/virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT2,
                'platform/mac/virtual/gpu/fast/canvas':
                ALL_PASS_TESTHARNESS_RESULT2,
            }, {
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
            }, {
                'platform/linux': ALL_PASS_TESTHARNESS_RESULT,
                '': '1'
            })

    def test_virtual_all_pass_testharness_falls_back_to_base(self):
        # The all-PASS baseline needs to be preserved in this case.
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_all_pass_testharness_falls_back_to_full_base_name(self):
        # The all-PASS baseline needs to be preserved in this case.
        self._assert_optimization(
            {
                'virtual/gpu/slow/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/slow/canvas': '1',
            }, {
                'virtual/gpu/slow/canvas': ALL_PASS_TESTHARNESS_RESULT,
                'platform/linux/slow/canvas': '1',
            },
            baseline_dirname='virtual/gpu/slow/canvas')

    def test_physical_under_dir_falls_back_to_virtual(self):
        self._assert_optimization(
            {
                'virtual/gpu/virtual/virtual_empty_bases/': '1',
                'virtual/virtual_empty_bases/': '1',
            }, {
                'virtual/gpu/virtual/virtual_empty_bases/': None,
                'virtual/virtual_empty_bases/': '1',
            },
            baseline_dirname='virtual/virtual_empty_bases/')

    def test_empty_at_root(self):
        self._assert_optimization({'': ''}, {'': None})

    def test_empty_at_linux(self):
        self._assert_optimization({
            'platform/linux': ''
        }, {'platform/linux': None})

    def test_empty_at_linux_and_win(self):
        # https://crbug.com/805008
        self._assert_optimization({
            'platform/linux': '',
            'platform/win': '',
        }, {
            'platform/linux': None,
            'platform/win': None,
        })

    def test_empty_at_virtual_root(self):
        self._assert_optimization({'virtual/gpu/fast/canvas': ''},
                                  {'virtual/gpu/fast/canvas': None},
                                  baseline_dirname='virtual/gpu/fast/canvas')

    def test_empty_at_virtual_linux(self):
        self._assert_optimization(
            {
                'platform/linux/virtual/gpu/fast/canvas': ''
            }, {'platform/linux/virtual/gpu/fast/canvas': None},
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_empty_falls_back_to_non_empty(self):
        # The empty baseline needs to be preserved in this case.
        self._assert_optimization({
            'platform/linux': '',
            '': '1',
        }, {
            'platform/linux': '',
            '': '1',
        })

    def test_virtual_empty_falls_back_to_non_empty(self):
        # The empty baseline needs to be preserved in this case.
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '',
                'platform/linux/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '',
                'platform/linux/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_subtree_identical_to_nonvirtual_alternating(self):
        self._assert_optimization(
            {
                'platform/mac-mac10.11/virtual/gpu/fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': '2',
                'virtual/gpu/fast/canvas': '3',
                'platform/mac-mac10.11/fast/canvas': '1',
                'platform/mac/fast/canvas': '2',
                'fast/canvas': '3',
            }, {
                'platform/mac-mac10.11/fast/canvas': '1',
                'platform/mac/fast/canvas': '2',
                'fast/canvas': '3',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_stable_webexposed_preserved(self):
        self._assert_optimization(
            {
                'platform/mac/virtual/gpu/webexposed': '1',
                'platform/win/virtual/gpu/webexposed': '1',
                'platform/mac/virtual/stable/webexposed': '1',
                'platform/win/virtual/stable/webexposed': '1',
                'platform/mac/webexposed': '1',
                'platform/win/webexposed': '1',
            },
            {
                'webexposed': '1',
                # Baselines are optimized among platforms, but not between the
                # virtual/nonvirtual trees for the "stable" suite, so this
                # virtual root should still exist.
                'virtual/stable/webexposed': '1',
            },
            baseline_dirname='webexposed')

    def test_extra_png_for_reftest_at_root(self):
        self._assert_reftest_optimization({'': 'extra'}, {'': None})

    def test_extra_png_for_reftest_at_linux(self):
        self._assert_reftest_optimization({
            'platform/linux': 'extra'
        }, {'platform/linux': None})

    def test_extra_png_for_reftest_at_linux_and_win(self):
        # https://crbug.com/805008
        self._assert_reftest_optimization({
            'platform/linux': 'extra1',
            'platform/win': 'extra2',
        }, {
            'platform/linux': None,
            'platform/win': None,
        })

    def test_extra_png_for_reftest_at_virtual_root(self):
        self._assert_reftest_optimization(
            {'virtual/gpu/fast/canvas': 'extra'},
            {'virtual/gpu/fast/canvas': None},
            test_path='fast/canvas',
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_extra_png_for_reftest_at_virtual_linux(self):
        self._assert_reftest_optimization(
            {
                'platform/linux/virtual/gpu/fast/canvas': 'extra'
            }, {'platform/linux/virtual/gpu/fast/canvas': None},
            test_path='fast/canvas',
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_extra_png_for_reftest_falls_back_to_base(self):
        # The extra png for reftest should be removed even if it's different
        # from the fallback.
        self._assert_reftest_optimization(
            {
                'platform/linux': 'extra1',
                '': 'extra2',
            }, {
                'platform/linux': None,
                '': None,
            })

    def test_virtual_extra_png_for_reftest_falls_back_to_base(self):
        # The extra png for reftest should be removed even if it's different
        # from the fallback.
        self._assert_reftest_optimization(
            {
                'virtual/gpu/fast/canvas': 'extra',
                'platform/linux/fast/canvas': 'extra2',
            }, {
                'virtual/gpu/fast/canvas': None,
                'platform/linux/fast/canvas': None,
            },
            test_path='fast/canvas',
            baseline_dirname='fast/canvas')

    def test_flag_specific_falls_back_to_base(self):
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'flag-specific/highdpi/fast/canvas': '1',
            }, {
                'fast/canvas': '1',
            },
            baseline_dirname='fast/canvas')

    def test_flag_specific_virtual_falls_back_to_virtual_base(self):
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'virtual/gpu/fast/canvas': '2',
                'flag-specific/highdpi/fast/canvas': '3',
                'flag-specific/highdpi/virtual/gpu/fast/canvas': '2',
            }, {
                'fast/canvas': '1',
                'virtual/gpu/fast/canvas': '2',
                'flag-specific/highdpi/fast/canvas': '3',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_flag_specific_virtual_falls_back_to_flag_specific(self):
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'flag-specific/highdpi/fast/canvas': '2',
                'flag-specific/highdpi/virtual/gpu/fast/canvas': '2',
            }, {
                'fast/canvas': '1',
                'flag-specific/highdpi/fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_both_flag_specific_falls_back_to_base(self):
        self._assert_optimization(
            {
                'fast/canvas': '1',
                'flag-specific/highdpi/fast/canvas': '1',
                'flag-specific/highdpi/virtual/gpu/fast/canvas': '1',
            }, {
                'fast/canvas': '1',
            },
            baseline_dirname='fast/canvas')

    def test_preorder_removal_more_favorable(self):
        # Regression test for crbug.com/1512264
        self._assert_optimization(
            {
                'platform/win10-arm64/slow/canvas': '3',
                'platform/win7/slow/canvas': '2',
                'platform/linux/slow/canvas': '1',
                'platform/win/virtual/gpu/slow/canvas':
                ALL_PASS_TESTHARNESS_RESULT,
                'platform/win10-arm64/virtual/gpu/slow/canvas':
                ALL_PASS_TESTHARNESS_RESULT,
                'platform/win7/virtual/gpu/slow/canvas': '2',
                'platform/linux/virtual/gpu/slow/canvas': '1',
            }, {
                'platform/win10-arm64/slow/canvas':
                '3',
                'platform/win7/slow/canvas':
                '2',
                'platform/linux/slow/canvas':
                '1',
                'platform/win10-arm64/virtual/gpu/slow/canvas':
                ALL_PASS_TESTHARNESS_RESULT,
            },
            baseline_dirname='slow/canvas')


class ResultDigestTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.fs = MockFileSystem()
        self.host.filesystem = self.fs
        self.fs.write_text_file('/all-pass/foo-expected.txt',
                                ALL_PASS_TESTHARNESS_RESULT)
        self.fs.write_text_file('/all-pass/bar-expected.txt',
                                ALL_PASS_TESTHARNESS_RESULT2)
        self.fs.write_text_file('/failures/baz-expected.txt', 'failure')
        self.fs.write_binary_file('/others/reftest-expected.png', b'extra')
        self.fs.write_binary_file('/others/reftest2-expected.png', b'extra2')
        self.fs.write_text_file('/others/empty-expected.txt', '')
        self.fs.write_binary_file('/others/something-expected.png',
                                  b'Something')
        self.fs.write_binary_file('/others/empty-expected.png', b'')

    def test_all_pass_testharness_result(self):
        self.assertTrue(
            ResultDigest.from_file(
                self.fs, '/all-pass/foo-expected.txt').is_extra_result)
        self.assertTrue(
            ResultDigest.from_file(
                self.fs, '/all-pass/bar-expected.txt').is_extra_result)
        self.assertFalse(
            ResultDigest.from_file(
                self.fs, '/failures/baz-expected.txt').is_extra_result)

    def test_canonicalize_testharness(self):
        self.fs.write_text_file(
            '/platform/x/failures/baz-expected.txt',
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] failing subtest
                Harness: the test ran to completion.
                """))
        self.fs.write_text_file(
            '/failures/baz-expected.txt',
            textwrap.dedent("""\

                This is a testharness.js-based test.
                [FAIL] failing subtest
                Harness: the test ran to completion.


                  """))
        self.assertEqual(
            ResultDigest.from_file(self.fs,
                                   '/platform/x/failures/baz-expected.txt'),
            ResultDigest.from_file(self.fs, '/failures/baz-expected.txt'))

    def test_empty_result(self):
        self.assertFalse(
            ResultDigest.from_file(
                self.fs, '/others/something-expected.png').is_extra_result)
        self.assertTrue(
            ResultDigest.from_file(
                self.fs, '/others/empty-expected.txt').is_extra_result)
        self.assertTrue(
            ResultDigest.from_file(
                self.fs, '/others/empty-expected.png').is_extra_result)

    def test_extra_png_for_reftest_result(self):
        self.assertFalse(
            ResultDigest.from_file(
                self.fs, '/others/something-expected.png').is_extra_result)
        self.assertTrue(
            ResultDigest.from_file(self.fs,
                                   '/others/reftest-expected.png',
                                   is_reftest=True).is_extra_result)

    def test_non_extra_result(self):
        self.assertFalse(
            ResultDigest.from_file(
                self.fs, '/others/something-expected.png').is_extra_result)

    def test_implicit_extra_result(self):
        # Implicit empty equal to any extra result but not failures.
        implicit = ResultDigest.from_file(None, None)
        self.assertEqual(
            implicit,
            ResultDigest.from_file(self.fs, '/all-pass/foo-expected.txt'))
        self.assertEqual(
            implicit,
            ResultDigest.from_file(self.fs, '/all-pass/bar-expected.txt'))
        self.assertNotEqual(
            implicit,
            ResultDigest.from_file(self.fs, '/failures/baz-expected.txt'))
        self.assertEqual(
            implicit,
            ResultDigest.from_file(self.fs,
                                   '/others/reftest-expected.png',
                                   is_reftest=True))

    def test_different_all_pass_results(self):
        x = ResultDigest.from_file(self.fs, '/all-pass/foo-expected.txt')
        y = ResultDigest.from_file(self.fs, '/all-pass/bar-expected.txt')
        self.assertTrue(x != y)
        self.assertFalse(x == y)

    def test_same_extra_png_for_reftest(self):
        x = ResultDigest.from_file(self.fs,
                                   '/others/reftest-expected.png',
                                   is_reftest=True)
        y = ResultDigest.from_file(self.fs,
                                   '/others/reftest2-expected.png',
                                   is_reftest=True)
        self.assertTrue(x == y)
        self.assertFalse(x != y)
