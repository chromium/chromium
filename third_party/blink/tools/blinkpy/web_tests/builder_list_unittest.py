# Copyright (C) 2016 Google Inc. All rights reserved.
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

from blinkpy.web_tests.builder_list import BuilderList


class BuilderListTest(unittest.TestCase):
    @staticmethod
    def sample_builder_list():
        return BuilderList({
            'some-wpt-bot': {
                'port_name': 'port-c',
                'specifiers': ['C', 'Release'],
                'steps': {
                    'wpt_tests_suite': {},
                },
                'is_try_builder': True,
            },
            'Blink A': {
                'port_name': 'port-a',
                'specifiers': ['A', 'Release']
            },
            'Blink B': {
                'port_name': 'port-b',
                'specifiers': ['B', 'Release']
            },
            'Blink B (dbg)': {
                'port_name': 'port-b',
                'specifiers': ['B', 'Debug']
            },
            'Blink C (dbg)': {
                'port_name': 'port-c',
                'specifiers': ['C', 'Release']
            },
            'Try A': {
                'port_name': 'port-a',
                'specifiers': ['A', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True
            },
            'Try B': {
                'port_name': 'port-b',
                'specifiers': ['B', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True
            },
            'CQ Try A': {
                'bucket': 'bucket.a',
                'port_name': 'port-a',
                'specifiers': ['A', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True,
                'is_cq_builder': True
            },
            'CQ Try B': {
                'bucket': 'bucket.b',
                'port_name': 'port-b',
                'specifiers': ['B', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True,
                'is_cq_builder': True
            },
            'CQ Try C': {
                'bucket': 'bucket.c',
                'port_name': 'port-c',
                'specifiers': ['c', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                    'high_dpi_blink_web_tests': {
                        'flag_specific': 'highdpi'
                    },
                    'blink_wpt_tests': {},
                    'high_dpi_blink_wpt_tests': {
                        'flag_specific': 'highdpi',
                    },
                },
                'is_try_builder': True,
                'is_cq_builder': True,
                'main': "luci",
            },
            'Flag Specific C': {
                'port_name': 'port-c',
                'specifiers': ['C', 'Release'],
                'steps': {
                    'high_dpi_blink_web_tests': {
                        'flag_specific': 'highdpi'
                    },
                },
                "is_try_builder": True
            },
        })

    def test_constructor_validates_list(self):
        with self.assertRaises(AssertionError):
            BuilderList({'Blink A': {}})
        with self.assertRaises(AssertionError):
            BuilderList({'Blink A': {'port_name': 'port-a', 'specifiers': []}})

    def test_all_builder_names(self):
        builders = self.sample_builder_list()
        self.assertEqual([
            'Blink A',
            'Blink B',
            'Blink B (dbg)',
            'Blink C (dbg)',
            'CQ Try A',
            'CQ Try B',
            'CQ Try C',
            'Flag Specific C',
            'Try A',
            'Try B',
            'some-wpt-bot',
        ], builders.all_builder_names())

    def test_all_continuous_builder_names(self):
        builders = self.sample_builder_list()
        self.assertEqual(
            ['Blink A', 'Blink B', 'Blink B (dbg)', 'Blink C (dbg)'],
            builders.all_continuous_builder_names())

    def test_all_try_builder_names(self):
        builders = self.sample_builder_list()
        self.assertEqual([
            'CQ Try A', 'CQ Try B', 'CQ Try C', 'Flag Specific C', 'Try A',
            'Try B', 'some-wpt-bot'
        ], builders.all_try_builder_names())

    def test_all_cq_try_builder_names(self):
        builders = self.sample_builder_list()
        self.assertEqual(
            ['CQ Try A', 'CQ Try B', 'CQ Try C'],
            builders.all_cq_try_builder_names())

    def test_all_flag_specific_builder_names(self):
        builders = self.sample_builder_list()
        self.assertEqual(['CQ Try C', 'Flag Specific C'],
                         builders.all_flag_specific_try_builder_names(
                             flag_specific="highdpi"))
        self.assertEqual(
            ['CQ Try C', 'Flag Specific C'],
            builders.all_flag_specific_try_builder_names(flag_specific="*"))

    def test_builders_for_rebaselining(self):
        builders = self.sample_builder_list()
        self.assertEqual(
            {
                'Try A', 'Try B', 'Flag Specific C', 'CQ Try A', 'CQ Try B',
                'CQ Try C', 'some-wpt-bot'
            }, builders.builders_for_rebaselining())

    def test_all_port_names(self):
        builders = self.sample_builder_list()
        self.assertEqual(['port-a', 'port-b', 'port-c'],
                         builders.all_port_names())

    def test_all_flag_specific_options(self):
        builders = self.sample_builder_list()
        self.assertEqual({'highdpi'}, builders.all_flag_specific_options())

    def test_bucket_for_builder_default_bucket(self):
        builders = self.sample_builder_list()
        self.assertEqual('', builders.bucket_for_builder('Try A'))

    def test_bucket_for_builder_configured_bucket(self):
        builders = self.sample_builder_list()
        self.assertEqual('bucket.a', builders.bucket_for_builder('CQ Try A'))

    def test_main_for_builder_default_main(self):
        builders = self.sample_builder_list()
        self.assertEqual('', builders.main_for_builder('Try A'))

    def test_main_for_builder_configured_main(self):
        builders = self.sample_builder_list()
        self.assertEqual('luci', builders.main_for_builder('CQ Try C'))

    def test_port_name_for_builder_name(self):
        builders = self.sample_builder_list()
        self.assertEqual('port-b',
                         builders.port_name_for_builder_name('Blink B'))

    def test_port_name_for_flag_specific_option(self):
        builders = self.sample_builder_list()
        self.assertEqual(
            'port-c', builders.port_name_for_flag_specific_option('highdpi'))

    def test_flag_specific_options_for_port_name(self):
        builders = self.sample_builder_list()
        self.assertEqual(
            set(), builders.flag_specific_options_for_port_name('port-a'))

    def test_reject_flag_specific_multiple_ports(self):
        with self.assertRaises(ValueError):
            BuilderList({
                'Flag Specific A': {
                    'port_name': 'port-a',
                    'specifiers': ['A', 'Release'],
                    'steps': {
                        'blink_web_tests': {
                            'flag_specific': 'highdpi',
                        },
                    },
                    'is_try_builder': True
                },
                'Flag Specific B': {
                    'port_name': 'port-b',
                    'specifiers': ['B', 'Release'],
                    'steps': {
                        'blink_web_tests': {
                            'flag_specific': 'highdpi',
                        },
                    },
                    'is_try_builder': True
                },
            })

    def test_specifiers_for_builder(self):
        builders = self.sample_builder_list()
        self.assertEqual(['B', 'Release'],
                         builders.specifiers_for_builder('Blink B'))

    def test_platform_specifier_for_builder(self):
        builders = self.sample_builder_list()
        self.assertEqual('B',
                         builders.platform_specifier_for_builder('Blink B'))

    def test_port_name_for_builder_name_with_missing_builder(self):
        builders = self.sample_builder_list()
        with self.assertRaises(KeyError):
            builders.port_name_for_builder_name('Blink_B')

    def test_specifiers_for_builder_with_missing_builder(self):
        builders = self.sample_builder_list()
        with self.assertRaises(KeyError):
            builders.specifiers_for_builder('Blink_B')

    def test_builder_name_for_port_name_with_no_debug_builder(self):
        builders = self.sample_builder_list()
        self.assertEqual('Blink A',
                         builders.builder_name_for_port_name('port-a'))

    def test_builder_name_for_port_name_with_debug_builder(self):
        builders = self.sample_builder_list()
        self.assertEqual('Blink B',
                         builders.builder_name_for_port_name('port-b'))

    def test_builder_name_for_port_name_with_only_debug_builder(self):
        builders = self.sample_builder_list()
        self.assertEqual('Blink C (dbg)',
                         builders.builder_name_for_port_name('port-c'))

    def test_version_specifier_for_port_name(self):
        builders = self.sample_builder_list()
        self.assertEqual('A',
                         builders.version_specifier_for_port_name('port-a'))
        self.assertEqual('B',
                         builders.version_specifier_for_port_name('port-b'))
        self.assertIsNone(builders.version_specifier_for_port_name('port-x'))
