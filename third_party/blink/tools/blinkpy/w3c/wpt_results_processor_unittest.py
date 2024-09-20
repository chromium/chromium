# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import contextlib
import json
import re
import textwrap
import typing
from unittest import mock

from blinkpy.common.host_mock import MockHost as BlinkMockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_results_processor import (
    EventProcessingError,
    StreamShutdown,
    TestType,
    WPTResultsProcessor,
)


class WPTResultsProcessorTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.host = BlinkMockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.fs = self.host.filesystem
        self.path_finder = PathFinder(self.fs)
        port = self.host.port_factory.get('test-linux-trusty')
        port.set_option_default('manifest_update', False)
        port.set_option_default('product', 'chrome')
        port.set_option_default('test_types', typing.get_args(TestType))

        # Create a testing manifest containing any test files that we
        # might interact with.
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['/reftest-ref.html', '==']], {}],
                        ],
                        'reftest-multiple.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [
                                None,
                                [['/reftest-ref.html', '=='],
                                 ['/reftest-mismatch.html', '!=']], {}
                            ],
                        ],
                    },
                    'testharness': {
                        'test.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}]
                        ],
                        'timeout.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}]
                        ],
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
                    },
                    'wdspec': {
                        'test.py': [
                            '61acc923e8eb3f6883d09bb4bfa220d7f757bbb8',
                            [None, {}]
                        ],
                    },
                },
            }))
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('wpt_internal',
                                                 'MANIFEST.json'),
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [
                                None,
                                [['/wpt_internal/reftest-ref.html', '==']], {}
                            ],
                        ],
                    },
                    'testharness': {
                        'dir': {
                            'multiglob.https.any.js': [
                                'd6498c3e388e0c637830fa080cca78b0ab0e5305',
                                ['dir/multiglob.https.any.html', {}],
                                ['dir/multiglob.https.any.worker.html', {}],
                            ],
                        },
                    },
                },
            }))
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('VirtualTestSuites'),
            json.dumps([{
                'prefix': 'fake-vts',
                'platforms': ['Linux'],
                'bases': ['external/wpt/reftest-multiple.html'],
                'args': ['--enable-features=FakeFeature'],
            }]))
        self.fs.write_text_file(
            self.path_finder.path_from_blink_tools('blinkpy', 'web_tests',
                                                   'results.html'),
            'results-viewer-body')
        self.fs.write_text_file(
            self.path_finder.path_from_blink_tools('blinkpy', 'web_tests',
                                                   'results.html.version'),
            'Version=1.0')
        self.wpt_report = {
            'run_info': {
                'os': 'linux',
                'version': '18.04',
                'product': 'chrome',
                'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
                'used_upstream': True,
            },
            'results': [{
                'test':
                '/a/b.html',
                'subtests': [{
                    'name': 'subtest',
                    'status': 'FAIL',
                    'message': 'remove this message',
                    'expected': 'PASS',
                    'known_intermittent': [],
                }],
                'status':
                'OK',
                'expected':
                'OK',
                'message':
                'remove this message from the compact version',
                'duration':
                1000,
                'known_intermittent': ['CRASH'],
            }],
        }

        self.processor = WPTResultsProcessor(
            self.fs,
            port,
            artifacts_dir=self.fs.join('/mock-checkout', 'out', 'Default',
                                       'layout-test-results'),
            sink=mock.Mock(batch_results=contextlib.nullcontext))
        self.processor.sink.host = port.typ_host()

    def _event(self, **fields):
        self.processor.process_event({
            'pid': 16000,
            'thread': 'TestRunner',
            'source': 'web-platform-tests',
            'time': 1000,
            **fields
        })

    def test_report_expected_pass(self):
        self._event(action='test_start', time=1000, test='/reftest.html')
        self._event(action='test_end',
                    time=3000,
                    test='/reftest.html',
                    status='PASS')

        self.assertEqual(self.processor.num_initial_failures, 0)
        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_called_once_with(
            test_name_prefix='',
            result=mock.ANY,
            artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                             'Default'),
            expectations=None,
            test_file_location=self.path_finder.path_from_web_tests(
                'external', 'wpt', 'reftest.html'),
            html_summary=None)

        result = report_mock.call_args.kwargs['result']
        self.assertEqual(result.name, 'external/wpt/reftest.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertFalse(result.unexpected)
        self.assertAlmostEqual(result.took, 2)
        self.assertEqual(result.artifacts, {})

    def test_report_unexpected_fail(self):
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Pass Timeout ]
                wpt_internal/reftest.html [ Pass Timeout ]
                """))
        self._event(action='test_start',
                    time=1000,
                    test='/wpt_internal/reftest.html')
        self._event(action='test_end',
                    time=1500,
                    test='/wpt_internal/reftest.html',
                    status='FAIL',
                    expected='PASS',
                    known_intermittent=['TIMEOUT'])

        self.assertEqual(self.processor.num_initial_failures, 1)
        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_called_once_with(
            test_name_prefix='',
            result=mock.ANY,
            artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                             'Default'),
            expectations=None,
            test_file_location=self.path_finder.path_from_web_tests(
                'wpt_internal', 'reftest.html'),
            html_summary=mock.ANY)

        result = report_mock.call_args.kwargs['result']
        self.assertEqual(result.name, 'wpt_internal/reftest.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS', 'TIMEOUT'})
        self.assertTrue(result.unexpected)
        self.assertAlmostEqual(result.took, 0.5)
        # `{expected,actual}_text` is not produced for WPT reftests.
        self.assertEqual(result.artifacts, {})
        summary = report_mock.call_args.kwargs['html_summary']
        self.assertRegex(
            summary,
            'This WPT was run against .*chrome.* using .*chromedriver')
        self.assertRegex(
            summary, 'See .*these instructions.* about running '
            'these tests locally and triaging failures')
        self.assertNotIn('wpt.fyi', summary)

    def test_report_pass_on_retry(self):
        self._event(action='suite_start', time=0)
        self._event(action='test_start',
                    test='/variant.html?foo=bar/abc',
                    time=1000)
        self._event(action='test_end',
                    test='/variant.html?foo=bar/abc',
                    time=2000,
                    status='ERROR',
                    expected='OK')
        self._event(action='suite_end', time=3000)
        self._event(action='suite_start', time=4000)
        self._event(action='test_start',
                    test='/variant.html?foo=bar/abc',
                    time=5000)
        self._event(action='test_status',
                    test='/variant.html?foo=bar/abc',
                    time=5500,
                    status='PASS',
                    subtest='subtest',
                    message='only compare message on failure')
        self._event(action='test_end',
                    test='/variant.html?foo=bar/abc',
                    time=6000,
                    status='OK',
                    message='message')
        self._event(action='suite_end', time=7000)

        self.assertEqual(self.processor.num_initial_failures, 1)
        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_has_calls([
            mock.call(test_name_prefix='',
                      result=mock.ANY,
                      artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                                       'Default'),
                      expectations=None,
                      test_file_location=self.path_finder.path_from_web_tests(
                          'external', 'wpt', 'variant.html'),
                      html_summary=mock.ANY),
        ] * 2)

        fail, ok = [
            call.kwargs['result'] for call in report_mock.call_args_list
        ]
        self.assertEqual(fail.name, 'external/wpt/variant.html?foo=bar/abc')
        self.assertEqual(fail.actual, 'FAIL')
        self.assertEqual(fail.expected, {'PASS'})
        self.assertTrue(fail.unexpected)
        self.assertEqual(
            fail.artifacts, {
                'actual_text': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'variant_foo=bar_abc-actual.txt'),
                ],
                'text_diff': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'variant_foo=bar_abc-diff.txt'),
                ],
                'pretty_text_diff': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'variant_foo=bar_abc-pretty-diff.html'),
                ],
            })
        self.assertEqual(ok.name, 'external/wpt/variant.html?foo=bar/abc')
        self.assertEqual(ok.actual, 'PASS')
        self.assertEqual(ok.expected, {'PASS'})
        self.assertFalse(ok.unexpected)
        self.assertEqual(
            ok.artifacts, {
                'crash_log': [
                    self.fs.join('layout-test-results', 'retry_1', 'external',
                                 'wpt', 'variant_foo=bar_abc-crash-log.txt'),
                ],
            })
        fail_summary = report_mock.call_args_list[0].kwargs['html_summary']
        self.assertRegex(
            fail_summary,
            'https://wpt.fyi/results/variant.html%3Ffoo%3Dbar%2Fabc')

    def test_report_subtest_fail_all_expected(self):
        """All (sub)tests running expectedly is reported as expected pass."""
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('test-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] fail
                [PASS] pass
                Harness: the test ran to completion.
                """))
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='fail',
                    status='FAIL')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='pass',
                    status='PASS')
        self._event(action='test_end', test='/test.html', status='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/test.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertFalse(result.unexpected)

    def test_report_subtest_unexpected_pass(self):
        """Unexpected subtest pass should be promoted to an unexpected failure."""
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='fail',
                    status='FAIL')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='pass',
                    status='PASS',
                    expected='FAIL')
        self._event(action='test_end', test='/test.html', status='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)
        self.assertEqual(
            result.artifacts, {
                'actual_text': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'test-actual.txt'),
                ],
                'text_diff': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'test-diff.txt'),
                ],
                'pretty_text_diff': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'test-pretty-diff.html'),
                ],
            })
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'test-actual.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] fail
                Harness: the test ran to completion.
                """))

    def test_report_subtest_unexpected_fail(self):
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='pass before',
                    status='PASS')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='unexpected fail',
                    status='FAIL',
                    expected='PASS')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='expected fail',
                    status='FAIL')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='unexpected pass after',
                    status='PASS',
                    expected='FAIL')
        self._event(action='test_end', test='/test.html', status='ERROR')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_fail_for_harness_mismatch(self):
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('test-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 1 , harness_status.message = ignore this
                Harness: the test ran to completion.
                """))
        self._event(action='test_start', test='/test.html')
        self._event(action='test_end',
                    test='/test.html',
                    status='OK',
                    expected='ERROR')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_fail_for_unknown_subtests(self):
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests(
                'external/wpt/test-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] does-not-exist
                Harness: the test ran to completion.
                """))
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='implicit-pass',
                    status='PASS')
        self._event(action='test_end', test='/test.html', status='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_fail_for_different_types(self):
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end',
                    test='/reftest.html',
                    status='ERROR',
                    expected='FAIL')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/reftest.html')
        # The unexpected flag is still set because the failures are of different
        # types.
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_timeout(self):
        self._event(action='test_start', test='/timeout.html')
        self._event(action='test_status',
                    test='/timeout.html',
                    subtest='timeout',
                    status='TIMEOUT',
                    expected='PASS')
        self._event(action='test_status',
                    test='/timeout.html',
                    subtest='notrun',
                    status='NOTRUN',
                    expected='PASS')
        self._event(action='process_output',
                    command='chromedriver',
                    data='Log this line',
                    process='101')
        self._event(action='test_end',
                    test='/timeout.html',
                    status='TIMEOUT',
                    expected='OK',
                    extra={'browser_pid': 101})

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/timeout.html')
        self.assertEqual(result.actual, 'TIMEOUT')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)
        # Timeouts and crashes shouldn't output `{actual,expected}_*` artifacts.
        self.assertEqual(
            result.artifacts, {
                'stderr': [
                    self.fs.join('layout-test-results', 'external', 'wpt',
                                 'timeout-stderr.txt'),
                ],
            })
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'timeout-stderr.txt')), 'Log this line\n')

    def test_report_expected_timeout_with_unexpected_fails(self):
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Pass Timeout ]
                external/wpt/timeout.html [ Timeout ]
                """))
        self._event(action='test_start', test='/timeout.html')
        self._event(action='test_status',
                    test='/timeout.html',
                    subtest='unexpected fail',
                    status='FAIL',
                    expected='PASS')
        self._event(action='test_status',
                    test='/timeout.html',
                    subtest='timeout',
                    status='TIMEOUT')
        self._event(action='test_end', test='/timeout.html', status='TIMEOUT')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/timeout.html')
        self.assertEqual(result.actual, 'TIMEOUT')
        self.assertEqual(result.expected, {'TIMEOUT'})
        self.assertFalse(result.unexpected)

    def test_report_skip(self):
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('NeverFixTests'),
            textwrap.dedent("""\
                # results: [ Skip ]
                external/wpt/reftest.html [ Skip ]
                """))
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end', test='/reftest.html', status='SKIP')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'external/wpt/reftest.html')
        self.assertEqual(result.actual, 'SKIP')
        self.assertEqual(result.expected, {'SKIP'})
        self.assertFalse(result.unexpected)

    def test_extract_text(self):
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests(
                'variant_foo=baz-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                  expected-message
                Harness: the test ran to completion.

                """))
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_status',
                        test='/variant.html?foo=baz',
                        subtest='subtest',
                        status='PRECONDITION_FAILED',
                        expected='FAIL',
                        message='actual-message')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='OK')

        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'variant_foo=baz-actual.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [PRECONDITION_FAILED] subtest
                  actual-message
                Harness: the test ran to completion.
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'variant_foo=baz-expected.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                  expected-message
                Harness: the test ran to completion.
                """))
        diff_lines = self.fs.read_text_file(
            self.fs.join('/mock-checkout', 'out', 'Default',
                         'layout-test-results', 'external', 'wpt',
                         'variant_foo=baz-diff.txt')).splitlines()
        self.assertIn('-  expected-message', diff_lines)
        self.assertIn('+  actual-message', diff_lines)
        pretty_diff = self.fs.read_text_file(
            self.fs.join('/mock-checkout', 'out', 'Default',
                         'layout-test-results', 'external', 'wpt',
                         'variant_foo=baz-pretty-diff.html'))
        self.assertIn('expected-message', pretty_diff)
        self.assertIn('actual-message', pretty_diff)

    def test_extract_text_multiglobal(self):
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests(
                'wpt_internal', 'dir',
                'multiglob.https.any.worker-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                Harness: the test ran to completion.
                """))
        with self.fs.patch_builtins():
            self._event(
                action='test_start',
                test='/wpt_internal/dir/multiglob.https.any.worker.html')
            self._event(
                action='test_end',
                test='/wpt_internal/dir/multiglob.https.any.worker.html',
                status='ERROR',
                expected='OK',
                message="Uncaught SyntaxError: Unexpected token ')'")
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'wpt_internal', 'dir',
                             'multiglob.https.any.worker-actual.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 1 , harness_status.message = Uncaught SyntaxError: Unexpected token ')'
                Harness: the test ran to completion.
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'wpt_internal', 'dir',
                             'multiglob.https.any.worker-expected.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                Harness: the test ran to completion.
                """))

    def test_extract_text_all_pass(self):
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_status',
                        test='/variant.html?foo=baz',
                        subtest='passing subtest',
                        status='PASS')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='OK')
        self.assertFalse(
            self.fs.exists(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'variant_foo=baz-actual.txt')))

    def test_extract_text_reset_results_testharness(self):
        self.processor.reset_results = True
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_status',
                        test='/variant.html?foo=baz',
                        subtest='passing subtest',
                        status='PASS')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='OK')
        self.assertEqual(
            self.fs.read_text_file(
                self.path_finder.path_from_web_tests(
                    'platform', 'test-linux-trusty', 'external', 'wpt',
                    'variant_foo=baz-expected.txt')),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                All subtests passed and are omitted for brevity.
                See https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/writing_web_tests.md#Text-Test-Baselines for details.
                Harness: the test ran to completion.
                """))

    def test_extract_text_reset_results_wdspec(self):
        self.processor.reset_results = True
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/test.py')
            self._event(action='test_status',
                        test='/test.py',
                        subtest='passing subtest',
                        status='PASS')
            self._event(action='test_end', test='/test.py', status='OK')
        self.assertEqual(
            self.fs.read_text_file(
                self.path_finder.path_from_web_tests('platform',
                                                     'test-linux-trusty',
                                                     'external', 'wpt',
                                                     'test-expected.txt')),
            textwrap.dedent("""\
                This is a wdspec test.
                All subtests passed and are omitted for brevity.
                See https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/writing_web_tests.md#Text-Test-Baselines for details.
                Harness: the test ran to completion.
                """))

    def test_extract_text_repeat_within_suite(self):
        """Extract only the last artifacts from `--repeat-each`."""
        self.processor.repeat_each = 2
        self._event(action='test_start', test='/test.html')
        self._event(action='test_end',
                    test='/test.html',
                    status='ERROR',
                    message='error 1')
        self._event(action='test_start', test='/test.html')
        self._event(action='test_end',
                    test='/test.html',
                    status='ERROR',
                    message='error 2')

        results_dir = self.fs.join('/mock-checkout', 'out', 'Default',
                                   'layout-test-results')
        output = self.fs.read_text_file(
            self.fs.join(results_dir, 'external', 'wpt', 'test-actual.txt'))
        self.assertIn('error 2', output)
        self.assertNotIn('error 1', output)
        self.assertFalse(self.fs.exists(self.fs.join(results_dir, 'retry_1')))

    def test_extract_screenshots(self):
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end',
                    test='/reftest.html',
                    status='FAIL',
                    expected='PASS',
                    extra={
                        'reftest_screenshots': [{
                            'url': '/reftest.html',
                            'screenshot': 'abcd',
                        }, '==', {
                            'url': '/reftest-ref.html',
                            'screenshot': 'bcde',
                        }],
                    })
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'reftest-actual.png')), base64.b64decode('abcd'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external',
                             'wpt', 'reftest-expected.png')),
            base64.b64decode('bcde'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'reftest-diff.png')), '\n'.join([
                                 '< bcde',
                                 '---',
                                 '> abcd',
                             ]))

    def test_extract_screenshots_match_and_mismatch(self):
        self._event(action='test_start',
                    test='/reftest-multiple.html',
                    subsuite='fake-vts')
        self._event(action='test_end',
                    test='/reftest-multiple.html',
                    subsuite='fake-vts',
                    status='FAIL',
                    expected='PASS',
                    extra={
                        'reftest_screenshots': [{
                            'url': '/reftest-ref.html',
                            'screenshot': 'abcd',
                        }, '!=', {
                            'url': '/reftest-mismatch.html',
                            'screenshot': 'abcd',
                        }],
                    })
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'virtual', 'fake-vts',
                             'external', 'wpt',
                             'reftest-multiple-actual.png')),
            base64.b64decode('abcd'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'virtual', 'fake-vts',
                             'external', 'wpt',
                             'reftest-multiple-expected.png')),
            base64.b64decode('abcd'))
        self.assertFalse(
            self.fs.exists(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'virtual', 'fake-vts',
                             'external', 'wpt', 'reftest-multiple-diff.png')))

    def test_extract_screenshots_for_wpt_internal(self):
        self._event(action='test_start', test='/wpt_internal/reftest.html')
        self._event(action='test_end',
                    test='/wpt_internal/reftest.html',
                    status='FAIL',
                    expected='PASS',
                    extra={
                        'reftest_screenshots': [{
                            'url': '/wpt_internal/reftest.html',
                            'screenshot': 'abcd',
                        }, '==', {
                            'url': '/wpt_internal/reftest-ref.html',
                            'screenshot': 'bcde',
                        }],
                    })
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'wpt_internal',
                             'reftest-actual.png')), base64.b64decode('abcd'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results',
                             'wpt_internal', 'reftest-expected.png')),
            base64.b64decode('bcde'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'wpt_internal',
                             'reftest-diff.png')), '\n'.join([
                                 '< bcde',
                                 '---',
                                 '> abcd',
                             ]))

    def test_no_diff_artifacts_on_pass(self):
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('test.html.ini'),
            textwrap.dedent("""\
                [test.html]
                  expected: OK
                """))
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/test.html')
            self._event(action='test_end', test='/test.html', status='OK')
            self._event(action='test_start', test='/reftest.html')
            self._event(action='test_end',
                        test='/reftest.html',
                        status='PASS',
                        extra={
                            'reftest_screenshots': [{
                                'url': '/reftest.html',
                                'screenshot': 'abcd',
                            }, {
                                'url': '/reftest-ref.html',
                                'screenshot': 'bcde'
                            }],
                        })
        for filename in (
                'test-expected.txt',
                'test-actual.txt',
                'test-diff.txt',
                'test-pretty-diff.html',
                'reftest-expected.png',
                'reftest-actual.png',
                'reftest-diff.png',
        ):
            self.assertFalse(
                self.fs.exists(
                    self.fs.join('/mock-checkout', 'out', 'Default',
                                 'layout-test-results', filename)))

    def test_extract_logs_interleaved(self):
        self.processor.browser_outputs.capacity = 2
        self._event(action='process_output',
                    command='chromedriver --port=101',
                    data='Running test.html',
                    process='101')
        self._event(action='process_output',
                    command='git rev-parse HEAD',
                    data='Do not log; unrelated executable',
                    process='99999')
        self._event(action='test_start', test='/test.html')
        self._event(action='test_start', test='/timeout.html')
        self._event(action='process_output',
                    command='chromedriver --port=101',
                    data='cp1252 cannot represent \u03c0, the Greek letter',
                    process='101')
        self._event(action='process_output',
                    command='chromedriver --port=202',
                    data='Running timeout.html',
                    process='202')
        self._event(action='test_end',
                    test='/test.html',
                    status='OK',
                    message='Remote-end stacktrace:\n\n#0 0xffff <unknown>\n',
                    extra={'browser_pid': 101})
        self._event(action='test_end',
                    test='/timeout.html',
                    status='TIMEOUT',
                    extra={'browser_pid': 202})
        self._event(action='process_output',
                    command='chromedriver --port=101',
                    data='Do not log; this event occurs after `test_end`',
                    process='101')

        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'test-crash-log.txt')),
            textwrap.dedent("""\
                Remote-end stacktrace:

                #0 0xffff <unknown>
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'test-stderr.txt')),
            textwrap.dedent("""\
                Running test.html
                cp1252 cannot represent \u03c0, the Greek letter
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'timeout-stderr.txt')),
            textwrap.dedent("""\
                Running timeout.html
                """))

    def test_extract_leak_log(self):
        leak_counters = {'live_documents': (1, 1), 'live_nodes': (4, 5)}
        self._event(action='test_start', test='/test.html')
        self._event(action='test_end',
                    test='/test.html',
                    status='CRASH',
                    expected='OK',
                    extra={'leak_counters': leak_counters})

        log_path = self.fs.join('/mock-checkout', 'out', 'Default',
                                'layout-test-results', 'external', 'wpt',
                                'test-leak-log.txt')
        self.assertEqual(json.loads(self.fs.read_text_file(log_path)), {
            'live_documents': [1, 1],
            'live_nodes': [4, 5],
        })

    def test_extract_command(self):
        self._event(action='test_start', test='/test.html')
        self._event(
            action='process_output',
            command='chromedriver',
            data=('[INFO] Launching chrome: /path/to/chrome --headless=new '
                  '--host-resolver-rules=MAP * ^NOTFOUND --switch data:,'),
            process='101')
        self._event(action='test_end',
                    test='/test.html',
                    status='OK',
                    extra={'browser_pid': 101})
        self._event(action='test_start', test='/timeout.html')
        self._event(action='test_end',
                    test='/timeout.html',
                    status='OK',
                    extra={'browser_pid': 101})
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end',
                    test='/reftest.html',
                    status='PASS',
                    extra={'browser_pid': 202})

        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'test-command.txt')),
            "/path/to/chrome '--host-resolver-rules=MAP * ^NOTFOUND' --switch "
            'http://web-platform.test:8001/test.html')
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'timeout-command.txt')),
            "/path/to/chrome '--host-resolver-rules=MAP * ^NOTFOUND' --switch "
            'http://web-platform.test:8001/timeout.html',
            'Command should be copied from the previous test.')
        self.assertFalse(
            self.fs.exists(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'external', 'wpt',
                             'reftest-command.txt')),
            'Command has not been observed for this browser yet.')

    def test_unknown_event(self):
        self._event(action='unknown', time=1000)
        (message, ) = self.logMessages()
        self.assertRegex(message,
                         "WARNING: 'unknown' event received, but not handled")

    def test_unknown_test(self):
        with self.assertRaises(EventProcessingError):
            self._event(action='test_start',
                        test='/does-not-exist-in-manifest.html')

    def test_unstarted_test(self):
        with self.assertRaises(EventProcessingError):
            self._event(action='test_end', test='/reftest.html', status='PASS')

    def test_shutdown(self):
        with self.assertRaises(StreamShutdown):
            self._event(action='shutdown')

    def test_shutdown_with_incomplete_tests(self):
        self._event(action='suite_start',
                    tests={
                        'fake-vts:/': ['/reftest-multiple.html'],
                        '/wpt_internal': ['/wpt_internal/reftest.html'],
                        '/wpt_internal/dir':
                        ['/wpt_internal/dir/multiglob.https.any.html'],
                    })
        self._event(action='test_start', test='/wpt_internal/reftest.html')
        self._event(action='test_end',
                    test='/wpt_internal/reftest.html',
                    status='PASS')
        self._event(action='test_start',
                    test='/reftest-multiple.html',
                    subsuite='fake-vts')
        with self.assertRaises(StreamShutdown):
            self._event(action='shutdown')

        self.assertLog([
            'WARNING: 2 test(s) never completed.\n',
        ])
        report_mock = self.processor.sink.report_individual_test_result
        results = {
            args.kwargs['result']
            for args in report_mock.call_args_list
        }
        unexpected_skips = {
            result.name: result
            for result in results
            if result.actual == ResultType.Skip and result.unexpected
        }
        self.assertEqual(
            set(unexpected_skips), {
                'virtual/fake-vts/external/wpt/reftest-multiple.html',
                'wpt_internal/dir/multiglob.https.any.html',
            })
        for test_name, result in unexpected_skips.items():
            self.assertEqual(result.took, 0, test_name)

    def test_early_exit_from_failures(self):
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests(
                'variant_foo=baz-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 1 , harness_status.message = ignore this
                Harness: the test ran to completion.
                """))
        self.processor.failure_threshold = 2
        with mock.patch('os.kill') as kill_mock:
            self._event(action='test_start', test='/variant.html?foo=bar/abc')
            self._event(action='test_end',
                        test='/variant.html?foo=bar/abc',
                        status='ERROR',
                        expected='OK')
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='ERROR')
            kill_mock.assert_not_called()
            self._event(action='test_start', test='/reftest.html')
            self._event(action='test_end',
                        test='/reftest.html',
                        status='FAIL',
                        expected='PASS')
            kill_mock.assert_called_once()

    def test_early_exit_from_crashes_and_timeouts(self):
        self.processor.crash_timeout_threshold = 2
        with mock.patch('os.kill') as kill_mock:
            self._event(action='test_start', test='/variant.html?foo=bar/abc')
            self._event(action='test_end',
                        test='/variant.html?foo=bar/abc',
                        status='ERROR',
                        expected='OK')
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='TIMEOUT',
                        expected='OK')
            kill_mock.assert_not_called()
            self._event(action='test_start', test='/reftest.html')
            self._event(action='test_end',
                        test='/reftest.html',
                        status='CRASH',
                        expected='OK')
            kill_mock.assert_called_once()

    def test_process_json(self):
        """Ensure that various JSONs are written to the correct locations."""
        self.processor.port.set_option_default('shard_index', 0)
        diff_stats = {'maxDifference': 100, 'maxPixels': 3}
        with mock.patch.object(self.processor.port,
                               'diff_image',
                               return_value=(..., diff_stats, ...)):
            for _ in range(2):
                self._event(action='suite_start',
                            tests={'/': ['/reftest.html']})
                self._event(action='test_start', test='/reftest.html')
                self._event(action='process_output',
                            process='101',
                            command='chromedriver --port=101',
                            data='[101:101:INFO] This is Chrome version 125')
                self._event(action='test_end',
                            test='/reftest.html',
                            status='FAIL',
                            expected='PASS',
                            extra={
                                'reftest_screenshots': [{
                                    'url': '/reftest-ref.html',
                                    'screenshot': 'abcd',
                                }, '==', {
                                    'url': '/reftest.html',
                                    'screenshot': 'bcde',
                                }],
                                'browser_pid':
                                101,
                            })
                self._event(action='test_start', test='/test.html')
                self._event(action='test_end', test='/test.html', status='OK')
                self._event(action='suite_end')
        self.processor.process_results_json()

        full_json = json.loads(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'full_results.json')))
        self.assertEqual(full_json['num_regressions'], 1)
        unexpected_fail = full_json['tests']['external']['wpt']['reftest.html']
        self.assertTrue(unexpected_fail['has_stderr'])
        self.assertEqual(unexpected_fail['artifacts']['stderr'], [
            self.fs.join('layout-test-results', 'external', 'wpt',
                         'reftest-stderr.txt'),
            self.fs.join('layout-test-results', 'retry_1', 'external', 'wpt',
                         'reftest-stderr.txt'),
        ])
        self.assertEqual(unexpected_fail['image_diff_stats'], diff_stats)
        self.assertEqual(unexpected_fail['shard'], 0)

        path_to_failing_results = self.fs.join('/mock-checkout', 'out',
                                               'Default',
                                               'layout-test-results',
                                               'failing_results.json')
        failing_results_match = re.fullmatch(
            'ADD_RESULTS\((?P<json>.*)\);',
            self.fs.read_text_file(path_to_failing_results))
        self.assertIsNotNone(failing_results_match)
        failing_results = json.loads(failing_results_match['json'])
        self.assertIn('external', failing_results['tests'])
        self.assertIn('wpt', failing_results['tests']['external'])
        self.assertIn('reftest.html',
                      failing_results['tests']['external']['wpt'])
        self.assertNotIn('test.html',
                         failing_results['tests']['external']['wpt'])
        self.assertRegex(self.fs.read_text_file(path_to_failing_results),
                         'ADD_RESULTS\(.*\);$')

    def test_trim_json_to_regressions(self):
        results = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'PASS',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                    },
                },
                'variant.html?foo=bar/abc': {
                    'expected': 'FAIL',
                    'actual': 'PASS',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                    },
                    'is_unexpected': True,
                },
                'variant.html?foo=baz': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['ERROR'],
                    },
                    'is_unexpected': True,
                    'is_regression': True,
                },
            },
            'path_delimiter': '/',
        }
        self.processor.trim_to_regressions(results['tests'])

        self.assertNotIn('test.html', results['tests'])
        self.assertNotIn('variant.html?foo=bar/abc', results['tests'])
        self.assertIn('variant.html?foo=baz', results['tests'])

    def test_process_wpt_report(self):
        report_src = self.fs.join('/mock-checkout', 'out', 'Default',
                                  'wpt_report.json')
        self.fs.write_text_file(report_src, json.dumps(self.wpt_report))
        self.processor.process_wpt_report(report_src)
        report_dest = self.fs.join('/mock-checkout', 'out', 'Default',
                                   'layout-test-results', 'wpt_report.json')
        self.processor.sink.report_invocation_level_artifacts.assert_called_once_with(
            {
                'wpt_report.json': {
                    'filePath': report_dest,
                },
            })
        report = json.loads(self.fs.read_text_file(report_dest))
        self.assertEqual(report['run_info'], self.wpt_report['run_info'])
        self.assertEqual(report['results'], self.wpt_report['results'])
