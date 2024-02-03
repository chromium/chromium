# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import textwrap

from blinkpy.common.path_finder import PathFinder
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

    def _exists(self, port, path: str):
        abs_path = self.tool.filesystem.join(port.web_tests_dir(), path)
        return self.tool.filesystem.exists(abs_path)

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
                'test_name_file': None,
            }), ['another/test.html'], self.tool)

        self.assertEqual(exit_code or 0, 0)
        self.assertFalse(
            self._exists(
                test_port,
                'platform/test-mac-mac10.10/another/test-expected.txt'))
        self.assertFalse(
            self._exists(
                test_port,
                'platform/test-mac-mac10.10/another/test-expected.png'))
        self.assertTrue(self._exists(test_port, 'another/test-expected.txt'))
        self.assertTrue(self._exists(test_port, 'another/test-expected.png'))

    def test_execute_with_test_name_file(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'optimized.html', 'Dummy contents')
        self._write_test_file(test_port, 'skipped.html', 'Dummy contents')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/optimized-expected.txt',
            'result A')
        self._write_test_file(
            test_port, 'platform/test-mac-mac10.10/skipped-expected.txt',
            'result A')
        self._write_test_file(test_port, 'optimized-expected.txt', 'result A')
        self._write_test_file(test_port, 'skipped-expected.txt', 'result A')
        test_name_file = self.tool.filesystem.mktemp()
        self.tool.filesystem.write_text_file(
            test_name_file,
            textwrap.dedent("""\
                optimized.html
                # This is allowed but will log a warning.
                does-not-exist.html
                """))
        exit_code = self.command.execute(
            optparse.Values({
                'suffixes': 'txt',
                'all_tests': False,
                'platform': None,
                'check': False,
                'verbose': False,
                'test_name_file': test_name_file,
            }), [], self.tool)

        self.assertEqual(exit_code or 0, 0)
        self.assertIn(
            "WARNING: 'does-not-exist.html' does not represent any tests "
            'and may be misspelled.\n', self.logMessages())
        self.assertFalse(
            self._exists(test_port,
                         'platform/test-mac-mac10.10/optimized-expected.txt'))
        self.assertTrue(
            self._exists(test_port,
                         'platform/test-mac-mac10.10/skipped-expected.txt'))
        self.assertTrue(self._exists(test_port, 'optimized-expected.txt'))
        self.assertTrue(self._exists(test_port, 'skipped-expected.txt'))

    def test_execute_filter_suffixes_for_valid_wpt_types(self):
        """Skip optimization for invalid test type-suffix combinations."""
        finder = PathFinder(self.tool.filesystem)
        self.tool.filesystem.write_text_file(
            finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'url_base': '/',
                'items': {
                    'manual': {
                        'test-manual.html': ['abcdef', [None, {}]],
                    },
                    'testharness': {
                        'testharness.html': ['abcdef', [None, {}]],
                    },
                    'reftest': {
                        'reftest.html': ['abcdef', [None, [], {}]],
                    },
                    'print-reftest': {
                        'print-reftest.html': ['abcdef', [None, [], {}]],
                    },
                    'crashtest': {
                        'test-crash.html': ['abcdef', [None, {}]],
                    },
                },
            }))
        self.tool.filesystem.write_text_file(
            finder.path_from_web_tests('wpt_internal', 'MANIFEST.json'),
            json.dumps({}))

        exit_code = self.command.check_arguments_and_execute(
            optparse.Values({
                'suffixes': 'txt,wav,png',
                'all_tests': False,
                'platform': 'test-mac-mac10.10',
                'check': True,
                'test_name_file': None,
                'manifest_update': False,
                'verbose': False,
            }), ['external/wpt'], self.tool)
        self.assertLog([
            'INFO: Checking external/wpt/test-manual.html (png)\n',
            'INFO: Checking external/wpt/testharness.html (txt)\n',
            'INFO: All baselines are optimal.\n',
        ])

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
                'test_name_file': None,
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
                'test_name_file': None,
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
