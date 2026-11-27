#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for run_dex_integration_test.py."""

import http.server
import json
import os
import signal
import sys
import tempfile
import threading
import unittest
from unittest import mock

import run_dex_integration_test


class ConvertSummaryToJsonV3Test(unittest.TestCase):
    def test_passing_run(self):
        summary = {
            'global_tags': ['OS_LINUX'],
            'per_iteration_data': [
                {
                    'DexIntegrationTest.RoundTrip': [
                        {'status': 'SUCCESS', 'elapsed_time_ms': 150}
                    ],
                }
            ],
        }
        result = run_dex_integration_test._convert_summary_to_json_v3(
            summary, start_time=1234.5, returncode=0
        )
        self.assertEqual(
            result,
            {
                'version': 3,
                'interrupted': False,
                'path_delimiter': '.',
                'seconds_since_epoch': 1234.5,
                'num_failures_by_type': {'PASS': 1},
                'tests': {
                    'DexIntegrationTest': {
                        'RoundTrip': {
                            'expected': 'PASS',
                            'actual': 'PASS',
                            'time': 0.15,
                        }
                    }
                },
            },
        )

    def test_fail_then_retry_pass(self):
        summary = {
            'per_iteration_data': [
                {
                    'DexIntegrationTest.Flaky': [
                        {'status': 'FAILURE', 'elapsed_time_ms': 100},
                        {'status': 'SUCCESS', 'elapsed_time_ms': 200},
                    ],
                }
            ],
        }
        result = run_dex_integration_test._convert_summary_to_json_v3(
            summary, start_time=10.0, returncode=0
        )
        self.assertFalse(result['interrupted'])
        self.assertEqual(result['num_failures_by_type'], {'PASS': 1})
        self.assertEqual(
            result['tests']['DexIntegrationTest']['Flaky'],
            {'expected': 'PASS', 'actual': 'FAIL PASS', 'time': 0.3},
        )

    def test_repeat_iterations(self):
        summary = {
            'per_iteration_data': [
                {
                    'DexIntegrationTest.Case': [
                        {'status': 'SUCCESS', 'elapsed_time_ms': 50}
                    ],
                },
                {
                    'DexIntegrationTest.Case': [
                        {'status': 'FAILURE_ON_EXIT', 'elapsed_time_ms': 75}
                    ],
                },
            ],
        }
        result = run_dex_integration_test._convert_summary_to_json_v3(
            summary, start_time=20.0, returncode=0
        )
        self.assertFalse(result['interrupted'])
        self.assertEqual(result['num_failures_by_type'], {'FAIL': 1})
        self.assertEqual(
            result['tests']['DexIntegrationTest']['Case'],
            {'expected': 'PASS', 'actual': 'PASS FAIL', 'time': 0.125},
        )

    def test_notrun_maps_to_skip(self):
        summary = {
            'per_iteration_data': [
                {
                    'DexIntegrationTest.Skipped': [
                        {'status': 'NOTRUN', 'elapsed_time_ms': 0}
                    ],
                }
            ],
        }
        result = run_dex_integration_test._convert_summary_to_json_v3(
            summary, start_time=30.0, returncode=0
        )
        self.assertEqual(result['num_failures_by_type'], {'SKIP': 1})
        self.assertEqual(
            result['tests']['DexIntegrationTest']['Skipped'],
            {
                'expected': 'PASS',
                'actual': 'SKIP',
                'time': 0.0,
                'is_unexpected': True,
            },
        )

    def test_interrupted_on_missing_empty_and_signal_terminated(self):
        cases = (
            ({}, 0, True),
            ({'per_iteration_data': [{}]}, 1, True),
            ({'per_iteration_data': [{}]}, 0, False),
            (
                {
                    'global_tags': ['CAUGHT_TERMINATION_SIGNAL'],
                    'per_iteration_data': [
                        {
                            'DexIntegrationTest.Case': [
                                {'status': 'SUCCESS', 'elapsed_time_ms': 10}
                            ]
                        }
                    ],
                },
                0,
                True,
            ),
        )
        for summary, returncode, expected_interrupted in cases:
            with self.subTest(summary=summary, returncode=returncode):
                result = run_dex_integration_test._convert_summary_to_json_v3(
                    summary, start_time=40.0, returncode=returncode
                )
                self.assertEqual(result['interrupted'], expected_interrupted)


class UploadToResultSinkTest(unittest.TestCase):
    def setUp(self):
        self.mock_client = mock.MagicMock()
        self.mock_client.__enter__.return_value = self.mock_client
        patcher = mock.patch.object(
            run_dex_integration_test.result_sink,
            'TryInitClient',
            return_value=self.mock_client,
        )
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_normal_upload_and_notrun_preserved(self):
        summary = {
            'per_iteration_data': [
                {
                    'DexIntegrationTest.Pass': [
                        {
                            'status': 'SUCCESS',
                            'elapsed_time_ms': 120,
                            'output_snippet': 'ok',
                        }
                    ],
                    'DexIntegrationTest.Unrun': [
                        {
                            'status': 'NOTRUN',
                            'elapsed_time_ms': 0,
                            'output_snippet': '',
                        }
                    ],
                    'DexIntegrationTest.ExitFail': [
                        {
                            'status': 'FAILURE_ON_EXIT',
                            'elapsed_time_ms': 50,
                            'output_snippet': 'exit fail',
                        }
                    ],
                }
            ],
        }
        run_dex_integration_test._upload_to_result_sink(summary, returncode=0)

        self.assertEqual(
            self.mock_client.Post.call_args_list,
            [
                mock.call(
                    'DexIntegrationTest.Pass',
                    run_dex_integration_test.result_types.PASS,
                    120,
                    'ok',
                    None,
                    test_id_structured={
                        'coarseName': None,
                        'fineName': 'DexIntegrationTest',
                        'caseNameComponents': ['Pass'],
                    },
                ),
                mock.call(
                    'DexIntegrationTest.Unrun',
                    run_dex_integration_test.result_types.NOTRUN,
                    0,
                    '',
                    None,
                    test_id_structured={
                        'coarseName': None,
                        'fineName': 'DexIntegrationTest',
                        'caseNameComponents': ['Unrun'],
                    },
                ),
                mock.call(
                    'DexIntegrationTest.ExitFail',
                    run_dex_integration_test.result_types.FAIL,
                    50,
                    'exit fail',
                    None,
                    test_id_structured={
                        'coarseName': None,
                        'fineName': 'DexIntegrationTest',
                        'caseNameComponents': ['ExitFail'],
                    },
                ),
            ],
        )

    def test_fallback_fixture_when_summary_missing_or_empty(self):
        rt = run_dex_integration_test.result_types
        cases = (
            ({}, 1, rt.CRASH),
            ({'per_iteration_data': [{}]}, 1, rt.FAIL),
            ({'per_iteration_data': [{}]}, -11, rt.CRASH),
        )
        for summary, returncode, expected_status in cases:
            with self.subTest(summary=summary, returncode=returncode):
                self.mock_client.Post.reset_mock()
                run_dex_integration_test._upload_to_result_sink(
                    summary, returncode=returncode
                )

                self.mock_client.Post.assert_called_once()
                args, kwargs = self.mock_client.Post.call_args
                self.assertEqual(
                    args[0], 'zucchini_dex_integration_test.*fixture'
                )
                self.assertEqual(args[1], expected_status)
                self.assertEqual(
                    kwargs['test_id_structured'],
                    {
                        'coarseName': None,
                        'fineName': 'zucchini_dex_integration_test',
                        'caseNameComponents': ['*fixture'],
                    },
                )
                self.assertIn('failure_reason', kwargs)

    def test_fallback_fixture_on_nonzero_exit_after_passing_attempts(self):
        rt = run_dex_integration_test.result_types
        run_dex_integration_test._upload_to_result_sink(
            {
                'per_iteration_data': [
                    {
                        'DexIntegrationTest.Pass': [
                            {'status': 'SUCCESS', 'elapsed_time_ms': 10}
                        ]
                    }
                ]
            },
            returncode=1,
        )
        self.assertEqual(self.mock_client.Post.call_count, 2)
        fixture_args, fixture_kwargs = self.mock_client.Post.call_args_list[1]
        self.assertEqual(
            fixture_args[0], 'zucchini_dex_integration_test.*fixture'
        )
        self.assertEqual(fixture_args[1], rt.FAIL)
        self.assertIn('without completing', fixture_kwargs['failure_reason'])

    def test_no_fallback_fixture_on_empty_summary_exit_zero(self):
        run_dex_integration_test._upload_to_result_sink(
            {'per_iteration_data': [{}]}, returncode=0
        )
        self.mock_client.Post.assert_not_called()


class MainResultSinkIntegrationTest(unittest.TestCase):
    def test_main_no_summary_uploads_fallback_to_real_result_sink_client(self):
        self.addCleanup(
            signal.signal, signal.SIGINT, signal.getsignal(signal.SIGINT)
        )
        self.addCleanup(
            signal.signal, signal.SIGTERM, signal.getsignal(signal.SIGTERM)
        )
        received = []

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                length = int(self.headers['Content-Length'])
                received.append(json.loads(self.rfile.read(length)))
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'{}')

            def log_message(self, *_args):
                pass

        server = http.server.HTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as tmp_dir:
                luci_context_path = os.path.join(tmp_dir, 'luci_context.json')
                output_path = os.path.join(tmp_dir, 'output.json')
                with open(luci_context_path, 'w', encoding='utf-8') as fp:
                    json.dump(
                        {
                            'result_sink': {
                                'address': f'127.0.0.1:{server.server_port}',
                                'auth_token': 'test-token',
                            }
                        },
                        fp,
                    )
                no_summary_script = os.path.join(tmp_dir, 'no_summary.py')
                with open(no_summary_script, 'w', encoding='utf-8') as fp:
                    fp.write('import sys\nsys.exit(2)\n')
                with mock.patch.dict(
                    os.environ,
                    {
                        'LUCI_CONTEXT': luci_context_path,
                        'NO_PROXY': '127.0.0.1,localhost',
                        'no_proxy': '127.0.0.1,localhost',
                    },
                ):
                    with mock.patch.object(
                        sys,
                        'argv',
                        [
                            'run_dex_integration_test.py',
                            sys.executable,
                            no_summary_script,
                            f'--isolated-script-test-output={output_path}',
                        ],
                    ):
                        exit_code = run_dex_integration_test.main()

                self.assertEqual(exit_code, 2)
                with open(output_path, encoding='utf-8') as fp:
                    v3 = json.load(fp)
                self.assertTrue(v3['interrupted'])
        finally:
            server.shutdown()
            server.server_close()

        self.assertEqual(len(received), 1)
        tr = received[0]['testResults'][0]
        self.assertEqual(tr['status'], 'CRASH')
        self.assertFalse(tr['expected'])
        self.assertEqual(
            tr['testIdStructured'],
            {
                'coarseName': None,
                'fineName': 'zucchini_dex_integration_test',
                'caseNameComponents': ['*fixture'],
            },
        )


if __name__ == '__main__':
    unittest.main()
