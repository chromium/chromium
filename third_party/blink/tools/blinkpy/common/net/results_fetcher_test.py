# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import json
import logging
import unittest

from blinkpy.common.net.results_fetcher import TestResultsFetcher, Build, filter_latest_builds
from blinkpy.common.net.web_mock import MockWeb
from blinkpy.common.system.log_testing import LoggingTestCase


class BuilderTest(LoggingTestCase):

    def setUp(self):
        self.set_logging_level(logging.DEBUG)

    def test_results_url_no_build_number(self):
        self.assertEqual(
            TestResultsFetcher().results_url('Test Builder'),
            'https://test-results.appspot.com/data/layout_results/Test_Builder/results/layout-test-results')

    def test_results_url_with_build_number(self):
        self.assertEqual(
            TestResultsFetcher().results_url('Test Builder', 10),
            'https://test-results.appspot.com/data/layout_results/Test_Builder/10/layout-test-results')

    def test_results_url_with_build_number_step_name(self):
        self.assertEqual(
            TestResultsFetcher().results_url('Test Builder', 10,
                                   'webkit_layout_tests (with patch)'),
            'https://test-results.appspot.com/data/layout_results/Test_Builder'
            '/10/webkit_layout_tests%20%28with%20patch%29/layout-test-results')

    def test_results_url_with_non_numeric_build_number(self):
        with self.assertRaisesRegexp(AssertionError, 'expected numeric build number'):
            TestResultsFetcher().results_url('Test Builder', 'ba5eba11')

    def test_builder_results_url_base(self):
        self.assertEqual(
            TestResultsFetcher().builder_results_url_base('WebKit Mac10.8 (dbg)'),
            'https://test-results.appspot.com/data/layout_results/WebKit_Mac10_8__dbg_')

    def test_accumulated_results_url(self):
        self.assertEqual(
            TestResultsFetcher().accumulated_results_url_base('WebKit Mac10.8 (dbg)'),
            'https://test-results.appspot.com/data/layout_results/WebKit_Mac10_8__dbg_/results/layout-test-results')

    def test_fetch_web_test_results_with_no_results_fetched(self):
        fetcher = TestResultsFetcher()
        fetcher.web = MockWeb()
        results = fetcher.fetch_web_test_results(fetcher.results_url('B'))
        self.assertIsNone(results)
        self.assertLog([
            'DEBUG: Got 404 response from:\n'
            'https://test-results.appspot.com/data/layout_results/B/results/layout-test-results/failing_results.json\n'
        ])

    def test_fetch_results_with_weird_step_name(self):
        fetcher = TestResultsFetcher()
        fetcher.web = MockWeb(urls={
            'https://test-results.appspot.com/testfile?buildnumber=123&'
            'callback=ADD_RESULTS&builder=builder&name=full_results.json':
                'ADD_RESULTS(%s);' % (json.dumps(
                    [{"TestType": "webkit_layout_tests on Intel GPU (with patch)"},
                     {"TestType": "base_unittests (with patch)"}])),
            'https://test-results.appspot.com/data/layout_results/builder/123/'
            'webkit_layout_tests%20on%20Intel%20GPU%20%28with%20patch%29/'
            'layout-test-results/failing_results.json':
                json.dumps({'passed': True}),
        })
        results = fetcher.fetch_results(Build('builder', 123))
        self.assertEqual(results._results, {  # pylint: disable=protected-access
            'passed': True
        })
        self.assertLog([])

    def test_fetch_results_without_build_number(self):
        fetcher = TestResultsFetcher()
        self.assertIsNone(fetcher.fetch_results(Build('builder', None)))

    def test_get_step_name(self):
        fetcher = TestResultsFetcher()
        fetcher.web = MockWeb(urls={
            'https://test-results.appspot.com/testfile?buildnumber=5&'
            'callback=ADD_RESULTS&builder=foo&name=full_results.json':
                'ADD_RESULTS(%s);' % (json.dumps(
                    [{"TestType": "webkit_layout_tests (with patch)"},
                     {"TestType": "not_site_per_process_webkit_layout_tests (with patch)"},
                     {"TestType": "webkit_layout_tests (retry with patch)"},
                     {"TestType": "base_unittests (with patch)"}]))
        })
        step_name = fetcher.get_layout_test_step_name(Build('foo', 5))
        self.assertEqual(step_name, 'webkit_layout_tests (with patch)')
        self.assertLog([])

    def test_get_step_name_without_build_number(self):
        fetcher = TestResultsFetcher()
        self.assertIsNone(
            fetcher.get_layout_test_step_name(Build('builder', None)))

    def test_fetch_webdriver_results_without_build_number(self):
        fetcher = TestResultsFetcher()
        self.assertIsNone(fetcher.fetch_webdriver_test_results(
            Build('builder', None), 'bar'))
        self.assertLog(
            ['DEBUG: Builder name or build number or master is None\n'])

    def test_fetch_webdriver_results_without_master(self):
        fetcher = TestResultsFetcher()
        self.assertIsNone(fetcher.fetch_webdriver_test_results(
            Build('builder', 1), ''))
        self.assertLog(
            ['DEBUG: Builder name or build number or master is None\n'])

    def test_fetch_webdriver_test_results_with_no_results(self):
        fetcher = TestResultsFetcher()
        fetcher.web = MockWeb()
        results = fetcher.fetch_webdriver_test_results(
            Build('bar-rel', 123), 'foo.chrome')
        self.assertIsNone(results)
        self.assertLog([
            'DEBUG: Got 404 response from:\n'
            'https://test-results.appspot.com/testfile?buildnumber=123&'
            'master=foo.chrome&builder=bar-rel&'
            'testtype=webdriver_tests_suite+%28with+patch%29&name=full_results.json\n'
        ])

    def test_fetch_webdriver_results_success(self):
        fetcher = TestResultsFetcher()
        fetcher.web = MockWeb(urls={
            'https://test-results.appspot.com/testfile?buildnumber=123&'
            'master=foo.chrome&builder=bar-rel&'
            'testtype=webdriver_tests_suite+%28with+patch%29&'
            'name=full_results.json':
                json.dumps({'passed': True}),
        })
        results = fetcher.fetch_webdriver_test_results(
            Build('bar-rel', 123), 'foo.chrome')
        self.assertEqual(results._results, {  # pylint: disable=protected-access
            'passed': True
        })
        self.assertLog([])


class TestResultsFetcherHelperFunctionTest(unittest.TestCase):

    def test_filter_latest_jobs_empty(self):
        self.assertEqual(filter_latest_builds([]), [])

    def test_filter_latest_jobs_higher_build_first(self):
        self.assertEqual(
            filter_latest_builds(
                [Build('foo', 5), Build('foo', 3), Build('bar', 5)]),
            [Build('bar', 5), Build('foo', 5)])

    def test_filter_latest_jobs_higher_build_last(self):
        self.assertEqual(
            filter_latest_builds(
                [Build('foo', 3), Build('bar', 5), Build('foo', 5)]),
            [Build('bar', 5), Build('foo', 5)])

    def test_filter_latest_jobs_no_build_number(self):
        self.assertEqual(
            filter_latest_builds(
                [Build('foo', 3), Build('bar'), Build('bar')]),
            [Build('bar'), Build('foo', 3)])
