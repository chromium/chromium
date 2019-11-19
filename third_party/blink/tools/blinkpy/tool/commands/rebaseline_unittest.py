# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import unittest

from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.tool.commands.rebaseline import (
    AbstractParallelRebaselineCommand, Rebaseline, TestBaselineSet
)
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory_mock import MockPortFactory


class BaseTestCase(unittest.TestCase):

    WEB_PREFIX = 'https://test-results.appspot.com/data/layout_results/MOCK_Mac10_11/results/layout-test-results'

    command_constructor = lambda: None

    def setUp(self):
        self.tool = MockBlinkTool()
        self.command = self.command_constructor()
        self.command._tool = self.tool   # pylint: disable=protected-access
        self.tool.builders = BuilderList({
            'MOCK Mac10.10 (dbg)': {'port_name': 'test-mac-mac10.10', 'specifiers': ['Mac10.10', 'Debug']},
            'MOCK Mac10.10': {'port_name': 'test-mac-mac10.10', 'specifiers': ['Mac10.10', 'Release']},
            'MOCK Mac10.11 (dbg)': {'port_name': 'test-mac-mac10.11', 'specifiers': ['Mac10.11', 'Debug']},
            'MOCK Mac10.11 ASAN': {'port_name': 'test-mac-mac10.11', 'specifiers': ['Mac10.11', 'Release']},
            'MOCK Mac10.11': {'port_name': 'test-mac-mac10.11', 'specifiers': ['Mac10.11', 'Release']},
            'MOCK Precise': {'port_name': 'test-linux-precise', 'specifiers': ['Precise', 'Release']},
            'MOCK Trusty': {'port_name': 'test-linux-trusty', 'specifiers': ['Trusty', 'Release']},
            'MOCK Win10': {'port_name': 'test-win-win10', 'specifiers': ['Win10', 'Release']},
            'MOCK Win7 (dbg)': {'port_name': 'test-win-win7', 'specifiers': ['Win7', 'Debug']},
            'MOCK Win7 (dbg)(1)': {'port_name': 'test-win-win7', 'specifiers': ['Win7', 'Debug']},
            'MOCK Win7 (dbg)(2)': {'port_name': 'test-win-win7', 'specifiers': ['Win7', 'Debug']},
            'MOCK Win7': {'port_name': 'test-win-win7', 'specifiers': ['Win7', 'Release']},
        })
        self.mac_port = self.tool.port_factory.get_from_builder_name('MOCK Mac10.11')
        self.test_expectations_path = self.mac_port.path_to_generic_test_expectations_file()

        # This file must exist for Port classes to function properly.
        self._write('VirtualTestSuites', '[]')
        # Create some dummy tests (note _setup_mock_build_data uses the same test names).
        self._write('userscripts/first-test.html', 'Dummy test contents')
        self._write('userscripts/second-test.html', 'Dummy test contents')

        # In AbstractParallelRebaselineCommand._rebaseline_commands, a default port
        # object is gotten using self.tool.port_factory.get(), which is used to get
        # test paths -- and the web tests directory may be different for the "test"
        # ports and real ports. Since only "test" ports are used in this class,
        # we can make the default port also a "test" port.
        self.original_port_factory_get = self.tool.port_factory.get
        test_port = self.tool.port_factory.get('test')

        def get_test_port(port_name=None, options=None, **kwargs):
            if not port_name:
                return test_port
            return self.original_port_factory_get(port_name, options, **kwargs)

        self.tool.port_factory.get = get_test_port

    def tearDown(self):
        self.tool.port_factory.get = self.original_port_factory_get

    def _expand(self, path):
        if self.tool.filesystem.isabs(path):
            return path
        return self.tool.filesystem.join(self.mac_port.web_tests_dir(), path)

    def _read(self, path):
        return self.tool.filesystem.read_text_file(self._expand(path))

    def _write(self, path, contents):
        self.tool.filesystem.write_text_file(self._expand(path), contents)

    def _zero_out_test_expectations(self):
        for port_name in self.tool.port_factory.all_port_names():
            port = self.tool.port_factory.get(port_name)
            for path in port.expectations_files():
                self._write(path, '')
        self.tool.filesystem.written_files = {}

    def _setup_mock_build_data(self):
        for builder in ['MOCK Win7', 'MOCK Win7 (dbg)', 'MOCK Mac10.11']:
            self.tool.results_fetcher.set_results(Build(builder), WebTestResults({
                'tests': {
                    'userscripts': {
                        'first-test.html': {
                            'expected': 'PASS',
                            'actual': 'FAIL',
                            'is_unexpected': True,
                            'artifacts': {
                                'actual_image': ['first-test-actual.png'],
                                'expected_image': ['first-test-expected.png'],
                                'actual_text': ['first-test-actual.txt'],
                                'expected_text': ['first-test-expected.txt']
                            }
                        },
                        'second-test.html': {
                            'expected': 'FAIL',
                            'actual': 'FAIL',
                            'artifacts': {
                                'actual_image': ['second-test-actual.png'],
                                'expected_image': ['second-test-expected.png'],
                                'actual_audio': ['second-test-actual.wav'],
                                'expected_audio': ['second-test-expected.wav']
                            }
                        }
                    }
                }
            }))



class TestAbstractParallelRebaselineCommand(BaseTestCase):
    """Tests for the base class of multiple rebaseline commands.

    This class only contains test cases for utility methods. Some common
    behaviours of various rebaseline commands are tested in TestRebaseline.
    """

    command_constructor = AbstractParallelRebaselineCommand

    def test_builders_to_fetch_from(self):
        # pylint: disable=protected-access
        builders_to_fetch = self.command._builders_to_fetch_from(
            ['MOCK Win10', 'MOCK Win7 (dbg)(1)', 'MOCK Win7 (dbg)(2)', 'MOCK Win7'])
        self.assertEqual(builders_to_fetch, {'MOCK Win7', 'MOCK Win10'})

    def test_generic_baseline_paths(self):
        test_baseline_set = TestBaselineSet(self.tool)
        # Multiple ports shouldn't produce duplicate baseline paths.
        test_baseline_set.add('passes/text.html', Build('MOCK Win7'))
        test_baseline_set.add('passes/text.html', Build('MOCK Win10'))

        # pylint: disable=protected-access
        baseline_paths = self.command._generic_baseline_paths(test_baseline_set)
        self.assertEqual(baseline_paths, [
            '/test.checkout/wtests/passes/text-expected.png',
            '/test.checkout/wtests/passes/text-expected.txt',
            '/test.checkout/wtests/passes/text-expected.wav',
        ])

    def test_unstaged_baselines(self):
        git = self.tool.git()
        git.unstaged_changes = lambda: {
            RELATIVE_WEB_TESTS + 'x/foo-expected.txt': 'M',
            RELATIVE_WEB_TESTS + 'x/foo-expected.something': '?',
            RELATIVE_WEB_TESTS + 'x/foo-expected.png': '?',
            RELATIVE_WEB_TESTS + 'x/foo.html': 'M',
            'docs/something.md': '?',
        }
        self.assertEqual(
            self.command.unstaged_baselines(),
            [
                '/mock-checkout/' + RELATIVE_WEB_TESTS + 'x/foo-expected.png',
                '/mock-checkout/' + RELATIVE_WEB_TESTS + 'x/foo-expected.txt',
            ])


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
        return optparse.Values(dict({
            'optimize': True,
            'verbose': True,
            'results_directory': None
        }, **kwargs))

    def test_rebaseline_test_passes_on_all_builders(self):
        self.tool.results_fetcher.set_results(Build('MOCK Win7'), WebTestResults({
            'tests': {
                'userscripts': {
                    'first-test.html': {
                        'expected': 'REBASELINE',
                        'actual': 'PASS'
                    }
                }
            }
        }))

        self._write(self.test_expectations_path, 'Bug(x) userscripts/first-test.html [ Failure ]\n')
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'))
        self.command.rebaseline(self.options(), test_baseline_set)

        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_all(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'))
        self.command.rebaseline(self.options(), test_baseline_set)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Win7',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]],
                [[
                    'python', 'echo', 'optimize-baselines',
                    '--no-manifest-update',
                    '--verbose',
                    '--suffixes', 'txt,png',
                    'userscripts/first-test.html',
                ]]
            ])

    def test_rebaseline_debug(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7 (dbg)'))

        self.command.rebaseline(self.options(), test_baseline_set)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Win7 (dbg)',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]],
                [[
                    'python', 'echo', 'optimize-baselines',
                    '--no-manifest-update',
                    '--verbose',
                    '--suffixes', 'txt,png',
                    'userscripts/first-test.html',
                ]]
            ])

    def test_no_optimize(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'))
        self.command.rebaseline(self.options(optimize=False), test_baseline_set)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',

                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Win7',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]]
            ])

    def test_results_directory(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'))
        self.command.rebaseline(self.options(optimize=False, results_directory='/tmp'), test_baseline_set)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Win7',
                    '--results-directory', '/tmp',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]],
            ])

    def test_rebaseline_with_different_port_name(self):
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Win7'), 'test-win-win10')
        self.command.rebaseline(self.options(), test_baseline_set)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win10',

                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win10',
                    '--builder', 'MOCK Win7',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]],
                [[
                    'python', 'echo', 'optimize-baselines',
                    '--no-manifest-update',
                    '--verbose',
                    '--suffixes', 'txt,png',
                    'userscripts/first-test.html',
                ]]
            ])


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
            'verbose': True,
            'results_directory': None
        })

    # In the following test cases, we use a mock rebaseline-test-internal to
    # pretend userscripts/first-test.html can be rebaselined on Mac10.11, so
    # the corresponding expectation (if exists) should be updated.

    def test_rebaseline_updates_expectations_file(self):
        self._write(
            self.test_expectations_path,
            ('Bug(x) [ Mac ] userscripts/first-test.html [ Failure ]\n'
             'bug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('Bug(x) [ Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             'bug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n'))

    def test_rebaseline_updates_expectations_file_all_platforms(self):
        self._write(self.test_expectations_path, 'Bug(x) userscripts/first-test.html [ Failure ]\n')
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n')

    def test_rebaseline_handles_platform_skips(self):
        # This test is just like test_rebaseline_updates_expectations_file_all_platforms(),
        # except that if a particular port happens to SKIP a test in an overrides file,
        # we count that as passing, and do not think that we still need to rebaseline it.
        self._write(self.test_expectations_path, 'Bug(x) userscripts/first-test.html [ Failure ]\n')
        self._write('NeverFixTests', 'Bug(y) [ Android ] userscripts [ WontFix ]\n')
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n')

    def test_rebaseline_handles_skips_in_file(self):
        # This test is like test_rebaseline_handles_platform_skips, except that the
        # Skip is in the same (generic) file rather than a platform file. In this case,
        # the Skip line should be left unmodified. Note that the first line is now
        # qualified as "[Linux Mac Win]"; if it was unqualified, it would conflict with
        # the second line.
        self._write(self.test_expectations_path,
                    ('Bug(x) [ Linux Mac ] userscripts/first-test.html [ Failure ]\n'
                     'Bug(y) [ Win ] userscripts/first-test.html [ Skip ]\n'))
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('Bug(x) [ Linux Mac10.10 ] userscripts/first-test.html [ Failure ]\n'
             'Bug(y) [ Win ] userscripts/first-test.html [ Skip ]\n'))

    def test_rebaseline_handles_smoke_tests(self):
        # This test is just like test_rebaseline_handles_platform_skips, except that we check for
        # a test not being in the SmokeTests file, instead of using overrides files.
        # If a test is not part of the smoke tests, we count that as passing on ports that only
        # run smoke tests, and do not think that we still need to rebaseline it.
        self._write(self.test_expectations_path, 'Bug(x) userscripts/first-test.html [ Failure ]\n')
        self._write('SmokeTests', 'fast/html/article-element.html')
        self._setup_mock_build_data()
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/first-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n')

    # In the following test cases, the tests produce no outputs (e.g. clean
    # passing reftests, skipped tests, etc.). Hence, there are no baselines to
    # fetch (empty baseline suffixes), and rebaseline-test-internal wouldn't be
    # called. However, in some cases the expectations still need to be updated.

    def test_rebaseline_keeps_skip_expectations(self):
        # [ Skip ], [ WontFix ] expectations should always be kept.
        self._write(self.test_expectations_path,
                    ('Bug(x) [ Mac ] userscripts/skipped-test.html [ WontFix ]\n'
                     'Bug(y) [ Win ] userscripts/skipped-test.html [ Skip ]\n'))
        self._write('userscripts/skipped-test.html', 'Dummy test contents')
        self.tool.results_fetcher.set_results(Build('MOCK Mac10.11'), WebTestResults({
            'tests': {
                'userscripts': {
                    'skipped-test.html': {
                        'expected': 'WONTFIX',
                        'actual': 'SKIP',
                    }
                }
            }
        }))
        self.tool.results_fetcher.set_results(Build('MOCK Win7'), WebTestResults({
            'tests': {
                'userscripts': {
                    'skipped-test.html': {
                        'expected': 'SKIP',
                        'actual': 'SKIP',
                    }
                }
            }
        }))
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/skipped-test.html', Build('MOCK Mac10.11'))
        test_baseline_set.add('userscripts/skipped-test.html', Build('MOCK Win7'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('Bug(x) [ Mac ] userscripts/skipped-test.html [ WontFix ]\n'
             'Bug(y) [ Win ] userscripts/skipped-test.html [ Skip ]\n'))
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_keeps_flaky_expectations(self):
        # Flaky expectations should be kept even if the test passes.
        self._write(self.test_expectations_path, 'Bug(x) userscripts/flaky-test.html [ Pass Failure ]\n')
        self._write('userscripts/flaky-test.html', 'Dummy test contents')
        self.tool.results_fetcher.set_results(Build('MOCK Mac10.11'), WebTestResults({
            'tests': {
                'userscripts': {
                    'flaky-test.html': {
                        'expected': 'PASS FAIL',
                        'actual': 'PASS',
                    }
                }
            }
        }))
        test_baseline_set = TestBaselineSet(self.tool)
        test_baseline_set.add('userscripts/flaky-test.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(x) userscripts/flaky-test.html [ Pass Failure ]\n')
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly(self):
        # The test passes without any output. Its expectation should be updated
        # without calling rebaseline-test-internal.
        self._write(self.test_expectations_path, 'Bug(foo) userscripts/all-pass.html [ Failure ]\n')
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool)
        self.tool.results_fetcher.set_results(Build('MOCK Mac10.11'), WebTestResults({
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
        test_baseline_set.add('userscripts/all-pass.html', Build('MOCK Mac10.11'))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(foo) [ Linux Mac10.10 Win ] userscripts/all-pass.html [ Failure ]\n')
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly_everywhere(self):
        # Similar to test_rebaseline_test_passes_unexpectedly, except that the
        # test passes on all ports.
        self._write(self.test_expectations_path, 'Bug(foo) userscripts/all-pass.html [ Failure ]\n')
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool)
        for builder in ['MOCK Win7', 'MOCK Win10', 'MOCK Mac10.10', 'MOCK Mac10.11', 'MOCK Precise', 'MOCK Trusty']:
            self.tool.results_fetcher.set_results(Build(builder), WebTestResults({
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
            test_baseline_set.add('userscripts/all-pass.html', Build(builder))

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(new_expectations, '')
        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_test_passes_unexpectedly_but_on_another_port(self):
        # Similar to test_rebaseline_test_passes_unexpectedly, except that the
        # build was run on a different port than the port we are rebaselining
        # (possible when rebaseline-cl --fill-missing), in which case we don't
        # update the expectations.
        self._write(self.test_expectations_path, 'Bug(foo) userscripts/all-pass.html [ Failure ]\n')
        self._write('userscripts/all-pass.html', 'Dummy test contents')
        test_baseline_set = TestBaselineSet(self.tool)
        self.tool.results_fetcher.set_results(Build('MOCK Mac10.11'), WebTestResults({
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
        test_baseline_set.add('userscripts/all-pass.html', Build('MOCK Mac10.11'), 'MOCK Mac10.10')

        self.command.rebaseline(self.options(), test_baseline_set)

        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, 'Bug(foo) userscripts/all-pass.html [ Failure ]\n')
        self.assertEqual(self.tool.executive.calls, [])


class TestRebaselineExecute(BaseTestCase):
    """Tests for the main execute function of the blink_tool.py rebaseline command."""

    command_constructor = Rebaseline

    @staticmethod
    def options():
        return optparse.Values({
            'results_directory': False,
            'optimize': False,
            'builders': None,
            'suffixes': 'txt,png',
            'verbose': True
        })

    def test_rebaseline(self):
        # pylint: disable=protected-access
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']
        self._setup_mock_build_data()
        self.command.execute(self.options(), ['userscripts/first-test.html'], self.tool)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [[
                    'python', 'echo', 'copy-existing-baselines-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                ]],
                [[
                    'python', 'echo', 'rebaseline-test-internal',
                    '--verbose',
                    '--test', 'userscripts/first-test.html',
                    '--suffixes', 'txt,png',
                    '--port-name', 'test-win-win7',
                    '--builder', 'MOCK Win7',
                    '--step-name', 'webkit_layout_tests (with patch)',
                ]]
            ])

    def test_rebaseline_directory(self):
        # pylint: disable=protected-access
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']

        self._setup_mock_build_data()
        self.command.execute(self.options(), ['userscripts'], self.tool)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [
                    [
                        'python', 'echo', 'copy-existing-baselines-internal',
                        '--verbose',
                        '--test', 'userscripts/first-test.html',
                        '--suffixes', 'txt,png',
                        '--port-name', 'test-win-win7',
                    ],
                    [
                        'python', 'echo', 'copy-existing-baselines-internal',
                        '--verbose',
                        '--test', 'userscripts/second-test.html',
                        '--suffixes', 'wav,png',
                        '--port-name', 'test-win-win7',
                    ]
                ],
                [
                    [
                        'python', 'echo', 'rebaseline-test-internal',
                        '--verbose',
                        '--test', 'userscripts/first-test.html',
                        '--suffixes', 'txt,png',
                        '--port-name', 'test-win-win7',
                        '--builder', 'MOCK Win7',
                        '--step-name', 'webkit_layout_tests (with patch)',
                    ],
                    [
                        'python', 'echo', 'rebaseline-test-internal',
                        '--verbose',
                        '--test', 'userscripts/second-test.html',
                        '--suffixes', 'wav,png',
                        '--port-name', 'test-win-win7',
                        '--builder', 'MOCK Win7',
                        '--step-name', 'webkit_layout_tests (with patch)',
                    ]
                ]
            ])


class TestBaselineSetTest(unittest.TestCase):

    def setUp(self):
        host = MockBlinkTool()
        host.port_factory = MockPortFactory(host)
        port = host.port_factory.get()
        base_dir = port.web_tests_dir()
        host.filesystem.write_text_file(base_dir + '/a/x.html', '<html>')
        host.filesystem.write_text_file(base_dir + '/a/y.html', '<html>')
        host.filesystem.write_text_file(base_dir + '/a/z.html', '<html>')
        host.builders = BuilderList({
            'MOCK Mac10.12': {'port_name': 'test-mac-mac10.12', 'specifiers': ['Mac10.12', 'Release']},
            'MOCK Trusty': {'port_name': 'test-linux-trusty', 'specifiers': ['Trusty', 'Release']},
            'MOCK Win10': {'port_name': 'test-win-win10', 'specifiers': ['Win10', 'Release']},
        })
        self.host = host

    def test_add_and_iter_tests(self):
        test_baseline_set = TestBaselineSet(host=self.host)
        test_baseline_set.add('a', Build('MOCK Trusty'))
        test_baseline_set.add('a/z.html', Build('MOCK Win10'))
        self.assertEqual(
            list(test_baseline_set),
            [
                ('a/x.html', Build(builder_name='MOCK Trusty'), 'test-linux-trusty'),
                ('a/y.html', Build(builder_name='MOCK Trusty'), 'test-linux-trusty'),
                ('a/z.html', Build(builder_name='MOCK Trusty'), 'test-linux-trusty'),
                ('a/z.html', Build(builder_name='MOCK Win10'), 'test-win-win10'),
            ])
        self.assertEqual(
            test_baseline_set.all_tests(), ['a/x.html', 'a/y.html', 'a/z.html'])

    def test_str_empty(self):
        test_baseline_set = TestBaselineSet(host=self.host)
        self.assertEqual(str(test_baseline_set), '<Empty TestBaselineSet>')

    def test_str_basic(self):
        test_baseline_set = TestBaselineSet(host=self.host)
        test_baseline_set.add('a/x.html', Build('MOCK Mac10.12'))
        test_baseline_set.add('a/x.html', Build('MOCK Win10'))
        self.assertEqual(
            str(test_baseline_set),
            ('<TestBaselineSet with:\n'
             '  a/x.html: Build(builder_name=\'MOCK Mac10.12\', build_number=None), test-mac-mac10.12\n'
             '  a/x.html: Build(builder_name=\'MOCK Win10\', build_number=None), test-win-win10>'))

    def test_getters(self):
        test_baseline_set = TestBaselineSet(host=self.host)
        test_baseline_set.add('a/x.html', Build('MOCK Mac10.12'))
        test_baseline_set.add('a/x.html', Build('MOCK Win10'))
        self.assertEqual(test_baseline_set.test_prefixes(), ['a/x.html'])
        self.assertEqual(
            test_baseline_set.build_port_pairs('a/x.html'),
            [
                (Build(builder_name='MOCK Mac10.12'), 'test-mac-mac10.12'),
                (Build(builder_name='MOCK Win10'), 'test-win-win10')
            ])
