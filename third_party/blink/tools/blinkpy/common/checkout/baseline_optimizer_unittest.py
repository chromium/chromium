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

import optparse
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
            'Fake Test Win10.20h2': {
                'port_name': 'win-win10.20h2',
                'specifiers': ['Win10.20h2', 'Release']
            },
            'Fake Test Win11': {
                'port_name': 'win-win11',
                'specifiers': ['Win11', 'Release']
            },
            'Fake Test Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'Fake Test Linux HighDPI': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'steps': {
                    'high_dpi_blink_web_tests (with patch)': {
                        'flag_specific': 'highdpi',
                    },
                },
            },
            'Fake Test Mac12.0': {
                'port_name': 'mac-mac12',
                'specifiers': ['Mac12', 'Release'],
            },
            'Fake Test Mac11.0': {
                'port_name': 'mac-mac11',
                'specifiers': ['Mac11', 'Release']
            },
            'Fake Test Mac10.15': {
                'port_name': 'mac-mac10.15',
                'specifiers': ['Mac10.15', 'Release']
            },
            'Fake Test Mac10.14': {
                'port_name': 'mac-mac10.14',
                'specifiers': ['Mac10.14', 'Release']
            },
            'Fake Test Mac10.13': {
                'port_name': 'mac-mac10.13',
                'specifiers': ['Mac10.13', 'Release']
            },
        })
        # Note: this is a pre-assumption of the tests in this file. If this
        # assertion fails, port configurations are likely changed, and the
        # tests need to be adjusted accordingly.
        self.assertEqual(sorted(self.host.port_factory.all_port_names()), [
            'linux-trusty', 'mac-mac10.13', 'mac-mac10.14', 'mac-mac10.15',
            'mac-mac11', 'mac-mac12', 'win-win10.20h2', 'win-win11'
        ])

    def _assert_optimization(self,
                             results_by_directory,
                             directory_to_new_results,
                             baseline_dirname='',
                             suffix='txt',
                             options=None):
        web_tests_dir = PathFinder(self.fs).web_tests_dir()
        test_name = 'mock-test.html'
        baseline_name = 'mock-test-expected.' + suffix
        self.fs.write_text_file(
            self.fs.join(web_tests_dir, 'VirtualTestSuites'),
            '[{"prefix": "gpu", "platforms": ["Linux", "Mac", "Win"], '
            '"bases": ["fast/canvas", "slow/canvas/mock-test.html"], '
            '"args": ["--foo"], "expires": "never"}]')
        self.fs.write_text_file(
            self.fs.join(web_tests_dir, 'FlagSpecificConfig'),
            '[{"name": "highdpi", "args": ["--force-device-scale-factor=1.5"]}]'
        )
        self.fs.write_text_file(
            self.fs.join(web_tests_dir, 'NeverFixTests'),
            '# tags: [ Linux Mac Mac10.13 Mac10.14 Mac10.15 Mac11 Mac12 Win Win10.20h2 Win11 ]\n'
            '# results: [ Skip Pass ]\n'
            '[ Win10.20h2 ] virtual/gpu/fast/canvas/mock-test.html [ Skip ] \n'
        )

        for dirname, contents in results_by_directory.items():
            self.fs.write_text_file(
                self.fs.join(web_tests_dir, dirname, baseline_name), contents)

        if options:
            options = optparse.Values(options)
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(options=options),
            self.host.port_factory.all_port_names())
        self.assertTrue(
            baseline_optimizer.optimize(
                self.fs.join(baseline_dirname, test_name), suffix))

        for dirname, contents in directory_to_new_results.items():
            path = self.fs.join(web_tests_dir, dirname, baseline_name)
            if contents is None:
                # Check files that are explicitly marked as absent.
                self.assertFalse(
                    self.fs.exists(path),
                    '%s should not exist after optimization' % path)
            else:
                self.assertEqual(self.fs.read_text_file(path), contents,
                                 'Content of %s != "%s"' % (path, contents))

        for dirname in results_by_directory:
            path = self.fs.join(web_tests_dir, dirname, baseline_name)
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
        web_tests_dir = PathFinder(self.fs).web_tests_dir()
        self.fs.write_text_file(
            self.fs.join(web_tests_dir, test_path, 'mock-test-expected.html'),
            'ref')
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
                'platform/mac-mac10.14': '1',
            }, {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.14': None,
            })

    def test_local_optimization_skipping_a_port_in_the_middle(self):
        # mac-mac10.13 -> mac-mac10.14 -> mac
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.13': '1',
            }, {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/mac-mac10.13': None,
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
        # Baseline optimization in this case changed the result for
        # virtual/gpu/fast/canvas on win10. The test previously expects
        # output '2'. After optimization it expects '1'.
        # TODO(crbug/1375568): consider do away with the patching virtual subtree operation.
        self._assert_optimization(
            {
                'platform/win/fast/canvas': '1',
                'platform/win10/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/mac/virtual/gpu/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '1',
                'platform/win/fast/canvas': '1',
                'platform/win10/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_fallback_to_same_baseline_after_optimization_2(self):
        # Baseline optimization in this case changed the result for
        # virtual/gpu/fast/canvas on mac11. The test previously expects
        # output '1'. After optimization it expects '2'.
        # TODO(crbug/1375568): consider do away with the patching virtual subtree operation.
        self._assert_optimization(
            {
                'platform/mac-mac10.15/virtual/gpu/fast/canvas': '3',
                'platform/mac-mac11/fast/canvas': '1',
                'fast/canvas': '2',
            }, {
                'platform/mac-mac10.15/virtual/gpu/fast/canvas': '3',
                'platform/mac-mac11/virtual/gpu/fast/canvas': None,
                'platform/mac/virtual/gpu/fast/canvas': None,
                'virtual/gpu/fast/canvas': '2',
                'platform/mac-mac11/fast/canvas': '1',
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_test_fallback_to_same_baseline_after_optimization_3(self):
        # Baseline optimization in this case removed the baseline for
        # virtual/gpu/fast/canvas on Linux. When patching virtual tree,
        # virtual/gpu/fast/canvas on Win get a baseline of '1', the algorithm
        # then decides the virtual baseline on both Win and Linux can be
        # removed, which is not correct.
        # TODO(crbug/1375568): consider do away with the patching virtual subtree operation.
        self._assert_optimization(
            {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/linux/virtual/gpu/fast/canvas': '1'
            }, {
                'platform/win/fast/canvas': '1',
                'platform/linux/fast/canvas': '2',
                'platform/mac/fast/canvas': '3',
                'platform/linux/virtual/gpu/fast/canvas': None
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_baseline_not_redundant_with_actual_root(self):
        # baseline optimization supprisingly added one baseline in this case.
        # This is because we are patching the virtual subtree first.
        # TODO(crbug/1375568): consider do away with the patching virtual subtree operation.
        self._assert_optimization(
            {
                'platform/mac-mac11/virtual/gpu/fast/canvas': '1',
                'platform/mac-mac11/fast/canvas': '1',
                'fast/canvas': '2',
            }, {
                'platform/mac-mac11/virtual/gpu/fast/canvas': '1',
                'platform/mac/virtual/gpu/fast/canvas': None,
                'virtual/gpu/fast/canvas': '2',
                'platform/mac-mac11/fast/canvas': '1',
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
                'platform/win10/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': None,
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
                'platform/win10/fast/canvas': '1',
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
                'platform/mac-mac11/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/mac-mac11/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_flag_specific_ancestors(self):
        # virtual root should not be removed when any flag specific non virtual
        # baseline differs with the virtual root, otherwise virtual flag
        # specific will fall back to a different baseline after optimization.
        # TODO: fix this together when we do away with patch virtual subtree.
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
                'flag-specific/highdpi/fast/canvas': '1',
            }, {
                'virtual/gpu/fast/canvas': None,
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
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

    def test_all_pass_testharness_at_win_and_mac_not_redundant(self):
        # Baseline optimization in the case removed the all pass baseline
        # for virtual/gpu/fast/canvas on Win and Mac, but failed to add a
        # generic virtual baseline.
        # TODO(1399685):  all pass baselines are removed incorrectly
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
                'virtual/gpu/fast/canvas': None
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

    # Tests for protected methods - pylint: disable=protected-access

    def test_move_baselines(self):
        self.fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/win/another/test-expected.txt',
            'result A')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/mac/another/test-expected.txt',
            'result A')
        self.fs.write_binary_file(MOCK_WEB_TESTS + 'another/test-expected.txt',
                                  'result B')
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(),
            self.host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt', {
                MOCK_WEB_TESTS + 'platform/win': 'aaa',
                MOCK_WEB_TESTS + 'platform/mac': 'aaa',
                MOCK_WEB_TESTS[:-1]: 'bbb',
            }, {
                MOCK_WEB_TESTS[:-1]: 'aaa',
            })
        self.assertEqual(
            self.fs.read_binary_file(MOCK_WEB_TESTS +
                                     'another/test-expected.txt'), 'result A')

    def test_move_baselines_skip_git_commands(self):
        self.fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/win/another/test-expected.txt',
            'result A')
        self.fs.write_binary_file(
            MOCK_WEB_TESTS + 'platform/mac/another/test-expected.txt',
            'result A')
        self.fs.write_binary_file(MOCK_WEB_TESTS + 'another/test-expected.txt',
                                  'result B')
        baseline_optimizer = BaselineOptimizer(
            self.host, self.host.port_factory.get(),
            self.host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt', {
                MOCK_WEB_TESTS + 'platform/win': 'aaa',
                MOCK_WEB_TESTS + 'platform/mac': 'aaa',
                MOCK_WEB_TESTS[:-1]: 'bbb',
            }, {
                MOCK_WEB_TESTS + 'platform/linux': 'bbb',
                MOCK_WEB_TESTS[:-1]: 'aaa',
            })
        self.assertEqual(
            self.fs.read_binary_file(MOCK_WEB_TESTS +
                                     'another/test-expected.txt'), 'result A')


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
            ResultDigest(self.fs,
                         '/all-pass/foo-expected.txt').is_extra_result)
        self.assertTrue(
            ResultDigest(self.fs,
                         '/all-pass/bar-expected.txt').is_extra_result)
        self.assertFalse(
            ResultDigest(self.fs,
                         '/failures/baz-expected.txt').is_extra_result)

    def test_empty_result(self):
        self.assertFalse(
            ResultDigest(self.fs,
                         '/others/something-expected.png').is_extra_result)
        self.assertTrue(
            ResultDigest(self.fs,
                         '/others/empty-expected.txt').is_extra_result)
        self.assertTrue(
            ResultDigest(self.fs,
                         '/others/empty-expected.png').is_extra_result)

    def test_extra_png_for_reftest_result(self):
        self.assertFalse(
            ResultDigest(self.fs,
                         '/others/something-expected.png').is_extra_result)
        self.assertTrue(
            ResultDigest(
                self.fs, '/others/reftest-expected.png',
                is_reftest=True).is_extra_result)

    def test_non_extra_result(self):
        self.assertFalse(
            ResultDigest(self.fs,
                         '/others/something-expected.png').is_extra_result)

    def test_implicit_extra_result(self):
        # Implicit empty equal to any extra result but not failures.
        implicit = ResultDigest(None, None)
        self.assertTrue(
            implicit == ResultDigest(self.fs, '/all-pass/foo-expected.txt'))
        self.assertTrue(
            implicit == ResultDigest(self.fs, '/all-pass/bar-expected.txt'))
        self.assertFalse(
            implicit == ResultDigest(self.fs, '/failures/baz-expected.txt'))
        self.assertTrue(implicit == ResultDigest(
            self.fs, '/others/reftest-expected.png', is_reftest=True))

    def test_different_all_pass_results(self):
        x = ResultDigest(self.fs, '/all-pass/foo-expected.txt')
        y = ResultDigest(self.fs, '/all-pass/bar-expected.txt')
        self.assertTrue(x != y)
        self.assertFalse(x == y)

    def test_same_extra_png_for_reftest(self):
        x = ResultDigest(
            self.fs, '/others/reftest-expected.png', is_reftest=True)
        y = ResultDigest(
            self.fs, '/others/reftest2-expected.png', is_reftest=True)
        self.assertTrue(x == y)
        self.assertFalse(x != y)
