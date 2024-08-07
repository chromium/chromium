# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import BuildStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_testing import LoggingTestCase

from blinkpy.w3c.gerrit_mock import MockGerritAPI
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.wpt_manifest import (
    WPTManifest, BASE_MANIFEST_NAME, MANIFEST_NAME)

from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.web_tests.port.test import MOCK_WEB_TESTS


@mock.patch('blinkpy.tool.commands.build_resolver.GerritAPI', MockGerritAPI)
class WPTExpectationsUpdaterTest(LoggingTestCase):
    def mock_host(self):
        """Returns a mock host with fake values set up for testing."""
        host = MockHost()
        host.port_factory = MockPortFactory(host)
        host.executive._output = ''

        # Set up a fake list of try builders.
        host.builders = BuilderList({
            'MOCK Try Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
            'MOCK Try Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
            'MOCK Try Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'main': 'tryserver.blink',
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                    'fake_flag_blink_wpt_tests': {
                        'flag_specific': 'fake-flag',
                    },
                },
            },
            'MOCK Try Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
            'MOCK Try Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
            'MOCK Try Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_wpt_tests': {},
                },
            },
        })
        # Null `SearchBuilds` response so that `BuildResolver` ignores CI
        # builds.
        host.web.append_prpc_response({})

        fs = host.filesystem
        port = host.port_factory.get()
        for path in ('TestExpectations', 'FlagExpectations/fake-flag'):
            fs.write_text_file(
                fs.join(port.web_tests_dir(), path),
                textwrap.dedent("""\
                    # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                    # results: [ Timeout Crash Pass Failure Skip ]
                    """))
        fs.write_text_file(fs.join(port.web_tests_dir(), 'FlagSpecificConfig'),
                           json.dumps([{
                               'name': 'fake-flag',
                               'args': [],
                           }]))
        # Write a dummy manifest file, describing what tests exist.
        fs.write_text_file(
            fs.join(port.web_tests_dir(), 'external', BASE_MANIFEST_NAME),
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'abcdef123',
                            [None, [['/reftest-ref.html', '==']], {}]
                        ]
                    },
                    'testharness': {
                        'test/path.html': ['abcdef123', [None, {}]],
                        'test/zzzz.html': ['ghijkl456', [None, {}]],
                        'fake/some_test.html':
                        ['ghijkl456', ['fake/some_test.html?HelloWorld', {}]],
                        'fake/file/deleted_path.html':
                        ['ghijkl456', [None, {}]],
                        'test/task.js': [
                            'mnpqrs789', ['test/task.html', {}],
                            ['test/task2.html', {}]
                        ],
                    },
                    'manual': {
                        'x-manual.html': ['abcdef123', [None, {}]],
                    },
                },
            }))

        return host

    def mock_updater(self, host) -> WPTExpectationsUpdater:
        updater = WPTExpectationsUpdater(host)
        updater.git_cl = MockGitCL(
            host, {
                Build('MOCK Try Mac10.10', 333, 'Build-1'):
                BuildStatus.FAILURE,
                Build('MOCK Try Mac10.11', 111, 'Build-2'):
                BuildStatus.FAILURE,
                Build('MOCK Try Trusty', 222, 'Build-3'): BuildStatus.FAILURE,
                Build('MOCK Try Precise', 333, 'Build-4'): BuildStatus.FAILURE,
                Build('MOCK Try Win10', 444, 'Build-5'): BuildStatus.FAILURE,
                Build('MOCK Try Win7', 555, 'Build-6'): BuildStatus.FAILURE,
            })
        return updater

    def test_suite_for_builder(self):
        host = self.mock_host()
        host.builders = BuilderList({
            'MOCK Try Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'blink_web_tests': {},
                    'blink_wpt_tests': {},
                    'webdriver_wpt_tests': {},
                    'fake_flag_blink_wpt_tests': {
                        'flag_specific': 'fake-flag',
                    },
                },
            },
        })

        updater = WPTExpectationsUpdater(host)
        self.assertEqual(
            updater.suites_for_builder('MOCK Try Trusty'), {
                'blink_wpt_tests',
                'webdriver_wpt_tests',
                'fake_flag_blink_wpt_tests',
            })

    def test_run_single_platform_failure(self):
        """Tests the main run method in a case where one test fails on one platform."""
        host = self.mock_host()

        # Fill in an initial value for TestExpectations
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            textwrap.dedent(f"""\
                # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                # results: [ Pass Timeout ]
                """))

        # Set up fake try job results.
        updater = WPTExpectationsUpdater(host)
        updater.git_cl = MockGitCL(
            updater.host, {
                Build('MOCK Try Mac10.10', 333, 'Build-1'):
                BuildStatus.FAILURE,
                Build('MOCK Try Mac10.11', 111, 'Build-2'):
                BuildStatus.SUCCESS,
                Build('MOCK Try Trusty', 222, 'Build-3'): BuildStatus.SUCCESS,
                Build('MOCK Try Precise', 333, 'Build-4'): BuildStatus.SUCCESS,
                Build('MOCK Try Win10', 444, 'Build-5'): BuildStatus.SUCCESS,
                Build('MOCK Try Win7', 555, 'Build-6'): BuildStatus.SUCCESS,
            })

        # Set up failing results for one try bot. It shouldn't matter what
        # results are for the other builders since we shouldn't need to even
        # fetch results, since the try job status already tells us that all
        # of the tests passed.
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/test/path.html': [{
                    'status': 'ABORT'
                }] * 3},
                # The real `TestResultsFetcher.gather_results` removes the `(with
                # patch)` suffix anyways. The mock is not that sophisticated, so
                # omit the suffix here.
                step_name='blink_wpt_tests'))

        self.assertEqual(0, updater.run())
        self.assertEqual(
            host.filesystem.read_text_file(expectations_path),
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                # results: [ Pass Timeout ]

                # ====== New tests from wpt-importer added here ======
                [ Mac10.10 ] external/wpt/test/path.html [ Timeout ]
                """))

    def test_run_webdriver_only_failure(self):
        host = self.mock_host()
        host.builders = BuilderList({
            'MOCK Try Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Linux', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'webdriver_wpt_tests': {},
                },
            },
        })
        updater = WPTExpectationsUpdater(host)
        expectations_path = updater.finder.path_from_web_tests(
            'TestExpectations')
        host.filesystem.write_text_file(
            expectations_path,
            textwrap.dedent("""\
                # results: [ Timeout ]
                external/wpt/not-a-wdspec-test.html [ Timeout ]
                # ====== New tests from wpt-importer added here ======
                """))

        updater.git_cl = MockGitCL(
            updater.host, {
                Build('MOCK Try Linux', 333, 'Build-4'): BuildStatus.FAILURE,
            })
        host.results_fetcher.set_results(
            Build('MOCK Try Linux', 333, 'Build-4'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/webdriver/test.py': [{
                    'status': 'ABORT'
                }] * 3},
                step_name='webdriver_wpt_tests'))

        self.assertEqual(0, updater.run())
        self.assertEqual(
            host.filesystem.read_text_file(expectations_path),
            textwrap.dedent("""\
                # results: [ Timeout ]
                external/wpt/not-a-wdspec-test.html [ Timeout ]
                # ====== New tests from wpt-importer added here ======
                external/wpt/webdriver/test.py [ Timeout ]
                """))

    def test_run_inherited_results(self):
        host = self.mock_host()
        # Fill in an initial value for TestExpectations
        port = host.port_factory.get('test-linux-trusty')
        expectations_path = port.path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                # results: [ Timeout Crash Pass Failure Skip ]

                # ====== New tests from wpt-importer added here ======
                """))
        host.filesystem.write_text_file(
            port.path_to_never_fix_tests_file(),
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                # results: [ Skip ]
                # Mac10.10 is an old platform that does not run the failing
                # test. The updater should still generate a `[ Mac ]`
                # expectation instead of `[ Mac10.11 ]`.
                [ Mac10.10 ] external/wpt/test/path.html [ Skip ]
                """))
        # Set up fake try job results.
        updater = WPTExpectationsUpdater(host)
        updater.git_cl = MockGitCL(
            updater.host, {
                Build('MOCK Try Mac10.10', 333, 'Build-1'):
                BuildStatus.SUCCESS,
                Build('MOCK Try Mac10.11', 111, 'Build-2'):
                BuildStatus.FAILURE,
                Build('MOCK Try Trusty', 222, 'Build-3'): BuildStatus.FAILURE,
                Build('MOCK Try Precise', 333, 'Build-4'):
                BuildStatus.INFRA_FAILURE,
                Build('MOCK Try Win10', 444, 'Build-5'): BuildStatus.SUCCESS,
                Build('MOCK Try Win7', 555, 'Build-6'): BuildStatus.SUCCESS,
            })

        rdb_results = {
            'external/wpt/test/path.html': [{
                'status': 'ABORT',
                'expected': False,
            }] * 3,
        }
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.11', 111, 'Build-2'),
            WebTestResults.from_rdb_responses(rdb_results,
                                              step_name='blink_wpt_tests'))
        # Precise should be filled in from Trusty.
        host.results_fetcher.set_results(
            Build('MOCK Try Precise', 333, 'Build-4'),
            WebTestResults.from_rdb_responses({}, step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(rdb_results,
                                              step_name='blink_wpt_tests'))

        self.assertEqual(0, updater.run())
        self.assertEqual(
            host.filesystem.read_text_file(expectations_path),
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac Trusty Precise Linux Win7 Win10 Win ]
                # results: [ Timeout Crash Pass Failure Skip ]

                # ====== New tests from wpt-importer added here ======
                [ Mac ] external/wpt/test/path.html [ Timeout ]
                [ Linux ] external/wpt/test/path.html [ Timeout ]
                """))

    def test_run_single_flag_specific_failure(self):
        """Tests the main run method in a case where one test fails on one
        flag-specific suite.
        """
        host = self.mock_host()

        # Fill in an initial value for TestExpectations
        expectations_path = \
            host.port_factory.get().path_to_flag_specific_expectations_file('fake-flag')
        host.filesystem.write_text_file(
            expectations_path, WPTExpectationsUpdater.MARKER_COMMENT + '\n')

        # Set up fake try job results.
        updater = WPTExpectationsUpdater(host)
        updater.git_cl = MockGitCL(
            updater.host, {
                Build('MOCK Try Mac10.10', 333, 'Build-1'):
                BuildStatus.SUCCESS,
                Build('MOCK Try Mac10.11', 111, 'Build-2'):
                BuildStatus.SUCCESS,
                Build('MOCK Try Trusty', 222, 'Build-3'): BuildStatus.FAILURE,
                Build('MOCK Try Precise', 333, 'Build-4'): BuildStatus.SUCCESS,
                Build('MOCK Try Win10', 444, 'Build-5'): BuildStatus.SUCCESS,
                Build('MOCK Try Win7', 555, 'Build-6'): BuildStatus.SUCCESS,
            })

        # Set up failing results for one try bot. It shouldn't matter what
        # results are for the other builders since we shouldn't need to even
        # fetch results, since the try job status already tells us that all
        # of the tests passed.
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/test/path.html': [{
                    'status': 'ABORT'
                }] * 3},
                step_name='fake_flag_blink_wpt_tests'))

        # `updater.run` does not update flag-specific expectations.
        updater.update_expectations()
        self.assertEqual(
            host.filesystem.read_text_file(expectations_path),
            '# ====== New tests from wpt-importer added here ======\n'
            'external/wpt/test/path.html [ Timeout ]\n')

    def test_filter_results_for_update_only_passing_results(self):
        host = self.mock_host()
        results = WebTestResults.from_rdb_responses(
            {
                'external/wpt/x/passing-test.html': [{
                    'status': 'PASS',
                    'expected': True,
                }] * 3,
            },
            step_name='blink_wpt_tests',
            build=Build('MOCK Try Mac10.10'))
        updater = WPTExpectationsUpdater(host)
        _, filtered_results = updater.filter_results_for_update(results)
        self.assertEqual(0, len(filtered_results))
        self.assertEqual('blink_wpt_tests', results.step_name())
        self.assertEqual('MOCK Try Mac10.10', results.builder_name)

    def test_filter_results_for_update_unexpected_pass(self):
        host = self.mock_host()
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/x/passing-test.html': [{
                'status': 'PASS'
            }] * 3},
            step_name='blink_wpt_tests',
            build=Build('MOCK Try Mac10.10'))
        updater = WPTExpectationsUpdater(host)
        _, filtered_results = updater.filter_results_for_update(results)
        self.assertEqual(0, len(filtered_results))

    def test_filter_results_for_update_some_failing_results(self):
        host = self.mock_host()
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/x/failing-test.html': [{
                'status': 'ABORT'
            }] * 3},
            step_name='blink_wpt_tests',
            build=Build('MOCK Try Mac10.10'))
        updater = WPTExpectationsUpdater(host)
        _, (result, ) = updater.filter_results_for_update(results)
        self.assertEqual('external/wpt/x/failing-test.html',
                         result.test_name())
        self.assertEqual({'TIMEOUT'}, set(result.actual_results()))

    def test_filter_results_for_update_non_wpt_test(self):
        host = self.mock_host()
        # This shouldn't happen because WPT suite results shouldn't contain
        # non-WPT tests in the first place.
        results = WebTestResults.from_rdb_responses(
            {'x/failing-test.html': [{
                'status': 'FAIL',
            }] * 3},
            step_name='blink_wpt_tests',
            build=Build('MOCK Try Mac10.10'))
        updater = WPTExpectationsUpdater(host)
        _, filtered_results = updater.filter_results_for_update(results)
        self.assertEqual(0, len(filtered_results))

    def test_filter_results_for_update_not_retried_test(self):
        host = self.mock_host()
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/reftest.html': [{
                'status': 'FAIL',
            }]},
            step_name='blink_wpt_tests',
            build=Build('MOCK Try Mac10.10'))
        updater = WPTExpectationsUpdater(host)
        _, filtered_results = updater.filter_results_for_update(results)
        self.assertEqual(0, len(filtered_results))

    def test_remove_configurations(self):
        host = self.mock_host()

        initial_expectations = textwrap.dedent("""\
            # tags: [ Android Fuchsia Linux Mac Mac10.12 Mac10.15 Mac11 Win Win7 Win10 ]
            # results: [ Timeout Crash Pass Failure Skip ]
            crbug.com/1234 external/wpt/test/foo.html [ Failure ]
            crbug.com/1235 [ Win ] external/wpt/test/bar.html [ Timeout ]
            """)

        final_expectations = textwrap.dedent("""\
            # tags: [ Android Fuchsia Linux Mac Mac10.12 Mac10.15 Mac11 Win Win7 Win10 ]
            # results: [ Timeout Crash Pass Failure Skip ]
            crbug.com/1234 [ Linux ] external/wpt/test/foo.html [ Failure ]
            crbug.com/1234 [ Mac ] external/wpt/test/foo.html [ Failure ]
            crbug.com/1234 [ Win10 ] external/wpt/test/foo.html [ Failure ]
            crbug.com/1235 [ Win10 ] external/wpt/test/bar.html [ Timeout ]

            # ====== New tests from wpt-importer added here ======
            [ Win7 ] external/wpt/test/bar.html [ Failure Timeout ]
            [ Win7 ] external/wpt/test/foo.html [ Failure Timeout ]
            """)

        # Fill in an initial value for TestExpectations
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(expectations_path, initial_expectations)

        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Win7', 555, 'Build-6'),
            WebTestResults.from_rdb_responses(
                {
                    'external/wpt/test/foo.html': [{
                        'status': 'FAIL',
                        'expected': True,
                    }] * 2 + [{
                        'status': 'ABORT',
                    }],
                    'external/wpt/test/bar.html': [{
                        'status': 'FAIL',
                    }] * 2 + [{
                        'status': 'ABORT',
                        'expected': True,
                    }],
                },
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(value, final_expectations)

    def test_create_line_dict_for_flag_specific(self):
        # In this example, there are three unexpected results for wpt tests.
        # One of them has match results in generic test expectations,
        # another one has non-match results, and the last one has no
        # corresponding line in generic test expectations.
        host = self.mock_host()
        port = host.port_factory.get('test-linux-trusty')

        # Fill in an initial value for TestExpectations
        expectations_path = port.path_to_generic_test_expectations_file()
        content = (
            "# results: [ Timeout Crash Pass Failure Skip ]\n"
            "external/wpt/reftest.html [ Failure ]\n"
            "external/wpt/test/path.html [ Failure ]\n")
        host.filesystem.write_text_file(expectations_path, content)

        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {
                    'external/wpt/reftest.html': [{
                        'status': 'FAIL',
                        'expected': True,
                    }] * 3,
                    'external/wpt/test/path.html': [{
                        'status': 'CRASH',
                    }] * 3,
                    'external/wpt/test/zzzz.html': [{
                        'status': 'CRASH',
                    }] * 3,
                },
                step_name='fake_flag_blink_wpt_tests'))

        updater.update_expectations()
        port.set_option_default('flag_specific', 'fake-flag')
        expectations = TestExpectations(port)
        self.assertEqual(
            expectations.get_expectations_from_file(
                expectations_path, 'external/wpt/test/zzzz.html'), [])

        expectations_path = port.path_to_flag_specific_expectations_file(
            'fake-flag')
        self.assertEqual(
            expectations.get_expectations_from_file(
                expectations_path, 'external/wpt/reftest.html'), [])
        (line, ) = expectations.get_expectations_from_file(
            expectations_path, 'external/wpt/test/path.html')
        self.assertEqual(line.reason, '')
        self.assertEqual(line.tags, set())
        self.assertEqual(line.results, {ResultType.Crash})
        (line, ) = expectations.get_expectations_from_file(
            expectations_path, 'external/wpt/test/zzzz.html')
        self.assertEqual(line.reason, '')
        self.assertEqual(line.tags, set())
        self.assertEqual(line.results, {ResultType.Crash})

    def test_create_line_dict_new_tests(self):
        # In this example, there are three unexpected results for wpt tests.
        # The new test expectation lines are sorted by test, and then specifier.
        host = self.mock_host()
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/test/zzzz.html': [{
                    'status': 'CRASH',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {
                    'virtual/foo/external/wpt/test/zzzz.html':
                    [{
                        'status': 'ABORT',
                    }] * 3,
                },
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.11', 111, 'Build-2'),
            WebTestResults.from_rdb_responses(
                {
                    'virtual/foo/external/wpt/test/zzzz.html':
                    [{
                        'status': 'ABORT',
                    }] * 3,
                },
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        expectations = TestExpectations(updater.port)
        path = updater.port.path_to_generic_test_expectations_file()
        (line, ) = expectations.get_expectations_from_file(
            path, 'external/wpt/test/zzzz.html')
        self.assertEqual(line.reason, '')
        self.assertEqual(line.tags, {'mac10.10'})
        self.assertEqual(line.results, {ResultType.Crash})

        lines = expectations.get_expectations_from_file(
            path, 'virtual/foo/external/wpt/test/zzzz.html')
        self.assertEqual(len(lines), 2)
        line1, line2 = sorted(lines, key=lambda line: line.tags)
        self.assertEqual(line1.reason, '')
        self.assertEqual(line1.tags, {'mac10.11'})
        self.assertEqual(line1.results, {ResultType.Timeout})
        self.assertEqual(line2.reason, '')
        self.assertEqual(line2.tags, {'trusty'})
        self.assertEqual(line2.results, {ResultType.Timeout})

    def test_create_line_dict_with_asterisks(self):
        # Literal asterisks in test names need to be escaped in expectations.
        updater = WPTExpectationsUpdater(self.mock_host())
        results = WebTestResults.from_rdb_responses(
            {
                'external/wpt/html/dom/interfaces.https.html?exclude=(Document.*|HTML.*)':
                [{
                    'status': 'FAIL',
                }] * 3
            },
            build=Build('MOCK Try Trusty'))
        line_dict = updater.write_to_test_expectations([results])
        self.assertEqual(
            line_dict, {
                'external/wpt/html/dom/interfaces.https.html?exclude=(Document.*|HTML.*)':
                [
                    'external/wpt/html/dom/'
                    'interfaces.https.html?exclude=(Document.\*|HTML.\*) '
                    '[ Failure ]',
                ],
            })

    def test_unsimplifiable_specifiers(self):
        host = self.mock_host()
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Win7', 555, 'Build-6'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x/z.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {
                    'external/wpt/x/y.html': [{
                        'status': 'ABORT',
                    }] * 3,
                    'external/wpt/x/z.html': [{
                        'status': 'ABORT',
                    }] * 3,
                },
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        expectations = TestExpectations(updater.port)
        path = updater.port.path_to_generic_test_expectations_file()
        (line, ) = expectations.get_expectations_from_file(
            path, 'external/wpt/x/y.html')
        self.assertEqual(line.tags, {'mac10.10'})
        line1, line2 = expectations.get_expectations_from_file(
            path, 'external/wpt/x/z.html')
        self.assertEqual(line1.tags | line2.tags, {'win7', 'mac10.10'})

    def test_specifiers_can_extend_to_all_platforms(self):
        host = self.mock_host()
        expectations_path = MOCK_WEB_TESTS + 'NeverFixTests'
        host.filesystem.write_text_file(
            expectations_path,
            ('# tags: [ Linux ]\n'
             '# results: [ Skip ]\n'
             'crbug.com/111 [ Linux ] external/wpt/reftest.html [ Skip ]\n'))
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/test.html', '')
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/reftest.html': [{
                    'status': 'FAIL',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Win10', 444, 'Build-5'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/reftest.html': [{
                    'status': 'FAIL',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Win7', 555, 'Build-6'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/reftest.html': [{
                    'status': 'FAIL',
                }] * 3},
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        expectations = TestExpectations(updater.port)
        path = updater.port.path_to_generic_test_expectations_file()
        line1, line2 = expectations.get_expectations_from_file(
            path, 'external/wpt/reftest.html')
        self.assertEqual(line1.tags | line2.tags, {'mac10.10', 'win'})

        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.11', 111, 'Build-2'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/reftest.html': [{
                    'status': 'FAIL',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.web.append_prpc_response({})
        updater.update_expectations()

        expectations = TestExpectations(updater.port)
        (line, ) = expectations.get_expectations_from_file(
            path, 'external/wpt/reftest.html')
        self.assertEqual(line.tags, set())

    def test_normalized_specifiers_with_skipped_test(self):
        host = self.mock_host()
        expectations_path = MOCK_WEB_TESTS + 'NeverFixTests'
        host.filesystem.write_text_file(
            expectations_path,
            ('# tags: [ Linux Mac10.11 ]\n'
             '# results: [ Skip ]\n'
             'crbug.com/111 [ Linux ] external/wpt/x/y.html [ Skip ]\n'
             'crbug.com/111 [ Mac10.11 ] external/wpt/x/y.html [ Skip ]\n'))
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/test.html', '')

        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x/y.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Win7', 555, 'Build-6'),
            WebTestResults.from_rdb_responses(
                {
                    'external/wpt/x/y.html': [{
                        'status': 'ABORT',
                    }] * 3,
                    'external/wpt/x/z.html': [{
                        'status': 'ABORT',
                    }] * 3,
                },
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Win10', 444, 'Build-5'),
            WebTestResults.from_rdb_responses(
                {
                    'external/wpt/x/y.html': [{
                        'status': 'ABORT',
                    }] * 3,
                    'external/wpt/x/z.html': [{
                        'status': 'ABORT',
                    }] * 3,
                },
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        expectations = TestExpectations(updater.port)
        path = updater.port.path_to_generic_test_expectations_file()
        (line, ) = expectations.get_expectations_from_file(
            path, 'external/wpt/x/y.html')
        self.assertEqual(line.tags, set())
        (line, ) = expectations.get_expectations_from_file(
            path, 'external/wpt/x/z.html')
        self.assertEqual(line.tags, {'win'})

    def test_no_expectations_to_write(self):
        host = self.mock_host()
        updater = self.mock_updater(host)
        _, exp_dict = updater.update_expectations()
        self.assertEqual(exp_dict, {})
        logs = ''.join(self.logMessages()).lower()
        self.assertIn('no lines to write to testexpectations.', logs)

    def test_cleanup_outside_affected_expectations_in_cl(self):
        host = self.mock_host()
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            '# tags: [ Linux ]\n' +
            '# results: [ Pass Failure ]\n' +
            WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
            '[ linux ] external/wpt/fake/some_test.html?HelloWorld [ Failure ]\n' +
            'external/wpt/fake/file/non_existent_file.html [ Pass ]\n' +
            'external/wpt/fake/file/deleted_path.html [ Pass ]\n')
        updater = WPTExpectationsUpdater(
            host, ['--clean-up-test-expectations-only'])
        updater.port.tests = lambda: {
            'external/wpt/fake/new.html?HelloWorld'}

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return 'external/wpt/fake/file/deleted_path.html'
            if '--diff-filter=R' in cmd:
                return 'C\texternal/wpt/fake/some_test.html\texternal/wpt/fake/new.html'
            return ''

        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path
        updater.run()

        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value, ('# tags: [ Linux ]\n' +
                    '# results: [ Pass Failure ]\n' +
                    WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
                    '[ linux ] external/wpt/fake/new.html?HelloWorld [ Failure ]\n'))

    def test_clean_expectations_for_deleted_test_harness(self):
        host = self.mock_host()
        port = host.port_factory.get()
        expectations_path = \
            port.path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            '# tags: [ Win Linux ]\n' +
            '# results: [ Pass Failure ]\n' +
            WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
            '[ linux ] wpt_internal/test/task.html [ Failure ]\n' +
            '[ win ] wpt_internal/test/task2.html [ Failure ]\n' +
            '[ linux ] external/wpt/test/task.html [ Failure ]\n' +
            'external/wpt/test/task2.html [ Pass ]\n')

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return '\n'.join(['external/wpt/test/task.js',
                                  'wpt_internal/test/task.js'])
            return ''

        wpt_manifest = port.wpt_manifest('external/wpt')
        host.filesystem.maybe_make_directory(
            port.web_tests_dir(), 'wpt_internal')
        host.filesystem.copyfile(
            host.filesystem.join(port.web_tests_dir(),
                                 'external', 'wpt', MANIFEST_NAME),
            host.filesystem.join(port.web_tests_dir(), 'wpt_internal',
                                 MANIFEST_NAME))
        wpt_internal_manifest = WPTManifest.from_file(
            port,
            host.filesystem.join(port.web_tests_dir(), 'wpt_internal',
                                 MANIFEST_NAME))

        updater = WPTExpectationsUpdater(
            host,
            ['--clean-up-affected-tests-only',
             '--clean-up-test-expectations-only'],
            [wpt_manifest, wpt_internal_manifest])
        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path
        updater.cleanup_test_expectations_files()

        results = WebTestResults.from_rdb_responses(
            {'external/wpt/fake/file/path.html': [{
                'status': 'FAIL',
            }] * 3},
            build=Build('MOCK Try Trusty'))
        skip_path = host.port_factory.get().path_to_never_fix_tests_file()
        skip_value_origin = host.filesystem.read_text_file(skip_path)

        updater.write_to_test_expectations([results])
        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            textwrap.dedent("""\
                # tags: [ Win Linux ]
                # results: [ Pass Failure ]

                # ====== New tests from wpt-importer added here ======
                external/wpt/fake/file/path.html [ Failure ]
                """))
        skip_value = host.filesystem.read_text_file(skip_path)
        self.assertMultiLineEqual(skip_value, skip_value_origin)

    def test_write_to_test_expectations_and_cleanup_expectations(self):
        host = self.mock_host()
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            '# tags: [ Linux ]\n' +
            '# results: [ Pass Failure ]\n' +
            WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
            '[ linux ] external/wpt/fake/some_test.html?HelloWorld [ Failure ]\n' +
            'external/wpt/fake/file/deleted_path.html [ Pass ]\n')
        updater = WPTExpectationsUpdater(
            host, ['--clean-up-affected-tests-only',
                   '--clean-up-test-expectations-only'])

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return 'external/wpt/fake/file/deleted_path.html'
            if '--diff-filter=R' in cmd:
                return 'C\texternal/wpt/fake/some_test.html\texternal/wpt/fake/new.html'
            return ''

        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path
        updater.cleanup_test_expectations_files()

        results = WebTestResults.from_rdb_responses(
            {'external/wpt/fake/file/path.html': [{
                'status': 'FAIL',
            }] * 3},
            build=Build('MOCK Try Trusty'))
        skip_path = host.port_factory.get().path_to_never_fix_tests_file()
        skip_value_origin = host.filesystem.read_text_file(skip_path)

        updater.write_to_test_expectations([results])
        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            ('# tags: [ Linux ]\n' + '# results: [ Pass Failure ]\n' +
             WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
             'external/wpt/fake/file/path.html [ Failure ]\n' +
             '[ linux ] external/wpt/fake/new.html?HelloWorld [ Failure ]\n'))
        skip_value = host.filesystem.read_text_file(skip_path)
        self.assertMultiLineEqual(skip_value, skip_value_origin)

    def test_clean_up_affected_tests_arg_raises_exception(self):
        host = self.mock_host()
        with self.assertRaises(AssertionError) as ctx:
            updater = WPTExpectationsUpdater(
                host, ['--clean-up-affected-tests-only'])
            updater.run()
        self.assertIn('Cannot use --clean-up-affected-tests-only',
                      str(ctx.exception))

    def test_clean_up_affected_tests_arg_does_not_raise_exception(self):
        host = self.mock_host()
        updater = WPTExpectationsUpdater(
            host, ['--clean-up-affected-tests-only',
                   '--clean-up-test-expectations'])

    def test_write_to_test_expectations_with_marker_comment(self):
        host = self.mock_host()
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            textwrap.dedent(f"""\
                # tags: [ Trusty ]
                # results: [ Timeout ]
                {WPTExpectationsUpdater.MARKER_COMMENT}
                """))
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x/y.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        skip_path = host.port_factory.get().path_to_never_fix_tests_file()
        skip_value_origin = host.filesystem.read_text_file(skip_path)
        value = host.filesystem.read_text_file(expectations_path)
        self.assertEqual(
            value,
            textwrap.dedent(f"""\
                # tags: [ Trusty ]
                # results: [ Timeout ]
                {WPTExpectationsUpdater.MARKER_COMMENT}
                [ Trusty ] external/wpt/x/y.html [ Timeout ]
                """))
        skip_value = host.filesystem.read_text_file(skip_path)
        self.assertMultiLineEqual(skip_value, skip_value_origin)

    def test_write_to_test_expectations_with_no_marker_comment(self):
        host = self.mock_host()
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.write_text_file(
            expectations_path,
            textwrap.dedent("""\
                # tags: [ Trusty ]
                # results: [ Pass Failure Timeout ]

                crbug.com/111 [ Trusty ] foo/bar.html [ Failure ]
                """))
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x/y.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        updater.update_expectations()

        skip_path = host.port_factory.get().path_to_never_fix_tests_file()
        skip_value_origin = host.filesystem.read_text_file(skip_path)
        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            textwrap.dedent("""\
                # tags: [ Trusty ]
                # results: [ Pass Failure Timeout ]

                crbug.com/111 [ Trusty ] foo/bar.html [ Failure ]

                # ====== New tests from wpt-importer added here ======
                [ Trusty ] external/wpt/x/y.html [ Timeout ]
                """))
        skip_value = host.filesystem.read_text_file(skip_path)
        self.assertMultiLineEqual(skip_value, skip_value_origin)

    def test_write_to_test_expectations_with_marker_and_no_lines(self):
        host = self.mock_host()
        expectations_path = \
            host.port_factory.get().path_to_generic_test_expectations_file()
        raw_exps = '# tags: [ Trusty ]\n# results: [ Pass ]\n'
        host.filesystem.write_text_file(
            expectations_path,
            raw_exps + '\n' + WPTExpectationsUpdater.MARKER_COMMENT + '\n' +
            '[ Trusty ] fake/file/path.html [ Pass ]\n')
        updater = self.mock_updater(host)
        updater.update_expectations()

        skip_path = host.port_factory.get().path_to_never_fix_tests_file()
        skip_value_origin = host.filesystem.read_text_file(skip_path)
        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value, raw_exps + '\n' + WPTExpectationsUpdater.MARKER_COMMENT +
            '\n' + '[ Trusty ] fake/file/path.html [ Pass ]\n')
        skip_value = host.filesystem.read_text_file(skip_path)
        self.assertMultiLineEqual(skip_value, skip_value_origin)

    def test_is_reference_test_given_testharness_test(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertFalse(updater.is_reference_test('test/path.html'))

    def test_is_reference_test_given_reference_test(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertTrue(updater.is_reference_test('external/wpt/reftest.html'))

    def test_is_reference_test_given_non_existent_file(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertFalse(updater.is_reference_test('foo/bar.html'))

    def test_get_test_to_rebaseline_returns_only_tests_with_failures(self):
        host = self.mock_host()
        updater = WPTExpectationsUpdater(host)
        raw_results = WebTestResults.from_rdb_responses({
            'external/wpt/x/y.html': [{
                'status': 'PASS',
            }] * 3,
            'external/wpt/x/z.html': [{
                'status': 'FAIL',
            }] * 3,
        })
        tests_to_rebaseline, results = updater.filter_results_for_update(
            raw_results)
        self.assertEqual(len(results), 0)
        self.assertEqual(tests_to_rebaseline, {'external/wpt/x/z.html'})

    def test_get_test_to_rebaseline_does_not_return_ref_tests(self):
        host = self.mock_host()
        updater = WPTExpectationsUpdater(host)
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/reftest.html': [{
                'status': 'FAIL',
            }] * 3})
        tests_to_rebaseline, _ = updater.filter_results_for_update(results)
        self.assertEqual(tests_to_rebaseline, set())
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/reftest.html': [{
                'status': 'ABORT',
            }] * 3})
        tests_to_rebaseline, _ = updater.filter_results_for_update(results)
        self.assertEqual(tests_to_rebaseline, set())
        results = WebTestResults.from_rdb_responses(
            {'external/wpt/reftest.html': [{
                'status': 'PASS',
            }] * 3})
        tests_to_rebaseline, _ = updater.filter_results_for_update(results)
        self.assertEqual(tests_to_rebaseline, set())

    def test_get_tests_to_rebaseline_some_rebaselined(self):
        host = self.mock_host()
        updater = WPTExpectationsUpdater(host)
        raw_results = WebTestResults.from_rdb_responses({
            'external/wpt/x/y.html': [{
                'status': 'FAIL',
            }] * 3,
            'external/wpt/x/z.html': [{
                'status': 'ABORT',
            }] * 3,
        })

        tests_to_rebaseline, (
            result, ) = updater.filter_results_for_update(raw_results)
        self.assertEqual(tests_to_rebaseline, {'external/wpt/x/y.html'})
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(result.test_name(), 'external/wpt/x/z.html')
        # The original container isn't modified.
        self.assertEqual(len(raw_results), 2)

    def test_run_no_builds(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        updater.git_cl = MockGitCL(updater.host, {})
        with self.assertRaises(ScriptError) as e:
            updater.run()
        self.assertEqual(e.exception.message,
                         'No try job information was collected.')

    @unittest.skip('The MISSING status is not supported by ResultDB')
    def test_new_manual_tests_get_skip_expectation(self):
        host = self.mock_host()
        updater = self.mock_updater(host)
        for build in [
                Build('MOCK Try Mac10.10', 333, 'Build-1'),
                Build('MOCK Try Mac10.11', 111, 'Build-2'),
                Build('MOCK Try Trusty', 222, 'Build-3'),
                Build('MOCK Try Precise', 333, 'Build-4'),
                Build('MOCK Try Win10', 444, 'Build-5'),
                Build('MOCK Try Win7', 555, 'Build-6'),
        ]:
            host.results_fetcher.set_results(
                build,
                WebTestResults.from_rdb_responses(
                    {'external/wpt/x-manual.html': [{
                        'status': 'FAIL',
                    }] * 3},
                    step_name='blink_wpt_tests'))
        _, line_dict = updater.update_expectations()
        self.assertEqual(
            line_dict, {
                'external/wpt/x-manual.html':
                ['external/wpt/x-manual.html [ Skip ]']
            })

    def test_same_platform_one_without_results(self):
        # In this example, there are two configs using the same platform
        # (Mac10.10), and one of them has no results while the other one does.
        # The specifiers are "filled in" and the failure is assumed to apply
        # to the platform with missing results.
        host = self.mock_host()
        updater = self.mock_updater(host)
        host.results_fetcher.set_results(
            Build('MOCK Try Precise', 333, 'Build-4'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Trusty', 222, 'Build-3'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        host.results_fetcher.set_results(
            Build('MOCK Try Mac10.10', 333, 'Build-1'),
            WebTestResults.from_rdb_responses(
                {'external/wpt/x.html': [{
                    'status': 'ABORT',
                }] * 3},
                step_name='blink_wpt_tests'))
        _, line_dict = updater.update_expectations()
        self.assertEqual(
            line_dict, {
                'external/wpt/x.html': [
                    '[ Linux ] external/wpt/x.html [ Timeout ]',
                    '[ Mac10.10 ] external/wpt/x.html [ Timeout ]',
                ],
            })

    def test_cleanup_all_deleted_tests_in_expectations_files(self):
        host = self.mock_host()
        port = host.port_factory.get()
        fs = host.filesystem
        expectations_path = fs.join(MOCK_WEB_TESTS, 'TestExpectations')

        fs.write_text_file(
            expectations_path,
            ('# results: [ Failure ]\n'
             'external/wpt/some/test/a.html?hello%20world [ Failure ]\n'
             'some/test/b.html [ Failure ]\n'
             '# This line should be deleted\n'
             'some/test/c.html [ Failure ]\n'
             '# line below should exist in new file\n'
             'some/test/d.html [ Failure ]\n'))
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'VirtualTestSuites'), '[]')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'new', 'a.html'), '')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'new', 'b.html'), '')
        fs.write_text_file(
            fs.join(port.web_tests_dir(), 'some', 'test', 'd.html'), '')

        updater = WPTExpectationsUpdater(host)

        def _git_command_return_val(cmd):
            if '--diff-filter=D' in cmd:
                return 'some/test/b.html'
            return ''

        updater.git.run = _git_command_return_val
        updater._relative_to_web_test_dir = lambda test_path: test_path
        updater.cleanup_test_expectations_files()
        self.assertMultiLineEqual(fs.read_text_file(expectations_path),
                                  ('# results: [ Failure ]\n'
                                   '# line below should exist in new file\n'
                                   'some/test/d.html [ Failure ]\n'))

    def test_skip_slow_timeout_tests(self):
        host = self.mock_host()
        fs = host.filesystem
        expectations_path = fs.join(MOCK_WEB_TESTS, 'TestExpectations')
        data = ('# results: [ Pass Failure Crash Timeout Skip ]\n'
                'foo/failure.html [ Failure ]\n'
                'foo/slow_timeout.html [ Timeout ]\n'
                'bar/text.html [ Pass ]\n')

        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'SlowTests'),
                           ('# results: [ Slow ]\n'
                            'foo/slow_timeout.html [ Slow ]\n'
                            'bar/slow.html [ Slow ]\n'))
        fs.write_text_file(expectations_path, data)

        newdata = data.replace('foo/slow_timeout.html [ Timeout ]',
                               'foo/slow_timeout.html [ Skip Timeout ]')
        updater = WPTExpectationsUpdater(host)
        rv = updater.skip_slow_timeout_tests(host.port_factory.get())
        self.assertTrue(rv)
        self.assertEqual(newdata, fs.read_text_file(expectations_path))

    def test_cleanup_all_test_expectations_files(self):
        host = self.mock_host()
        fs = host.filesystem
        test_expect_path = fs.join(MOCK_WEB_TESTS, 'TestExpectations')
        fs.write_text_file(
            test_expect_path,
            (
                '# results: [ Failure ]\n'
                'some/test/a.html [ Failure ]\n'
                'some/test/b.html [ Failure ]\n'
                'ignore/globs/* [ Failure ]\n'
                'some/test/c\*.html [ Failure ]\n'
                # default test case, line below should exist in new file
                'some/test/d.html [ Failure ]\n'))
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'VirtualTestSuites'), '[]')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'new', 'a.html'), '')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'new', 'b.html'), '')

        updater = WPTExpectationsUpdater(
            host, ['--clean-up-test-expectations-only',
                   '--clean-up-affected-tests-only'])
        deleted_files = [
            'some/test/b.html',
        ]
        renamed_file_pairs = {
            'some/test/a.html': 'new/a.html',
            'some/test/c*.html': 'new/c*.html',
        }
        updater._list_deleted_files = lambda: deleted_files
        updater._list_renamed_files = lambda: renamed_file_pairs
        updater.cleanup_test_expectations_files()
        self.assertMultiLineEqual(fs.read_text_file(test_expect_path),
                                  ('# results: [ Failure ]\n'
                                   'new/a.html [ Failure ]\n'
                                   'ignore/globs/* [ Failure ]\n'
                                   'new/c\*.html [ Failure ]\n'
                                   'some/test/d.html [ Failure ]\n'))

    def test_merging_platforms_if_possible(self):
        host = self.mock_host()
        updater = self.mock_updater(host)
        for build in [
                Build('MOCK Try Mac10.10', 333, 'Build-1'),
                Build('MOCK Try Mac10.11', 111, 'Build-2'),
                Build('MOCK Try Trusty', 222, 'Build-3'),
                Build('MOCK Try Precise', 333, 'Build-4'),
                Build('MOCK Try Win7', 555, 'Build-6'),
        ]:
            host.results_fetcher.set_results(
                build,
                WebTestResults.from_rdb_responses(
                    {'external/wpt/x.html': [{
                        'status': 'ABORT',
                    }] * 3},
                    step_name='blink_wpt_tests'))
        _, line_dict = updater.update_expectations()
        self.assertEqual(
            line_dict, {
                'external/wpt/x.html': [
                    '[ Linux ] external/wpt/x.html [ Timeout ]',
                    '[ Mac ] external/wpt/x.html [ Timeout ]',
                    '[ Win7 ] external/wpt/x.html [ Timeout ]',
                ],
            })

    def test_inheriting_results(self):
        # We make sure that platforms that have no results are able to inherit
        # results from other builds.
        host = self.mock_host()
        # Reset the fake list of try builders to use 3 Macs, 2 Wins and 1 Linux.
        host.builders = BuilderList({
            'MOCK Try Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac11-arm64': {
                'port_name': 'test-mac-mac11-arm64',
                'specifiers': ['Mac11-arm64', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'main': 'tryserver.blink',
                'is_try_builder': True,
            },
            'MOCK Try Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
            },
        })
        updater = WPTExpectationsUpdater(host)
        test_name = 'external/wpt/x.html'
        completed_results = [
            WebTestResults.from_rdb_responses(
                {
                    test_name: [{
                        'status': 'ABORT'
                    }],
                    'external/wpt/skipped-on-mac.html': [{
                        'status': 'PASS'
                    }],
                },
                build=Build('MOCK Try Win7')),
            WebTestResults.from_rdb_responses(
                {test_name: [{
                    'status': 'CRASH'
                }]},
                build=Build('MOCK Try Mac10.10')),
            WebTestResults.from_rdb_responses(
                {test_name: [{
                    'status': 'FAIL'
                }]},
                build=Build('MOCK Try Mac10.11')),
        ]

        # Win10 will inherit the result from win7
        filled_results = updater.fill_missing_results(
            WebTestResults([], build=Build('MOCK Try Win10')),
            completed_results)
        self.assertEqual(
            {'TIMEOUT'},
            set(filled_results.result_for_test(test_name).actual_results()))

        # Mac11-arm64 will inherit the union of results from Mac10.10 and
        # Mac10.11
        filled_results = updater.fill_missing_results(
            WebTestResults([], build=Build('MOCK Try Mac11-arm64')),
            completed_results)
        self.assertEqual(
            {'CRASH', 'FAIL'},
            set(filled_results.result_for_test(test_name).actual_results()))

        # Linux will inherit all results from Mac and Win since there is no
        # other Linux result to take.
        filled_results = updater.fill_missing_results(
            WebTestResults([], build=Build('MOCK Try Trusty')),
            completed_results)
        self.assertEqual(
            {'CRASH', 'FAIL', 'TIMEOUT'},
            set(filled_results.result_for_test(test_name).actual_results()))

    def test_inheriting_results_dedupe(self):
        # In this test we make sure that we dedupe the inherited results.
        host = self.mock_host()
        # Set up a fake list of try builders.
        # This uses 2 Macs, 2 Wins and 1 Linux.
        host.builders = BuilderList({
            'MOCK Try Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'main': 'tryserver.blink',
                'is_try_builder': True,
            },
            'MOCK Try Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
            },
        })
        updater = WPTExpectationsUpdater(host)
        test_name = 'external/wpt/x.html'
        completed_results = [
            WebTestResults.from_rdb_responses(
                {test_name: [{
                    'status': 'ABORT'
                }]},
                build=Build('MOCK Try Win7')),
            WebTestResults.from_rdb_responses(
                {test_name: [{
                    'status': 'ABORT'
                }]},
                build=Build('MOCK Try Mac10.10')),
            WebTestResults.from_rdb_responses(
                {test_name: [{
                    'status': 'FAIL'
                }]},
                build=Build('MOCK Try Mac10.11')),
        ]

        # Linux will inherit all results from Mac and Win since there is no
        # other Linux result to take. The results are deduped so we should not
        # get two TIMEOUT statuses in the result.
        filled_results = updater.fill_missing_results(
            WebTestResults([], build=Build('MOCK Try Trusty')),
            completed_results)
        self.assertEqual(
            {'FAIL', 'TIMEOUT'},
            set(filled_results.result_for_test(test_name).actual_results()))

    def test_no_fill_skipped(self):
        host = self.mock_host()
        host.builders = BuilderList({
            'MOCK Try Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
        })
        port = host.port_factory.get('test-mac-mac10.11')
        host.filesystem.write_text_file(
            port.path_to_never_fix_tests_file(),
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 ]
                # results: [ Skip ]
                # Simulate an old platform that runs fewer tests.
                [ Mac10.10 ] external/wpt/x.html [ Skip ]
                """))
        updater = WPTExpectationsUpdater(host)
        completed_results = [
            WebTestResults.from_rdb_responses(
                {'external/wpt/x.html': [{
                    'status': 'FAIL'
                }]},
                build=Build('MOCK Try Mac10.11')),
        ]
        filled_results = updater.fill_missing_results(
            WebTestResults([], build=Build('MOCK Try Mac10.10')),
            completed_results)
        self.assertIsNone(
            filled_results.result_for_test('external/wpt/x.html'))
