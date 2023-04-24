# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import re
import textwrap
from unittest import mock

from blinkpy.common.host_mock import MockHost as BlinkMockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_results_processor import (
    EventProcessingError,
    StreamShutdown,
    WPTResultsProcessor,
)


class WPTResultsProcessorTest(LoggingTestCase):
    def setUp(self):
        super().setUp()

        self.host = BlinkMockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.fs = self.host.filesystem
        self.path_finder = PathFinder(self.fs)
        port = self.host.port_factory.get()

        # Create a testing manifest containing any test files that we
        # might interact with.
        self.fs.write_text_file(
            self.fs.join(port.web_tests_dir(), 'external', BASE_MANIFEST_NAME),
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['reftest-ref.html', '==']], {}],
                        ]
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
                            [None, [['reftest-ref.html', '==']], {}],
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
            sink=mock.Mock())
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

        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_called_once_with(
            test_name_prefix='',
            result=mock.ANY,
            artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                             'Default'),
            expectations=None,
            test_file_location=self.path_finder.path_from_web_tests(
                'external', 'wpt', 'reftest.html'))

        result = report_mock.call_args.kwargs['result']
        self.assertEqual(result.name, 'reftest.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertFalse(result.unexpected)
        self.assertAlmostEqual(result.took, 2)
        self.assertEqual(result.artifacts, {})
        self.assertFalse(self.processor.has_regressions)

    def test_report_unexpected_fail(self):
        self._event(action='test_start',
                    time=1000,
                    test='/wpt_internal/reftest.html')
        self._event(action='test_end',
                    time=1500,
                    test='/wpt_internal/reftest.html',
                    status='FAIL',
                    expected='PASS',
                    known_intermittent=['TIMEOUT'])

        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_called_once_with(
            test_name_prefix='',
            result=mock.ANY,
            artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                             'Default'),
            expectations=None,
            test_file_location=self.path_finder.path_from_web_tests(
                'wpt_internal', 'reftest.html'))

        result = report_mock.call_args.kwargs['result']
        self.assertEqual(result.name, 'wpt_internal/reftest.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS', 'TIMEOUT'})
        self.assertTrue(result.unexpected)
        self.assertAlmostEqual(result.took, 0.5)
        self.assertEqual(
            result.artifacts, {
                'actual_text': [
                    self.fs.join('layout-test-results', 'wpt_internal',
                                 'reftest-actual.txt'),
                ],
                'pretty_text_diff': [
                    self.fs.join('layout-test-results', 'wpt_internal',
                                 'reftest-pretty-diff.html'),
                ]
            })
        self.assertTrue(self.processor.has_regressions)

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
                    message="subtest")
        self._event(action='test_end',
                    test='/variant.html?foo=bar/abc',
                    time=6000,
                    status='OK')
        self._event(action='suite_end', time=7000)

        report_mock = self.processor.sink.report_individual_test_result
        report_mock.assert_has_calls([
            mock.call(test_name_prefix='',
                      result=mock.ANY,
                      artifact_output_dir=self.fs.join('/mock-checkout', 'out',
                                                       'Default'),
                      expectations=None,
                      test_file_location=self.path_finder.path_from_web_tests(
                          'external', 'wpt', 'variant.html')),
        ] * 2)

        fail, ok = [
            call.kwargs['result'] for call in report_mock.call_args_list
        ]
        self.assertEqual(fail.name, 'variant.html?foo=bar/abc')
        self.assertEqual(fail.actual, 'FAIL')
        self.assertEqual(fail.expected, {'PASS'})
        self.assertTrue(fail.unexpected)
        self.assertEqual(ok.name, 'variant.html?foo=bar/abc')
        self.assertEqual(ok.actual, 'PASS')
        self.assertEqual(ok.expected, {'PASS'})
        self.assertFalse(ok.unexpected)
        self.assertEqual(
            ok.artifacts, {
                'stderr': [
                    self.fs.join('layout-test-results', 'retry_1',
                                 'variant_foo=bar_abc-stderr.txt'),
                ],
            })

    def test_report_subtest_fail_all_expected(self):
        """Subtest failures should be promoted to the test level.

        When there are only expected subtest failures and expected subtest passes,
        the overall result reported is expected failure.
        """
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
        self.assertEqual(result.name, 'test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'FAIL'})
        self.assertFalse(result.unexpected)

    def test_report_subtest_upexpected_pass(self):
        """Unexpected subtest pass should be promoted to the test level.

        An unexpected subtest pass has priority over an expected subtest fail, and
        the overall result reported is unexpected pass.
        """
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
        self.assertEqual(result.name, 'test.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_subtest_fail(self):
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
        self._event(action='test_end', test='/test.html', status='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_pass_against_notrun(self):
        """Any result against NOTRUN is an unexpected pass."""
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='notrun',
                    status='FAIL',
                    expected='NOTRUN')
        self._event(action='test_end', test='/test.html', status='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'test.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_fail_for_notrun(self):
        """A NOTRUN against other results is an unexpected fail."""
        self._event(action='test_start', test='/test.html')
        self._event(action='test_status',
                    test='/test.html',
                    subtest='notrun',
                    status='NOTRUN',
                    expected='FAIL')
        self._event(action='test_end',
                    test='/test.html',
                    status='ERROR',
                    expected='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'test.html')
        self.assertEqual(result.actual, 'FAIL')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_unexpected_timeout(self):
        """NOTRUN should not be promoted to the test level."""
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
        self._event(action='test_end',
                    test='/timeout.html',
                    status='TIMEOUT',
                    expected='OK')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'timeout.html')
        self.assertEqual(result.actual, 'TIMEOUT')
        self.assertEqual(result.expected, {'PASS'})
        self.assertTrue(result.unexpected)

    def test_report_expected_timeout_with_unexpected_fails(self):
        """Timeouts are always reported, even with subtest failures.

        We want to surface when the harness fails to run to completion, even if
        the failure to complete is expected. Timeouts/crashes need to be fixed
        before the failed assertions.
        """
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
        self.assertEqual(result.name, 'timeout.html')
        self.assertEqual(result.actual, 'TIMEOUT')
        self.assertEqual(result.expected, {'TIMEOUT'})
        self.assertFalse(result.unexpected)

    def test_report_sanitizer_fail(self):
        self._event(action='suite_start', run_info={'sanitizer_enabled': True})
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end',
                    test='/reftest.html',
                    status='FAIL',
                    expected='PASS')
        self._event(action='suite_end')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'reftest.html')
        self.assertEqual(result.actual, 'PASS')
        self.assertEqual(result.expected, {'PASS'})
        self.assertFalse(result.unexpected)

    def test_report_skip(self):
        self._event(action='test_start', test='/reftest.html')
        self._event(action='test_end', test='/reftest.html', status='SKIP')

        result = self.processor.sink.report_individual_test_result.call_args.kwargs[
            'result']
        self.assertEqual(result.name, 'reftest.html')
        self.assertEqual(result.actual, 'SKIP')
        self.assertEqual(result.expected, {'SKIP'})
        self.assertFalse(result.unexpected)

    def test_extract_text(self):
        # There are multiple variants (test parameterizations) in the same
        # metadata file. The processor should extract the relevant section as
        # the expected text and generate diffs.
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('variant.html.ini'),
            textwrap.dedent("""\
                [variant.html?foo=bar/abc]
                  expected: OK

                [variant.html?foo=baz]
                  expected: FAIL
                """))
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/variant.html?foo=baz')
            self._event(action='test_end',
                        test='/variant.html?foo=baz',
                        status='CRASH',
                        expected='FAIL')

        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results',
                             'variant_foo=baz-actual.txt')),
            textwrap.dedent("""\
                [variant.html?foo=baz]
                  expected: CRASH
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results',
                             'variant_foo=baz-expected.txt')),
            textwrap.dedent("""\
                [variant.html?foo=baz]
                  expected: FAIL
                """))
        diff_lines = self.fs.read_text_file(
            self.fs.join('/mock-checkout', 'out', 'Default',
                         'layout-test-results',
                         'variant_foo=baz-diff.txt')).splitlines()
        self.assertIn('-  expected: FAIL', diff_lines)
        self.assertIn('+  expected: CRASH', diff_lines)
        pretty_diff = self.fs.read_text_file(
            self.fs.join('/mock-checkout', 'out', 'Default',
                         'layout-test-results',
                         'variant_foo=baz-pretty-diff.html'))

        self.assertIn('expected: FAIL', pretty_diff)
        self.assertIn('actual: CRASH', pretty_diff)

    def test_extract_text_multiglobal(self):
        # Similar to a test with variants, the processor should extract the
        # correct section from a metadata file with multiple.
        self.fs.write_text_file(
            self.path_finder.path_from_web_tests('wpt_internal', 'dir',
                                                 'multiglob.https.any.js.ini'),
            textwrap.dedent("""\
                [multiglob.https.any.worker.html]
                  [subtest]
                    expected: FAIL

                [multiglob.https.any.html]
                  expected: OK
                """))
        with self.fs.patch_builtins():
            self._event(
                action='test_start',
                test='/wpt_internal/dir/multiglob.https.any.worker.html')
            self._event(
                action='test_end',
                test='/wpt_internal/dir/multiglob.https.any.worker.html',
                status='CRASH',
                expected='OK')
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'wpt_internal', 'dir',
                             'multiglob.https.any.worker-expected.txt')),
            textwrap.dedent("""\
                [multiglob.https.any.worker.html]
                  [subtest]
                    expected: FAIL
                """))

    def test_extract_text_with_conditions(self):
        """Evaluate expectations with the latest `suite_start`'s `run_info`."""
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('test.html.ini'),
            textwrap.dedent("""\
                [test.html]
                  expected:
                    if flag_specific != "fake-flag": FAIL
                    TIMEOUT
                """))
        with self.fs.patch_builtins():
            self._event(action='suite_start',
                        run_info={'flag_specific': 'fake-flag'})
            self._event(action='test_start', test='/test.html')
            self._event(action='test_end',
                        test='/test.html',
                        status='FAIL',
                        expected='OK')
            self._event(action='suite_end')
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'test-expected.txt')),
            textwrap.dedent("""\
                [test.html]
                  expected: TIMEOUT
                """))

    def test_extract_text_with_invalid_metadata(self):
        self.fs.write_text_file(
            self.path_finder.path_from_wpt_tests('test.html.ini'),
            textwrap.dedent("""\
                [test.html
                """))
        with self.fs.patch_builtins():
            self._event(action='test_start', test='/test.html')
            self._event(action='test_end',
                        test='/test.html',
                        status='CRASH',
                        expected='OK')
        self.assertTrue(
            self.fs.exists(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'test-actual.txt')))
        self.assertFalse(
            self.fs.exists(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'test-expected.txt')))
        self.assertLog([
            'WARNING: Unable to parse metadata for test.html: EOL in heading:  line 1\n',
        ])

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
                        }, {
                            'url': '/reftest-ref.html',
                            'screenshot': 'bcde',
                        }],
                    })
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'reftest-actual.png')),
            base64.b64decode('abcd'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'reftest-expected.png')),
            base64.b64decode('bcde'))
        self.assertEqual(
            self.fs.read_binary_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'reftest-diff.png')),
            '\n'.join([
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

    def test_extract_logs(self):
        self._event(action='process_output',
                    command='content_shell --run-web-tests',
                    data='[ERROR] Log this line')
        self._event(action='process_output',
                    command='git rev-parse HEAD',
                    data='[ERROR] Do not log this line')
        self._event(action='test_start', test='/test.html')
        self._event(
            action='test_status',
            test='/test.html',
            status='PASS',
            # The Greek letter pi, which 'cp1252' cannot represent.
            subtest='subtest with Unicode \u03c0',
            message='assert_eq(a, b)')
        self._event(action='test_end',
                    test='/test.html',
                    status='OK',
                    message='Test ran to completion.')

        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'test-stderr.txt')),
            textwrap.dedent("""\
                Harness: Test ran to completion.
                subtest with Unicode \u03c0: assert_eq(a, b)
                """))
        self.assertEqual(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'test-crash-log.txt')),
            textwrap.dedent("""\
                [ERROR] Log this line
                """))

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

    def test_shutdown_with_unreported_tests(self):
        self._event(action='test_start', test='/test.html')
        with self.assertRaises(StreamShutdown):
            self._event(action='shutdown')
        self.assertLog([
            'WARNING: Some tests have unreported results:\n',
            'WARNING:   test.html\n',
        ])

    def test_process_json(self):
        """Ensure that various JSONs are written to the correct locations."""
        diff_stats = {'maxDifference': 100, 'maxPixels': 3}
        with mock.patch.object(self.processor.port,
                               'diff_image',
                               return_value=(..., diff_stats, ...)):
            for _ in range(2):
                self._event(action='suite_start')
                self._event(action='test_start', test='/test.html')
                self._event(action='test_status',
                            test='/test.html',
                            status='FAIL',
                            expected='PASS',
                            subtest='subtest',
                            message='assert_eq(a, b)')
                self._event(action='test_end',
                            test='/test.html',
                            status='OK',
                            extra={
                                'reftest_screenshots': [{
                                    'url': '/test.html',
                                    'screenshot': 'abcd',
                                }]
                            })
                self._event(action='suite_end')

        results_json = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['ERROR'],
                    },
                    'is_unexpected': True,
                    'is_regression': True,
                },
            },
            'path_delimiter': '/',
        }
        full_json_path = self.fs.join('/mock-checkout', 'out', 'Default',
                                      'layout-test-results',
                                      'raw_wpt_output.json')
        self.fs.write_text_file(full_json_path, json.dumps(results_json))

        self.processor.process_results_json(full_json_path)
        full_json = json.loads(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'full_results.json')))
        unexpected_fail = full_json['tests']['test.html']
        self.assertEqual(unexpected_fail['has_stderr'], True)
        self.assertEqual(unexpected_fail['artifacts']['stderr'], [
            self.fs.join('layout-test-results', 'test-stderr.txt'),
            self.fs.join('layout-test-results', 'retry_1', 'test-stderr.txt'),
        ])
        self.assertEqual(unexpected_fail['image_diff_stats'], diff_stats)

    def test_trim_json_to_regressions(self):
        results_json = {
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
        full_json_path = self.fs.join('/mock-checkout', 'out', 'Default',
                                      'layout-test-results',
                                      'raw_wpt_output.json')
        self.fs.write_text_file(full_json_path, json.dumps(results_json))

        self.processor.process_results_json(full_json_path)

        failing_results_match = re.fullmatch(
            'ADD_RESULTS\((?P<json>.*)\);',
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'failing_results.json')))
        self.assertIsNotNone(failing_results_match)
        failing_results = json.loads(failing_results_match['json'])
        self.assertNotIn('test.html', failing_results['tests'])
        self.assertNotIn('variant.html?foo=bar/abc', failing_results['tests'])
        self.assertIn('variant.html?foo=baz', failing_results['tests'])
        self.assertRegex(
            self.fs.read_text_file(
                self.fs.join('/mock-checkout', 'out', 'Default',
                             'layout-test-results', 'full_results_jsonp.js')),
            'ADD_FULL_RESULTS\(.*\);$')

    def test_process_wpt_report(self):
        report_src = self.fs.join('/mock-checkout', 'out', 'Default',
                                  'wpt_report.json')
        self.fs.write_text_file(report_src,
                                (json.dumps(self.wpt_report) + '\n') * 2)
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
        self.assertEqual(report['results'], self.wpt_report['results'] * 2)

    def test_process_wpt_report_compact(self):
        report_src = self.fs.join('/mock-checkout', 'out', 'Default',
                                  'wpt_report.json')
        self.wpt_report['run_info']['used_upstream'] = False
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
        self.assertEqual(
            report['run_info'], {
                'os': 'linux',
                'version': '18.04',
                'product': 'chrome',
                'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
                'used_upstream': False,
            })
        self.assertEqual(report['results'], [{
            'test':
            '/a/b.html',
            'subtests': [{
                'name': 'subtest',
                'status': 'FAIL',
                'expected': 'PASS',
            }],
            'status':
            'OK',
            'duration':
            1000,
            'known_intermittent': ['CRASH'],
        }])
