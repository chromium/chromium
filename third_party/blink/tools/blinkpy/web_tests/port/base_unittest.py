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

import json
import optparse
import unittest

from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.common.system.platform_info_mock import MockPlatformInfo
from blinkpy.common.system.system_host import SystemHost
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port.base import Port, VirtualTestSuite
from blinkpy.web_tests.port.test import add_unit_tests_to_mock_filesystem, WEB_TEST_DIR, TestPort


MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS

class PortTest(LoggingTestCase):

    def make_port(self, executive=None, with_tests=False, port_name=None, **kwargs):
        host = MockSystemHost()
        if executive:
            host.executive = executive
        if with_tests:
            add_unit_tests_to_mock_filesystem(host.filesystem)
            return TestPort(host, **kwargs)
        return Port(host, port_name or 'baseport', **kwargs)

    def test_validate_wpt_dirs(self):
        # Keys should not have trailing slashes.
        for wpt_path in Port.WPT_DIRS.keys():
            self.assertFalse(wpt_path.endswith('/'))
        # Values should not be empty (except the last one).
        for url_prefix in Port.WPT_DIRS.values()[:-1]:
            self.assertNotEqual(url_prefix, '/')
        self.assertEqual(Port.WPT_DIRS.values()[-1], '/')

    def test_validate_wpt_regex(self):
        self.assertEquals(Port.WPT_REGEX.match('external/wpt/foo/bar.html').groups(),
                          ('external/wpt', 'foo/bar.html'))
        self.assertEquals(Port.WPT_REGEX.match('virtual/test/external/wpt/foo/bar.html').groups(),
                          ('external/wpt', 'foo/bar.html'))
        self.assertEquals(Port.WPT_REGEX.match('wpt_internal/foo/bar.html').groups(),
                          ('wpt_internal', 'foo/bar.html'))
        self.assertEquals(Port.WPT_REGEX.match('virtual/test/wpt_internal/foo/bar.html').groups(),
                          ('wpt_internal', 'foo/bar.html'))

    def test_setup_test_run(self):
        port = self.make_port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_test_dirs(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(port.web_tests_dir() + '/canvas/test', '')
        port.host.filesystem.write_text_file(port.web_tests_dir() + '/css2.1/test', '')
        dirs = port.test_dirs()
        self.assertIn('canvas', dirs)
        self.assertIn('css2.1', dirs)

    def test_get_option__set(self):
        options, _ = optparse.OptionParser().parse_args([])
        options.foo = 'bar'
        port = self.make_port(options=options)
        self.assertEqual(port.get_option('foo'), 'bar')

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
        self.assertEqual(port.output_filename(test_file, '-expected', '.txt'),
                         'fast/test-expected.txt')
        self.assertEqual(port.output_filename(test_file, '-expected-mismatch', '.png'),
                         'fast/test-expected-mismatch.png')

        # Test filename with query string
        test_file = 'fast/test.html?wss&run_type=1'
        self.assertEqual(port.output_filename(test_file, '-expected', '.txt'),
                         'fast/test_wss_run_type=1-expected.txt')
        self.assertEqual(port.output_filename(test_file, '-actual', '.png'),
                         'fast/test_wss_run_type=1-actual.png')

        # Test filename with query string containing a dot
        test_file = 'fast/test.html?include=HTML.*'
        self.assertEqual(port.output_filename(test_file, '-expected', '.txt'),
                         'fast/test_include=HTML._-expected.txt')
        self.assertEqual(port.output_filename(test_file, '-actual', '.png'),
                         'fast/test_include=HTML._-actual.png')

    def test_expected_baselines_basic(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        # The default baseline doesn't exist.
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(None, 'fast/test-expected.txt')])
        self.assertIsNone(port.expected_filename(test_file, '.txt', return_default=False))
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))

        # The default baseline exists.
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS[:-1], 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'fast/test-expected.txt')

    def test_expected_baselines_mismatch(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        self.assertEqual(port.expected_baselines(test_file, '.txt', match=False),
                         [(None, 'fast/test-expected-mismatch.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt', match=False),
                         MOCK_WEB_TESTS + 'fast/test-expected-mismatch.txt')

    def test_expected_baselines_platform_specific(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        self.assertEqual(port.baseline_version_dir(),
                         MOCK_WEB_TESTS + 'platform/foo')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')

        # The default baseline doesn't exist.
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS + 'platform/foo', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(test_file, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(test_file, '.txt'))

        # The default baseline exists.
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS + 'platform/foo', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(test_file, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEquals(port.fallback_expected_filename(test_file, '.txt'),
                          MOCK_WEB_TESTS + 'fast/test-expected.txt')
        port.host.filesystem.remove(MOCK_WEB_TESTS + 'fast/test-expected.txt')

    def test_expected_baselines_flag_specific(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')

        # pylint: disable=protected-access
        port._options.additional_platform_directory = []
        port._options.additional_driver_flag = ['--special-flag']
        self.assertEqual(port.baseline_search_path(), [
            MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo',
            MOCK_WEB_TESTS + 'flag-specific/special-flag',
            MOCK_WEB_TESTS + 'platform/foo'])
        self.assertEqual(port.baseline_version_dir(),
                         MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo')

        # Flag-specific baseline
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'flag-specific/special-flag/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(MOCK_WEB_TESTS + 'flag-specific/special-flag', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(test_file, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'flag-specific/special-flag/fast/test-expected.txt')
        self.assertEqual(port.fallback_expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # Flag-specific platform-specific baseline
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo', 'fast/test-expected.txt')])
        self.assertEqual(
            port.expected_filename(test_file, '.txt'),
            MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo/fast/test-expected.txt')
        self.assertEqual(
            port.expected_filename(test_file, '.txt', return_default=False),
            MOCK_WEB_TESTS + 'flag-specific/special-flag/platform/foo/fast/test-expected.txt')
        self.assertEqual(port.fallback_expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'flag-specific/special-flag/fast/test-expected.txt')

    def test_expected_baselines_virtual(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        virtual_test = 'virtual/flag/fast/test.html'
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'VirtualTestSuites',
            '[{ "prefix": "flag", "bases": ["fast"], "args": ["--flag"]}]')

        # The default baseline for base test
        self.assertEqual(port.expected_baselines(virtual_test, '.txt'),
                         [(None, 'virtual/flag/fast/test-expected.txt')])
        self.assertIsNone(port.expected_filename(virtual_test, '.txt', return_default=False))
        self.assertEqual(port.expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')
        self.assertIsNone(port.expected_filename(virtual_test, '.txt', return_default=False, fallback_base_for_virtual=False))
        self.assertEqual(port.expected_filename(virtual_test, '.txt', fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertIsNone(port.fallback_expected_filename(virtual_test, '.txt'))

        # Platform-specific baseline for base test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(virtual_test, '.txt'),
                         [(None, 'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(virtual_test, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')
        self.assertIsNone(port.expected_filename(virtual_test, '.txt', return_default=False, fallback_base_for_virtual=False))
        self.assertEqual(port.expected_filename(virtual_test, '.txt', fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.fallback_expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # The default baseline for virtual test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(virtual_test, '.txt'),
                         [(MOCK_WEB_TESTS[:-1], 'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(virtual_test, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt', return_default=False, fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt', fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.fallback_expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/fast/test-expected.txt')

        # Platform-specific baseline for virtual test
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'platform/foo/virtual/flag/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(virtual_test, '.txt'),
                         [(MOCK_WEB_TESTS + 'platform/foo', 'virtual/flag/fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(virtual_test, '.txt', return_default=False),
                         MOCK_WEB_TESTS + 'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt', return_default=False, fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.expected_filename(virtual_test, '.txt', fallback_base_for_virtual=False),
                         MOCK_WEB_TESTS + 'platform/foo/virtual/flag/fast/test-expected.txt')
        self.assertEqual(port.fallback_expected_filename(virtual_test, '.txt'),
                         MOCK_WEB_TESTS + 'virtual/flag/fast/test-expected.txt')

    def test_additional_platform_directory(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'VirtualTestSuites', '[]')
        test_file = 'fast/test.html'

        # Simple additional platform directory
        port._options.additional_platform_directory = ['/tmp/local-baselines']  # pylint: disable=protected-access
        self.assertEqual(port.baseline_version_dir(), '/tmp/local-baselines')

        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [(None, 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt', return_default=False), None)
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         MOCK_WEB_TESTS + 'fast/test-expected.txt')

        port.host.filesystem.write_text_file('/tmp/local-baselines/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         '/tmp/local-baselines/fast/test-expected.txt')

        # Multiple additional platform directories
        port._options.additional_platform_directory = ['/foo', '/tmp/local-baselines']  # pylint: disable=protected-access
        self.assertEqual(port.baseline_version_dir(), '/foo')

        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         '/tmp/local-baselines/fast/test-expected.txt')

        port.host.filesystem.write_text_file('/foo/fast/test-expected.txt', 'foo')
        self.assertEqual(port.expected_baselines(test_file, '.txt'),
                         [('/foo', 'fast/test-expected.txt')])
        self.assertEqual(port.expected_filename(test_file, '.txt'),
                         '/foo/fast/test-expected.txt')

    def test_nonexistant_expectations(self):
        port = self.make_port(port_name='foo')
        port.expectations_files = lambda: [MOCK_WEB_TESTS + 'platform/exists/TestExpectations',
                                           MOCK_WEB_TESTS + 'platform/nonexistant/TestExpectations']
        port.host.filesystem.write_text_file(MOCK_WEB_TESTS + 'platform/exists/TestExpectations', '')
        self.assertEqual('\n'.join(port.expectations_dict().keys()),
                         MOCK_WEB_TESTS + 'platform/exists/TestExpectations')

    def _make_port_for_test_additional_expectations(self, options_dict={}):
        port = self.make_port(port_name='foo', options=optparse.Values(options_dict))
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
        self.assertEqual(port.expectations_dict().values(), [])

    def test_additional_expectations_1(self):
        port = self._make_port_for_test_additional_expectations(
            {'additional_expectations': ['/tmp/additional-expectations-1.txt']})
        self.assertEqual(port.expectations_dict().values(), ['content1\n'])

    def test_additional_expectations_nonexistent_and_1(self):
        port = self._make_port_for_test_additional_expectations(
            {'additional_expectations': ['/tmp/nonexistent-file',
                                         '/tmp/additional-expectations-1.txt']})
        self.assertEqual(port.expectations_dict().values(), ['content1\n'])

    def test_additional_expectations_2(self):
        port = self._make_port_for_test_additional_expectations(
            {'additional_expectations': ['/tmp/additional-expectations-1.txt',
                                         '/tmp/additional-expectations-2.txt']})
        self.assertEqual(port.expectations_dict().values(), ['content1\n', 'content2\n'])

    def test_additional_expectations_additional_flag(self):
        port = self._make_port_for_test_additional_expectations(
            {'additional_expectations': ['/tmp/additional-expectations-1.txt',
                                         '/tmp/additional-expectations-2.txt'],
             'additional_driver_flag': ['--special-flag']})
        self.assertEqual(port.expectations_dict().values(), ['content3', 'content1\n', 'content2\n'])

    def test_flag_specific_expectations(self):
        port = self.make_port(port_name='foo')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/special-flag-a', 'aa')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/special-flag-b', 'bb')
        port.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'FlagExpectations/README.txt', 'cc')

        self.assertEqual(port.expectations_dict().values(), [])
        # all_expectations_dict() is an OrderedDict, but its order depends on
        # file system walking order.
        self.assertEqual(sorted(port.all_expectations_dict().values()), ['aa', 'bb'])

    def test_flag_specific_expectations_identify_unreadable_file(self):
        port = self.make_port(port_name='foo')

        non_utf8_file = MOCK_WEB_TESTS + 'FlagExpectations/non-utf8-file'
        invalid_utf8 = '\xC0'
        port.host.filesystem.write_binary_file(non_utf8_file, invalid_utf8)

        with self.assertRaises(UnicodeDecodeError):
            port.all_expectations_dict()

        # The UnicodeDecodeError does not indicate which file we failed to read,
        # so ensure that the file is identified in a log message.
        self.assertLog(['ERROR: Failed to read expectations file: \'' +
                        non_utf8_file + '\'\n'])

    def test_flag_specific_config_name_from_options(self):
        port_a = self.make_port(options=optparse.Values({}))
        # pylint: disable=protected-access
        self.assertEqual(port_a._specified_additional_driver_flags(), []);
        self.assertIsNone(port_a._flag_specific_config_name())

        port_b = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb']}))
        self.assertEqual(port_b._specified_additional_driver_flags(), ['--bb']);
        self.assertEqual(port_b._flag_specific_config_name(), 'bb')

        port_c = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--cc', '--dd']}))
        self.assertEqual(port_c._specified_additional_driver_flags(), ['--cc', '--dd']);
        self.assertEqual(port_c._flag_specific_config_name(), 'cc')

    def test_flag_specific_config_name_from_options_and_file(self):
        flag_file = MOCK_WEB_TESTS + 'additional-driver-flag.setting'

        port_a = self.make_port(options=optparse.Values({}))
        port_a.host.filesystem.write_text_file(flag_file, '--aa')
        # pylint: disable=protected-access
        self.assertEqual(port_a._specified_additional_driver_flags(), ['--aa']);
        self.assertEqual(port_a._flag_specific_config_name(), 'aa')

        port_b = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb']}))
        port_b.host.filesystem.write_text_file(flag_file, '--aa')
        self.assertEqual(port_b._specified_additional_driver_flags(), ['--aa', '--bb']);
        self.assertEqual(port_b._flag_specific_config_name(), 'aa')

        port_c = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--cc']}))
        port_c.host.filesystem.write_text_file(flag_file, '--bb --dd')
        # We don't remove duplicated flags at this time.
        self.assertEqual(port_c._specified_additional_driver_flags(), ['--bb', '--dd', '--bb', '--cc']);
        self.assertEqual(port_c._flag_specific_config_name(), 'bb')

    def _write_flag_specific_config(self, port):
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'FlagSpecificConfig'),
            '['
            '  {"name": "a", "args": ["--aa"]},'
            '  {"name": "b", "args": ["--bb", "--aa"]},'
            '  {"name": "c", "args": ["--bb", "--cc"]}'
            ']')

    def test_flag_specific_config_name_from_options_and_config(self):
        port_a1 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--aa']}))
        self._write_flag_specific_config(port_a1)
        # pylint: disable=protected-access
        self.assertEqual(port_a1._flag_specific_config_name(), 'a')

        port_a2 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--aa', '--dd']}))
        self._write_flag_specific_config(port_a2)
        self.assertEqual(port_a2._flag_specific_config_name(), 'a')

        port_a3 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--aa', '--bb']}))
        self._write_flag_specific_config(port_a3)
        self.assertEqual(port_a3._flag_specific_config_name(), 'a')

        port_b1 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--aa']}))
        self._write_flag_specific_config(port_b1)
        self.assertEqual(port_b1._flag_specific_config_name(), 'b')

        port_b2 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--aa', '--cc']}))
        self._write_flag_specific_config(port_b2)
        self.assertEqual(port_b2._flag_specific_config_name(), 'b')

        port_b3 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--aa', '--dd']}))
        self._write_flag_specific_config(port_b3)
        self.assertEqual(port_b3._flag_specific_config_name(), 'b')

        port_c1 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--cc']}))
        self._write_flag_specific_config(port_c1)
        self.assertEqual(port_c1._flag_specific_config_name(), 'c')

        port_c2 = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb', '--cc', '--aa']}))
        self._write_flag_specific_config(port_c2)
        self.assertEqual(port_c2._flag_specific_config_name(), 'c')

    def test_flag_specific_fallback(self):
        port_b = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--bb']}))
        self._write_flag_specific_config(port_b)
        # No match. Fallback to first specified flag.
        self.assertEqual(port_b._flag_specific_config_name(), 'bb')

        port_d = self.make_port(options=optparse.Values(
            {'additional_driver_flag': ['--dd', '--ee']}))
        self._write_flag_specific_config(port_d)
        # pylint: disable=protected-access
        self.assertEqual(port_d._flag_specific_config_name(), 'dd')

    def test_flag_specific_option(self):
        port_a = self.make_port(options=optparse.Values({'flag_specific': 'a'}))
        self._write_flag_specific_config(port_a)
        # pylint: disable=protected-access
        self.assertEqual(port_a._flag_specific_config_name(), 'a')

        port_b = self.make_port(options=optparse.Values(
            {'flag_specific': 'a', 'additional_driver_flag': ['--bb']}))
        self._write_flag_specific_config(port_b)
        self.assertEqual(port_b._flag_specific_config_name(), 'a')

        port_d = self.make_port(options=optparse.Values({'flag_specific': 'd'}))
        self._write_flag_specific_config(port_d)
        self.assertRaises(AssertionError, port_d._flag_specific_config_name)

    def test_duplicate_flag_specific_name(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'FlagSpecificConfig'),
            '[{"name": "a", "args": ["--aa"]}, {"name": "a", "args": ["--aa", "--bb"]}]')
        # pylint: disable=protected-access
        self.assertRaises(ValueError, port._flag_specific_configs)

    def test_duplicate_flag_specific_args(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'FlagSpecificConfig'),
            '[{"name": "a", "args": ["--aa"]}, {"name": "b", "args": ["--aa"]}]')
        # pylint: disable=protected-access
        self.assertRaises(ValueError, port._flag_specific_configs)

    def test_invalid_flag_specific_name(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'FlagSpecificConfig'),
            '[{"name": "a/", "args": ["--aa"]}]')
        # pylint: disable=protected-access
        self.assertRaises(ValueError, port._flag_specific_configs)

    def test_additional_env_var(self):
        port = self.make_port(options=optparse.Values({'additional_env_var': ['FOO=BAR', 'BAR=FOO']}))
        self.assertEqual(port.get_option('additional_env_var'), ['FOO=BAR', 'BAR=FOO'])
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
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/MANIFEST.json', '{}')
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

    @staticmethod
    def _add_manifest_to_mock_file_system(port):
        # Disable manifest update otherwise they'll be overwritten.
        port.set_option_default('manifest_update', False)
        filesystem = port.host.filesystem
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/MANIFEST.json', json.dumps({
            'items': {
                'testharness': {
                    'dom/ranges/Range-attributes.html': [
                        ['dom/ranges/Range-attributes.html', {}]
                    ],
                    'dom/ranges/Range-attributes-slow.html': [
                        ['dom/ranges/Range-attributes-slow.html', {'timeout': 'long'}]
                    ],
                    'console/console-is-a-namespace.any.js': [
                        ['console/console-is-a-namespace.any.html', {}],
                        ['console/console-is-a-namespace.any.worker.html', {'timeout': 'long'}],
                    ],
                    'html/parse.html': [
                        ['html/parse.html?run_type=uri', {}],
                        ['html/parse.html?run_type=write', {'timeout': 'long'}],
                    ],
                },
                'manual': {},
                'reftest': {
                    'html/dom/elements/global-attributes/dir_auto-EN-L.html': [
                        [
                            'html/dom/elements/global-attributes/dir_auto-EN-L.html',
                            [
                                [
                                    '/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html',
                                    '=='
                                ]
                            ],
                            {'timeout': 'long'}
                        ]
                    ],
                },
            }}))
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/dom/ranges/Range-attributes.html', '')
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/dom/ranges/Range-attributes-slow.html', '')
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/console/console-is-a-namespace.any.js', '')
        filesystem.write_text_file(WEB_TEST_DIR + '/external/wpt/common/blank.html', 'foo')

        filesystem.write_text_file(WEB_TEST_DIR + '/wpt_internal/MANIFEST.json', json.dumps({
            'items': {
                'testharness': {
                    'dom/bar.html': [
                        ['dom/bar.html', {}]
                    ]
                }
            }}))
        filesystem.write_text_file(WEB_TEST_DIR + '/wpt_internal/dom/bar.html', 'baz')

    def test_find_none_if_not_in_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)
        self.assertNotIn('external/wpt/common/blank.html', port.tests([]))
        self.assertNotIn('external/wpt/console/console-is-a-namespace.any.js', port.tests([]))

    def test_find_one_if_in_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)
        self.assertIn('external/wpt/dom/ranges/Range-attributes.html', port.tests([]))
        self.assertIn('external/wpt/console/console-is-a-namespace.any.html', port.tests([]))

    def test_wpt_tests_paths(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)
        all_wpt = [
            'external/wpt/console/console-is-a-namespace.any.html',
            'external/wpt/console/console-is-a-namespace.any.worker.html',
            'external/wpt/dom/ranges/Range-attributes-slow.html',
            'external/wpt/dom/ranges/Range-attributes.html',
            'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
            'external/wpt/html/parse.html?run_type=uri',
            'external/wpt/html/parse.html?run_type=write',
        ]
        # test.any.js shows up on the filesystem as one file but it effectively becomes two test files:
        # test.any.html and test.any.worker.html. We should support running test.any.js by name and
        # indirectly by specifying a parent directory.
        self.assertEqual(sorted(port.tests(['external'])), all_wpt)
        self.assertEqual(sorted(port.tests(['external/'])), all_wpt)
        self.assertEqual(port.tests(['external/csswg-test']), [])
        self.assertEqual(sorted(port.tests(['external/wpt'])), all_wpt)
        self.assertEqual(sorted(port.tests(['external/wpt/'])), all_wpt)
        self.assertEqual(sorted(port.tests(['external/wpt/console'])),
                         ['external/wpt/console/console-is-a-namespace.any.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html'])
        self.assertEqual(sorted(port.tests(['external/wpt/console/'])),
                         ['external/wpt/console/console-is-a-namespace.any.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html'])
        self.assertEqual(sorted(port.tests(['external/wpt/console/console-is-a-namespace.any.js'])),
                         ['external/wpt/console/console-is-a-namespace.any.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html'])
        self.assertEqual(port.tests(['external/wpt/console/console-is-a-namespace.any.html']),
                         ['external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(sorted(port.tests(['external/wpt/dom'])),
                         ['external/wpt/dom/ranges/Range-attributes-slow.html',
                          'external/wpt/dom/ranges/Range-attributes.html'])
        self.assertEqual(sorted(port.tests(['external/wpt/dom/'])),
                         ['external/wpt/dom/ranges/Range-attributes-slow.html',
                          'external/wpt/dom/ranges/Range-attributes.html'])
        self.assertEqual(port.tests(['external/wpt/dom/ranges/Range-attributes.html']),
                         ['external/wpt/dom/ranges/Range-attributes.html'])

        # wpt_internal should work the same.
        self.assertEqual(port.tests(['wpt_internal']), ['wpt_internal/dom/bar.html'])

    def test_virtual_wpt_tests_paths(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)
        all_wpt = [
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html',
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html',
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes-slow.html',
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html',
            'virtual/virtual_wpt/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
            'virtual/virtual_wpt/external/wpt/html/parse.html?run_type=uri',
            'virtual/virtual_wpt/external/wpt/html/parse.html?run_type=write',
        ]
        dom_wpt = [
            'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes-slow.html',
            'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html',
        ]

        self.assertEqual(sorted(port.tests(['virtual/virtual_wpt/external/'])), all_wpt)
        self.assertEqual(sorted(port.tests(['virtual/virtual_wpt/external/wpt/'])), all_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt/external/wpt/console']),
                         ['virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html',
                          'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html'])

        self.assertEqual(sorted(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/'])), dom_wpt)
        self.assertEqual(sorted(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/ranges/'])), dom_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html']),
                         ['virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html'])

        # wpt_internal should work the same.
        self.assertEqual(port.tests(['virtual/virtual_wpt_dom/wpt_internal']),
                         ['virtual/virtual_wpt_dom/wpt_internal/dom/bar.html'])
        self.assertEqual(sorted(port.tests(['virtual/virtual_wpt_dom/'])),
                         dom_wpt + ['virtual/virtual_wpt_dom/wpt_internal/dom/bar.html'])

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
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected-mismatch.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected-mismatch.svg'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-expected-mismatch.xhtml'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-ref.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-notref.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-notref.xht'))
        self.assertFalse(port.is_non_wpt_test_file('', 'foo-ref.xhtml'))
        self.assertFalse(port.is_non_wpt_test_file('', 'ref-foo.html'))
        self.assertFalse(port.is_non_wpt_test_file('', 'notref-foo.xhr'))

        self.assertFalse(port.is_non_wpt_test_file(WEB_TEST_DIR + '/external/wpt/common', 'blank.html'))
        self.assertFalse(port.is_non_wpt_test_file(WEB_TEST_DIR + '/external/wpt/console', 'console-is-a-namespace.any.js'))
        self.assertFalse(port.is_non_wpt_test_file(WEB_TEST_DIR + '/external/wpt', 'testharness_runner.html'))
        self.assertTrue(port.is_non_wpt_test_file(WEB_TEST_DIR + '/external/wpt_automation', 'foo.html'))
        self.assertFalse(port.is_non_wpt_test_file(WEB_TEST_DIR + '/wpt_internal/console', 'console-is-a-namespace.any.js'))

    def test_is_wpt_test(self):
        self.assertTrue(Port.is_wpt_test('external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(Port.is_wpt_test('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'))
        self.assertFalse(Port.is_wpt_test('dom/domparsing/namespaces-1.html'))
        self.assertFalse(Port.is_wpt_test('rutabaga'))

        self.assertTrue(Port.is_wpt_test('virtual/a-name/external/wpt/baz/qux.htm'))
        self.assertFalse(Port.is_wpt_test('virtual/external/wpt/baz/qux.htm'))
        self.assertFalse(Port.is_wpt_test('not-virtual/a-name/external/wpt/baz/qux.htm'))

    def test_should_use_wptserve(self):
        self.assertTrue(Port.should_use_wptserve('external/wpt/dom/interfaces.html'))
        self.assertTrue(Port.should_use_wptserve('virtual/a-name/external/wpt/dom/interfaces.html'))
        self.assertFalse(Port.should_use_wptserve('harness-tests/wpt/console_logging.html'))
        self.assertFalse(Port.should_use_wptserve('dom/domparsing/namespaces-1.html'))

    def test_is_slow_wpt_test(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)

        self.assertFalse(port.is_slow_wpt_test('external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/dom/ranges/Range-attributes-slow.html'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'))

    def test_is_slow_wpt_test_with_variations(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)

        self.assertFalse(port.is_slow_wpt_test('external/wpt/console/console-is-a-namespace.any.html'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/console/console-is-a-namespace.any.worker.html'))
        self.assertFalse(port.is_slow_wpt_test('external/wpt/html/parse.html?run_type=uri'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/html/parse.html?run_type=write'))

    def test_is_slow_wpt_test_takes_virtual_tests(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)

        self.assertFalse(port.is_slow_wpt_test('virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(port.is_slow_wpt_test('virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes-slow.html'))

    def test_is_slow_wpt_test_returns_false_for_illegal_paths(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)

        self.assertFalse(port.is_slow_wpt_test('dom/ranges/Range-attributes.html'))
        self.assertFalse(port.is_slow_wpt_test('dom/ranges/Range-attributes-slow.html'))
        self.assertFalse(port.is_slow_wpt_test('/dom/ranges/Range-attributes.html'))
        self.assertFalse(port.is_slow_wpt_test('/dom/ranges/Range-attributes-slow.html'))

    def test_reference_files(self):
        port = self.make_port(with_tests=True)
        self.assertEqual(port.reference_files('passes/svgreftest.svg'),
                         [('==', port.web_tests_dir() + '/passes/svgreftest-expected.svg')])
        self.assertEqual(port.reference_files('passes/xhtreftest.svg'),
                         [('==', port.web_tests_dir() + '/passes/xhtreftest-expected.html')])
        self.assertEqual(port.reference_files('passes/phpreftest.php'),
                         [('!=', port.web_tests_dir() + '/passes/phpreftest-expected-mismatch.svg')])

    def test_reference_files_from_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port)

        self.assertEqual(port.reference_files('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'),
                         [('==', port.web_tests_dir() +
                           '/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html')])
        self.assertEqual(port.reference_files('virtual/layout_ng/' +
                                              'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'),
                         [('==', port.web_tests_dir() +
                           '/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html')])

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())

    def test_http_server_supports_ipv6(self):
        port = self.make_port()
        self.assertTrue(port.http_server_supports_ipv6())
        port.host.platform.os_name = 'win'
        self.assertFalse(port.http_server_supports_ipv6())

    def test_http_server_requires_http_protocol_options_unsafe(self):
        port = self.make_port(executive=MockExecutive(stderr=(
            "Invalid command 'INTENTIONAL_SYNTAX_ERROR', perhaps misspelled or"
            " defined by a module not included in the server configuration\n")))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        self.assertTrue(
            port.http_server_requires_http_protocol_options_unsafe())

    def test_http_server_doesnt_require_http_protocol_options_unsafe(self):
        port = self.make_port(executive=MockExecutive(stderr=(
            "Invalid command 'HttpProtocolOptions', perhaps misspelled or"
            " defined by a module not included in the server configuration\n")))
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
        self.assertTrue(port.test_exists('virtual/virtual_passes/passes/text.html'))

        self.assertTrue(port.test_exists('virtual/virtual_empty_bases/physical1.html'))
        self.assertTrue(port.test_exists('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(port.test_exists('virtual/virtual_empty_bases/does_not_exist.html'))

    def test_test_isfile(self):
        port = self.make_port(with_tests=True)
        self.assertFalse(port.test_isfile('passes'))
        self.assertTrue(port.test_isfile('passes/text.html'))
        self.assertFalse(port.test_isfile('passes/does_not_exist.html'))

        self.assertFalse(port.test_isfile('virtual'))
        self.assertTrue(port.test_isfile('virtual/virtual_passes/passes/text.html'))
        self.assertFalse(port.test_isfile('virtual/does_not_exist.html'))

        self.assertTrue(port.test_isfile('virtual/virtual_empty_bases/physical1.html'))
        self.assertTrue(port.test_isfile('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(port.test_exists('virtual/virtual_empty_bases/does_not_exist.html'))

    def test_test_isdir(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_isdir('passes'))
        self.assertFalse(port.test_isdir('passes/text.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist/'))

        self.assertTrue(port.test_isdir('virtual'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist.html'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist/'))
        self.assertFalse(port.test_isdir('virtual/virtual_passes/passes/text.html'))

        self.assertTrue(port.test_isdir('virtual/virtual_empty_bases/'))
        self.assertTrue(port.test_isdir('virtual/virtual_empty_bases/dir'))
        self.assertFalse(port.test_isdir('virtual/virtual_empty_bases/dir/physical2.html'))
        self.assertFalse(port.test_isdir('virtual/virtual_empty_bases/does_not_exist/'))

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
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html', tests)
        self.assertIn('virtual/virtual_passes/passes_two/test-virtual-passes.html', tests)

        tests = port.tests(['virtual/virtual_passes/'])
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html', tests)
        self.assertIn('virtual/virtual_passes/passes_two/test-virtual-passes.html', tests)

        tests = port.tests(['virtual/virtual_passes/passes'])
        self.assertNotIn('passes/text.html', tests)
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes_two/test-virtual-passes.html', tests)
        self.assertNotIn('passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/test-virtual-virtual/passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/virtual_passes/passes/test-virtual-passes.html', tests)

        tests = port.tests(['virtual/virtual_passes/passes/test-virtual-passes.html'])
        self.assertEquals(['virtual/virtual_passes/passes/test-virtual-passes.html'], tests)

        tests = port.tests(['virtual/virtual_empty_bases'])
        self.assertEquals(['virtual/virtual_empty_bases/physical1.html',
                           'virtual/virtual_empty_bases/dir/physical2.html'], tests)

        tests = port.tests(['virtual/virtual_empty_bases/dir'])
        self.assertEquals(['virtual/virtual_empty_bases/dir/physical2.html'], tests)

        tests = port.tests(['virtual/virtual_empty_bases/dir/physical2.html'])
        self.assertEquals(['virtual/virtual_empty_bases/dir/physical2.html'], tests)

    def test_build_path(self):
        # Test for a protected method - pylint: disable=protected-access
        # Test that optional paths are used regardless of whether they exist.
        options = optparse.Values({'configuration': 'Release', 'build_directory': 'xcodebuild'})
        self.assertEqual(self.make_port(options=options)._build_path(), '/mock-checkout/xcodebuild/Release')

        # Test that "out" is used as the default.
        options = optparse.Values({'configuration': 'Release', 'build_directory': None})
        self.assertEqual(self.make_port(options=options)._build_path(), '/mock-checkout/out/Release')

    def test_dont_require_http_server(self):
        port = self.make_port()
        self.assertEqual(port.requires_http_server(), False)

    def test_can_load_actual_virtual_test_suite_file(self):
        port = Port(SystemHost(), 'baseport')

        # If this call returns successfully, we found and loaded the web_tests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_good_virtual_test_suite_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'VirtualTestSuites'),
            '[{"prefix": "bar", "bases": ["fast/bar"], "args": ["--bar"]}]')

        # If this call returns successfully, we found and loaded the web_tests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_duplicate_virtual_prefix_in_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'VirtualTestSuites'),
            '['
            '{"prefix": "bar", "bases": ["fast/bar"], "args": ["--bar"]},'
            '{"prefix": "bar", "bases": ["fast/foo"], "args": ["--bar"]}'
            ']')

        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_virtual_test_suite_file_is_not_json(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(), 'VirtualTestSuites'),
            '{[{[')
        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_lookup_virtual_test_base(self):
        port = self.make_port(with_tests=True)
        self.assertIsNone(port.lookup_virtual_test_base('non/virtual'))
        self.assertIsNone(port.lookup_virtual_test_base('passes/text.html'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/non-existing/test.html'))

        # lookup_virtual_test_base() checks virtual prefix and bases, but doesn't
        # check existence of test.
        self.assertEqual('passes/text.html', port.lookup_virtual_test_base('virtual/virtual_passes/passes/text.html'))
        self.assertEqual('passes/any.html', port.lookup_virtual_test_base('virtual/virtual_passes/passes/any.html'))
        self.assertEqual('passes_two/any.html', port.lookup_virtual_test_base('virtual/virtual_passes/passes_two/any.html'))
        self.assertEqual('passes/', port.lookup_virtual_test_base('virtual/virtual_passes/passes/'))
        self.assertEqual('passes/', port.lookup_virtual_test_base('virtual/virtual_passes/passes'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_passes/'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_passes'))
        # 'failures' is not a specified base of virtual/virtual_passes
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_passes/failures/unexpected/text.html'))
        self.assertEqual('failures/unexpected/text.html', port.lookup_virtual_test_base('virtual/virtual_failures/failures/unexpected/text.html'))
        # 'failures/expected' is not a specified base of virtual/virtual_failures
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_failures/failures/expected/image.html'))

        # Partial match of base with multiple levels.
        self.assertEqual('failures/', port.lookup_virtual_test_base('virtual/virtual_failures/failures/'))
        self.assertEqual('failures/', port.lookup_virtual_test_base('virtual/virtual_failures/failures'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_failures/'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_failures'))

        # Empty bases.
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_empty_bases/physical1.html'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_empty_bases/passes/text.html'))
        self.assertIsNone(port.lookup_virtual_test_base('virtual/virtual_empty_bases'))

    def test_args_for_test(self):
        port = self.make_port(with_tests=True)
        self.assertEqual([], port.args_for_test('non/virtual'))
        self.assertEqual([], port.args_for_test('passes/text.html'))
        self.assertEqual([], port.args_for_test('virtual/non-existing/test.html'))

        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes/passes/text.html'))
        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes/passes/any.html'))
        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes/passes/'))
        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes/passes'))
        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes/'))
        self.assertEqual(['--virtual-arg'], port.args_for_test('virtual/virtual_passes'))

    def test_missing_virtual_test_suite_file(self):
        port = self.make_port()
        self.assertRaises(AssertionError, port.virtual_test_suites)

    def test_default_results_directory(self):
        port = self.make_port(options=optparse.Values({'target': 'Default', 'configuration': 'Release'}))
        # By default the results directory is in the build directory: out/<target>.
        self.assertEqual(port.default_results_directory(), '/mock-checkout/out/Default/layout-test-results')

    def test_results_directory(self):
        port = self.make_port(options=optparse.Values({'results_directory': 'some-directory/results'}))
        # A results directory can be given as an option, and it is relative to current working directory.
        self.assertEqual(port.host.filesystem.cwd, '/')
        self.assertEqual(port.results_directory(), '/some-directory/results')

    def _assert_config_file_for_platform(self, port, platform, config_file):
        port.host.platform = MockPlatformInfo(os_name=platform)
        self.assertEqual(port._apache_config_file_name_for_platform(), config_file)  # pylint: disable=protected-access

    def _assert_config_file_for_linux_distribution(self, port, distribution, config_file):
        port.host.platform = MockPlatformInfo(os_name='linux', linux_distribution=distribution)
        self.assertEqual(port._apache_config_file_name_for_platform(), config_file)  # pylint: disable=protected-access

    def test_apache_config_file_name_for_platform(self):
        port = self.make_port()
        port._apache_version = lambda: '2.2'  # pylint: disable=protected-access
        self._assert_config_file_for_platform(port, 'linux', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'arch', 'arch-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'debian', 'debian-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'fedora', 'fedora-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'slackware', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'redhat', 'redhat-httpd-2.2.conf')

        self._assert_config_file_for_platform(port, 'mac', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_platform(port, 'win32', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_platform(port, 'barf', 'apache2-httpd-2.2.conf')

    def test_skips_test_in_smoke_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: True
        port.host.filesystem.write_text_file(
            port.path_to_smoke_tests_file(),
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
            'Bug(test) failures/expected/image.html [ WontFix ]\n')
        self.assertTrue(port.skips_test('failures/expected/image.html'))

    def test_split_webdriver_test_name(self):
        self.assertEqual(
            Port.split_webdriver_test_name("tests/accept_alert/accept.py>>foo"),
            ("tests/accept_alert/accept.py", "foo"))
        self.assertEqual(
            Port.split_webdriver_test_name("tests/accept_alert/accept.py"),
            ("tests/accept_alert/accept.py", None))

    def test_add_webdriver_subtest_suffix(self):
        self.assertEqual(
            Port.add_webdriver_subtest_suffix("abd", "bar"), "abd>>bar")
        self.assertEqual(
            Port.add_webdriver_subtest_suffix("abd", None), "abd")

    def test_add_webdriver_subtest_pytest_suffix(self):
        port = self.make_port()
        wb_test_name = "abd"
        sub_test_name = "bar"

        full_webdriver_name = port.add_webdriver_subtest_pytest_suffix(
            wb_test_name, sub_test_name)

        self.assertEqual(full_webdriver_name, "abd::bar")


class NaturalCompareTest(unittest.TestCase):

    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        # pylint: disable=protected-access
        self.assertEqual(cmp(self._port._natural_sort_key(x), self._port._natural_sort_key(y)), result)

    def test_natural_compare(self):
        self.assert_cmp('a', 'a', 0)
        self.assert_cmp('ab', 'a', 1)
        self.assert_cmp('a', 'ab', -1)
        self.assert_cmp('', '', 0)
        self.assert_cmp('', 'ab', -1)
        self.assert_cmp('1', '2', -1)
        self.assert_cmp('2', '1', 1)
        self.assert_cmp('1', '10', -1)
        self.assert_cmp('2', '10', -1)
        self.assert_cmp('foo_1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.html', 'foo_10.html', -1)
        self.assert_cmp('foo_2.html', 'foo_10.html', -1)
        self.assert_cmp('foo_23.html', 'foo_10.html', 1)
        self.assert_cmp('foo_23.html', 'foo_100.html', -1)


class KeyCompareTest(unittest.TestCase):

    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        self.assertEqual(cmp(self._port.test_key(x), self._port.test_key(y)), result)

    def test_test_key(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)


class VirtualTestSuiteTest(unittest.TestCase):

    def test_basic(self):
        suite = VirtualTestSuite(prefix='suite', bases=['base/foo', 'base/bar'], args=['--args'])
        self.assertEqual(suite.full_prefix, 'virtual/suite/')
        self.assertEqual(suite.bases, ['base/foo', 'base/bar'])
        self.assertEqual(suite.args, ['--args'])

    def test_empty_bases(self):
        suite = VirtualTestSuite(prefix='suite', bases=[], args=['--args'])
        self.assertEqual(suite.full_prefix, 'virtual/suite/')
        self.assertEqual(suite.bases, [])
        self.assertEqual(suite.args, ['--args'])

    def test_no_slash(self):
        self.assertRaises(AssertionError, VirtualTestSuite, prefix='suite/bar', bases=['base/foo'], args=['--args'])
