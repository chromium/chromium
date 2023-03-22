# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.commands.optimize_baselines import OptimizeBaselines
from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase


class TestOptimizeBaselines(BaseTestCase, LoggingTestCase):
    command_constructor = OptimizeBaselines

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.web_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def test_optimize_all_suffixes_by_default(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html',
                              'Dummy test contents')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/another/test-expected.txt',
            'result A')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/another/test-expected.png',
            'result A png')
        self._write_test_file(test_port, 'another/test-expected.txt',
                              'result A')
        self._write_test_file(test_port, 'another/test-expected.png',
                              'result A png')

        exit_code = self.command.execute(
            optparse.Values({
                'suffixes': 'txt,wav,png',
                'all_tests': False,
                'platform': 'test-mac-mac10.10',
                'check': False,
            }), ['another/test.html'], self.tool)

        self.assertEqual(exit_code or 0, 0)
        self.assertFalse(
            self.tool.filesystem.exists(
                self.tool.filesystem.join(
                    test_port.web_tests_dir(),
                    'platform/mac/another/test-expected.txt')))
        self.assertFalse(
            self.tool.filesystem.exists(
                self.tool.filesystem.join(
                    test_port.web_tests_dir(),
                    'platform/mac/another/test-expected.png')))
        self.assertTrue(
            self.tool.filesystem.exists(
                self.tool.filesystem.join(test_port.web_tests_dir(),
                                          'another/test-expected.txt')))
        self.assertTrue(
            self.tool.filesystem.exists(
                self.tool.filesystem.join(test_port.web_tests_dir(),
                                          'another/test-expected.png')))

    def test_check_optimal(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html',
                              'Dummy test contents')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/another/test-expected.txt',
            'result A')
        self._write_test_file(test_port, 'another/test-expected.txt',
                              'result B')
        fs_before = dict(self.tool.filesystem.files)

        exit_code = self.command.execute(
            optparse.Values({
                'suffixes': 'txt',
                'all_tests': False,
                'platform': 'test-mac-mac10.10',
                'check': True,
                'verbose': False,
            }), ['another/test.html'], self.tool)

        self.assertEqual(exit_code or 0, 0)
        self.assertEqual(fs_before, self.tool.filesystem.files)
        self.assertLog([
            'INFO: Checking another/test.html (txt)\n',
            'INFO: All baselines are optimal.\n',
        ])

    def test_check_nonoptimal(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html',
                              'Dummy test contents')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/another/test-expected.txt',
            'result A')
        self._write_test_file(test_port, 'another/test-expected.txt',
                              'result A')
        fs_before = dict(self.tool.filesystem.files)

        exit_code = self.command.execute(
            optparse.Values({
                'suffixes': 'txt',
                'all_tests': False,
                'platform': 'test-mac-mac10.10',
                'check': True,
                'verbose': False,
            }), ['another/test.html'], self.tool)

        self.assertNotEqual(exit_code or 0, 0)
        self.assertEqual(fs_before, self.tool.filesystem.files)
        self.assertLog([
            'INFO: Checking another/test.html (txt)\n',
            'INFO:   Can remove /mock-checkout/third_party/blink/web_tests/'
            'platform/test-mac-mac10.10/another/test-expected.txt '
            '(redundant)\n',
            'WARNING: Some baselines require further optimization.\n',
            'WARNING: Rerun `optimize-baselines` without `--check` to fix '
            'these issues.\n',
        ])
