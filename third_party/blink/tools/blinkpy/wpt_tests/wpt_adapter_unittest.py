# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import os
import textwrap
import unittest
from datetime import datetime
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.base import VirtualTestSuite
from blinkpy.wpt_tests.wpt_adapter import WPTAdapter


@mock.patch('blinkpy.wpt_tests.wpt_adapter.WPTAdapter.using_upstream_wpt',
            False)
class WPTAdapterTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.host.builders = BuilderList({
            'linux-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Linux', 'Release'],
                'steps': {
                    'blink_wpt_tests': {},
                    'fake_flag_blink_wpt_tests': {
                        'flag_specific': 'fake-flag',
                    },
                },
            },
            'linux-wpt-chrome-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Linux', 'Release'],
                'steps': {
                    'blink_wpt_tests': {
                        'product': 'chrome',
                    },
                },
            },
            'mac-rel': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
        })

        self.fs = self.host.filesystem
        self.finder = PathFinder(self.fs)
        self.fs.write_text_file(
            self.finder.path_from_web_tests('FlagSpecificConfig'),
            json.dumps([{
                'name': 'fake-flag',
                'args': ['--enable-features=FakeFeature'],
                'smoke_file': 'TestLists/fake-flag',
            }]))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'fake-flag'),
            textwrap.dedent("""\
                # The non-WPT test should be excluded.
                external/wpt/dir/
                not/a/wpt.html
                wpt_internal/variant.html
                """))
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
                'url_base': '/wpt_internal/',
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
        self.fs.write_text_file(
            self.finder.path_from_web_tests('VirtualTestSuites'),
            json.dumps([]))

        self._mocks = contextlib.ExitStack()
        self._mocks.enter_context(self.fs.patch_builtins())
        vts1 = VirtualTestSuite(prefix='fake-vts-1',
                                platforms=['Linux', 'Mac'],
                                bases=['wpt_internal/variant.html?xyz'],
                                args=['--enable-features=FakeFeature'])
        vts2 = VirtualTestSuite(prefix='fake-vts-2',
                                platforms=['Linux'],
                                bases=['wpt_internal/'],
                                args=['--enable-features=FakeFeature'])
        self._mocks.enter_context(
            mock.patch(
                'blinkpy.web_tests.port.test.TestPort.virtual_test_suites',
                return_value=[vts1, vts2]))

        self.output_stream = io.StringIO()
        stream_mock = mock.Mock(wraps=self.output_stream)
        stream_mock.isatty.return_value = False
        stream_mock.fileno.return_value = 1
        self._mocks.enter_context(mock.patch('sys.stdout', stream_mock))

        self.results_processor = mock.Mock()
        self.results_processor.stream_results.return_value = (
            contextlib.nullcontext(mock.Mock()))
        self.results_processor.num_initial_failures = 0
        self._mocks.enter_context(
            mock.patch('blinkpy.wpt_tests.wpt_adapter.WPTResultsProcessor',
                       return_value=self.results_processor))

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
            'headless_shell',
            '-j',
            '5',
            '--iterations=7',
            '--repeat-each=9',
            '--num-retries=11',
            '--zero-tests-executed-ok',
            '--timeout-multiplier=2.5',
            '--fully-parallel',
            '--no-manifest-update',
            'external/wpt/dir/',
        ]
        adapter = WPTAdapter.from_args(self.host, args, 'test-linux-trusty')
        with adapter.test_env() as options:
            # `run_wpt_tests.py` treats `chrome` and `headless_shell` as
            # distinct products, but `wpt run` does not.
            self.assertEqual(
                options.product, 'headless_shell',
                'adapter did not coerce `headless_shell` to `chrome`')
            self.assertEqual(options.processes, 5)
            self.assertEqual(options.repeat, 7)
            self.assertEqual(options.rerun, 9)
            self.assertEqual(options.retry_unexpected, 11)
            self.assertEqual(options.default_exclude, True)
            self.assertEqual(set(options.exclude), set())
            self.assertAlmostEqual(options.timeout_multiplier, 2.5)
            self.assertTrue(options.fully_parallel)
            self.assertIsNot(options.run_by_dir, 0)
            self.assertEqual(options.include, ['dir/reftest.html'])
            self.assertNotIn('--run-web-tests', options.binary_args)
            self.assertIn('--enable-blink-test-features', options.binary_args)
            self.assertTrue(options.enable_experimental)
            ignore_cert_flags = [
                flag for flag in options.binary_args
                if flag.startswith('--ignore-certificate-errors-spki-list=')
            ]
            self.assertEqual(len(ignore_cert_flags), 1)

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
                00:00:01.000 INFO: Running tests for headless_shell
                00:00:02.000 INFO: Using port "test-linux-trusty"
                00:00:03.000 INFO: View the test results at file:///tmp/layout-test-results/results.html
                00:00:04.000 INFO: Using Debug build
                """))

    @mock.patch('blinkpy.web_tests.port.test.TestPort.default_child_processes',
                return_value=8)
    def test_wrapper_option(self, _):
        args = [
            '--product=headless_shell',
            '--no-manifest-update',
            '--wrapper=rr record --disable-avx-512',
            'external/wpt/dir/',
        ]
        adapter = WPTAdapter.from_args(self.host, args, 'test-linux-trusty')
        with adapter.test_env() as options:
            self.assertEqual(options.debugger, 'rr')
            self.assertEqual(options.debugger_args, 'record --disable-avx-512')
            self.assertEqual(options.processes, 1)

    def test_scratch_directory_cleanup(self):
        """Only test results should be left behind, even with an exception."""
        adapter = WPTAdapter.from_args(
            self.host, ['--product=headless_shell', '--no-manifest-update'])
        files_before = dict(self.fs.files)
        with self.assertRaises(KeyboardInterrupt):
            with adapter.test_env() as options:
                raise KeyboardInterrupt
        # Remove deleted temporary files (represented with null contents).
        files = {
            path: contents
            for path, contents in self.fs.files.items() if contents is not None
        }
        self.assertEqual(files, files_before)

    def test_parse_isolated_filter_nonexistent_tests(self):
        """Check that no tests run if all tests in the filter do not exist.

        This often occurs when the build retries the suite without a patch that
        adds new failing tests.
        """
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell',
            '--zero-tests-executed-ok',
            '--isolated-script-test-filter',
            'does-not-exist.any.html::does-not-exist.any.worker.html',
        ])
        with adapter.test_env() as options:
            self.assertEqual(options.include, [])
            self.assertTrue(options.default_exclude)

    def test_run_all_with_zero_tests_executed_ok(self):
        # `--zero-tests-executed-ok` without explicit tests should still run the
        # entire suite. This matches the `run_web_tests.py` behavior.
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell', '--zero-tests-executed-ok',
            '--no-manifest-update'
        ])
        with adapter.test_env() as options:
            self.assertEqual(sorted(options.include), ([
                'dir/reftest.html', 'wpt_internal/variant.html?foo=bar/abc',
                'wpt_internal/variant.html?foo=baz',
                'wpt_internal/variant.html?xyz'
            ]))
            self.assertTrue(options.default_exclude)

    def test_binary_args_propagation(self):
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell',
            '--no-manifest-update',
            '--additional-driver-flag=--enable-features=FakeFeature',
            '--additional-driver-flag=--remote-debugging-address=0.0.0.0:8080',
        ])
        with adapter.test_env() as options:
            self.assertLessEqual(
                {
                    '--enable-features=FakeFeature',
                    '--remote-debugging-address=0.0.0.0:8080',
                }, set(options.binary_args))

    def test_flag_specific(self):
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell', '--flag-specific=fake-flag',
            '--no-manifest-update'
        ])
        with adapter.test_env() as options:
            self.assertIn('--enable-features=FakeFeature', options.binary_args)
            self.assertEqual(sorted(options.include), ([
                'dir/reftest.html', 'wpt_internal/variant.html?foo=bar/abc',
                'wpt_internal/variant.html?foo=baz',
                'wpt_internal/variant.html?xyz'
            ]))
            run_info = self._read_run_info(options)
            self.assertEqual(run_info['flag_specific'], 'fake-flag')

    @unittest.skip("unskipping this when enable virtual tests")
    def test_run_virtual_tests(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('VirtualTestSuites'),
            json.dumps([{
                "prefix":
                "fake",
                "platforms": ["Linux", "Mac", "Win"],
                "bases": [
                    "external/wpt/dir/reftest.html",
                    "wpt_internal/variant.html?xyz",
                ],
                "args": ["--features=a"],
                "owners": ["x@google.com"],
                "expires":
                "Jan 1, 2024"
            }]))
        adapter = WPTAdapter.from_args(self.host, ['--no-manifest-update'])
        with adapter.test_env() as options:
            self.assertEqual(sorted(options.include), ([
                'dir/reftest.html', 'wpt_internal/variant.html?foo=bar/abc',
                'wpt_internal/variant.html?foo=baz',
                'wpt_internal/variant.html?xyz'
            ]))
            self.assertEqual(options.subsuites, ['fake'])
            with open(options.subsuite_file) as fp:
                subsuite_config = json.load(fp)
                self.assertEqual(len(subsuite_config), 1)
                self.assertIn('fake', subsuite_config)
                self.assertEqual(subsuite_config['fake']['name'], 'fake')
                self.assertEqual(subsuite_config['fake']['config'],
                                 {'binary_args': ['--features=a']})
                self.assertEqual(subsuite_config['fake']['run_info'],
                                 {'virtual_suite': 'fake'})
                self.assertEqual(
                    sorted(subsuite_config['fake']['include']),
                    ['dir/reftest.html', 'wpt_internal/variant.html?xyz'])

        adapter = WPTAdapter.from_args(self.host, [
            '--no-manifest-update',
            'virtual/fake/external/wpt/dir/reftest.html'
        ])
        with adapter.test_env() as options:
            self.assertEqual(sorted(options.include), [])
            self.assertEqual(options.subsuites, ['fake'])
            with open(options.subsuite_file) as fp:
                self.assertEqual(subsuite_config['fake']['include'],
                                 ['dir/reftest.html'])

    def test_sanitizer_enabled(self):
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell', '--no-manifest-update',
            '--enable-sanitizer'
        ])
        with adapter.test_env() as options:
            self.assertAlmostEqual(options.timeout_multiplier, 5)
            run_info = self._read_run_info(options)
            self.assertTrue(run_info['sanitizer_enabled'])

    def test_retry_unexpected(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'Default.txt'),
            textwrap.dedent("""\
                # The non-WPT test should be excluded.
                external/wpt/dir/reftest.html
                """))
        adapter = WPTAdapter.from_args(
            self.host, ['--product=headless_shell', '--no-manifest-update'])
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 3)

        # TODO We should not retry failures when running with '--use-upstream-wpt'
        # Consider add a unit test for that

        # Create default mock smoke test file
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'MacOld.txt'), "")
        adapter = WPTAdapter.from_args(
            self.host,
            ['--product=headless_shell', '--no-manifest-update', '--smoke'])
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 3)

        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell', '--no-manifest-update',
            'external/wpt/dir/reftest.html'
        ])
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 0)

    def test_env_var(self):
        adapter = WPTAdapter.from_args(self.host, [
            '--product=headless_shell', '--no-manifest-update',
            '--additional-env-var=NEW_ENV_VAR=new_env_var_value'
        ])
        with adapter.test_env():
            self.assertEqual(os.environ['NEW_ENV_VAR'], 'new_env_var_value')

    def test_show_results(self):
        adapter = WPTAdapter.from_args(
            self.host, ['--product=headless_shell', '--no-manifest-update'])
        post_run_tasks = mock.Mock()
        self._mocks.enter_context(
            mock.patch('blinkpy.web_tests.port.base.Port.clean_up_test_run',
                       post_run_tasks.clean_up_test_run))
        self._mocks.enter_context(
            mock.patch.object(self.host.user, 'open_url',
                              post_run_tasks.open_url))
        with adapter.test_env() as options:
            self.results_processor.num_initial_failures = 1
        self.assertEqual(post_run_tasks.mock_calls, [
            mock.call.clean_up_test_run(),
            mock.call.open_url('file:///mock-checkout/out/Release/'
                               'layout-test-results/results.html'),
        ])

    def test_font_config(self):
        adapter = WPTAdapter.from_args(
            self.host, ['--product=headless_shell', '--no-manifest-update'])
        with adapter.test_env() as options:
            config_path = self.fs.join(self.host.environ['XDG_CONFIG_HOME'],
                                       'fontconfig', 'fonts.conf')
            self.assertTrue(self.fs.exists(config_path))
