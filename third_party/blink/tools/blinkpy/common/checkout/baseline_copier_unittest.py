# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
from unittest import mock

from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.common.checkout.baseline_optimizer_unittest import BaselineTest
from blinkpy.common.net.rpc import Build
from blinkpy.tool.commands.rebaseline import TestBaselineSet
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.base import VirtualTestSuite


class BaselineCopierTest(BaselineTest):
    """Tests for copying baselines to be preserved before rebaselining.

    The tests here depend on the fallback path graph set up in
    `TestPort.FALLBACK_PATHS`.
    """
    def setUp(self):
        super().setUp()
        self.host.builders = BuilderList({
            'MOCK linux-trusty-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Release', 'Trusty'],
                'steps': {
                    'blink_web_tests': {},
                    'fake_flag_blink_web_tests': {
                        'flag_specific': 'fake-flag',
                    },
                },
            },
            'MOCK linux-precise-rel': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Release', 'Precise'],
            },
            'MOCK mac10.10-rel': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Release', 'Mac10.10'],
            },
            'MOCK mac10.11-rel': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Release', 'Mac10.11'],
            },
            'MOCK win7-rel': {
                'port_name': 'test-win-win7',
                'specifiers': ['Release', 'Win7'],
            },
            'MOCK win10-rel': {
                'port_name': 'test-win-win10',
                'specifiers': ['Release', 'Win10'],
            },
        })
        self.fs.write_text_file(
            self.finder.path_from_web_tests('FlagSpecificConfig'),
            json.dumps([{
                'name': 'fake-flag',
                'args': []
            }]))
        self.default_port = self.host.port_factory.get('test-linux-trusty')
        self.copier = BaselineCopier(self.host, self.default_port)

    def _baseline_set(self, runs):
        baseline_set = TestBaselineSet(self.host.builders)
        for test, builder_name, step_name in runs:
            build = Build(builder_name, 1000)
            baseline_set.add(test, build, step_name or 'blink_web_tests')
        return baseline_set

    def test_demote(self):
        """Verify that generic baselines are copied down.

        Demotion is the inverse operation of the promotion step in the
        optimizer. This ensures rebaselining leaves behind a generic baseline
        if and only if it was promoted. That is, we prefer Figure 1 over 2,
        where (*) denotes a file that exists. Both states are equivalent, but
        Figure 2's generic baseline can be misleading.

              (generic)               (generic)*
             /         \             /         \
             |         |             |         |
            win*      mac*          win*      mac

              Figure 1                Figure 2

        """
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                '': 'generic result',
                'virtual/virtual_failures/': 'generic virtual result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10': None,
                'platform/test-mac-mac10.11': None,
                'platform/test-win-win10/virtual/virtual_failures/': None,
                'platform/test-mac-mac10.11/virtual/virtual_failures/': None,
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK win10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt',
            {
                'platform/test-win-win10':
                None,  # Will be rebaselined
                'platform/test-win-win7':
                'generic result',
                'platform/test-mac-mac10.11':
                'generic result',
                'platform/test-win-win10/virtual/virtual_failures/':
                'generic virtual result',
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'generic virtual result',
            })

    def test_copy_baseline_to_virtual_for_multiple_rebaselined_ports(self):
        """Rebaselining a test for adjacent ports patches the virtual tree."""
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/': 'test-mac-mac10.11 result',
                'platform/test-mac-mac10.10/': 'test-mac-mac10.10 result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/virtual/virtual_failures/': None,
                'platform/test-mac-mac10.10/virtual/virtual_failures/': None,
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK mac10.11-rel', None),
            ('failures/expected/image.html', 'MOCK mac10.10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/':
                'test-mac-mac10.11 result',
                'platform/test-mac-mac10.10/':
                'test-mac-mac10.10 result',
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'test-mac-mac10.11 result',
                'platform/test-mac-mac10.10/virtual/virtual_failures/':
                'test-mac-mac10.10 result',
            })

    def test_copy_baseline_all_rebaselined(self):
        """Rebaselining every port does not required any copies."""
        self._write_baselines('failures/expected/image-expected.txt', {
            '': 'do not copy',
        })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK mac10.11-rel', None),
            ('failures/expected/image.html', 'MOCK mac10.10-rel', None),
            ('failures/expected/image.html', 'MOCK win10-rel', None),
            ('failures/expected/image.html', 'MOCK win7-rel', None),
            ('failures/expected/image.html', 'MOCK linux-trusty-rel', None),
            ('failures/expected/image.html', 'MOCK linux-trusty-rel',
             'fake_flag_blink_web_tests'),
            ('failures/expected/image.html', 'MOCK linux-precise-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        # For simplicity, only consider the nonvirtual tree for this test.
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/': None,
                'platform/test-mac-mac10.10/': None,
                'platform/test-win-win10/': None,
                'platform/test-win-win7/': None,
                'platform/test-linux-trusty/': None,
                'platform/test-linux-precise/': None,
                'flag-specific/fake-flag/': None,
            })

    def test_copy_baseline_mac_newer_to_older_version(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/': 'test-mac-mac10.11 result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/': None,
                'platform/test-mac-mac10.11/virtual/virtual_failures/': None,
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK mac10.11-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/':
                'test-mac-mac10.11 result',
                'platform/test-mac-mac10.10/':
                'test-mac-mac10.11 result',
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'test-mac-mac10.11 result',
            })

    def test_copy_baseline_linux_newer_to_older_and_flag_specific(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-trusty/': 'test-linux-trusty result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-precise/': None,
                'flag-specific/fake-flag/': None,
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK linux-trusty-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-trusty/': 'test-linux-trusty result',
                'platform/test-linux-precise/': 'test-linux-trusty result',
                'flag-specific/fake-flag/': 'test-linux-trusty result',
            })

    def test_copy_baseline_successor_to_predecessor(self):
        self._write_baselines('failures/expected/image-expected.txt', {
            '': 'generic result',
        })
        self._assert_baselines('failures/expected/image-expected.txt', {
            'platform/test-linux-precise/': None,
        })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK linux-trusty-rel', None),
        ])
        # Verify that copying works across multiple levels.
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-trusty/': None,
                'platform/test-linux-precise/': 'generic result',
            })

    def test_copy_virtual_baseline_for_flag_specific(self):
        self._write_baselines('failures/expected/image-expected.txt', {
            'flag-specific/fake-flag/': 'fake-flag result',
        })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-trusty/virtual/virtual_failures/': None,
                'flag-specific/fake-flag/virtual/virtual_failures/': None,
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK linux-trusty-rel',
             'fake_flag_blink_web_tests'),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'flag-specific/fake-flag/virtual/virtual_failures/':
                'fake-flag result',
            })

    def test_no_copy_nonvirtual_to_virtual_with_intermediate_flag_spec(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'flag-specific/fake-flag/':
                'fake-flag result',
                'platform/test-linux-trusty/virtual/virtual_failures/':
                'virtual test-linux-trusty result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'flag-specific/fake-flag/virtual/virtual_failures/': None,
            })
        # Virtual trusty supplies the baseline for virtual `fake-flag`, so
        # nonvirtual `fake-flag` should not be copied into its virtual
        # counterpart.
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK linux-trusty-rel',
             'fake_flag_blink_web_tests'),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'flag-specific/fake-flag/virtual/virtual_failures/': None,
            })

    def test_no_copy_nonvirtual_to_virtual_with_intermediate(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/':
                'test-mac-mac10.10 result',
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'virtual test-mac-mac10.11 result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/virtual/virtual_failures/': None,
            })
        # Virtual mac10.11 supplies the baseline for virtual mac10.10, so
        # nonvirtual mac10.10 should not be copied into its virtual counterpart.
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK mac10.10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/virtual/virtual_failures/': None,
            })

    def test_copy_baseline_to_multiple_immediate_predecessors(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
            })
        self._assert_baselines('failures/expected/image-expected.txt', {
            'platform/test-win-win7/': None,
            'platform/test-linux-trusty/': None,
        })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK win10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
                'platform/test-win-win7/': 'test-win-win10 result',
                'platform/test-linux-trusty/': 'test-win-win10 result',
            })

    def test_no_overwrite_existing_baseline(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
                'platform/test-linux-trusty/': 'test-linux-trusty result',
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK win10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
                'platform/test-linux-trusty/': 'test-linux-trusty result',
            })

    def test_no_copy_skipped_test(self):
        """Verify that no baselines are copied to skipped platforms."""
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
            })
        self.fs.write_text_file(
            self.default_port.path_to_generic_test_expectations_file(),
            textwrap.dedent("""\
                # tags: [ Win Linux ]
                # results: [ Failure Skip ]
                [ Win ] failures/expected/image.html [ Failure ]
                [ Linux ] failures/expected/image.html [ Skip ]
                """))
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK win10-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines('failures/expected/image-expected.txt', {
            'platform/test-linux-trusty/': None,
        })

    def test_no_copy_skipped_test_virtual_test(self):
        """Verify that a virtual test skipped for a platform is not copied."""
        linux_only_suite = VirtualTestSuite(prefix='linux-only',
                                            platforms=['Linux'],
                                            bases=['failures/expected'],
                                            args=['--dummy-flag'])
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10/': 'test-win-win10 result',
            })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK win10-rel', None),
        ])
        with mock.patch(
                'blinkpy.web_tests.port.test.TestPort.virtual_test_suites',
                return_value=[linux_only_suite]):
            self.copier.write_copies(
                self.copier.find_baselines_to_copy(
                    'failures/expected/image.html', 'txt', baseline_set))

        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-linux-trusty/': 'test-win-win10 result',
                'platform/test-win-win10/virtual/linux-only/': None,
            })

    def test_copy_for_virtual_only_rebaseline(self):
        """Verify that nonvirtual baselines are not copied unnecessarily."""
        self._write_baselines('failures/expected/image-expected.txt', {
            'platform/test-mac-mac10.11/': 'mac10.11 result',
        })
        baseline_set = self._baseline_set([
            ('virtual/virtual_failures/failures/expected/image.html',
             'MOCK mac10.11-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy(
                'virtual/virtual_failures/failures/expected/image.html', 'txt',
                baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/':
                None,
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                None,
                'platform/test-mac-mac10.10/virtual/virtual_failures/':
                'mac10.11 result',
            })

    def test_no_copy_when_virtual_test_also_fails(self):
        self._write_baselines('failures/expected/image-expected.txt', {
            'platform/test-mac-mac10.11/': 'mac10.11 result',
        })
        baseline_set = self._baseline_set([
            ('failures/expected/image.html', 'MOCK mac10.11-rel', None),
            ('virtual/virtual_failures/failures/expected/image.html',
             'MOCK mac10.11-rel', None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/virtual/virtual_failures/': None,
            })

    def test_copy_for_platform_failures_different_across_virtual_suites(self):
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'virtual mac10.11 result',
                'platform/test-win-win10/virtual/other_virtual/':
                'virtual win10 result',
            })
        suites = [
            VirtualTestSuite(prefix='virtual_failures',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['failures/expected'],
                             args=['--flag-causing-mac-fail']),
            VirtualTestSuite(prefix='other_virtual',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['failures/expected'],
                             args=['--flag-causing-win-fail']),
        ]
        baseline_set = self._baseline_set([
            ('virtual/virtual_failures/failures/expected/image.html',
             'MOCK mac10.11-rel', None),
            ('virtual/other_virtual/failures/expected/image.html',
             'MOCK win10-rel', None),
        ])
        with mock.patch(
                'blinkpy.web_tests.port.test.TestPort.virtual_test_suites',
                return_value=suites):
            self.copier.write_copies(
                self.copier.find_baselines_to_copy(
                    'failures/expected/image.html', 'txt', baseline_set))
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-mac-mac10.10/':
                None,
                'platform/test-mac-mac10.10/virtual/virtual_failures/':
                'virtual mac10.11 result',
                'platform/test-mac-mac10.10/virtual/other_virtual/':
                None,
                'platform/test-win-win7/':
                None,
                'platform/test-win-win7/virtual/virtual_failures/':
                None,
                'platform/test-win-win7/virtual/other_virtual/':
                'virtual win10 result',
            })

    def test_copy_for_physical_test_under_virtual_dir(self):
        self._write_baselines(
            'physical1-expected.txt', {
                'platform/test-mac-mac10.11/virtual/virtual_empty_bases/':
                'virtual mac10.11 result',
            })
        baseline_set = self._baseline_set([
            ('virtual/virtual_empty_bases/physical1.html', 'MOCK mac10.11-rel',
             None),
        ])
        self.copier.write_copies(
            self.copier.find_baselines_to_copy(
                'virtual/virtual_empty_bases/physical1.html', 'txt',
                baseline_set))
        self._assert_baselines(
            'physical1-expected.txt', {
                'platform/test-mac-mac10.10/virtual/virtual_empty_bases/':
                'virtual mac10.11 result',
            })
