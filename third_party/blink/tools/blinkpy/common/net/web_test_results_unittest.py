# Copyright (c) 2010, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from blinkpy.common.net.web_test_results import WebTestResults


class WebTestResultsTest(unittest.TestCase):
    # The real files have no whitespace, but newlines make this much more readable.
    example_full_results_json = b"""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "many-mismatches.html": {
                    "expected": "PASS",
                    "actual": "FAIL",
                    "artifacts": {
                        "actual_text": ["fast/dom/many-mismatches-actual.txt"],
                        "expected_text": ["fast/dom/many-mismatches-expected.txt"],
                        "actual_image": ["fast/dom/many-mismatches-actual.png"],
                        "expected_image": ["fast/dom/many-mismatches-expected.png"]
                    },
                    "is_unexpected": true
                },
                "mismatch-implicit-baseline.html": {
                    "expected": "PASS",
                    "actual": "FAIL",
                    "artifacts": {
                        "actual_text": ["fast/dom/mismatch-implicit-baseline-actual.txt"]
                    },
                    "is_unexpected": true
                },
                "reference-mismatch.html": {
                    "expected": "PASS",
                    "actual": "FAIL",
                    "artifacts": {
                        "actual_image": ["fast/dom/reference-mismatch-actual.png"],
                        "expected_image": ["fast/dom/reference-mismatch-expected.png"],
                        "reference_file_mismatch": ["reference-mismatch-ref.html"]
                    },
                    "is_unexpected": true
                },
                "unexpected-pass.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                },
                "unexpected-flaky.html": {
                    "expected": "PASS",
                    "actual": "PASS FAIL",
                    "is_unexpected": true
                },
                "expected-flaky.html": {
                    "expected": "PASS FAIL",
                    "actual": "PASS FAIL"
                },
                "expected-fail.html": {
                    "expected": "FAIL",
                    "actual": "FAIL"
                },
                "missing-text.html": {
                    "expected": "PASS",
                    "actual": "FAIL",
                    "artifacts": {
                        "actual_text": ["fast/dom/missing-text-actual.txt"]
                    },
                    "is_unexpected": true,
                    "is_missing_text": true
                },
                "prototype-slow.html": {
                    "expected": "SLOW",
                    "actual": "FAIL",
                    "is_unexpected": true
                }
            }
        },
        "svg": {
            "dynamic-updates": {
                "SVGFEDropShadowElement-dom-stdDeviation-attr.html": {
                    "expected": "PASS",
                    "actual": "FAIL",
                    "has_stderr": true,
                    "is_unexpected": true
                }
            }
        }
    },
    "skipped": 450,
    "num_regressions": 6,
    "layout_tests_dir": "/b/build/slave/Webkit_Mac10_5/build/src/third_party/blink/web_tests",
    "version": 3,
    "num_passes": 1,
    "num_flaky": 1,
    "chromium_revision": "1234",
    "builder_name": "mock_builder_name"
});"""

    def test_empty_results_from_string(self):
        self.assertIsNone(WebTestResults.results_from_string(None))
        self.assertIsNone(WebTestResults.results_from_string(''))

    def test_was_interrupted(self):
        results = WebTestResults.results_from_string(
            b'ADD_RESULTS({"tests":{},"interrupted":true});')
        self.assertIsNotNone(results.incomplete_reason)
        results = WebTestResults.results_from_string(
            b'ADD_RESULTS({"tests":{},"interrupted":false});')
        self.assertIsNone(results.incomplete_reason)

    def test_chromium_revision(self):
        self.assertEqual(
            WebTestResults.results_from_string(
                self.example_full_results_json).chromium_revision(), 1234)

    def test_didnt_run_as_expected_results(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertEqual([
            r.test_name() for r in results.didnt_run_as_expected_results()
        ], [
            'fast/dom/many-mismatches.html',
            'fast/dom/mismatch-implicit-baseline.html',
            'fast/dom/missing-text.html',
            'fast/dom/prototype-slow.html',
            'fast/dom/reference-mismatch.html',
            'fast/dom/unexpected-flaky.html',
            'fast/dom/unexpected-pass.html',
            'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html',
        ])

    def test_result_for_test_non_existent(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertFalse(results.result_for_test('nonexistent.html'))

    # The following are tests for a single WebTestResult.

    def test_actual_results(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertEqual(
            results.result_for_test(
                'fast/dom/unexpected-pass.html').actual_results(), ['PASS'])
        self.assertEqual(
            results.result_for_test(
                'fast/dom/unexpected-flaky.html').actual_results(),
            ['PASS', 'FAIL'])

    def test_expected_results(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertEqual(
            results.result_for_test('fast/dom/many-mismatches.html').
            expected_results(), 'PASS')
        self.assertEqual(
            results.result_for_test('fast/dom/expected-flaky.html').
            expected_results(), 'PASS FAIL')

    def test_has_mismatch(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertTrue(
            results.result_for_test(
                'fast/dom/many-mismatches.html').has_mismatch())
        self.assertTrue(
            results.result_for_test(
                'fast/dom/mismatch-implicit-baseline.html').has_mismatch())

    def test_is_missing_baseline(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        self.assertTrue(
            results.result_for_test('fast/dom/missing-text.html').
            is_missing_baseline())
        self.assertFalse(
            results.result_for_test('fast/dom/many-mismatches.html').
            is_missing_baseline())

    def test_suffixes_for_test_result(self):
        results = WebTestResults.results_from_string(
            self.example_full_results_json)
        result = results.result_for_test('fast/dom/many-mismatches.html')
        self.assertEqual(set(result.baselines_by_suffix()), {'txt', 'png'})
        result = results.result_for_test('fast/dom/missing-text.html')
        self.assertEqual(set(result.baselines_by_suffix()), {'txt'})
