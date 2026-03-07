# Copyright (C) 2011 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

from blinkpy.web_tests.models.test_configuration import TestConfiguration


def make_mock_all_test_configurations_set():
    all_test_configurations = set()
    for version, architecture in (('snowleopard',
                                   'x86'), ('win7', 'x86'), ('vista', 'x86'),
                                  ('precise', 'x86_64'), ('trusty', 'x86_64')):
        for build_type in ('debug', 'release'):
            all_test_configurations.add(
                TestConfiguration(version, architecture, build_type))
    return all_test_configurations


MOCK_MACROS = {
    'mac': ['snowleopard'],
    'win': ['vista', 'win7'],
    'linux': ['precise', 'trusty'],
}


class TestConfigurationTest(unittest.TestCase):
    def test_items(self):
        config = TestConfiguration('win7', 'x86', 'release')
        result_config_dict = {}
        for category, specifier in config.items():
            result_config_dict[category] = specifier
        self.assertEqual({
            'version': 'win7',
            'architecture': 'x86',
            'build_type': 'release'
        }, result_config_dict)

    def test_keys(self):
        config = TestConfiguration('win7', 'x86', 'release')
        result_config_keys = []
        for category in config.keys():
            result_config_keys.append(category)
        self.assertEqual(
            set(['version', 'architecture', 'build_type']),
            set(result_config_keys))

    def test_str(self):
        config = TestConfiguration('win7', 'x86', 'release')
        self.assertEqual('<win7, x86, release>', str(config))

    def test_repr(self):
        config = TestConfiguration('win7', 'x86', 'release')
        self.assertEqual(
            "TestConfig(version='win7', architecture='x86', build_type='release')",
            repr(config))

    def test_hash(self):
        config_dict = {}
        config_dict[TestConfiguration('win7', 'x86', 'release')] = True
        self.assertIn(TestConfiguration('win7', 'x86', 'release'), config_dict)
        self.assertTrue(config_dict[TestConfiguration('win7', 'x86',
                                                      'release')])

        def query_unknown_key():
            return config_dict[TestConfiguration('win7', 'x86', 'debug')]

        with self.assertRaises(KeyError):
            query_unknown_key()
        self.assertIn(TestConfiguration('win7', 'x86', 'release'), config_dict)
        self.assertNotIn(
            TestConfiguration('win7', 'x86', 'debug'), config_dict)
        configs_list = [
            TestConfiguration('win7', 'x86', 'release'),
            TestConfiguration('win7', 'x86', 'debug'),
            TestConfiguration('win7', 'x86', 'debug')
        ]
        self.assertEqual(len(configs_list), 3)
        self.assertEqual(len(set(configs_list)), 2)

    def test_eq(self):
        self.assertEqual(
            TestConfiguration('win7', 'x86', 'release'),
            TestConfiguration('win7', 'x86', 'release'))
        self.assertNotEquals(
            TestConfiguration('win7', 'x86', 'release'),
            TestConfiguration('win7', 'x86', 'debug'))

    def test_values(self):
        config = TestConfiguration('win7', 'x86', 'release')
        result_config_values = []
        for value in config.values():
            result_config_values.append(value)
        self.assertEqual(
            set(['win7', 'x86', 'release']), set(result_config_values))
