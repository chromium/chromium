# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import textwrap
import queue
import unittest
from datetime import datetime
from unittest import mock

from blinkpy.common import exit_codes
from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.base import VirtualTestSuite
from blinkpy.wpt_tests.wpt_adapter import WPTAdapter


class WPTAdapterTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.host.builders = BuilderList(
            {
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
            }
        )

        self.fs = self.host.filesystem
        self.finder = PathFinder(self.fs)
        self.fs.write_text_file(
            self.finder.path_from_web_tests('FlagSpecificConfig'),
            json.dumps(
                [
                    {
                        'name': 'fake-flag',
                        'args': ['--enable-features=FakeFeature'],
                        'smoke_file': 'TestLists/fake-flag',
                    }
                ]
            ),
        )
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'fake-flag'),
            textwrap.dedent("""\
                # The non-WPT test should be excluded.
                external/wpt/dir/
                not/a/wpt.html
                wpt_internal/variant.html
                """),
        )
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('config.tmpl.json'),
            json.dumps({'aliases': []}),
        )
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps(
                {
                    'version': 9,
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
                }
            ),
        )
        self.fs.write_text_file(
            self.finder.path_from_web_tests('wpt_internal', 'MANIFEST.json'),
            json.dumps(
                {
                    'version': 9,
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
                }
            ),
        )

        self._mocks = contextlib.ExitStack()
        self._mocks.enter_context(self.fs.patch_builtins())
        vts1 = VirtualTestSuite(
            prefix='fake-vts-1',
            platforms=['Linux', 'Mac'],
            bases=['wpt_internal/variant.html?xyz'],
            args=['--enable-features=FeatureA'],
        )
        vts2 = VirtualTestSuite(
            prefix='fake-vts-2',
            platforms=['Linux'],
            bases=['external/wpt/dir/'],
            args=['--enable-features=FeatureB'],
        )
        self._mocks.enter_context(
            mock.patch(
                'blinkpy.web_tests.port.test.TestPort.virtual_test_suites',
                return_value=[vts1, vts2],
            )
        )

        self.output_stream = io.StringIO()
        stream_mock = mock.Mock(wraps=self.output_stream)
        stream_mock.isatty.return_value = False
        stream_mock.fileno.return_value = 1
        self._mocks.enter_context(mock.patch('sys.stdout', stream_mock))

        event_queue = queue.SimpleQueue()
        self._mocks.enter_context(
            mock.patch(
                'blinkpy.wpt_tests.wpt_adapter.WPTResultsProcessor.stream_results',
                return_value=contextlib.nullcontext(event_queue),
            )
        )
        self._mocks.enter_context(
            mock.patch('blinkpy.wpt_tests.wpt_adapter.run.run', return_value=1)
        )

    def tearDown(self):
        self._mocks.close()

    def _read_run_info(self, options):
        run_info_path = self.fs.join(options.run_info, 'mozinfo.json')
        return json.loads(self.fs.read_text_file(run_info_path))

    def _make_port_test_adapter(self):
        adapter = WPTAdapter.from_args(
            self.host, ['--no-manifest-update'], 'test-linux-trusty'
        )
        calls = mock.Mock()
        calls.setup.return_value = None
        calls.run.return_value = False
        calls.load_runner.return_value = calls.run
        for target, name, replacement in [
            (adapter.port, 'setup_test_run', calls.setup),
            (adapter.port, 'clean_up_test_run', calls.cleanup),
            (adapter.processor, 'copy_results_viewer', calls.copy_viewer),
            (adapter.processor, 'process_results_json', calls.process_results),
        ]:
            self._mocks.enter_context(
                mock.patch.object(target, name, replacement)
            )
        calls.process_results.return_value = {'num_regressions': 0}
        self._mocks.enter_context(
            mock.patch(
                'blinkpy.wpt_tests.wpt_adapter._load_entry_point',
                calls.load_runner,
            )
        )
        return adapter, calls

    def test_port_setup_failure_stops_runner(self):
        adapter, calls = self._make_port_test_adapter()
        calls.setup.return_value = exit_codes.SYS_DEPS_EXIT_STATUS

        self.assertEqual(adapter.run_tests(), exit_codes.SYS_DEPS_EXIT_STATUS)
        calls.load_runner.assert_not_called()
        calls.run.assert_not_called()
        calls.cleanup.assert_called_once_with()

    def test_port_setup_exception_cleans_up(self):
        adapter, calls = self._make_port_test_adapter()
        calls.setup.side_effect = RuntimeError('setup failed')

        with self.assertRaisesRegex(RuntimeError, 'setup failed'):
            adapter.run_tests()
        calls.cleanup.assert_called_once_with()
        calls.load_runner.assert_not_called()

    def test_port_setup_interrupt_cleans_up(self):
        adapter, calls = self._make_port_test_adapter()
        calls.setup.side_effect = KeyboardInterrupt

        self.assertEqual(
            adapter.run_tests(), exit_codes.INTERRUPTED_EXIT_STATUS
        )
        calls.cleanup.assert_called_once_with()
        calls.load_runner.assert_not_called()

    def test_simultaneous_run_fails_clearly(self):
        adapter, calls = self._make_port_test_adapter()

        with self.fs.lock_file('/tmp/WebKit/web_tests.lock'):
            with self.assertLogs('run_wpt_tests', level='ERROR') as log_ctx:
                exit_code = adapter.run_tests()

        self.assertEqual(exit_code, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
        self.assertIn(
            'ERROR:run_wpt_tests:'
            'Another instance of run_web_tests.py or run_wpt_tests.py is '
            'holding the OS lock (/tmp/WebKit/web_tests.lock). Simultaneous '
            'runs are not supported due to shared server ports; wait for the '
            'other run to finish and try again.',
            log_ctx.output,
        )
        calls.setup.assert_not_called()
        calls.load_runner.assert_not_called()
        calls.run.assert_not_called()
        calls.copy_viewer.assert_not_called()
        calls.process_results.assert_not_called()

        # Once the other run releases the lock, running tests succeeds.
        self.assertEqual(adapter.run_tests(), exit_codes.OK_EXIT_STATUS)
        calls.setup.assert_called_once_with()
        calls.load_runner.assert_called_once()
        calls.run.assert_called_once()

    def test_simultaneous_run_on_linux_suggests_and_supports_namespace(self):
        self.host.platform.os_name = 'linux'
        self.host.platform.os_version = 'linux'
        adapter, calls = self._make_port_test_adapter()

        with self.fs.lock_file('/tmp/WebKit/web_tests.lock'):
            with self.assertLogs('run_wpt_tests', level='ERROR') as log_ctx:
                exit_code = adapter.run_tests()

            self.assertEqual(exit_code, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
            self.assertIn(
                'ERROR:run_wpt_tests:'
                'Another instance of run_web_tests.py or run_wpt_tests.py is '
                'holding the OS lock (/tmp/WebKit/web_tests.lock). Pass '
                '--namespace to run tests from separate checkouts '
                'simultaneously, or wait for the other run to finish and try '
                'again.',
                log_ctx.output,
            )
            calls.setup.assert_not_called()

            # With --namespace, a separate checkout runs even while the shared
            # /tmp/WebKit lock is held.
            adapter.options.namespace = True
            self.assertEqual(adapter.run_tests(), exit_codes.OK_EXIT_STATUS)
            self.assertTrue(self.host.in_namespace)
            calls.setup.assert_called_once_with()

    def test_namespace_rejected_on_unsupported_platform(self):
        # Default MockHost uses os_name='mac'.
        adapter, calls = self._make_port_test_adapter()
        adapter.options.namespace = True
        with self.assertLogs('run_wpt_tests', level='ERROR') as log_ctx:
            exit_code = adapter.run_tests()
        self.assertEqual(exit_code, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
        self.assertIn(
            'ERROR:run_wpt_tests:'
            '--namespace is only supported on Linux with unprivileged user '
            'namespaces enabled.',
            log_ctx.output,
        )
        calls.setup.assert_not_called()

    def test_basic_passthrough(self):
        mock_datetime = self._mocks.enter_context(
            mock.patch('blinkpy.wpt_tests.logging.datetime')
        )
        mock_datetime.now.side_effect = lambda: datetime(
            2023, 1, 1, 12, 0, mock_datetime.now.call_count
        )

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
            '--timeout-multiplier=2.5',
            '--fully-parallel',
            '--no-manifest-update',
            'external/wpt/dir/',
        ]
        adapter = WPTAdapter.from_args(self.host, args, 'test-linux-trusty')
        with adapter.test_env() as options:
            self.assertEqual(options.product, 'chrome')
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
            ignore_cert_flags = [
                flag
                for flag in options.binary_args
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
                2023-01-01 12:00:02.000 INFO Running tests for chrome
                2023-01-01 12:00:03.000 INFO Using port "test-linux-trusty"
                2023-01-01 12:00:04.000 INFO View the test results at file:///tmp/layout-test-results/results.html
                2023-01-01 12:00:05.000 INFO Using Debug build
                """),
        )

    @mock.patch(
        'blinkpy.web_tests.port.test.TestPort.default_child_processes',
        return_value=8,
    )
    def test_wrapper_option(self, _):
        args = [
            '--no-manifest-update',
            '--wrapper=rr record --disable-avx-512',
            'external/wpt/dir/',
        ]
        adapter = WPTAdapter.from_args(self.host, args, 'test-linux-trusty')
        with adapter.test_env() as options:
            self.assertEqual(options.processes, 1)
            self.assertIn('--no-sandbox', options.binary_args)
            self.assertIn('--disable-hang-monitor', options.binary_args)

    def test_scratch_directory_cleanup(self):
        """Only test results should be left behind, even with an exception."""
        adapter = WPTAdapter.from_args(
            self.host, ['--no-manifest-update'], 'test-linux-trusty'
        )
        files_before = dict(self.fs.files)
        with self.assertRaises(KeyboardInterrupt):
            with adapter.test_env() as options:  # noqa: F841
                raise KeyboardInterrupt
        # Remove deleted temporary files (represented with null contents).
        files = {
            path: contents
            for path, contents in self.fs.files.items()
            if contents is not None
        }
        self.assertEqual(files, files_before)

    def test_parse_isolated_filter_nonexistent_tests(self):
        """Check that no tests run if all tests in the filter do not exist.

        This often occurs when the build retries the suite without a patch that
        adds new failing tests.
        """
        adapter = WPTAdapter.from_args(
            self.host,
            [
                '--zero-tests-executed-ok',
                '--isolated-script-test-filter',
                'does-not-exist.any.html::does-not-exist.any.worker.html',
            ],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertEqual(options.include, [])
            self.assertTrue(options.default_exclude)

    def test_run_all_with_zero_tests_executed_ok(self):
        # `--zero-tests-executed-ok` without explicit tests should still run the
        # entire suite. This matches the `run_web_tests.py` behavior.
        adapter = WPTAdapter.from_args(
            self.host,
            ['--zero-tests-executed-ok', '--no-manifest-update'],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertEqual(sorted(options.include), (['dir/reftest.html']))
            self.assertTrue(options.default_exclude)
            # Virtual tests should be included through the subsuites option, but
            # that is already tested in test_run_virtual_tests, so do not
            # duplicate the code here.

    def test_binary_args_propagation(self):
        adapter = WPTAdapter.from_args(
            self.host,
            [
                '--no-manifest-update',
                '--additional-driver-flag=--enable-features=FakeFeature',
                '--additional-driver-flag=--remote-debugging-address=0.0.0.0:8080',
            ],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertLessEqual(
                {
                    '--enable-features=FakeFeature',
                    '--remote-debugging-address=0.0.0.0:8080',
                },
                set(options.binary_args),
            )

    def test_flag_specific(self):
        adapter = WPTAdapter.from_args(
            self.host,
            ['--flag-specific=fake-flag', '--no-manifest-update'],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertIn('--enable-features=FakeFeature', options.binary_args)
            self.assertEqual(sorted(options.include), (['dir/reftest.html']))
            run_info = self._read_run_info(options)
            self.assertEqual(run_info['flag_specific'], 'fake-flag')

    def test_run_virtual_tests(self):
        adapter = WPTAdapter.from_args(
            self.host, ['--no-manifest-update'], 'test-linux-trusty'
        )
        with adapter.test_env() as options:
            self.assertEqual(options.product, 'headless_shell')
            self.assertEqual(sorted(options.include), (['dir/reftest.html']))
            self.assertEqual(options.subsuites, ['fake-vts-2'])
            with open(options.subsuite_file) as fp:
                subsuite_config = json.load(fp)
                self.assertEqual(len(subsuite_config), 1)
                self.assertIn('fake-vts-2', subsuite_config)
                self.assertEqual(
                    subsuite_config['fake-vts-2']['name'], 'fake-vts-2'
                )
                self.assertEqual(
                    subsuite_config['fake-vts-2']['config'],
                    {'binary_args': ['--enable-features=FeatureB']},
                )
                self.assertEqual(
                    subsuite_config['fake-vts-2']['run_info'],
                    {'virtual_suite': 'fake-vts-2'},
                )
                self.assertEqual(
                    sorted(subsuite_config['fake-vts-2']['include']),
                    ['dir/reftest.html'],
                )

        adapter = WPTAdapter.from_args(
            self.host,
            [
                '--no-manifest-update',
                'virtual/fake-vts-2/external/wpt/dir/reftest.html',
            ],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertEqual(sorted(options.include), [])
            self.assertEqual(options.subsuites, ['fake-vts-2'])
            with open(options.subsuite_file) as fp:
                self.assertEqual(
                    subsuite_config['fake-vts-2']['include'],
                    ['dir/reftest.html'],
                )

    def test_sanitizer_enabled(self):
        adapter = WPTAdapter.from_args(
            self.host,
            ['--no-manifest-update', '--enable-sanitizer'],
            'test-linux-trusty',
        )
        with adapter.test_env() as options:
            self.assertEqual(options.timeout_multiplier, 5)
            self.assertTrue(options.sanitizer_enabled)
            run_info = self._read_run_info(options)
            self.assertTrue(run_info['sanitizer_enabled'])

    def test_retry_unexpected(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'Default.txt'),
            textwrap.dedent("""\
                # The non-WPT test should be excluded.
                external/wpt/dir/reftest.html
                """),
        )
        adapter = WPTAdapter.from_args(
            self.host, ['--no-manifest-update'], 'test-linux-trusty'
        )
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 3)

        # TODO We should not retry failures when running with '--use-upstream-wpt'
        # Consider add a unit test for that

        # Create default mock smoke test file
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestLists', 'MacOld.txt'), ""
        )
        adapter = WPTAdapter.from_args(
            self.host, ['--no-manifest-update', '--smoke'], 'test-linux-trusty'
        )
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 3)

        adapter = WPTAdapter.from_args(
            self.host,
            ['--no-manifest-update', 'external/wpt/dir/reftest.html'],
            'test-linux-trusty',
        )
        adapter.set_up_derived_options()
        with adapter.test_env() as options:
            self.assertEqual(options.retry_unexpected, 0)

    def test_env_var(self):
        adapter = WPTAdapter.from_args(
            self.host,
            [
                '--no-manifest-update',
                '--additional-env-var=NEW_ENV_VAR=new_env_var_value',
            ],
            'test-linux-trusty',
        )
        with adapter.test_env():
            self.assertEqual(
                self.host.environ['NEW_ENV_VAR'], 'new_env_var_value'
            )

    def test_show_results(self):
        self.host.filesystem.write_text_file(
            self.finder.path_from_blink_tools(
                'blinkpy', 'web_tests', 'results.html'
            ),
            '<h1>Test run summary</h1> ...',
        )
        self.host.filesystem.write_text_file(
            self.finder.path_from_blink_tools(
                'blinkpy', 'web_tests', 'results.html.version'
            ),
            '1.0',
        )
        post_run_tasks = mock.Mock()
        self._mocks.enter_context(
            mock.patch(
                'blinkpy.web_tests.port.base.Port.clean_up_test_run',
                post_run_tasks.clean_up_test_run,
            )
        )
        self._mocks.enter_context(
            mock.patch.object(
                self.host.user, 'open_url', post_run_tasks.open_url
            )
        )

        adapter = WPTAdapter.from_args(
            self.host,
            [
                '--no-manifest-update',
                '--build-directory=/mock-checkout/out/Release',
                '--results-directory=/mock-checkout/out/Release',
            ],
            'test-linux-trusty',
        )
        adapter.processor.process_event(
            {
                'action': 'test_start',
                'time': 1000,
                'thread': 'MainThread',
                'pid': 128,
                'source': 'wpt',
                'test': '/dir/reftest.html',
            }
        )
        adapter.processor.process_event(
            {
                'action': 'test_end',
                'time': 2000,
                'thread': 'MainThread',
                'pid': 128,
                'source': 'wpt',
                'test': '/dir/reftest.html',
                'status': 'FAIL',
                'expected': 'PASS',
            }
        )
        exit_code = adapter.run_tests()

        self.assertEqual(exit_code, 1)
        self.assertNotIn('XDG_CONFIG_HOME', self.host.environ)
        # Ensure Xvfb is stopped before opening a browser.
        self.assertEqual(
            post_run_tasks.mock_calls,
            [
                mock.call.clean_up_test_run(),
                mock.call.open_url(
                    'file:///mock-checkout/out/Release/'
                    'layout-test-results/results.html'
                ),
            ],
        )
