#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs zucchini_dex_integration_test_bin as an isolated script test."""

import argparse
import json
import logging
import os
import shlex
import sys
import tempfile
import time

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
)
sys.path.insert(0, os.path.join(_SRC_DIR, 'testing'))
sys.path.insert(0, os.path.join(_SRC_DIR, 'build', 'util'))

# pylint: disable=wrong-import-position
from lib.results import result_sink  # noqa: E402
from lib.results import result_types  # noqa: E402
import test_env  # noqa: E402
# pylint: enable=wrong-import-position

_SUITE_NAME = 'zucchini_dex_integration_test'

_JSON_STATUS_MAP = {
    'SUCCESS': 'PASS',
    'SKIPPED': 'SKIP',
    'NOTRUN': 'SKIP',
    'TIMEOUT': 'TIMEOUT',
    'CRASH': 'CRASH',
}

# Global tags emitted by //base/test/launcher/test_launcher.cc when the test
# run was aborted, killed by a signal, or produced unreliable results.
_INTERRUPTED_GLOBAL_TAGS = frozenset(
    {
        'BROKEN_TEST_EARLY_EXIT',
        'CAUGHT_TERMINATION_SIGNAL',
        'EARLY_SUMMARY',
        'UNRELIABLE_RESULTS',
    }
)


def _is_interrupted(summary, returncode):
    """Returns whether the gtest launcher summary indicates an interrupted run.

    Args:
        summary: Parsed JSON dict from `--test-launcher-summary-output`.
        returncode: Process exit code of the gtest launcher.

    Returns:
        True if the run was killed by a signal, produced no summary, exited
        non-zero with no test results, or emitted an interruption global tag.
    """
    if returncode < 0 or not summary:
        return True
    iterations = summary.get('per_iteration_data', [])
    has_results = any(bool(d) for d in iterations)
    if returncode != 0 and not has_results:
        return True
    global_tags = set(summary.get('global_tags', ()))
    return bool(global_tags & _INTERRUPTED_GLOBAL_TAGS)


def _convert_summary_to_json_v3(summary, start_time, returncode):
    """Converts a gtest launcher summary dict into JSON Test Results v3 format.

    Args:
        summary: Parsed JSON dict from `--test-launcher-summary-output`.
        start_time: Epoch timestamp (seconds) when the test run started.
        returncode: Process exit code of the gtest launcher.

    Returns:
        A dict formatted according to the JSON Test Results v3 schema.
    """
    interrupted = _is_interrupted(summary, returncode)

    all_attempts = {}
    for iteration in summary.get('per_iteration_data', []):
        for test_name, attempts in iteration.items():
            if attempts:
                all_attempts.setdefault(test_name, []).extend(attempts)

    tests = {}
    num_failures_by_type = {}
    for test_name, attempts in all_attempts.items():
        statuses = [
            _JSON_STATUS_MAP.get(a.get('status'), 'FAIL') for a in attempts
        ]
        total_ms = sum(a.get('elapsed_time_ms', 0) for a in attempts)
        # Use the final attempt status so num_failures_by_type matches
        # actual.split()[-1] consistently across retries and --gtest_repeat.
        final_status = statuses[-1]
        num_failures_by_type[final_status] = (
            num_failures_by_type.get(final_status, 0) + 1
        )
        entry = {
            'expected': 'PASS',
            'actual': ' '.join(statuses),
            'time': total_ms / 1000.0,
        }
        if attempts[-1].get('status') == 'NOTRUN':
            entry['is_unexpected'] = True
        if '.' in test_name:
            suite, case = test_name.split('.', 1)
            tests.setdefault(suite, {})[case] = entry
        else:
            tests[test_name] = entry

    return {
        'version': 3,
        'interrupted': interrupted,
        'path_delimiter': '.',
        'seconds_since_epoch': start_time,
        'num_failures_by_type': num_failures_by_type,
        'tests': tests,
    }


def _upload_to_result_sink(summary, returncode):
    """Uploads per-attempt gtest results to ResultSink if active.

    Args:
        summary: Parsed JSON dict from `--test-launcher-summary-output`.
        returncode: Subprocess exit code from running the test binary.
    """
    client = result_sink.TryInitClient()
    if not client:
        return
    with client:
        posted_any = False
        posted_failure = False
        for iteration in summary.get('per_iteration_data', []):
            for test_name, attempts in iteration.items():
                suite, _, case = test_name.partition('.')
                struct_test_dict = {
                    'coarseName': None,
                    'fineName': suite if case else _SUITE_NAME,
                    'caseNameComponents': [case if case else test_name],
                }
                for attempt in attempts:
                    posted_any = True
                    status = attempt.get('status')
                    if status not in result_sink.RESULT_MAP:
                        status = result_types.FAIL
                    if status not in (result_types.PASS, result_types.SKIP):
                        posted_failure = True
                    client.Post(
                        test_name,
                        status,
                        attempt.get('elapsed_time_ms', 0),
                        attempt.get('output_snippet', ''),
                        None,
                        test_id_structured=struct_test_dict,
                    )

        if returncode != 0 and (not posted_any or not posted_failure):
            status = (
                result_types.CRASH
                if returncode < 0 or not summary
                else result_types.FAIL
            )
            detail = (
                'completing test execution'
                if posted_any
                else 'producing test results'
            )
            message = (
                f'Test launcher exited with code {returncode} without {detail}.'
            )
            client.Post(
                f'{_SUITE_NAME}.*fixture',
                status,
                0,
                message,
                None,
                test_id_structured={
                    'coarseName': None,
                    'fineName': _SUITE_NAME,
                    'caseNameComponents': ['*fixture'],
                },
                failure_reason=message,
            )


def main():
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('test_bin', help='Path to the gtest executable.')
    parser.add_argument(
        '--isolated-script-test-output',
        help='Path to write JSON Test Results v3 output.',
    )
    parser.add_argument(
        '--isolated-script-test-chartjson-output',
        help='Unused isolated script test flag accepted for compatibility.',
    )
    parser.add_argument(
        '--isolated-script-test-perf-output',
        help='Unused isolated script test flag accepted for compatibility.',
    )
    args, extra_args = parser.parse_known_args()

    with tempfile.TemporaryDirectory() as tmp_dir:
        summary_path = os.path.join(tmp_dir, 'summary.json')
        cmd = (
            [args.test_bin]
            + extra_args
            + [f'--test-launcher-summary-output={summary_path}']
        )

        print('Running:', shlex.join(cmd), flush=True)
        start_time = time.time()
        returncode = test_env.run_command(cmd, log=False)

        summary = {}
        try:
            with open(summary_path, encoding='utf-8') as fp:
                summary = json.load(fp)
        except (OSError, ValueError):
            pass
        if args.isolated_script_test_output:
            results_json = _convert_summary_to_json_v3(
                summary, start_time, returncode=returncode
            )
            with open(
                args.isolated_script_test_output,
                'w',
                encoding='utf-8',
                newline='',
            ) as fp:
                json.dump(results_json, fp, indent=2)

        try:
            _upload_to_result_sink(summary, returncode)
        except Exception:  # pylint: disable=broad-except
            logging.exception('Failed to upload results to ResultSink')

        return returncode


if __name__ == '__main__':
    sys.exit(main())
