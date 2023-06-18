# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import textwrap
import unittest
from datetime import datetime
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.wpt_tests.wpt_adapter import WPTAdapter


class WPTAdapterTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.fs = self.host.filesystem
        self.finder = PathFinder(self.fs)
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'reftest': {
                        'dir': {
                            'reftest.html': [
                                'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                                [None, [['reftest-ref.html', '==']], {}],
                            ],
                        },
                    },
                },
            }))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('wpt_internal', 'MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'testharness': {
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                            ['variant.html?xyz', {}],
                        ],
                    },
                },
            }))
        self._mocks = contextlib.ExitStack()
        self._mocks.enter_context(self.fs.patch_builtins())
        self.output_stream = io.StringIO()
        stream_mock = mock.Mock(wraps=self.output_stream)
        stream_mock.isatty.return_value = False
        stream_mock.fileno.return_value = 1
        self._mocks.enter_context(mock.patch('sys.stdout', stream_mock))

    def tearDown(self):
        self._mocks.close()

    def _read_run_info(self, options):
        run_info_path = self.fs.join(options.run_info, 'mozinfo.json')
        return json.loads(self.fs.read_text_file(run_info_path))

    def test_basic_passthrough(self):
        mock_datetime = self._mocks.enter_context(
            mock.patch('blinkpy.wpt_tests.wpt_adapter.datetime'))
        mock_datetime.now.side_effect = lambda: datetime(
            2023, 1, 1, 12, 0, mock_datetime.now.call_count)

        args = [
            '-t',
            'Debug',
            '-p',
            'chrome',
            '-j',
            '5',
            '--iterations=7',
            '--repeat-each=9',
            '--num-retries=11',
            '--zero-tests-executed-ok',
            '--ignore-tests=wpt_internal/variant.html',
            'dir/',
        ]
        adapter = WPTAdapter.from_args(self.host, args, 'test-linux-trusty')
        with adapter.test_env() as options:
            self.assertEqual(options.product, 'chrome')
            self.assertEqual(options.processes, 5)
            self.assertEqual(options.repeat, 7)
            self.assertEqual(options.rerun, 9)
            self.assertEqual(options.retry_unexpected, 11)
            self.assertEqual(options.default_exclude, True)
            self.assertEqual(
                set(options.exclude),
                {
                    'wpt_internal/variant.html',
                    # `*webdriver/` tests are always implicitly excluded.
                    'webdriver',
                    'infrastructure/webdriver',
                })
            self.assertEqual(options.include, ['dir/'])

            run_info = self._read_run_info(options)
            self.assertEqual(run_info['os'], 'linux')
            self.assertEqual(run_info['port'], 'trusty')
            self.assertTrue(run_info['debug'])
            self.assertEqual(run_info['flag_specific'], '')
            self.assertFalse(run_info['used_upstream'])
            self.assertFalse(run_info['sanitizer_enabled'])

        self.assertEqual(
            self.output_stream.getvalue(),
            textwrap.dedent("""\
                00:00:01 INFO: Running tests for chrome
                00:00:02 INFO: Using port "test-linux-trusty"
                00:00:03 INFO: View the test results at file:///tmp/layout-test-results/results.html
                00:00:04 INFO: Using Debug build
                """))

    def test_scratch_directory_cleanup(self):
        """Only test results should be left behind, even with an exception."""
        adapter = WPTAdapter.from_args(self.host, [])
        files_before = dict(self.fs.files)
        with self.assertRaises(KeyboardInterrupt):
            with adapter.test_env() as options:
                raise KeyboardInterrupt
        # Remove deleted temporary files (represented with null contents).
        files = {
            path: contents
            for path, contents in self.fs.files.items() if contents is not None
        }
        files.pop('/mock-checkout/out/Release/wpt_reports.json')
        self.assertEqual(files, files_before)

    def test_binary_args_propagation(self):
        adapter = WPTAdapter.from_args(self.host, [
            '--enable-leak-detection',
            '--additional-driver-flag=--enable-features=FakeFeature',
            '--additional-driver-flag=--remote-debugging-address=0.0.0.0:8080',
        ])
        with adapter.test_env() as options:
            self.assertLessEqual(
                {
                    '--enable-leak-detection',
                    '--enable-features=FakeFeature',
                    '--remote-debugging-address=0.0.0.0:8080',
                }, set(options.binary_args))

    def test_flag_specific(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('SmokeTests', 'fake-flag'),
            textwrap.dedent("""\
                # The non-WPT test should be excluded.
                external/wpt/dir/
                not/a/wpt.html
                wpt_internal/variant.html
                """))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('FlagSpecificConfig'),
            json.dumps([{
                'name': 'fake-flag',
                'args': ['--enable-features=FakeFeature'],
                'smoke_file': 'SmokeTests/fake-flag',
            }]))
        adapter = WPTAdapter.from_args(self.host,
                                       ['--flag-specific=fake-flag'])
        with adapter.test_env() as options:
            self.assertIn('--enable-features=FakeFeature', options.binary_args)
            self.assertEqual(set(options.include), {
                '/dir/',
                '/wpt_internal/variant.html',
            })
            run_info = self._read_run_info(options)
            self.assertEqual(run_info['flag_specific'], 'fake-flag')

    def test_sanitizer_enabled(self):
        adapter = WPTAdapter.from_args(self.host, ['--enable-sanitizer'])
        with adapter.test_env() as options:
            self.assertEqual(options.timeout_multiplier, 2)
            run_info = self._read_run_info(options)
            self.assertTrue(run_info['sanitizer_enabled'])

    def test_sharding(self):
        self.host.environ['GTEST_SHARD_INDEX'] = 4
        self.host.environ['GTEST_TOTAL_SHARDS'] = 5
        adapter = WPTAdapter.from_args(self.host, [])
        with adapter.test_env() as options:
            # Convert from a 0-based index to 1-based.
            self.assertEqual(options.this_chunk, 5)
            self.assertEqual(options.total_chunks, 5)
