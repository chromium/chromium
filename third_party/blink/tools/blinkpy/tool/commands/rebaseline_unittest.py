# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import optparse
import pickle
import unittest
from unittest import mock

from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import (
    Artifact,
    WebTestResult,
    WebTestResults,
)
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.tool.commands.rebaseline import (
    AbstractParallelRebaselineCommand, Rebaseline, TestBaselineSet)
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.web_tests.port.test import MOCK_WEB_TESTS


class BaseTestCase(unittest.TestCase):
    command_constructor = lambda: None

    def setUp(self):
        self.tool = MockBlinkTool()
        self.command = self.command_constructor()
        self.command._tool = self.tool  # pylint: disable=protected-access
        self.tool.builders = BuilderList({
            'MOCK Mac10.10 (dbg)': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Debug'],
            },
            'MOCK Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
            },
            'MOCK Mac10.11 (dbg)': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Debug'],
            },
            'MOCK Mac10.11 ASAN': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
            },
            'MOCK Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
            },
            'MOCK Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                },
            },
            'MOCK Trusty Multiple Steps': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                    'not_site_per_process_blink_web_tests': {
                        'flag_specific': 'disable-site-isolation-trials',
                    },
                },
            },
            'MOCK Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release'],
            },
            'MOCK Win7 (dbg)': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Debug'],
            },
            'MOCK Win7 (dbg)(1)': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Debug'],
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK Win7 (dbg)(2)': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Debug'],
            },
            'MOCK Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
            },
            'MOCK wpt(1)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
            'MOCK wpt(2)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
        })
        self.mac_port = self.tool.port_factory.get_from_builder_name(
            'MOCK Mac10.11')
        self.test_expectations_path = self.mac_port.path_to_generic_test_expectations_file(
        )

        self._write(
            'VirtualTestSuites',
            json.dumps([{
                "prefix":
                "prefix",
                "platforms": ["Linux", "Mac"],
                "bases": [
                    "userscripts/first-test.html",
                    'userscripts/second-test.html'
                ],
                "args": ["--enable-features=flag"]
            }]))

        self._write(
            'FlagSpecificConfig',
            json.dumps([
                {
                    'name': 'disable-site-isolation-trials',
                    'args': ['--disable-site-isolation-trials'],
                },
            ]))
        for wpt_dir in self.mac_port.WPT_DIRS:
            self._write(self.tool.filesystem.join(wpt_dir, 'MANIFEST.json'),
                        json.dumps({}))

        # Create some dummy tests (note _setup_mock_build_data uses the same
        # test names). Also, create some dummy baselines to avoid the implicit
        # all-pass warning.
        self._write('userscripts/first-test.html', 'Dummy test contents')
        self._write('userscripts/first-test-expected.txt', 'Dummy baseline')
        self._write('userscripts/first-test-expected.png', 'Dummy baseline')
        self._write('userscripts/first-test-expected.wav', 'Dummy baseline')
        self._write('userscripts/second-test.html', 'Dummy test contents')
        self._write('userscripts/second-test-expected.txt', 'Dummy baseline')
        self._write('userscripts/second-test-expected.png', 'Dummy baseline')
        self._write('userscripts/second-test-expected.wav', 'Dummy baseline')
        self._write('userscripts/third-test.html', 'Dummy test contents')

        # In AbstractParallelRebaselineCommand._rebaseline_commands, a default port
        # object is gotten using self.tool.port_factory.get(), which is used to get
        # test paths -- and the web tests directory may be different for the "test"
        # ports and real ports. Since only "test" ports are used in this class,
        # we can make the default port also a "test" port.
        self.original_port_factory_get = self.tool.port_factory.get
        self._test_port = self.tool.port_factory.get('test')
        self._test_port.set_option_default('manifest_update', False)

        def get_test_port(port_name=None, options=None, **kwargs):
            if not port_name:
                return self._test_port
            return self.original_port_factory_get(port_name, options, **kwargs)

        self._mocks = contextlib.ExitStack()
        self._mock_copier = mock.Mock(wraps=BaselineCopier(self.tool))
        # See https://docs.python.org/3/library/unittest.mock.html#where-to-patch
        # for why `blinkpy.common.checkout.baseline_copier.BaselineCopier` is
        # not patched instead.
        self._mocks.enter_context(
            mock.patch('blinkpy.tool.commands.rebaseline.BaselineCopier',
                       return_value=self._mock_copier))
        self._mocks.enter_context(
            mock.patch('blinkpy.tool.blink_tool.BlinkTool',
                       return_value=self.tool))
        self._mocks.enter_context(
            mock.patch.object(self.tool, 'main', create=True, return_value=0))
        self._mocks.enter_context(
            mock.patch('blinkpy.common.message_pool.get', self._get_mock_pool))
        self._mocks.enter_context(
            mock.patch.object(self.tool.port_factory, 'get', get_test_port))
        self._mocks.enter_context(
            mock.patch.object(self.tool, 'web', mock.Mock()))
        self.tool.web.get_binary.side_effect = lambda url: url.encode()

    def _get_mock_pool(self, caller, worker_factory, num_workers):
        """A mock for `message_pool.get(...)`.

        This simply invokes a single worker serially according to the message
        pool protocol.
        """
        worker_process = mock.Mock()
        worker_process.host = self.tool
        worker_process.post = lambda name, *args: caller.handle(
            name, 'worker/0', *_serialize_round_trip(args))
        worker = worker_factory(worker_process)

        def run(tasks):
            if hasattr(worker, 'start'):
                worker.start()
            for message_name, *args in tasks:
                worker.handle(message_name, 'manager',
                              *_serialize_round_trip(args))
            if hasattr(worker, 'stop'):
                worker.stop()

        message_pool = mock.Mock()
        message_pool.run = run
        message_pool = contextlib.nullcontext(message_pool)
        return message_pool

    def tearDown(self):
        self._mocks.close()

    def _expand(self, path):
        if self.tool.filesystem.isabs(path):
            return path
        return self.tool.filesystem.join(self.mac_port.web_tests_dir(), path)

    def _read(self, path):
        return self.tool.filesystem.read_text_file(self._expand(path))

    def _write(self, path, contents):
        self.tool.filesystem.write_text_file(self._expand(path), contents)

    def _remove(self, path):
        self.tool.filesystem.remove(self._expand(path))

    def _zero_out_test_expectations(self):
        for port_name in self.tool.port_factory.all_port_names():
            port = self.tool.port_factory.get(port_name)
            for path in port.default_expectations_files():
                self._write(path, '')
        self.tool.filesystem.written_files = {}

    def _setup_mock_build_data(self):
        for builder in ['MOCK Win7', 'MOCK Win7 (dbg)', 'MOCK Mac10.11']:
            self.tool.results_fetcher.set_results(
                Build(builder),
                WebTestResults.from_json(
                    {
                        'tests': {
                            'userscripts': {
                                'first-test.html': {
                                    'expected': 'PASS',
                                    'actual': 'FAIL',
                                    'is_unexpected': True,
                                    # The real format of these URLs is more
                                    # complex, but adding that detail to the
                                    # test doesn't add value. We mostly just
                                    # care about which builder and test the
                                    # baseline was downloaded for.
                                    'artifacts': {
                                        'actual_image': [
                                            f'https://results.api.cr.dev/{builder}/first/actual_image'
                                        ],
                                        'expected_image': [
                                            f'https://results.api.cr.dev/{builder}/first/expected_image'
                                        ],
                                        'actual_text': [
                                            f'https://results.api.cr.dev/{builder}/first/actual_text'
                                        ],
                                        'expected_text': [
                                            f'https://results.api.cr.dev/{builder}/first/expected_text'
                                        ],
                                    }
                                },
                                'second-test.html': {
                                    'expected': 'FAIL',
                                    'actual': 'FAIL',
                                    'artifacts': {
                                        'actual_image': [
                                            f'https://results.api.cr.dev/{builder}/second/actual_image'
                                        ],
                                        'expected_image': [
                                            f'https://results.api.cr.dev/{builder}/second/expected_image'
                                        ],
                                        'actual_audio': [
                                            f'https://results.api.cr.dev/{builder}/second/actual_audio'
                                        ],
                                        'expected_audio': [
                                            f'https://results.api.cr.dev/{builder}/second/expected_audio'
                                        ],
                                    }
                                }
                            }
                        }
                    },
                    step_name='blink_web_tests'))

    def _assert_baseline_downloaded(self, url: str, dest: str):
        self.tool.web.get_binary.assert_any_call(url)
        self.assertEqual(self._read(dest), url)


class TestAbstractParallelRebaselineCommand(BaseTestCase):
    """Tests for the base class of multiple rebaseline commands.

    This class only contains test cases for utility methods. Some common
    behaviours of various rebaseline commands are tested in TestRebaseline.
    """

    command_constructor = AbstractParallelRebaselineCommand

    def test_builders_to_fetch_from(self):
        build_steps_to_fetch = self.command.build_steps_to_fetch_from([
            ('MOCK Win10', 'blink_web_tests'),
            ('MOCK Win7 (dbg)(1)', 'blink_web_tests'),
            ('MOCK Win7 (dbg)(2)', 'blink_web_tests'),
            ('MOCK Win7', 'blink_web_tests'),
        ])
        # Win7 debug builders are shadowed by release builder.
        self.assertEqual(build_steps_to_fetch, {
            ('MOCK Win7', 'blink_web_tests'),
            ('MOCK Win10', 'blink_web_tests'),
        })

    def test_builders_to_fetch_from_flag_specific(self):
        build_steps_to_fetch = self.command.build_steps_to_fetch_from([
            ('MOCK Trusty', 'blink_web_tests'),
            ('MOCK Trusty', 'blink_wpt_tests'),
            ('MOCK Trusty Multiple Steps', 'blink_web_tests'),
            ('MOCK Trusty Multiple Steps', 'blink_wpt_tests'),
        ])
        self.assertEqual(
            build_steps_to_fetch, {
                ('MOCK Trusty', 'blink_web_tests'),
                ('MOCK Trusty', 'blink_wpt_tests'),
            })

        build_steps_to_fetch = self.command.build_steps_to_fetch_from([
            ('MOCK Trusty Multiple Steps', 'blink_web_tests'),
            ('MOCK Trusty Multiple Steps',
             'not_site_per_process_blink_web_tests'),
        ])
        self.assertEqual(len(build_steps_to_fetch), 2)
        self.assertIn(('MOCK Trusty Multiple Steps', 'blink_web_tests'),
                      build_steps_to_fetch)
        self.assertIn(('MOCK Trusty Multiple Steps',
                       'not_site_per_process_blink_web_tests'),
                      build_steps_to_fetch)

    def test_unstaged_baselines(self):
        git = self.tool.git()
        git.unstaged_changes = lambda: {
            RELATIVE_WEB_TESTS + 'x/foo-expected.txt': 'M',
            RELATIVE_WEB_TESTS + 'x/foo-expected.something': '?',
            RELATIVE_WEB_TESTS + 'x/foo-expected.png': '?',
            RELATIVE_WEB_TESTS + 'x/foo.html': 'M',
            'docs/something.md': '?', }
        self.assertEqual(self.command.unstaged_baselines(), [
            MOCK_WEB_TESTS + 'x/foo-expected.png',
            MOCK_WEB_TESTS + 'x/foo-expected.txt',
        ])

    def test_suffixes_for_actual_failures_for_non_wpt(self):
        # pylint: disable=protected-access
        build = Build('MOCK Win7')
        self.tool.results_fetcher.set_results(
            build,
            WebTestResults.from_json({
                'tests': {
                    'pixel.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'artifacts': {
                            'actual_image': ['pixel-actual.png'],
                        },
                    }
                }
            }))
        self.assertEqual(
            self.command._suffixes_for_actual_failures('pixel.html', build),
            {'png'},
        )


class TestRebaseline(BaseTestCase):
    """Tests for the blink_tool.py rebaseline command.

    Also tests some common behaviours of all rebaseline commands.
    """

    command_constructor = Rebaseline

    def setUp(self):
        super(TestRebaseline, self).setUp()
        self.tool.executive = MockExecutive()
        self._setup_mock_build_data()

    def tearDown(self):
        super(TestRebaseline, self).tearDown()

    @staticmethod
    def options(**kwargs):
        return optparse.Values(
            dict(
                {
                    'optimize': True,
                    'dry_run': False,
                    'verbose': True,
                    'results_directory': None,
                }, **kwargs))

    def test_rebaseline_test_passes_on_all_builders(self):
        self.tool.results_fetcher.set_results(
            Build('MOCK Win7'),
            WebTestResults.from_json(
                {
                    'tests': {
                        'userscripts': {
                            'first-test.html': {
                                'expected': 'PASS',
                                'actual': 'PASS',
                            },
                        },
                    },
                },
                step_name='blink_web_tests'))

        self._write(
            self.test_expectations_path, '# results: [ Failure ]\n'
            'userscripts/first-test.html [ Failure ]\n')
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)
        self.tool.main.assert_not_called()

    def test_rebaseline_all(self):
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt',
                          test_baseline_set),
                mock.call('userscripts/first-test.html', 'png',
                          test_baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_text',
            'platform/test-win-win7/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_image',
            'platform/test-win-win7/userscripts/first-test-expected.png')
        self.tool.main.assert_called_once_with([
            'echo',
            'optimize-baselines',
            '--no-manifest-update',
            '--verbose',
            'userscripts/first-test.html',
        ])

    def test_rebaseline_debug(self):
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7 (dbg)'), 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt',
                          test_baseline_set),
                mock.call('userscripts/first-test.html', 'png',
                          test_baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7 (dbg)/first/actual_text',
            'platform/test-win-win7/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7 (dbg)/first/actual_image',
            'platform/test-win-win7/userscripts/first-test-expected.png')
        self.tool.main.assert_called_once_with([
            'echo',
            'optimize-baselines',
            '--no-manifest-update',
            '--verbose',
            'userscripts/first-test.html',
        ])

    def test_rebaseline_reftest_with_text_failure(self):
        """Ensure that a reftest can still have any text output [0] rebaselined.

        [0]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/writing_web_tests.md#tests-that-are-both-pixel_reference-tests-and-text-tests
        """
        build = Build('MOCK Win7', 1000)
        self.tool.results_fetcher.set_results(
            build,
            WebTestResults.from_json(
                {
                    'tests': {
                        'reftest.html': {
                            'expected': 'PASS',
                            'actual': 'FAIL',
                            'is_unexpected': True,
                            'artifacts': {
                                'actual_text': [
                                    'https://results.api.cr.dev/reftest-actual.txt',
                                ],
                                'actual_image': [
                                    'https://results.api.cr.dev/reftest-actual.png',
                                ],
                            },
                        },
                    },
                },
                step_name='blink_web_tests'))
        self._write('reftest.html', 'Dummy test contents')
        self._write('reftest-expected.html', 'reference page')

        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('reftest.html', build, 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_called_once_with(
            'reftest.html', 'txt', test_baseline_set)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/reftest-actual.txt',
            'platform/test-win-win7/reftest-expected.txt')
        self.assertNotIn(
            mock.call('https://results.api.cr.dev/reftest-actual.png'),
            self.tool.web.get_binary.call_args_list)
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand('platform/test-win-win7/reftest-expected.png')))

    def test_rebaseline_without_expected_image(self):
        build = Build('MOCK Win10', 1000)
        self.tool.results_fetcher.set_results(
            build,
            WebTestResults.from_json(
                {
                    'tests': {
                        'pixel-test.html': {
                            'expected': 'PASS',
                            'actual': 'FAIL',
                            'is_unexpected': True,
                            'artifacts': {
                                'actual_image': [
                                    'https://results.api.cr.dev/pixel-test-actual.png',
                                ],
                            },
                        },
                    },
                },
                step_name='blink_web_tests'))

        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('pixel-test.html', build, 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)

        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/pixel-test-actual.png',
            'platform/test-win-win10/pixel-test-expected.png')
        self.assertEqual(
            self._read('platform/test-win-win7/pixel-test-expected.png'), '')

    def test_rebaseline_with_cache_hit(self):
        results = WebTestResults([
            WebTestResult('userscripts/first-test.html', {
                'actual': 'FAIL',
                'is_unexpected': True,
            }, {
                'actual_image': [
                    Artifact('https://results.usercontent.cr.dev/actual_image',
                             '3a778bf'),
                ],
            }),
        ],
                                 step_name='blink_web_tests')
        self.tool.web.get_binary.side_effect = lambda _: b'actual image'
        self.tool.results_fetcher.set_results(Build('MOCK Win7'), results)
        self.tool.results_fetcher.set_results(Build('MOCK Mac10.11'), results)
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')
        self.command.rebaseline(self.options(), test_baseline_set)

        self.tool.web.get_binary.assert_called_once_with(
            'https://results.usercontent.cr.dev/actual_image')
        self.assertEqual(
            self._read(
                'platform/test-win-win7/userscripts/first-test-expected.png'),
            'actual image')
        self.assertEqual(
            self._read('platform/test-mac-mac10.11/'
                       'userscripts/first-test-expected.png'), 'actual image')
        self.assertEqual(self.command.baseline_cache_stats.hit_count, 1)
        self.assertEqual(self.command.baseline_cache_stats.hit_bytes, 12)
        self.assertEqual(self.command.baseline_cache_stats.total_count, 2)
        self.assertEqual(self.command.baseline_cache_stats.total_bytes, 24)

    def test_no_optimize(self):
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')
        self.command.rebaseline(self.options(optimize=False),
                                test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt',
                          test_baseline_set),
                mock.call('userscripts/first-test.html', 'png',
                          test_baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_text',
            'platform/test-win-win7/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_image',
            'platform/test-win-win7/userscripts/first-test-expected.png')
        self.tool.main.assert_not_called()

    def test_results_directory(self):
        self._write('/tmp/userscripts/first-test-actual.txt', 'actual text')
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')
        self.command.rebaseline(
            self.options(optimize=False, results_directory='/tmp'),
            test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt',
                          test_baseline_set),
                mock.call('userscripts/first-test.html', 'png',
                          test_baseline_set),
            ],
            any_order=True)
        self.assertEqual(
            self._read(
                'platform/test-win-win7/userscripts/first-test-expected.txt'),
            'actual text')
        self.assertFalse(
            self.tool.filesystem.exists(
                self._expand(
                    'platform/test-win-win7/userscripts/first-test-expected.png'
                )))
        self.tool.main.assert_not_called()

    def test_rebaseline_with_different_port_name(self):
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Win7'), 'blink_web_tests',
                              'test-win-win10')
        self.command.rebaseline(self.options(), test_baseline_set)

        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt',
                          test_baseline_set),
                mock.call('userscripts/first-test.html', 'png',
                          test_baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_text',
            'platform/test-win-win10/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_image',
            'platform/test-win-win10/userscripts/first-test-expected.png')
        self.tool.main.assert_called_once_with([
            'echo',
            'optimize-baselines',
            '--no-manifest-update',
            '--verbose',
            'userscripts/first-test.html',
        ])


@unittest.skip('Disabled because this does not reflect the behavior of '
               "'rebaseline-test-internal' now. Reenable after implementing "
               'crbug.com/1149035.')
class TestRebaselineUpdatesExpectationsFiles(BaseTestCase):
    """Tests for the logic related to updating the test expectations file."""

    command_constructor = Rebaseline

    def setUp(self):
        super(TestRebaselineUpdatesExpectationsFiles, self).setUp()

        def mock_run_command(*args, **kwargs):  # pylint: disable=unused-argument
            return '{"add": [], "remove-lines": [{"test": "userscripts/first-test.html", "port_name": "test-mac-mac10.11"}]}\n'

        self.tool.executive = MockExecutive(run_command_fn=mock_run_command)

    @staticmethod
    def options():
        return optparse.Values({
            'optimize': False,
            'dry_run': False,
            'verbose': True,
            'results_directory': None,
        })

    # In the following test cases, we use a mock rebaseline-test-internal to
    # pretend userscripts/first-test.html can be rebaselined on Mac10.11, so
    # the corresponding expectation (if exists) should be updated.

    def test_rebaseline_updates_expectations_file(self):
        self._write(self.test_expectations_path, (
            '# tags: [ Mac10.10 Mac Linux ]\n'
            '# tags: [ Debug ]\n'
            '# results: [ Failure ]\n'
            'crbug.com/123 [ Debug Mac ] userscripts/first-test.html [ Failure ]\n'
            '[ Linux ] userscripts/first-test.html [ Failure ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(new_expectations, (
            '# tags: [ Mac10.10 Mac Linux ]\n'
            '# tags: [ Debug ]\n'
            '# results: [ Failure ]\n'
            'crbug.com/123 [ Debug Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
            '[ Linux ] userscripts/first-test.html [ Failure ]\n'))

    def test_rebaseline_updates_expectations_file_all_platforms(self):
        self._write(self.test_expectations_path,
                    ('# tags: [ linux mac10.10 win ]\n# results: [ Failure ]\n'
                     'userscripts/first-test.html [ Failure ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ linux mac10.10 win ]\n'
             '# results: [ Failure ]\n'
             '[ Linux ] userscripts/first-test.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             '[ Win ] userscripts/first-test.html [ Failure ]\n'))

    def test_rebaseline_handles_platform_skips(self):
        # This test is just like test_rebaseline_updates_expectations_file_all_platforms(),
        # except that if a particular port happens to SKIP a test in an overrides file,
        # we count that as passing, and do not think that we still need to rebaseline it.
        self._write(
            self.test_expectations_path,
            '# tags: [ Linux Mac10.10 Win ]\n# results: [ Failure ]\nuserscripts/first-test.html [ Failure ]\n'
        )
        self._write('NeverFixTests', ('# tags: [ Android ]\n'
                                      '# results: [ Skip ]\n'
                                      '[ Android ] userscripts [ Skip ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Linux Mac10.10 Win ]\n'
             '# results: [ Failure ]\n'
             '[ Linux ] userscripts/first-test.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             '[ Win ] userscripts/first-test.html [ Failure ]\n'))

    def test_rebaseline_handles_skips_in_file(self):
        # This test is like test_rebaseline_handles_platform_skips, except that the
        # Skip is in the same (generic) file rather than a platform file. In this case,
        # the Skip line should be left unmodified. Note that the first line is now
        # qualified as "[Linux Mac Win]"; if it was unqualified, it would conflict with
        # the second line.
        self._write(self.test_expectations_path,
                    ('# tags: [ Linux Mac Mac10.10 Win ]\n'
                     '# results: [ Failure Skip ]\n'
                     '[ Linux ] userscripts/first-test.html [ Failure ]\n'
                     '[ Mac ] userscripts/first-test.html [ Failure ]\n'
                     '[ Win ] userscripts/first-test.html [ Skip ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Linux Mac Mac10.10 Win ]\n'
             '# results: [ Failure Skip ]\n'
             '[ Linux ] userscripts/first-test.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             '[ Win ] userscripts/first-test.html [ Skip ]\n'))

    def test_rebaseline_handles_slow_in_file(self):
        self._write(self.test_expectations_path,
                    ('# tags: [ Linux Mac Mac10.10 Win ]\n'
                     '# results: [ Failure Slow ]\n'
                     '[ Linux ] userscripts/first-test.html [ Failure ]\n'
                     '[ Mac ] userscripts/first-test.html [ Failure ]\n'
                     '[ Win ] userscripts/first-test.html [ Failure Slow ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Linux Mac Mac10.10 Win ]\n'
             '# results: [ Failure Slow ]\n'
             '[ Linux ] userscripts/first-test.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             '[ Win ] userscripts/first-test.html [ Failure Slow ]\n'))

    def test_rebaseline_handles_smoke_tests(self):
        # This test is just like test_rebaseline_handles_platform_skips, except that we check for
        # a test not being in the SmokeTests file, instead of using overrides files.
        # If a test is not part of the smoke tests, we count that as passing on ports that only
        # run smoke tests, and do not think that we still need to rebaseline it.
        self._write(
            self.test_expectations_path,
            '# tags: [ Linux Mac10.10 Win ]\n# results: [ Failure ]\nuserscripts/first-test.html [ Failure ]\n'
        )
        self._write('SmokeTests', 'fast/html/article-element.html')
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/first-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Linux Mac10.10 Win ]\n'
             '# results: [ Failure ]\n'
             '[ Linux ] userscripts/first-test.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             '[ Win ] userscripts/first-test.html [ Failure ]\n'))

    # In the following test cases, the tests produce no outputs (e.g. clean
    # passing reftests, skipped tests, etc.). Hence, there are no baselines to
    # fetch (empty baseline suffixes), and rebaseline-test-internal wouldn't be
    # called. However, in some cases the expectations still need to be updated.

    def test_rebaseline_keeps_skip_expectations(self):
        # [ Skip ] expectations should always be kept.
        self._write(self.test_expectations_path,
                    ('# tags: [ Mac Win ]\n'
                     '# results: [ Skip ]\n'
                     '[ Mac ] userscripts/skipped-test.html [ Skip ]\n'
                     '[ Win ] userscripts/skipped-test.html [ Skip ]\n'))
        self._write('userscripts/skipped-test.html', 'Dummy test contents')
        self.tool.results_fetcher.set_results(
            Build('MOCK Mac10.11'),
            WebTestResults.from_json({
                'tests': {
                    'userscripts': {
                        'skipped-test.html': {
                            'expected': 'SKIP',
                            'actual': 'SKIP',
                        }
                    }
                }
            }))
        self.tool.results_fetcher.set_results(
            Build('MOCK Win7'),
            WebTestResults.from_json({
                'tests': {
                    'userscripts': {
                        'skipped-test.html': {
                            'expected': 'SKIP',
                            'actual': 'SKIP',
                        }
                    }
                }
            }))
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/skipped-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')
        test_baseline_set.add('userscripts/skipped-test.html',
                              Build('MOCK Win7'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Mac Win ]\n'
             '# results: [ Skip ]\n'
             '[ Mac ] userscripts/skipped-test.html [ Skip ]\n'
             '[ Win ] userscripts/skipped-test.html [ Skip ]\n'))
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_keeps_flaky_expectations(self):
        # Flaky expectations should be kept even if the test passes.
        self._write(
            self.test_expectations_path,
            '# results: [ Pass Failure ]\nuserscripts/flaky-test.html [ Pass Failure ]\n'
        )
        self._write('userscripts/flaky-test.html', 'Dummy test contents')
        self.tool.results_fetcher.set_results(
            Build('MOCK Mac10.11'),
            WebTestResults.from_json({
                'tests': {
                    'userscripts': {
                        'flaky-test.html': {
                            'expected': 'PASS FAIL',
                            'actual': 'PASS',
                        }
                    }
                }
            }))
        test_baseline_set = TestBaselineSet(self.tool.builders)
        test_baseline_set.add('userscripts/flaky-test.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            '# results: [ Pass Failure ]\nuserscripts/flaky-test.html [ Pass Failure ]\n'
        )
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly(self):
        # The test passes without any output. Its expectation should be updated
        # without calling rebaseline-test-internal.
        self._write(
            self.test_expectations_path,
            '# tags: [ Linux Mac10.10 Win ]\n# results: [ Failure ]\nuserscripts/all-pass.html [ Failure ]\n'
        )
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool.builders)
        self.tool.results_fetcher.set_results(
            Build('MOCK Mac10.11'),
            WebTestResults.from_json({
                'tests': {
                    'userscripts': {
                        'all-pass.html': {
                            'expected': 'FAIL',
                            'actual': 'PASS',
                            'is_unexpected': True
                        }
                    }
                }
            }))
        test_baseline_set.add('userscripts/all-pass.html',
                              Build('MOCK Mac10.11'), 'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('# tags: [ Linux Mac10.10 Win ]\n'
             '# results: [ Failure ]\n'
             '[ Linux ] userscripts/all-pass.html [ Failure ]\n'
             '[ Mac10.10 ] userscripts/all-pass.html [ Failure ]\n'
             '[ Win ] userscripts/all-pass.html [ Failure ]\n'))
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly_everywhere(self):
        # Similar to test_rebaseline_test_passes_unexpectedly, except that the
        # test passes on all ports.
        self._write(
            self.test_expectations_path,
            '# results: [ Failure ]\nuserscripts/all-pass.html [ Failure ]\n')
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool.builders)
        for builder in [
                'MOCK Win7', 'MOCK Win10', 'MOCK Mac10.10', 'MOCK Mac10.11',
                'MOCK Precise', 'MOCK Trusty'
        ]:
            self.tool.results_fetcher.set_results(
                Build(builder),
                WebTestResults.from_json({
                    'tests': {
                        'userscripts': {
                            'all-pass.html': {
                                'expected': 'FAIL',
                                'actual': 'PASS',
                                'is_unexpected': True
                            }
                        }
                    }
                }))
            test_baseline_set.add('userscripts/all-pass.html', Build(builder),
                                  'blink_web_tests')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(new_expectations, '# results: [ Failure ]\n')
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly_but_on_another_port(self):
        # Similar to test_rebaseline_test_passes_unexpectedly, except that the
        # build was run on a different port than the port we are rebaselining
        # (possible when rebaseline-cl fills in missing results), in which case
        # we don't update the expectations.
        self._write(
            self.test_expectations_path,
            '# results: [ Failure ]\nuserscripts/all-pass.html [ Failure ]\n')
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool.builders)
        self.tool.results_fetcher.set_results(
            Build('MOCK Mac10.11'),
            WebTestResults.from_json({
                'tests': {
                    'userscripts': {
                        'all-pass.html': {
                            'expected': 'FAIL',
                            'actual': 'PASS',
                            'is_unexpected': True
                        }
                    }
                }
            }))
        test_baseline_set.add('userscripts/all-pass.html',
                              Build('MOCK Mac10.11'), 'MOCK Mac10.10')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            '# results: [ Failure ]\nuserscripts/all-pass.html [ Failure ]\n')
        self.assertEqual(self.tool.executive.calls, [])


class TestRebaselineExecute(BaseTestCase):
    """Tests for the main execute function of the blink_tool.py rebaseline command."""

    command_constructor = Rebaseline

    @staticmethod
    def options():
        return optparse.Values({
            'results_directory': False,
            'optimize': False,
            'dry_run': False,
            'builders': None,
            'verbose': True,
        })

    def test_rebaseline(self):
        # pylint: disable=protected-access
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']
        self._setup_mock_build_data()
        self.command.execute(self.options(), ['userscripts/first-test.html'],
                             self.tool)

        baseline_set = TestBaselineSet(self.tool.builders)
        baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'),
                         'blink_web_tests')
        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt', baseline_set),
                mock.call('userscripts/first-test.html', 'png', baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_text',
            'platform/test-win-win7/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_image',
            'platform/test-win-win7/userscripts/first-test-expected.png')
        self.tool.main.assert_not_called()

    def test_rebaseline_directory(self):
        # pylint: disable=protected-access
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']

        self._setup_mock_build_data()
        self.command.execute(self.options(), ['userscripts'], self.tool)

        baseline_set = TestBaselineSet(self.tool.builders)
        baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'),
                         'blink_web_tests')
        self._mock_copier.find_baselines_to_copy.assert_has_calls(
            [
                mock.call('userscripts/first-test.html', 'txt', baseline_set),
                mock.call('userscripts/first-test.html', 'png', baseline_set),
            ],
            any_order=True)
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_text',
            'platform/test-win-win7/userscripts/first-test-expected.txt')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/first/actual_image',
            'platform/test-win-win7/userscripts/first-test-expected.png')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/second/actual_audio',
            'platform/test-win-win7/userscripts/second-test-expected.wav')
        self._assert_baseline_downloaded(
            'https://results.api.cr.dev/MOCK Win7/second/actual_image',
            'platform/test-win-win7/userscripts/second-test-expected.png')
        self.tool.main.assert_not_called()


class TestBaselineSetTest(unittest.TestCase):
    def setUp(self):
        host = MockBlinkTool()
        host.port_factory = MockPortFactory(host)
        port = host.port_factory.get()
        base_dir = port.web_tests_dir()
        host.filesystem.write_text_file(base_dir + 'a/x.html', '<html>')
        host.filesystem.write_text_file(base_dir + 'a/y.html', '<html>')
        host.filesystem.write_text_file(base_dir + 'a/z.html', '<html>')
        host.builders = BuilderList({
            'MOCK Mac10.12': {
                'port_name': 'test-mac-mac10.12',
                'specifiers': ['Mac10.12', 'Release']
            },
            'MOCK Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
            'MOCK Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release']
            },
            'some-wpt-bot': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self.host = host

    def test_add_and_iter_tests(self):
        test_baseline_set = TestBaselineSet(self.host.builders)
        test_baseline_set.add('a/x.html', Build('MOCK Trusty'),
                              'blink_web_tests')
        test_baseline_set.add('a/y.html', Build('MOCK Trusty'),
                              'blink_web_tests')
        test_baseline_set.add('a/z.html', Build('MOCK Trusty'),
                              'blink_web_tests')
        test_baseline_set.add('a/z.html', Build('MOCK Win10'),
                              'blink_web_tests')
        self.assertEqual(list(test_baseline_set), [
            ('a/x.html', Build(builder_name='MOCK Trusty'), 'blink_web_tests',
             'test-linux-trusty'),
            ('a/y.html', Build(builder_name='MOCK Trusty'), 'blink_web_tests',
             'test-linux-trusty'),
            ('a/z.html', Build(builder_name='MOCK Trusty'), 'blink_web_tests',
             'test-linux-trusty'),
            ('a/z.html', Build(builder_name='MOCK Win10'), 'blink_web_tests',
             'test-win-win10'),
        ])
        self.assertEqual(test_baseline_set.all_tests(),
                         ['a/x.html', 'a/y.html', 'a/z.html'])

    def test_str_empty(self):
        test_baseline_set = TestBaselineSet(self.host.builders)
        self.assertEqual(str(test_baseline_set), '<Empty TestBaselineSet>')

    def test_str_basic(self):
        test_baseline_set = TestBaselineSet(self.host.builders)
        test_baseline_set.add('a/x.html', Build('MOCK Mac10.12'),
                              'blink_web_tests')
        test_baseline_set.add('a/x.html', Build('MOCK Win10'),
                              'blink_web_tests')
        self.assertRegex(str(test_baseline_set),
                         'a/x.html: .*, blink_web_tests, test-mac-mac10\.12')
        self.assertRegex(str(test_baseline_set),
                         'a/x.html: .*, blink_web_tests, test-win-win10')

    def test_getters(self):
        test_baseline_set = TestBaselineSet(self.host.builders)
        test_baseline_set.add('a/x.html', Build('MOCK Mac10.12'),
                              'blink_web_tests')
        test_baseline_set.add('a/x.html', Build('MOCK Win10'),
                              'blink_web_tests')
        self.assertEqual(test_baseline_set.all_tests(), ['a/x.html'])
        self.assertEqual(
            test_baseline_set.build_port_pairs('a/x.html'),
            [(Build(builder_name='MOCK Mac10.12'), 'test-mac-mac10.12'),
             (Build(builder_name='MOCK Win10'), 'test-win-win10')])

    def test_non_prefix_mode(self):
        test_baseline_set = TestBaselineSet(self.host.builders)
        # This test does not exist in setUp.
        test_baseline_set.add('wpt/foo.html', Build('some-wpt-bot'),
                              'blink_web_tests')
        # But it should still appear in various getters since no test lookup is
        # done when prefix_mode=False.
        self.assertEqual(list(test_baseline_set),
                         [('wpt/foo.html', Build('some-wpt-bot'),
                           'blink_web_tests', 'linux-trusty')])
        self.assertEqual(test_baseline_set.all_tests(), ['wpt/foo.html'])
        self.assertEqual(test_baseline_set.build_port_pairs('wpt/foo.html'),
                         [(Build('some-wpt-bot'), 'linux-trusty')])


def _serialize_round_trip(obj):
    """An identity function that raises when the argument is not pickleable.

    The purpose of this function is to simulate passing messages across a
    process boundary. A test that attempts to pass an unpickleable object across
    the simulated boundary should fail, as it would with real processes.
    """
    return pickle.loads(pickle.dumps(obj))
