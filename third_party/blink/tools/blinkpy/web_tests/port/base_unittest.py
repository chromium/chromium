# Copyright (C) 2010 Google Inc. All rights reserved.
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

import hashlib
import json
import operator
import optparse
import time
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.git import FileStatus, FileStatusType
from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.common.system.platform_info_mock import MockPlatformInfo
from blinkpy.common.system.system_host import SystemHost
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port.base import Port, VirtualTestSuite
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.port.test import (add_unit_tests_to_mock_filesystem,
                                         add_manifest_to_mock_filesystem,
                                         MOCK_WEB_TESTS, TestPort)


class PortTest(LoggingTestCase):
    def make_port(self,
                  executive=None,
                  with_tests=False,
                  port_name=None,
                  **kwargs):
        host = MockHost()
        if executive:
            host.executive = executive
        if with_tests:
            add_unit_tests_to_mock_filesystem(host.filesystem)
            return TestPort(host, **kwargs)
        port = Port(host, port_name or 'baseport', **kwargs)
        port.operating_system = lambda: 'linux'
        return port

    def test_validate_wpt_dirs(self):
        # Keys should not have trailing slashes.
        for wpt_path in Port.WPT_DIRS.keys():
            self.assertFalse(wpt_path.endswith('/'))
        # Values should not be empty (except the last one).
        for url_prefix in list(Port.WPT_DIRS.values())[:-1]:
            self.assertNotEqual(url_prefix, '/')
        self.assertEqual(list(Port.WPT_DIRS.values())[-1], '/')

    def test_validate_wpt_regex(self):
        self.assertEquals(
            Port.WPT_REGEX.match('external/wpt/foo/bar.html').groups(),
            ('external/wpt', 'foo/bar.html'))
        self.assertEquals(
            Port.WPT_REGEX.match('virtual/test/external/wpt/foo/bar.html').
            groups(), ('external/wpt', 'foo/bar.html'))
        self.assertEquals(
            Port.WPT_REGEX.match('wpt_internal/foo/bar.html').groups(),
            ('wpt_internal', 'foo/bar.html'))
        self.assertEquals(
            Port.WPT_REGEX.match('virtual/test/wpt_internal/foo/bar.html').
            groups(), ('wpt_internal', 'foo/bar.html'))

    def test_setup_test_run(self):
        port = self.make_port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_get_option__set(self):
        options, _ = optparse.OptionParser().parse_args([])
        options.foo = 'bar'
        port = self.make_port(options=options)
        self.assertEqual(port.get_option('foo'), 'bar')

    def test_options_unchanged(self):
        options = optparse.Values()
        self.make_port(options=options)
        self.assertEqual(options, optparse.Values())

    def test_get_option__unset(self):
        port = self.make_port()
        self.assertIsNone(port.get_option('foo'))

    def test_get_option__default(self):
        port = self.make_port()
        self.assertEqual(port.get_option('foo', 'bar'), 'bar')

    def test_output_filename(self):
        port = self.make_port()

        # Normal test filename
        test_file = 'fast/test.html'
        self.assertEqual(
            port.output_filename(test_file, '-expected', '.txt'),
            'fast/test-expected.txt')
        self.assertEqual(
            port.output_filename(test_file, '-expected-mismatch', '.png'),
            'fast/test-expected-mismatch.png')

        # Test filename with query string
        test_file = 'fast/test.html?wss&run_type=1'
        self.assertEqual(
            port.output_filename(test_file, '-expected', '.txt'),
            'fast/test_wss_run_type=1-expected.txt')
        self.assertEqual(
            port.output_filename(test_file, '-actual', '.png'),
            'fast/test_wss_run_type=1-actual.png')

        # Test filename with query string containing a dot
        test_file = 'fast/test.html?include=HTML.*'
        self.assertEqual(
            port.output_filename(test_file, '-expected', '.txt'),
            'fast/test_include=HTML._-expected.txt')
        self.assertEqual(
            port.output_filename(test_file, '-actual', '.png'),
            'fast/test_include=HTML._-actual.png')

    def test_parse_output_filename(self):
        port = self.make_port()

        location, base_path = port.parse_output_filename('passes/text.html')
        self.assertEqual(location.platform, '')
        self.assertEqual(location.flag_specific, '')
        self.assertEqual(location.virtual_suite, '')
        self.assertEqual(base_path, 'passes/text.html')

        location, base_path = port.parse_output_filename(
            '/mock-checkout/third_party/blink/web_tests/'
            'flag-specific/fake-flag')
        self.assertEqual(location.platform, '')
        self.assertEqual(location.flag_specific, 'fake-flag')
        self.assertEqual(location.virtual_suite, '')
        self.assertEqual(base_path, '')

        location, base_path = port.parse_output_filename(
            'platform/mac/virtual/fake-vts/passes/text.html')
        self.assertEqual(location.platform, 'mac')
        self.assertEqual(location.flag_specific, '')
        self.assertEqual(location.virtual_suite, 'fake-vts')
        self.assertEqual(base_path, 'passes/text.html')

        with self.assertRaises(ValueError):
            port.parse_output_filename('/mock-checkout/not/web_tests')

    def test_test_from_output_filename_html(self):
        port = self.make_port()
        virtual_suite = {
            'prefix': 'fake-vts',
            'platforms': [],
            'bases': ['fast'],
            'args': ['--fake-flag'],
        }
        fs = port.host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'fast/test.html', '')
        fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites',
                           json.dumps([virtual_suite]))

        self.assertEqual(
            port.test_from_output_filename('fast/test-expected.txt'),
            'fast/test.html')
        self.assertEqual(
            port.test_from_output_filename('fast/test-expected.png'),
            'fast/test.html')
        self.assertEqual(
            port.test_from_output_filename(
                'virtual/fake-vts/fast/test-expected.png'),
            'virtual/fake-vts/fast/test.html')
        self.assertIsNone(
            port.test_from_output_filename('fast/does-not-exist-expected.txt'))

    def test_test_from_output_filename_wpt_variants(self):
        port = self.make_port()
        port.set_option_default('manifest_update', False)
        manifest = {
            'items': {
                'testharness': {
                    'has-variants.html': [
                        '0123abcd',
                        ['has-variants.html?a', {}],
                        ['has-variants.html?b', {}],
                    ],
                },
            },
        }
        fs = port.host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json',
                           json.dumps(manifest))
        fs.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites',
                           json.dumps([]))

        self.assertEqual(
            port.test_from_output_filename(
                'external/wpt/has-variants_a-expected.txt'),
            'external/wpt/has-variants.html?a')
        self.assertEqual(
            port.test_from_output_filename(
                'external/wpt/has-variants_b-expected.txt'),
            'external/wpt/has-variants.html?b')
        self.assertIsNone(
            port.test_from_output_filename(
                'external/wpt/has-variants-expected.txt'))

    def test_expected_baselines_basic(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        # The default baseline doesn't exist.
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(None, 'fast/test-expected.txt')])
        self.assertIsNone(
            port.expected_filename(test_file, '.txt', return_default=False))
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))

        # The default baseline exists.
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS[:-1], 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'fast/test-expected.txt')

    def test_expected_baselines_mismatch(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        self.assertEqual(
            port.expected_baselines(test_file, '.txt', match=False),
            [(None, 'fast/test-expected-mismatch.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt', match=False),
            MOCK_WEB_TESTS + 'fast/test-expected-mismatch.txt')

    def test_expected_baselines_platform_specific(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        self.assertEqual(port.baseline_version_dir(),
                         MOCK_WEB_TESTS + 'platform/foo')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')

        # The default baseline doesn't exist.
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(MOCK_WEB_TESTS + 'platform/foo', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(test_file, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))

        # The default baseline exists.
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(MOCK_WEB_TESTS + 'platform/foo', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(test_file, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEquals(port.fallback_expected_filename(test_file, '.txt'),
                          MOCK_WEB_TESTS + 'fast/test-expected.txt')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'fast/test-expected.txt')

    def test_expected_baselines_flag_specific(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites',
            '[{ "prefix": "bar", "platforms": ["Linux", "Mac", "Win"],'
            ' "bases": ["fast"], "args": ["--bar"], "expires": "never"}]')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagSpecificConfig',
            '[{"name": "special-flag", "args": ["--special"]}]')

        # pylint: disable=protected-access
        port._options.additional_platform_directory = []
        port._options.additional_driver_flag = ['--flag-not-affecting']
        port._options.flag_specific = 'special-flag'
        self.assertEqual(port.baseline_search_path(), [
            MOCK_WEB_TESTS + 'flag-specific/special-flag',
            MOCK_WEB_TESTS + 'platform/foo'
        ])
        self.assertEqual(port.baseline_version_dir(),
                         MOCK_WEB_TESTS + 'flag-specific/special-flag')

        # Flag-specific baseline
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS +
            'flag-specific/special-flag/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(MOCK_WEB_TESTS + 'flag-specific/special-flag',
              'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'), MOCK_WEB_TESTS +
            'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(test_file, '.txt',
                                   return_default=False), MOCK_WEB_TESTS +
            'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(
            port.fallback_expected_filename(test_file, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # Before the flag-specific and virtual baseline exists, fall back to
        # the flag-specific but nonvirtual baseline.
        fs = port.host.filesystem
        self.assertEqual(
            port.expected_filename('virtual/bar/fast/test.html', '.txt'),
            fs.join(MOCK_WEB_TESTS,
                    'flag-specific/special-flag/fast/test-expected.txt'))
        fs.write_text_file(
            fs.join(
                MOCK_WEB_TESTS,
                'flag-specific/special-flag/virtual/bar/fast/test-expected.txt'
            ), 'foo')
        # Switch to the most specific baseline.
        self.assertEqual(
            port.expected_filename('virtual/bar/fast/test.html', '.txt'),
            fs.join(
                MOCK_WEB_TESTS,
                'flag-specific/special-flag/virtual/bar/fast/test-expected.txt'
            ))
        self.assertEqual(
            port.expected_baselines('virtual/bar/fast/test.html', '.txt'),
            [(fs.join(MOCK_WEB_TESTS, 'flag-specific/special-flag'),
              'virtual/bar/fast/test-expected.txt')])

        # Flag-specific platform-specific baseline
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS +
            'flag-specific/special-flag/platform/foo/fast/test-expected.txt',
            'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS + 'flag-specific/special-flag',
                           'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'), MOCK_WEB_TESTS +
            'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(test_file, '.txt',
                                   return_default=False), MOCK_WEB_TESTS +
            'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(
            port.fallback_expected_filename(test_file, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

    def test_expected_baselines_virtual(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        virtual_test = 'virtual/flag/fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites',
            '[{ "prefix": "flag", "platforms": ["Linux", "Mac", "Win"],'
            ' "bases": ["fast"], "args": ["--flag"], "expires": "never"}]')

        # The default baseline for base test
        self.assertEqual(
            port.expected_baselines(virtual_test, '.txt'),
            [(None, 'virtual/flag/fast/test-expected.txt')])
        self.assertIsNone(
            port.expected_filename(virtual_test, '.txt', return_default=False))
        self.assertEqual(port.expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(
            port.expected_filename(
                virtual_test,
                '.txt',
                return_default=False,
                fallback_base_for_virtual=False))
        self.assertEqual(
            port.expected_filename(virtual_test,
                                   '.txt',
                                   fallback_base_for_virtual=False),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertIsNone(
            port.fallback_expected_filename(virtual_test, '.txt'))

        # Platform-specific baseline for base test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(virtual_test, '.txt'),
            [(None, 'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertIsNone(
            port.expected_filename(
                virtual_test,
                '.txt',
                return_default=False,
                fallback_base_for_virtual=False))
        self.assertEqual(
            port.expected_filename(virtual_test,
                                   '.txt',
                                   fallback_base_for_virtual=False),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.fallback_expected_filename(virtual_test, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # The default baseline for virtual test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(virtual_test, '.txt'),
            [(MOCK_WEB_TESTS[:-1], 'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt'),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(virtual_test,
                                   '.txt',
                                   return_default=False,
                                   fallback_base_for_virtual=False),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(virtual_test,
                                   '.txt',
                                   fallback_base_for_virtual=False),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.fallback_expected_filename(virtual_test, '.txt'),
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # Platform-specific baseline for virtual test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS +
            'platform/foo/virtual/flag/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(virtual_test, '.txt'),
            [(MOCK_WEB_TESTS + 'platform/foo',
              'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt',
                                   return_default=False), MOCK_WEB_TESTS +
            'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(virtual_test, '.txt'), MOCK_WEB_TESTS +
            'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(
                virtual_test,
                '.txt',
                return_default=False,
                fallback_base_for_virtual=False), MOCK_WEB_TESTS +
            'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(
                virtual_test, '.txt',
                fallback_base_for_virtual=False), MOCK_WEB_TESTS +
            'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(
            port.fallback_expected_filename(virtual_test, '.txt'),
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')

    def test_additional_platform_directory(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        test_file = 'fast/test.html'

        # Simple additional platform directory
        port._options.additional_platform_directory = ['/tmp/local-baselines']  # pylint: disable=protected-access
        self.assertEqual(port.baseline_version_dir(), '/tmp/local-baselines')

        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(None, 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt', return_default=False),
            None)
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')

        port.host.filesystem.write_text_file(
            '/tmp/local-baselines/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            '/tmp/local-baselines/fast/test-expected.txt')

        # Multiple additional platform directories
        port._options.additional_platform_directory = [  # pylint: disable=protected-access
            '/foo', '/tmp/local-baselines'
        ]
        self.assertEqual(port.baseline_version_dir(), '/foo')

        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            '/tmp/local-baselines/fast/test-expected.txt')

        port.host.filesystem.write_text_file('/foo/fast/test-expected.txt',
                                             'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/foo', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            '/foo/fast/test-expected.txt')

    def test_nonexistant_expectations(self):
        port = self.make_port(port_name='foo')
        port.default_expectations_files = lambda: [
            MOCK_WEB_TESTS + 'platform/exists/TestExpectations', MOCK_WEB_TESTS
            + 'platform/nonexistant/TestExpectations'
        ]
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/exists/TestExpectations', '')
        self.assertEqual('\n'.join(port.expectations_dict().keys()),
                         MOCK_WEB_TESTS + 'platform/exists/TestExpectations')

    def _make_port_for_test_additional_expectations(self, options_dict={}):
        port = self.make_port(
            port_name='foo', options=optparse.Values(options_dict))
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'TestExpectations')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'NeverFixTests')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'SlowTests')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/TestExpectations', '')
        port.host.filesystem.write_text_file(
            '/tmp/additional-expectations-1.txt', 'content1\n')
        port.host.filesystem.write_text_file(
            '/tmp/additional-expectations-2.txt', 'content2\n')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/special-flag', 'content3')
        return port

    def test_additional_expectations_empty(self):
        port = self._make_port_for_test_additional_expectations()
        self.assertEqual(list(port.expectations_dict().values()), [])

    def test_additional_expectations_1(self):
        port = self._make_port_for_test_additional_expectations({
            'additional_expectations': ['/tmp/additional-expectations-1.txt']
        })
        self.assertEqual(list(port.expectations_dict().values()),
                         ['content1\n'])

    def test_additional_expectations_2(self):
        port = self._make_port_for_test_additional_expectations({
            'additional_expectations': [
                '/tmp/additional-expectations-1.txt',
                '/tmp/additional-expectations-2.txt'
            ]
        })
        self.assertEqual(list(port.expectations_dict().values()),
                         ['content1\n', 'content2\n'])

    def test_additional_expectations_additional_flag(self):
        port = self._make_port_for_test_additional_expectations({
            'additional_expectations': [
                '/tmp/additional-expectations-1.txt',
                '/tmp/additional-expectations-2.txt'
            ],
            'additional_driver_flag': ['--special-flag']
        })
        # --additional-driver-flag doesn't affect baseline search path.
        self.assertEqual(list(port.expectations_dict().values()),
                         ['content1\n', 'content2\n'])

    def test_flag_specific_expectations(self):
        port = self.make_port(port_name='foo')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'TestExpectations')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'NeverFixTests')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'SlowTests')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/special-flag-a', 'aa')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/special-flag-b', 'bb')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/README.txt', 'cc')

        self.assertEqual(list(port.expectations_dict().values()), [])
        # all_expectations_dict() is an OrderedDict, but its order depends on
        # file system walking order.
        self.assertEqual(
            sorted(port.all_expectations_dict().values()), ['aa', 'bb'])

    def test_flag_specific_expectations_identify_unreadable_file(self):
        port = self.make_port(port_name='foo')

        non_utf8_file = MOCK_WEB_TESTS + 'FlagExpectations/non-utf8-file'
        invalid_utf8 = b'\xC0'
        port.host.filesystem.write_binary_file(non_utf8_file, invalid_utf8)

        with self.assertRaises(UnicodeDecodeError):
            port.all_expectations_dict()

        # The UnicodeDecodeError does not indicate which file we failed to read,
        # so ensure that the file is identified in a log message.
        self.assertLog([
            'ERROR: Failed to read expectations file: \'' + non_utf8_file +
            '\'\n'
        ])

    def test_flag_specific_config_name_from_options(self):
        port_a = self.make_port(options=optparse.Values({}))
        # pylint: disable=protected-access
        self.assertEqual(port_a._specified_additional_driver_flags(), [])
        self.assertIsNone(port_a.flag_specific_config_name())

        port_b = self.make_port(
            options=optparse.Values({
                'additional_driver_flag': ['--bb']
            }))
        self.assertEqual(port_b._specified_additional_driver_flags(), ['--bb'])
        self.assertIsNone(port_b.flag_specific_config_name())

        port_c = self.make_port(
            options=optparse.Values({
                'additional_driver_flag': ['--cc', '--dd']
            }))
        self.assertEqual(port_c._specified_additional_driver_flags(),
                         ['--cc', '--dd'])
        self.assertIsNone(port_c.flag_specific_config_name())

    def test_flag_specific_config_name_from_options_and_file(self):
        flag_file = MOCK_WEB_TESTS + 'additional-driver-flag.setting'

        port_a = self.make_port(options=optparse.Values({}))
        port_a.host.filesystem.write_text_file(flag_file, '--aa')
        # pylint: disable=protected-access
        self.assertEqual(port_a._specified_additional_driver_flags(), ['--aa'])
        # Additional driver flags don't affect flag_specific_config_name.
        self.assertIsNone(port_a.flag_specific_config_name())

        port_b = self.make_port(
            options=optparse.Values({
                'additional_driver_flag': ['--bb']
            }))
        port_b.host.filesystem.write_text_file(flag_file, '--aa')
        self.assertEqual(port_b._specified_additional_driver_flags(),
                         ['--aa', '--bb'])
        self.assertIsNone(port_a.flag_specific_config_name())

        port_c = self.make_port(
            options=optparse.Values({
                'additional_driver_flag': ['--bb', '--cc']
            }))
        port_c.host.filesystem.write_text_file(flag_file, '--bb --dd')
        # We don't remove duplicated flags at this time.
        self.assertEqual(port_c._specified_additional_driver_flags(),
                         ['--bb', '--dd', '--bb', '--cc'])
        self.assertIsNone(port_a.flag_specific_config_name())

    def _write_flag_specific_config(self, port):
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'FlagSpecificConfig'), '['
            '  {"name": "a", "args": ["--aa"]},'
            '  {"name": "b", "args": ["--bb", "--aa"]},'
            '  {"name": "c", "args": ["--bb", "--cc"]}'
            ']')

    def test_flag_specific_option(self):
        port_a = self.make_port(
            options=optparse.Values({
                'flag_specific': 'a'
            }))
        self._write_flag_specific_config(port_a)
        # pylint: disable=protected-access
        self.assertEqual(port_a.flag_specific_config_name(), 'a')
        self.assertEqual(port_a._specified_additional_driver_flags(), ['--aa'])

        port_b = self.make_port(
            options=optparse.Values({
                'flag_specific': 'a',
                'additional_driver_flag': ['--bb']
            }))
        self._write_flag_specific_config(port_b)
        self.assertEqual(port_b.flag_specific_config_name(), 'a')
        self.assertEqual(port_b._specified_additional_driver_flags(),
                         ['--aa', '--bb'])

        port_d = self.make_port(
            options=optparse.Values({
                'flag_specific': 'd'
            }))
        self._write_flag_specific_config(port_d)
        self.assertRaises(AssertionError, port_d.flag_specific_config_name)
        self.assertRaises(AssertionError,
                          port_d._specified_additional_driver_flags)

    def test_duplicate_flag_specific_name(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'FlagSpecificConfig'),
            '[{"name": "a", "args": ["--aa"]}, {"name": "a", "args": ["--aa", "--bb"]}]'
        )
        self.assertRaises(ValueError, port.flag_specific_configs)

    def test_duplicate_flag_specific_args(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'FlagSpecificConfig'),
            '[{"name": "a", "args": ["--aa"]}, {"name": "b", "args": ["--aa"]}]'
        )
        self.assertRaises(ValueError, port.flag_specific_configs)

    def test_invalid_flag_specific_name(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'FlagSpecificConfig'),
            '[{"name": "a/", "args": ["--aa"]}]')
        self.assertRaises(ValueError, port.flag_specific_configs)

    def test_additional_env_var(self):
        port = self.make_port(
            options=optparse.Values({
                'additional_env_var': ['FOO=BAR', 'BAR=FOO']
            }))
        self.assertEqual(
            port.get_option('additional_env_var'), ['FOO=BAR', 'BAR=FOO'])
        environment = port.setup_environ_for_server()
        self.assertTrue(('FOO' in environment) & ('BAR' in environment))
        self.assertEqual(environment['FOO'], 'BAR')
        self.assertEqual(environment['BAR'], 'FOO')

    def test_find_no_paths_specified(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertNotEqual(len(tests), 0)

    def test_find_one_test(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/image.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/im*'])
        self.assertEqual(len(tests), 2)

    def test_find_with_skipped_directories(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts'])
        self.assertNotIn('userscripts/resources/iframe.html', tests)

    def test_find_with_skipped_directories_2(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts/resources'])
        self.assertEqual(tests, [])

    def test_update_manifest_once_by_default(self):
        # pylint: disable=protected-access
        port = self.make_port(with_tests=True)
        port.wpt_manifest('external/wpt')
        port.wpt_manifest('external/wpt')
        self.assertEqual(len(port.host.filesystem.written_files), 1)
        self.assertEqual(len(port.host.executive.calls), 1)

    def test_no_manifest_update_with_existing_manifest(self):
        # pylint: disable=protected-access
        port = self.make_port(with_tests=True)
        port.set_option_default('manifest_update', False)
        filesystem = port.host.filesystem
        filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', '{}')
        filesystem.clear_written_files()

        port.wpt_manifest('external/wpt')
        self.assertEqual(len(port.host.filesystem.written_files), 0)
        self.assertEqual(len(port.host.executive.calls), 0)

    def test_no_manifest_update_without_existing_manifest(self):
        # pylint: disable=protected-access
        port = self.make_port(with_tests=True)
        port.set_option_default('manifest_update', False)

        port.wpt_manifest('external/wpt')
        self.assertEqual(len(port.host.filesystem.written_files), 1)
        self.assertEqual(len(port.host.executive.calls), 1)

    def test_should_update_manifest_no_cached_digest(self):
        port = self.make_port(with_tests=True)
        fs = port.host.filesystem
        fs.write_text_file(f'{MOCK_WEB_TESTS}external/wpt/MANIFEST.json', '{}')

        mock_git = mock.Mock()
        mock_git.run.side_effect = lambda command: {
            'rev-parse': '012345\n',
            'ls-files': '',
        }[command[0]]
        mock_git.changed_files.return_value = {
            'third_party/blink/web_tests/external/wpt/deleted.html':
            FileStatus(FileStatusType.DELETE),
        }

        with mock.patch.object(port.host, 'git', return_value=mock_git):
            self.assertTrue(port.should_update_manifest('external/wpt'))
        digest_path = ('/mock-checkout/third_party/wpt_tools/wpt/'
                       '.wptcache/external/wpt/digest')
        digest = self._wpt_digest(f"""\
            012345
            {MOCK_WEB_TESTS}external/wpt/deleted.html:
            """)
        self.assertEqual(fs.read_text_file(digest_path), digest,
                         'cached digest should be updated')

    def test_should_update_manifest_cached_digest_same(self):
        port = self.make_port(with_tests=True)
        fs = port.host.filesystem
        digest_path = ('/mock-checkout/third_party/wpt_tools/wpt/'
                       '.wptcache/external/wpt/digest')
        digest = self._wpt_digest(f"""\
            012345
            {MOCK_WEB_TESTS}external/wpt/uncommitted.html:3f786850e387550fdab836ed7e6dc881de23001b
            {MOCK_WEB_TESTS}external/wpt/untracked.html:89e6c98d92887913cadf06b2adb97f26cde4849b
            """)
        fs.write_text_file(digest_path, digest)
        fs.write_text_file(f'{MOCK_WEB_TESTS}external/wpt/MANIFEST.json', '{}')
        fs.write_text_file(f'{MOCK_WEB_TESTS}external/wpt/uncommitted.html',
                           'a\n')
        fs.write_text_file(f'{MOCK_WEB_TESTS}external/wpt/untracked.html',
                           'b\n')

        mock_git = mock.Mock()
        mock_git.run.side_effect = lambda command: {
            'rev-parse':
            '012345\n',
            'ls-files':
            'third_party/blink/web_tests/external/wpt/untracked.html\x00',
        }[command[0]]
        mock_git.changed_files.return_value = {
            'third_party/blink/web_tests/external/wpt/uncommitted.html':
            FileStatus(FileStatusType.ADD),
        }

        with mock.patch.object(port.host, 'git', return_value=mock_git):
            self.assertFalse(port.should_update_manifest('external/wpt'))
        self.assertEqual(fs.read_text_file(digest_path), digest,
                         'cached digest should be the same')
        mock_git.run.assert_has_calls([
            mock.call([
                'rev-parse',
                'HEAD:third_party/blink/web_tests/external/wpt',
            ]),
            mock.call([
                'ls-files',
                '--other',
                '--exclude-standard',
                '-z',
                'HEAD',
                f'{MOCK_WEB_TESTS}external/wpt',
            ]),
        ])
        mock_git.changed_files.assert_called_once_with(
            path=f'{MOCK_WEB_TESTS}external/wpt')

    def test_should_update_manifest_cached_digest_different(self):
        port = self.make_port(with_tests=True)
        fs = port.host.filesystem
        digest_path = ('/mock-checkout/third_party/wpt_tools/wpt/'
                       '.wptcache/wpt_internal/digest')
        digest = self._wpt_digest(f"""\
            012345
            {MOCK_WEB_TESTS}wpt_internal/changed.html:3f786850e387550fdab836ed7e6dc881de23001b
            """)
        fs.write_text_file(digest_path, digest)
        fs.write_text_file(f'{MOCK_WEB_TESTS}wpt_internal/MANIFEST.json', '{}')
        # `changed.html` had contents 'a\n'.
        fs.write_text_file(f'{MOCK_WEB_TESTS}wpt_internal/changed.html', 'b\n')

        mock_git = mock.Mock()
        mock_git.run.side_effect = lambda command: {
            'rev-parse': '012345\n',
            'ls-files': '',
        }[command[0]]
        mock_git.changed_files.return_value = {
            'third_party/blink/web_tests/wpt_internal/changed.html':
            FileStatus(FileStatusType.MODIFY),
        }

        with mock.patch.object(port.host, 'git', return_value=mock_git):
            self.assertTrue(port.should_update_manifest('wpt_internal'))
        digest = self._wpt_digest(f"""\
            012345
            {MOCK_WEB_TESTS}wpt_internal/changed.html:89e6c98d92887913cadf06b2adb97f26cde4849b
            """)
        self.assertEqual(fs.read_text_file(digest_path), digest,
                         'cached digest should be updated')

    def _wpt_digest(self, raw_preimage: str) -> str:
        return hashlib.sha256(
            textwrap.dedent(raw_preimage).encode()).hexdigest()

    def test_find_none_if_not_in_manifest(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        self.assertNotIn('external/wpt/common/blank.html', port.tests([]))
        self.assertNotIn('external/wpt/console/console-is-a-namespace.any.js',
                         port.tests([]))

    def test_find_one_if_in_manifest(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        self.assertIn('external/wpt/dom/ranges/Range-attributes.html',
                      port.tests([]))
        self.assertIn('external/wpt/console/console-is-a-namespace.any.html',
                      port.tests([]))

    def test_wpt_tests_paths(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        all_wpt = [
            'external/wpt/console/console-is-a-namespace.any.html',
            'external/wpt/console/console-is-a-namespace.any.worker.html',
            'external/wpt/dom/ranges/Range-attributes-slow.html',
            'external/wpt/dom/ranges/Range-attributes.html',
            'external/wpt/foo/bar/test-print.html',
            'external/wpt/foo/print/test.html',
            'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
            'external/wpt/html/parse.html?run_type=uri',
            'external/wpt/html/parse.html?run_type=write',
            'external/wpt/portals/portals-no-frame-crash.html',
        ]
        # test.any.js shows up on the filesystem as one file but it effectively becomes two test files:
        # test.any.html and test.any.worker.html. We should support running test.any.js by name and
        # indirectly by specifying a parent directory.
        self.assertEqual(sorted(port.tests(['external'])), all_wpt)
        self.assertEqual(sorted(port.tests(['external/'])), all_wpt)
        self.assertEqual(port.tests(['external/csswg-test']), [])
        self.assertEqual(sorted(port.tests(['external/wpt'])), all_wpt)
        self.assertEqual(sorted(port.tests(['external/wpt/'])), all_wpt)
        self.assertEqual(
            sorted(port.tests(['external/wpt/console'])), [
                'external/wpt/console/console-is-a-namespace.any.html',
                'external/wpt/console/console-is-a-namespace.any.worker.html'
            ])
        self.assertEqual(
            sorted(port.tests(['external/wpt/console/'])), [
                'external/wpt/console/console-is-a-namespace.any.html',
                'external/wpt/console/console-is-a-namespace.any.worker.html'
            ])
        self.assertEqual(
            sorted(
                port.tests(
                    ['external/wpt/console/console-is-a-namespace.any.js'])),
            [
                'external/wpt/console/console-is-a-namespace.any.html',
                'external/wpt/console/console-is-a-namespace.any.worker.html'
            ])
        self.assertEqual(
            port.tests(
                ['external/wpt/console/console-is-a-namespace.any.html']),
            ['external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(
            sorted(port.tests(['external/wpt/dom'])), [
                'external/wpt/dom/ranges/Range-attributes-slow.html',
                'external/wpt/dom/ranges/Range-attributes.html'
            ])
        self.assertEqual(
            sorted(port.tests(['external/wpt/dom/'])), [
                'external/wpt/dom/ranges/Range-attributes-slow.html',
                'external/wpt/dom/ranges/Range-attributes.html'
            ])
        self.assertEqual(
            port.tests(['external/wpt/dom/ranges/Range-attributes.html']),
            ['external/wpt/dom/ranges/Range-attributes.html'])

        # wpt_internal should work the same.
        self.assertEqual(
            port.tests(['wpt_internal']), ['wpt_internal/dom/bar.html'])

    def test_virtual_wpt_tests_paths(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        all_wpt = [
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html',
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html',
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes-slow.html',
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html',
            'virtual/virtual_wpt/external/wpt/foo/bar/test-print.html',
            'virtual/virtual_wpt/external/wpt/foo/print/test.html',
            'virtual/virtual_wpt/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
            'virtual/virtual_wpt/external/wpt/html/parse.html?run_type=uri',
            'virtual/virtual_wpt/external/wpt/html/parse.html?run_type=write',
            'virtual/virtual_wpt/external/wpt/portals/portals-no-frame-crash.html',
        ]
        dom_wpt = [
            'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes-slow.html',
            'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html',
        ]

        self.assertEqual(
            sorted(port.tests(['virtual/virtual_wpt/external/'])), all_wpt)
        self.assertEqual(
            sorted(port.tests(['virtual/virtual_wpt/external/wpt/'])), all_wpt)
        self.assertEqual(
            sorted(port.tests(['virtual/virtual_wpt/external/wpt/console'])), [
                'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html',
                'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html'
            ])

        self.assertEqual(
            sorted(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/'])),
            dom_wpt)
        self.assertEqual(
            sorted(
                port.tests(
                    ['virtual/virtual_wpt_dom/external/wpt/dom/ranges/'])),
            dom_wpt)
        self.assertEqual(
            port.tests([
                'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html'
            ]), [
                'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html'
            ])

        # wpt_internal should work the same.
        self.assertEqual(
            port.tests(['virtual/virtual_wpt_dom/wpt_internal']),
            ['virtual/virtual_wpt_dom/wpt_internal/dom/bar.html'])
        self.assertEqual(
            sorted(port.tests(['virtual/virtual_wpt_dom/'])),
            dom_wpt + ['virtual/virtual_wpt_dom/wpt_internal/dom/bar.html'])

        all_virtual_console = set([
            'virtual/virtual_console/external/wpt/console/console-is-a-namespace.any.html',
            'virtual/virtual_console/external/wpt/console/console-is-a-namespace.any.worker.html'
        ])
        self.assertLessEqual(all_virtual_console, set(port.tests()))

    def test_virtual_wpt_tests_paths_with_generated_bases(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertEqual(
            {
                'virtual/generated_wpt/external/wpt/html/parse.html?run_type=uri',
                'virtual/generated_wpt/external/wpt/console/console-is-a-namespace.any.html',
            }, set(port.tests(['virtual/generated_wpt/'])))

        all_tests = port.tests()
        self.assertIn(
            'virtual/generated_wpt/external/wpt/html/parse.html?run_type=uri',
            all_tests)
        self.assertIn(
            'virtual/generated_wpt/external/wpt/console/console-is-a-namespace.any.html',
            all_tests)

    def test_virtual_test_paths(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        ssl_tests = [
            'virtual/mixed_wpt/http/tests/ssl/text.html',
        ]
        http_passes_tests = [
            'virtual/mixed_wpt/http/tests/passes/image.html',
            'virtual/mixed_wpt/http/tests/passes/text.html',
        ]
        dom_tests = [
            'virtual/mixed_wpt/external/wpt/dom/ranges/Range-attributes-slow.html',
            'virtual/mixed_wpt/external/wpt/dom/ranges/Range-attributes.html',
        ]
        physical_tests_under_virtual = [
            'virtual/mixed_wpt/virtual/virtual_empty_bases/dir/physical2.html',
            'virtual/mixed_wpt/virtual/virtual_empty_bases/physical1.html',
        ]

        #  The full set of tests must be returned when running the entire suite.
        self.assertEqual(
            sorted(port.tests(['virtual/mixed_wpt/'])), dom_tests +
            http_passes_tests + ssl_tests + physical_tests_under_virtual)

        self.assertEqual(sorted(port.tests(['virtual/mixed_wpt/external'])),
                         dom_tests)

        self.assertEqual(sorted(port.tests(['virtual/mixed_wpt/http'])),
                         http_passes_tests + ssl_tests)
        self.assertEqual(
            sorted(
                port.tests([
                    'virtual/mixed_wpt/http/tests/ssl',
                    'virtual/mixed_wpt/external/wpt/dom'
                ])), dom_tests + ssl_tests)

        # Make sure we don't run a non-existent test.
        self.assertEqual(sorted(port.tests(['virtual/mixed_wpt/passes'])), [])

    def test_is_non_wpt_test_file(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.is_non_wpt_test_file('', 'foo.html'))
        self.assertTrue(port.is_non_wpt_test_file('', 'foo.svg'))
        self.assertTrue(port.is_non_wpt_test_file('', 'test-ref-test.html'))
        self.assertTrue(port.is_non_wpt_test_file('devtools', 'a.js'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo.png'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected.svg'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected.xht'))
        self.assertFalse(
            port.is_non_wpt_test_file('', 'foo-expected-mismatch.html'))
        self.assertFalse(
            port.is_non_wpt_test_file('', 'foo-expected-mismatch.svg'))
        self.assertFalse(
            port.is_non_wpt_test_file('', 'foo-expected-mismatch.xhtml'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-ref.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-notref.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-notref.xht'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-ref.xhtml'))
        self.assertFalse(port.is_non_wpt_test_file('', 'ref-foo.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'notref-foo.xhr'))

        self.assertFalse(
            port.is_non_wpt_test_file(MOCK_WEB_TESTS + 'external/wpt/common',
                                      'blank.html'))
        self.assertFalse(
            port.is_non_wpt_test_file(MOCK_WEB_TESTS + 'external/wpt/console',
                                      'console-is-a-namespace.any.js'))
        self.assertFalse(
            port.is_non_wpt_test_file(MOCK_WEB_TESTS + 'external/wpt',
                                      'testharness_runner.html'))
        self.assertTrue(
            port.is_non_wpt_test_file(
                MOCK_WEB_TESTS + '/external/wpt_automation', 'foo.html'))
        self.assertFalse(
            port.is_non_wpt_test_file(MOCK_WEB_TESTS + 'wpt_internal/console',
                                      'console-is-a-namespace.any.js'))

    def test_is_wpt_test(self):
        self.assertTrue(
            Port.is_wpt_test('external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(
            Port.is_wpt_test(
                'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
            ))
        self.assertFalse(Port.is_wpt_test('dom/domparsing/namespaces-1.html'))
        self.assertFalse(Port.is_wpt_test('rutabaga'))

        self.assertTrue(
            Port.is_wpt_test('virtual/a-name/external/wpt/baz/qux.htm'))
        self.assertFalse(Port.is_wpt_test('virtual/external/wpt/baz/qux.htm'))
        self.assertFalse(
            Port.is_wpt_test('not-virtual/a-name/external/wpt/baz/qux.htm'))

    def test_is_wpt_idlharness_test(self):
        self.assertTrue(
            Port.is_wpt_idlharness_test(
                'external/wpt/css/css-pseudo/idlharness.html'))
        self.assertTrue(
            Port.is_wpt_idlharness_test(
                'external/wpt/payment-handler/idlharness.https.any.html'))
        self.assertTrue(
            Port.is_wpt_idlharness_test(
                'external/wpt/payment-handler/idlharness.https.any.serviceworker.html'
            ))
        self.assertFalse(
            Port.is_wpt_idlharness_test(
                'external/wpt/css/foo/interfaces.html'))
        self.assertFalse(
            Port.is_wpt_idlharness_test(
                'external/wpt/css/idlharness/bar.html'))

    def test_should_use_wptserve(self):
        self.assertTrue(
            Port.should_use_wptserve('external/wpt/dom/interfaces.html'))
        self.assertTrue(
            Port.should_use_wptserve(
                'virtual/a-name/external/wpt/dom/interfaces.html'))
        self.assertFalse(
            Port.should_use_wptserve('harness-tests/wpt/console_logging.html'))
        self.assertFalse(
            Port.should_use_wptserve('dom/domparsing/namespaces-1.html'))

    def test_is_wpt_crash_test(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertTrue(
            port.is_wpt_crash_test(
                'external/wpt/portals/portals-no-frame-crash.html'))
        self.assertFalse(
            port.is_wpt_crash_test(
                'external/wpt/nonexistent/i-dont-exist-crash.html'))
        self.assertFalse(
            port.is_wpt_crash_test(
                'external/wpt/dom/ranges/Range-attributes.html'))
        self.assertFalse(
            port.is_wpt_crash_test('portals/portals-no-frame-crash.html'))

    def test_is_wpt_print_reftest(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertTrue(
            port.is_wpt_print_reftest('external/wpt/foo/bar/test-print.html'))
        self.assertTrue(
            port.is_wpt_print_reftest('external/wpt/foo/print/test.html'))
        self.assertFalse(port.is_wpt_print_reftest('not/a/wpt/test.html'))
        self.assertFalse(
            port.is_wpt_print_reftest(
                'external/wpt/nonexistent/test-print.html'))
        self.assertFalse(
            port.is_wpt_print_reftest(
                'external/wpt/dom/ranges/Range-attributes.html'))

    def test_is_slow_wpt_test(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertFalse(
            port.is_slow_wpt_test(
                'external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(
            port.is_slow_wpt_test(
                'external/wpt/dom/ranges/Range-attributes-slow.html'))
        self.assertTrue(
            port.is_slow_wpt_test(
                'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
            ))
        self.assertFalse(
            port.is_slow_wpt_test(
                'external/wpt/css/css-pseudo/idlharness.html'))

    def test_is_slow_wpt_test_idlharness_with_dcheck(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        port.host.filesystem.write_text_file(port.build_path('args.gn'),
                                             'dcheck_always_on=true\n')
        # We always consider idlharness tests slow, even if they aren't marked
        # such in the manifest. See https://crbug.com/1047818
        self.assertTrue(
            port.is_slow_wpt_test(
                'external/wpt/css/css-pseudo/idlharness.html'))

    def test_is_slow_wpt_test_with_variations(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertFalse(
            port.is_slow_wpt_test(
                'external/wpt/console/console-is-a-namespace.any.html'))
        self.assertTrue(
            port.is_slow_wpt_test(
                'external/wpt/console/console-is-a-namespace.any.worker.html'))
        self.assertFalse(
            port.is_slow_wpt_test('external/wpt/html/parse.html?run_type=uri'))
        self.assertTrue(
            port.is_slow_wpt_test(
                'external/wpt/html/parse.html?run_type=write'))

    def test_is_slow_wpt_test_takes_virtual_tests(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertFalse(
            port.is_slow_wpt_test(
                'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html'
            ))
        self.assertTrue(
            port.is_slow_wpt_test(
                'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes-slow.html'
            ))

    def test_is_slow_wpt_test_returns_false_for_illegal_paths(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertFalse(
            port.is_slow_wpt_test('dom/ranges/Range-attributes.html'))
        self.assertFalse(
            port.is_slow_wpt_test('dom/ranges/Range-attributes-slow.html'))
        self.assertFalse(
            port.is_slow_wpt_test('/dom/ranges/Range-attributes.html'))
        self.assertFalse(
            port.is_slow_wpt_test('/dom/ranges/Range-attributes-slow.html'))

    def test_is_testharness_test_wpt(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        self.assertTrue(
            port.is_testharness_test(
                'external/wpt/dom/ranges/Range-attributes.html'))
        self.assertFalse(
            port.is_testharness_test(
                'external/wpt/portals/portals-no-frame-crash.html'))

    def test_is_testhanress_test_legacy(self):
        port = self.make_port(with_tests=True)
        fs = port.host.filesystem
        fs.write_text_file(
            fs.join(port.web_tests_dir(), 'testharness.html'),
            '<html><script src="../../resources/testharness.js"></script>')
        fs.write_text_file(
            fs.join(port.web_tests_dir(), 'not-testharness.html'),
            '<html><script src="../../resources/js-test.js"></script>')

        self.assertTrue(port.is_testharness_test('testharness.html'))
        self.assertFalse(port.is_testharness_test('not-testharness.html'))
        self.assertFalse(port.is_testharness_test('does-not-exist.html'))

    def test_get_wpt_fuzzy_metadata_for_non_wpt_test(self):
        port = self.make_port(with_tests=True)

        rt_path = port.abspath_for_test("passes/reftest.html")

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"15;300\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([15, 15], [300, 300]))

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"3-20;300\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([3, 20], [300, 300]))

        port._filesystem.write_text_file(
            rt_path, "foo<meta name=fuzzy content=\"ref.html:0;1-200\">bar")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([0, 0], [1, 200]))

        port._filesystem.write_text_file(
            rt_path,
            "<meta   name=fuzzy\ncontent=\"ref.html:maxDifference=30;totalPixels=1-2\">"
        )
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([30, 30], [1, 2]))

        result = port.get_wpt_fuzzy_metadata(
            "virtual/virtual_passes/passes/reftest.html")
        self.assertEqual(result, ([30, 30], [1, 2]))

    def test_get_wpt_fuzzy_metadata_for_non_wpt_test_with_non_virtual_dsf(
            self):
        port = self.make_port(with_tests=True)
        port._options.additional_driver_flag = [
            '--force-device-scale-factor=2'
        ]
        rt_path = port.abspath_for_test("passes/reftest.html")

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"15;300\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([15, 15], [1200, 1200]))

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"3-20;100\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([3, 20], [400, 400]))

        port._filesystem.write_text_file(
            rt_path, "foo<meta name=fuzzy content=\"ref.html:0;1-200\">bar")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([0, 0], [4, 800]))

    def test_get_wpt_fuzzy_metadata_for_non_wpt_test_with_virtual_dsf(self):
        port = self.make_port(with_tests=True)
        port.args_for_test = unittest.mock.MagicMock(
            return_value=['--force-device-scale-factor=2'])
        rt_path = port.abspath_for_test("passes/reftest.html")

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"15;300\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([15, 15], [1200, 1200]))

        port._filesystem.write_text_file(
            rt_path, "<meta name=fuzzy content=\"3-20;100\">")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([3, 20], [400, 400]))

        port._filesystem.write_text_file(
            rt_path, "foo<meta name=fuzzy content=\"ref.html:0;1-200\">bar")
        result = port.get_wpt_fuzzy_metadata("passes/reftest.html")
        self.assertEqual(result, ([0, 0], [4, 800]))

    def test_get_wpt_fuzzy_metadata_for_wpt_test(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        result = port.get_wpt_fuzzy_metadata(
            'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
        )
        self.assertEqual(result, ([0, 255], [0, 200]))
        result = port.get_wpt_fuzzy_metadata(
            'external/wpt/dom/ranges/Range-attributes.html')
        self.assertEqual(result, (None, None))

    def test_get_wpt_fuzzy_metadata_for_wpt_test_with_dsf(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)
        port.args_for_test = unittest.mock.MagicMock(
            return_value=['--force-device-scale-factor=2'])
        result = port.get_wpt_fuzzy_metadata(
            'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
        )
        self.assertEqual(result, ([0, 255], [0, 800]))
        result = port.get_wpt_fuzzy_metadata(
            'external/wpt/dom/ranges/Range-attributes.html')
        self.assertEqual(result, (None, None))

    def test_get_file_path_for_wpt_test(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertEqual(
            port.get_file_path_for_wpt_test(
                'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html'
            ),
            'external/wpt/dom/ranges/Range-attributes.html',
        )
        self.assertEqual(
            port.get_file_path_for_wpt_test(
                'external/wpt/console/console-is-a-namespace.any.worker.html'),
            'external/wpt/console/console-is-a-namespace.any.js',
        )
        self.assertEqual(
            port.get_file_path_for_wpt_test(
                'external/wpt/html/parse.html?run_type=uri'),
            'external/wpt/html/parse.html',
        )

        self.assertIsNone(port.get_file_path_for_wpt_test('non-wpt/test.html'))
        self.assertIsNone(
            port.get_file_path_for_wpt_test('external/wpt/non-existent.html'))

    def test_reference_files(self):
        port = self.make_port(with_tests=True)
        port.set_option_default('manifest_update', False)
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json',
            json.dumps({
                'items': {
                    'reftest': {
                        'blank.html': [
                            'abcdef123',
                            [None, [['about:blank', '==']], {}],
                        ],
                    },
                },
            }))
        self.assertEqual(
            port.reference_files('passes/svgreftest.svg'),
            [('==', port.web_tests_dir() + 'passes/svgreftest-expected.svg')])
        self.assertEqual(
            port.reference_files('passes/xhtreftest.svg'),
            [('==', port.web_tests_dir() + 'passes/xhtreftest-expected.html')])
        self.assertEqual(port.reference_files('passes/phpreftest.php'),
                         [('!=', port.web_tests_dir() +
                           'passes/phpreftest-expected-mismatch.svg')])
        self.assertEqual(port.reference_files('external/wpt/blank.html'),
                         [('==', 'about:blank')])

    def test_reference_files_from_manifest(self):
        port = self.make_port(with_tests=True)
        add_manifest_to_mock_filesystem(port)

        self.assertEqual(
            port.reference_files(
                'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
            ),
            [('==', port.web_tests_dir() +
              'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html'
              )])
        self.assertEqual(
            port.reference_files(
                'virtual/layout_ng/' +
                'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'
            ),
            [('==', port.web_tests_dir() +
              'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html'
              )])

    def test_http_server_supports_ipv6(self):
        port = self.make_port()
        self.assertTrue(port.http_server_supports_ipv6())
        port.host.platform.os_name = 'win'
        self.assertFalse(port.http_server_supports_ipv6())

    def test_http_server_requires_http_protocol_options_unsafe(self):
        port = self.make_port(
            executive=MockExecutive(
                stderr=
                ("Invalid command 'INTENTIONAL_SYNTAX_ERROR', perhaps misspelled or"
                 " defined by a module not included in the server configuration\n"
                 )))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        self.assertTrue(
            port.http_server_requires_http_protocol_options_unsafe())

    def test_http_server_doesnt_require_http_protocol_options_unsafe(self):
        port = self.make_port(
            executive=MockExecutive(
                stderr=
                ("Invalid command 'HttpProtocolOptions', perhaps misspelled or"
                 " defined by a module not included in the server configuration\n"
                 )))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        self.assertFalse(
            port.http_server_requires_http_protocol_options_unsafe())

    def test_check_httpd_success(self):
        port = self.make_port(executive=MockExecutive())
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        capture = OutputCapture()
        capture.capture_output()
        self.assertTrue(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('', logs)

    def test_httpd_returns_error_code(self):
        port = self.make_port(executive=MockExecutive(exit_code=1))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        capture = OutputCapture()
        capture.capture_output()
        self.assertFalse(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('httpd seems broken. Cannot run http tests.\n', logs)

    def test_test_exists(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_exists('passes'))
        self.assertTrue(port.test_exists('passes/text.html'))
        self.assertFalse(port.test_exists('passes/does_not_exist.html'))

        self.assertTrue(port.test_exists('virtual'))
        self.assertFalse(port.test_exists('virtual/does_not_exist.html'))
        self.assertTrue(
            port.test_exists('virtual/virtual_passes/passes/text.html'))

        self.assertTrue(
            port.test_exists('virtual/virtual_empty_bases/physical1.html'))
        self.assertTrue(
            port.test_exists('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(
            port.test_exists(
                'virtual/virtual_empty_bases/does_not_exist.html'))

    def test_read_test(self):
        port = self.make_port(with_tests=True)

        port._filesystem.write_text_file(
            port.abspath_for_test("passes/text.html"), "Foo")
        self.assertEqual(port.read_test("passes/text.html"), "Foo")
        self.assertEqual(
            port.read_test("virtual/virtual_passes/passes/text.html"), "Foo")

        port._filesystem.write_text_file(
            port.abspath_for_test("virtual/virtual_passes/passes/text.html"),
            "Bar")
        self.assertEqual(
            port.read_test("virtual/virtual_passes/passes/text.html"), "Bar")

        port._filesystem.write_text_file(
            port.abspath_for_test(
                "virtual/virtual_empty_bases/physical1.html"), "Baz")
        self.assertEqual(
            port.read_test("virtual/virtual_empty_bases/physical1.html"),
            "Baz")

        port._filesystem.write_binary_file(
            port.abspath_for_test("passes/text.html"), "Foo".encode("utf16"))
        self.assertEqual(port.read_test("passes/text.html", "utf16"), "Foo")

    def test_test_isfile(self):
        port = self.make_port(with_tests=True)
        self.assertFalse(port.test_isfile('passes'))
        self.assertTrue(port.test_isfile('passes/text.html'))
        self.assertFalse(port.test_isfile('passes/does_not_exist.html'))

        self.assertFalse(port.test_isfile('virtual'))
        self.assertTrue(
            port.test_isfile('virtual/virtual_passes/passes/text.html'))
        self.assertFalse(port.test_isfile('virtual/does_not_exist.html'))

        self.assertTrue(
            port.test_isfile('virtual/virtual_empty_bases/physical1.html'))
        self.assertTrue(
            port.test_isfile('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(
            port.test_exists(
                'virtual/virtual_empty_bases/does_not_exist.html'))

    def test_test_isdir(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_isdir('passes'))
        self.assertFalse(port.test_isdir('passes/text.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist/'))

        self.assertTrue(port.test_isdir('virtual'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist.html'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist/'))
        self.assertFalse(
            port.test_isdir('virtual/virtual_passes/passes/text.html'))

        self.assertTrue(port.test_isdir('virtual/virtual_empty_bases/'))
        self.assertTrue(port.test_isdir('virtual/virtual_empty_bases/dir'))
        self.assertFalse(
            port.test_isdir('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(
            port.test_isdir('virtual/virtual_empty_bases/does_not_exist/'))

    def test_tests(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertIn('passes/text.html', tests)
        self.assertIn('virtual/virtual_passes/passes/text.html', tests)
        self.assertIn('virtual/virtual_empty_bases/physical1.html', tests)
        self.assertIn('virtual/virtual_empty_bases/dir/physical2.html', tests)

        tests = port.tests(['passes'])
        self.assertIn('passes/text.html', tests)
        self.assertIn('passes/virtual_passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/text.html', tests)

        # crbug.com/880609: test trailing slashes
        tests = port.tests(['virtual/virtual_passes'])
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html',
                      tests)
        self.assertIn(
            'virtual/virtual_passes/passes_two/test-virtual-passes.html',
            tests)

        tests = port.tests(['virtual/virtual_passes/'])
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html',
                      tests)
        self.assertIn(
            'virtual/virtual_passes/passes_two/test-virtual-passes.html',
            tests)

        tests = port.tests(['virtual/virtual_passes/passes'])
        self.assertNotIn('passes/text.html', tests)
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html',
                      tests)
        self.assertNotIn(
            'virtual/virtual_passes/passes_two/test-virtual-passes.html',
            tests)
        self.assertNotIn('passes/test-virtual-passes.html', tests)
        self.assertNotIn(
            'virtual/virtual_passes/passes/test-virtual-virtual/passes.html',
            tests)
        self.assertNotIn(
            'virtual/virtual_passes/passes/virtual_passes/passes/test-virtual-passes.html',
            tests)

        tests = port.tests(
            ['virtual/virtual_passes/passes/test-virtual-passes.html'])
        self.assertEquals(
            ['virtual/virtual_passes/passes/test-virtual-passes.html'], tests)

        tests = port.tests(['virtual/virtual_empty_bases'])
        self.assertEquals([
            'virtual/virtual_empty_bases/physical1.html',
            'virtual/virtual_empty_bases/dir/physical2.html'
        ], tests)

        tests = port.tests(['virtual/virtual_empty_bases/dir'])
        self.assertEquals(['virtual/virtual_empty_bases/dir/physical2.html'],
                          tests)

        tests = port.tests(['virtual/virtual_empty_bases/dir/physical2.html'])
        self.assertEquals(['virtual/virtual_empty_bases/dir/physical2.html'],
                          tests)

    def test_build_path(self):
        # Test for a protected method - pylint: disable=protected-access
        # Test that optional paths are used regardless of whether they exist.
        options = optparse.Values({
            'configuration': 'Release',
            'build_directory': 'xcodebuild'
        })
        self.assertEqual(
            self.make_port(options=options).build_path(),
            '/mock-checkout/xcodebuild/Release')

        # Test that "out" is used as the default.
        options = optparse.Values({
            'configuration': 'Release',
            'build_directory': None
        })
        self.assertEqual(
            self.make_port(options=options).build_path(),
            '/mock-checkout/out/Release')

    def test_dont_require_http_server(self):
        port = self.make_port()
        self.assertEqual(port.requires_http_server(), False)

    def test_can_load_actual_virtual_test_suite_file(self):
        port = Port(SystemHost(), 'baseport')
        port.operating_system = lambda: 'linux'

        # If this call returns successfully, we found and loaded the web_tests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_good_virtual_test_suite_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'VirtualTestSuites'),
            '[{"prefix": "bar", "platforms": ["Linux", "Mac", "Win"], '
            '"bases": ["fast/bar"], "args": ["--bar"], "expires": "never"}]')

        # If this call returns successfully, we found and loaded the web_tests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_duplicate_virtual_prefix_in_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'VirtualTestSuites'), '['
            '{"prefix": "bar", "platforms": ["Linux"], "bases": ["fast/bar"], '
            '"args": ["--bar"], "expires": "never"},'
            '{"prefix": "bar", "platforms": ["Linux"], "bases": ["fast/foo"], '
            '"args": ["--bar"], "expires": "never"}'
            ']')

        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_virtual_test_suite_file_is_not_json(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'VirtualTestSuites'), '{[{[')
        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_lookup_virtual_test_base(self):
        port = self.make_port(with_tests=True)
        self.assertIsNone(port.lookup_virtual_test_base('non/virtual'))
        self.assertIsNone(port.lookup_virtual_test_base('passes/text.html'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/non-existing/test.html'))

        # lookup_virtual_test_base() checks virtual prefix and bases, but doesn't
        # check existence of test.
        self.assertEqual(
            'passes/text.html',
            port.lookup_virtual_test_base(
                'virtual/virtual_passes/passes/text.html'))
        self.assertEqual(
            'passes/any.html',
            port.lookup_virtual_test_base(
                'virtual/virtual_passes/passes/any.html'))
        self.assertEqual(
            'passes_two/any.html',
            port.lookup_virtual_test_base(
                'virtual/virtual_passes/passes_two/any.html'))
        self.assertEqual(
            'passes/',
            port.lookup_virtual_test_base('virtual/virtual_passes/passes/'))
        self.assertEqual(
            'passes/',
            port.lookup_virtual_test_base('virtual/virtual_passes/passes'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/virtual_passes/'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/virtual_passes'))
        # 'failures' is not a specified base of virtual/virtual_passes
        self.assertIsNone(
            port.lookup_virtual_test_base(
                'virtual/virtual_passes/failures/unexpected/text.html'))
        self.assertEqual(
            'failures/unexpected/text.html',
            port.lookup_virtual_test_base(
                'virtual/virtual_failures/failures/unexpected/text.html'))
        # 'passes' is not a specified base of virtual/virtual_failures
        self.assertIsNone(
            port.lookup_virtual_test_base(
                'virtual/virtual_failures/passes/text.html'))

        # Partial match of base with multiple levels.
        self.assertEqual(
            'failures/',
            port.lookup_virtual_test_base(
                'virtual/virtual_failures/failures/'))
        self.assertEqual(
            'failures/',
            port.lookup_virtual_test_base('virtual/virtual_failures/failures'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/virtual_failures/'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/virtual_failures'))

        # Empty bases.
        self.assertIsNone(
            port.lookup_virtual_test_base(
                'virtual/virtual_empty_bases/physical1.html'))
        self.assertIsNone(
            port.lookup_virtual_test_base(
                'virtual/virtual_empty_bases/passes/text.html'))
        self.assertIsNone(
            port.lookup_virtual_test_base('virtual/virtual_empty_bases'))

    def test_args_for_test(self):
        port = self.make_port(with_tests=True)
        self.assertEqual([
            '--disable-threaded-compositing', '--disable-threaded-animation',
            '--enable-unsafe-swiftshader'
        ], port.args_for_test('non/virtual'))
        self.assertEqual([
            '--disable-threaded-compositing', '--disable-threaded-animation',
            '--enable-unsafe-swiftshader'
        ], port.args_for_test('passes/text.html'))
        self.assertEqual([
            '--disable-threaded-compositing', '--disable-threaded-animation',
            '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/non-existing/test.html'))

        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes/passes/text.html'))
        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes/passes/any.html'))
        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes/passes/'))
        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes/passes'))
        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes/'))
        self.assertEqual([
            '--virtual-arg', '--disable-threaded-compositing',
            '--disable-threaded-animation', '--enable-unsafe-swiftshader'
        ], port.args_for_test('virtual/virtual_passes'))

    def test_missing_virtual_test_suite_file(self):
        port = self.make_port()
        self.assertRaises(AssertionError, port.virtual_test_suites)

    def test_virtual_test_expires(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v1", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-a"], "expires": "Jul 1, 2022"},'
            '{"prefix": "v2", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-b"], "expires": "Jul 1, 2222"},'
            '{"prefix": "v3", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-c"], "expires": "never"}'
            ']')
        fs.write_text_file(fs.join(web_tests_dir, 'test', 'test.html'), '')
        # expires won't have an effect when loading the tests
        self.assertTrue("virtual/v1/test/test.html" in port.tests())
        self.assertTrue("virtual/v2/test/test.html" in port.tests())
        self.assertTrue("virtual/v3/test/test.html" in port.tests())

    def test_virtual_test_disabled(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v1", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-a"], "disabled": false},'
            '{"prefix": "v2", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-b"], "disabled": true},'
            '{"prefix": "v3", "platforms": ["Linux"], "bases": ["test"],'
            ' "args": ["-c"]}'
            ']')
        fs.write_text_file(fs.join(web_tests_dir, 'test', 'test.html'), '')

        self.assertFalse(
            port.virtual_test_skipped_due_to_disabled(
                "virtual/v1/test/test.html"))
        self.assertTrue(
            port.virtual_test_skipped_due_to_disabled(
                "virtual/v2/test/test.html"))
        self.assertFalse(
            port.virtual_test_skipped_due_to_disabled(
                "virtual/v3/test/test.html"))

    def test_virtual_exclusive_tests(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v1", "platforms": ["Linux"], "bases": ["b1", "b2"],'
            ' "exclusive_tests": "ALL", '
            '"args": ["-a"], "expires": "never"},'
            '{"prefix": "v2", "platforms": ["Linux"], "bases": ["b2"],'
            ' "exclusive_tests": ["b2/test.html"], '
            '"args": ["-b"], "expires": "never"},'
            '{"prefix": "v3", "platforms": ["Linux"], "bases": ["b3"],'
            ' "args": ["-c"], "expires": "never"}'
            ']')
        fs.write_text_file(fs.join(web_tests_dir, 'b1', 'test.html'), '')
        fs.write_text_file(fs.join(web_tests_dir, 'b1', 'test2.html'), '')
        fs.write_text_file(fs.join(web_tests_dir, 'b2', 'test.html'), '')
        fs.write_text_file(fs.join(web_tests_dir, 'b2', 'test.html'), '')
        fs.write_text_file(fs.join(web_tests_dir, 'b3', 'test.html'), '')

        self.assertTrue(port.skipped_due_to_exclusive_virtual_tests('b1'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests('b1/test.html'))
        self.assertTrue(port.skipped_due_to_exclusive_virtual_tests('b2'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests('b2/test.html'))
        self.assertFalse(port.skipped_due_to_exclusive_virtual_tests('b3'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests('b3/test.html'))

        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests('virtual/v1/b1'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/b1/test.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests('virtual/v1/b2'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/b2/test.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/b2/test2.html'))

        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests('virtual/v2/b2'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/b2/test.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/b2/test2.html'))

    def test_virtual_exclusive_tests_with_generated_tests(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v1", "platforms": ["Linux"], "bases": ["external/wpt/console/b1.any.js"],'
            ' "exclusive_tests": "ALL", '
            '"args": ["-a"], "expires": "never"},'
            '{"prefix": "v2", "platforms": ["Linux"], "bases": ["external/wpt/console/b1.any.js",'
            '                                                   "external/wpt/console/b2.any.js"],'
            ' "exclusive_tests": ["external/wpt/console/b2.any.js"], '
            '"args": ["-b"], "expires": "never"}'
            ']')
        fs.write_text_file(
            fs.join(web_tests_dir, 'external/wpt/console', 'b1.any.js'), '')
        fs.write_text_file(
            fs.join(web_tests_dir, 'external/wpt/console', 'b2.any.js'), '')

        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b1.any.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b1.any.sharedworker.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b1.any.worker.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/external/wpt/console/b1.any.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/external/wpt/console/b1.any.sharedworker.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v1/external/wpt/console/b1.any.worker.html'))

        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b2.any.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b2.any.sharedworker.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'external/wpt/console/b2.any.worker.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b1.any.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b1.any.sharedworker.html'))
        self.assertTrue(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b1.any.worker.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b2.any.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b2.any.sharedworker.html'))
        self.assertFalse(
            port.skipped_due_to_exclusive_virtual_tests(
                'virtual/v2/external/wpt/console/b2.any.worker.html'))

    def test_virtual_skip_base_tests(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v1", "platforms": ["Linux"], "bases": ["b1", "b2"],'
            '"args": ["-a"], "expires": "never"},'
            '{"prefix": "v2", "platforms": ["Linux"], "bases": ["b1"],'
            '"skip_base_tests": "ALL",'
            '"args": ["-a"], "expires": "never"}'
            ']')
        fs.write_text_file(fs.join(web_tests_dir, 'b1', 'test1.html'), '')
        fs.write_text_file(fs.join(web_tests_dir, 'b2', 'test2.html'), '')

        self.assertTrue(port.skipped_due_to_skip_base_tests('b1/test.html'))
        self.assertFalse(
            port.skipped_due_to_skip_base_tests('virtual/v1/b1/test1.html'))
        self.assertFalse(port.skipped_due_to_skip_base_tests('b2/test2.html'))
        self.assertFalse(
            port.skipped_due_to_skip_base_tests('virtual/v1/b2/test2.html'))

    # test.any.js shows up on the filesystem as one file but it effectively becomes two test files:
    # test.any.html and test.any.worker.html. We should support skipping test.any.js.
    def test_virtual_skip_base_tests_with_generated_tests(self):
        port = self.make_port()
        fs = port.host.filesystem
        web_tests_dir = port.web_tests_dir()
        fs.write_text_file(
            fs.join(web_tests_dir, 'VirtualTestSuites'), '['
            '{"prefix": "v", "platforms": ["Linux"], "bases": ["external/wpt/console/test.any.js"],'
            '"skip_base_tests": "ALL",'
            '"args": ["-a"], "expires": "never"}'
            ']')
        fs.write_text_file(
            fs.join(web_tests_dir, 'external/wpt/console', 'test.any.js'), '')

        self.assertTrue(
            port.skipped_due_to_skip_base_tests(
                'external/wpt/console/test.any.html'))
        self.assertTrue(
            port.skipped_due_to_skip_base_tests(
                'external/wpt/console/test.any.worker.html'))
        self.assertFalse(
            port.skipped_due_to_skip_base_tests(
                'virtual/v/external/wpt/console/test.any.html'))
        self.assertFalse(
            port.skipped_due_to_skip_base_tests(
                'virtual/v/external/wpt/console/test.any.worker.html'))

    def test_default_results_directory(self):
        port = self.make_port(
            options=optparse.Values({
                'target': 'Default',
                'configuration': 'Release'
            }))
        # By default the results directory is in the build directory: out/<target>.
        self.assertEqual(port.default_results_directory(),
                         '/mock-checkout/out/Default')

    def test_results_directory(self):
        port = self.make_port(
            options=optparse.Values({
                'results_directory':
                'some-directory/results'
            }))
        # A results directory can be given as an option, and it is relative to current working directory.
        self.assertEqual(port.host.filesystem.cwd, '/')
        self.assertEqual(port.results_directory(), '/some-directory/results')

    def _make_fake_test_result(self, host, results_directory):
        host.filesystem.maybe_make_directory(results_directory)
        host.filesystem.write_binary_file(results_directory + '/results.html',
                                          'This is a test results file')

    def test_rename_results_folder(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        self._make_fake_test_result(port.host, '/tmp/layout-test-results')
        self.assertTrue(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        timestamp = time.strftime(
            '%Y-%m-%d-%H-%M-%S',
            time.localtime(
                port.host.filesystem.mtime(
                    '/tmp/layout-test-results/results.html')))
        archived_file_name = '/tmp/layout-test-results' + '_' + timestamp
        port.rename_results_folder()
        self.assertFalse(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        self.assertTrue(port.host.filesystem.exists(archived_file_name))

    def test_clobber_old_results(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        self._make_fake_test_result(port.host, '/tmp/layout-test-results')
        self.assertTrue(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        port.clobber_old_results()
        self.assertFalse(
            port.host.filesystem.exists('/tmp/layout-test-results'))

    def test_limit_archived_results_count(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        for x in range(1, 31):
            dir_name = '/tmp/layout-test-results' + '_' + str(x)
            self._make_fake_test_result(port.host, dir_name)
        port.limit_archived_results_count()
        deleted_dir_count = 0
        for x in range(1, 6):
            dir_name = '/tmp/layout-test-results' + '_' + str(x)
            self.assertFalse(port.host.filesystem.exists(dir_name))
        for x in range(6, 31):
            dir_name = '/tmp/layout-test-results' + '_' + str(x)
            self.assertTrue(port.host.filesystem.exists(dir_name))

    def _assert_config_file_for_platform(self, port, platform, config_file):
        port.host.platform = MockPlatformInfo(os_name=platform)
        self.assertEqual(
            port._apache_config_file_name_for_platform(),  # pylint: disable=protected-access
            config_file)

    def _assert_config_file_for_linux_distribution(self, port, distribution,
                                                   config_file):
        port.host.platform = MockPlatformInfo(
            os_name='linux', linux_distribution=distribution)
        self.assertEqual(
            port._apache_config_file_name_for_platform(),  # pylint: disable=protected-access
            config_file)

    def test_apache_config_file_name_for_platform(self):
        port = self.make_port()
        port._apache_version = lambda: '2.4'  # pylint: disable=protected-access
        self._assert_config_file_for_platform(port, 'linux',
                                              'apache2-httpd-2.4-php7.conf')
        self._assert_config_file_for_linux_distribution(
            port, 'arch', 'apache2-httpd-2.4-php7.conf')

        self._assert_config_file_for_platform(port, 'mac',
                                              'apache2-httpd-2.4-php7.conf')
        self._assert_config_file_for_platform(port, 'win32',
                                              'apache2-httpd-2.4-php7.conf')
        self._assert_config_file_for_platform(port, 'barf',
                                              'apache2-httpd-2.4-php7.conf')

    def test_skips_test_in_smoke_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: True
        port.host.filesystem.write_text_file(port.path_to_smoke_tests_file(),
                                             'passes/text.html\n')
        self.assertTrue(port.skips_test('failures/expected/image.html'))

    def test_skips_test_no_skip_smoke_tests_file(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: True
        self.assertFalse(port.skips_test('failures/expected/image.html'))

    def test_skips_test_port_doesnt_skip_smoke_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        self.assertFalse(port.skips_test('failures/expected/image.html'))

    def test_skips_test_in_test_expectations(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        port.host.filesystem.write_text_file(
            port.path_to_generic_test_expectations_file(),
            'Bug(test) failures/expected/image.html [ Skip ]\n')
        self.assertFalse(port.skips_test('failures/expected/image.html'))

    def test_skips_test_in_never_fix_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        port.host.filesystem.write_text_file(
            port.path_to_never_fix_tests_file(),
            '# results: [ Skip ]\nfailures/expected/image.html [ Skip ]\n')
        self.assertTrue(port.skips_test('failures/expected/image.html'))

    def test_enable_tracing(self):
        options, _ = optparse.OptionParser().parse_args([])
        options.enable_tracing = '*,-blink'
        port = self.make_port(with_tests=True, options=options)
        with mock.patch('time.strftime', return_value='TIME'):
            self.assertEqual([
                '--disable-threaded-compositing',
                '--disable-threaded-animation',
                '--enable-unsafe-swiftshader',
                '--trace-startup=*,-blink',
                '--trace-startup-duration=0',
                '--trace-startup-file=trace_layout_test_non_virtual_TIME.pftrace',
            ], port.args_for_test('non/virtual'))

    def test_all_systems(self):
        # Port.ALL_SYSTEMS should match CONFIGURATION_SPECIFIER_MACROS.
        all_systems = []
        for system in Port.ALL_SYSTEMS:
            self.assertEqual(len(system), 2)
            all_systems.append(system[0])
        all_systems.sort()
        configuration_specifier_macros = []
        for macros in Port.CONFIGURATION_SPECIFIER_MACROS.values():
            configuration_specifier_macros += macros
        configuration_specifier_macros.sort()
        self.assertListEqual(all_systems, configuration_specifier_macros)

    def test_configuration_specifier_macros(self):
        # CONFIGURATION_SPECIFIER_MACROS should contain all SUPPORTED_VERSIONS
        # of each port. Must use real Port classes in this test.
        for port_name, versions in Port.CONFIGURATION_SPECIFIER_MACROS.items():
            port_class, _ = PortFactory.get_port_class(port_name)
            self.assertIsNotNone(port_class, port_name)
            self.assertListEqual(versions, list(port_class.SUPPORTED_VERSIONS))

    def test_used_expectations_files_pardir(self):
        port = self.make_port()
        original_dir = port.host.filesystem.getcwd()
        try:
            subdir = port.host.filesystem.join(port.web_tests_dir(),
                                               'some_directory')
            port.host.filesystem.maybe_make_directory(subdir)
            port.host.filesystem.chdir(subdir)
            port._options.additional_expectations = [
                port.host.filesystem.join('..', 'some_file')
            ]
            self.assertIn(
                port.host.filesystem.join(port.web_tests_dir(), 'some_file'),
                port.used_expectations_files())
        finally:
            port.host.filesystem.chdir(original_dir)


class NaturalCompareTest(unittest.TestCase):
    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_order(self, x, y, predicate):
        self.assertTrue(
            predicate(self._port._natural_sort_key(x),
                      self._port._natural_sort_key(y)))

    def test_natural_compare(self):
        self.assert_order('a', 'a', operator.eq)
        self.assert_order('ab', 'a', operator.gt)
        self.assert_order('a', 'ab', operator.lt)
        self.assert_order('', '', operator.eq)
        self.assert_order('', 'ab', operator.lt)
        self.assert_order('1', '2', operator.lt)
        self.assert_order('2', '1', operator.gt)
        self.assert_order('1', '10', operator.lt)
        self.assert_order('2', '10', operator.lt)
        self.assert_order('foo_1.html', 'foo_2.html', operator.lt)
        self.assert_order('foo_1.1.html', 'foo_2.html', operator.lt)
        self.assert_order('foo_1.html', 'foo_10.html', operator.lt)
        self.assert_order('foo_2.html', 'foo_10.html', operator.lt)
        self.assert_order('foo_23.html', 'foo_10.html', operator.gt)
        self.assert_order('foo_23.html', 'foo_100.html', operator.lt)


class KeyCompareTest(unittest.TestCase):
    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_order(self, x, y, predicate):
        self.assertTrue(
            predicate(self._port.test_key(x), self._port.test_key(y)))

    def test_test_key(self):
        self.assert_order('/a', '/a', operator.eq)
        self.assert_order('/a', '/b', operator.lt)
        self.assert_order('/a2', '/a10', operator.lt)
        self.assert_order('/a2/foo', '/a10/foo', operator.lt)
        self.assert_order('/a/foo11', '/a/foo2', operator.gt)
        self.assert_order('/ab', '/a/a/b', operator.lt)
        self.assert_order('/a/a/b', '/ab', operator.gt)
        self.assert_order('/foo-bar/baz', '/foo/baz', operator.lt)


class VirtualTestSuiteTest(unittest.TestCase):
    def test_basic(self):
        suite = VirtualTestSuite(prefix='suite',
                                 platforms=['Linux', 'Mac', 'Win'],
                                 bases=['base/foo', 'base/bar'],
                                 args=['--args'])
        self.assertEqual(suite.full_prefix, 'virtual/suite/')
        self.assertEqual(suite.platforms, ['linux', 'mac', 'win'])
        self.assertEqual(suite.bases, ['base/foo', 'base/bar'])
        self.assertEqual(suite.args, ['--args'])

    def test_empty_bases(self):
        suite = VirtualTestSuite(prefix='suite',
                                 platforms=['Linux', 'Mac', 'Win'],
                                 bases=[],
                                 args=['--args'])
        self.assertEqual(suite.full_prefix, 'virtual/suite/')
        self.assertEqual(suite.platforms, ['linux', 'mac', 'win'])
        self.assertEqual(suite.bases, [])
        self.assertEqual(suite.args, ['--args'])

    def test_no_slash(self):
        self.assertRaises(
            AssertionError,
            VirtualTestSuite,
            prefix='suite/bar',
            bases=['base/foo'],
            args=['--args'])
