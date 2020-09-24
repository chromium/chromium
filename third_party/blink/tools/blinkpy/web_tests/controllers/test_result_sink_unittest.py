# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import mock
import sys
import unittest
from urlparse import urlparse

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.controllers.test_result_sink import CreateTestResultSink
from blinkpy.web_tests.controllers.test_result_sink import TestResultSink
from blinkpy.web_tests.models import test_results
from blinkpy.web_tests.models.typ_types import ResultType


class TestResultSinkTestBase(unittest.TestCase):
    def setUp(self):
        super(TestResultSinkTestBase, self).setUpClass()
        self.port = MockHost().port_factory.get()

    def luci_context(self, **section_values):
        if not section_values:
            return

        host = self.port.host
        f, fname = host.filesystem.open_text_tempfile()
        json.dump(section_values, f)
        f.close()
        host.environ['LUCI_CONTEXT'] = f.path


class TestCreateTestResultSink(TestResultSinkTestBase):
    def test_without_luci_context(self):
        self.assertIsNone(CreateTestResultSink(self.port))

    def test_without_result_sink_section(self):
        self.luci_context(app={'foo': 'bar'})
        self.assertIsNone(CreateTestResultSink(self.port))

    @mock.patch('urllib2.urlopen')
    def test_with_result_sink_section(self, urlopen):
        ctx = {'address': 'localhost:123', 'auth_token': 'secret'}
        self.luci_context(result_sink=ctx)
        r = CreateTestResultSink(self.port)
        self.assertIsNotNone(r)
        r.sink(True, test_results.TestResult('test'))

        urlopen.assert_called_once()
        req = urlopen.call_args[0][0]
        self.assertEqual(urlparse(req.get_full_url()).netloc, ctx['address'])
        self.assertEqual(req.get_header('Authorization'),
                         'ResultSink ' + ctx['auth_token'])


class TestResultSinkMessage(TestResultSinkTestBase):
    """Tests ResulkSink.sink."""

    def setUp(self):
        super(TestResultSinkMessage, self).setUp()
        patcher = mock.patch.object(TestResultSink, '_send')
        self.mock_send = patcher.start()
        self.addCleanup(patcher.stop)

        ctx = {'address': 'localhost:123', 'auth_token': 'super-secret'}
        self.luci_context(result_sink=ctx)
        self.r = CreateTestResultSink(self.port)

    def sink(self, expected, test_result):
        self.r.sink(expected, test_result)
        self.mock_send.called_once()
        return self.mock_send.call_args[0][0]['testResults'][0]

    def test_sink(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.total_run_time = 123.456
        tr.type = ResultType.Crash
        sent_data = self.sink(True, tr)

        self.assertEqual(sent_data['testId'], 'test-name')
        self.assertEqual(sent_data['expected'], True)
        self.assertEqual(sent_data['status'], 'CRASH')
        self.assertEqual(sent_data['duration'], '123.456s')

    def test_test_location(self):
        tr = test_results.TestResult('')
        prefix = '//third_party/blink/web_tests/'
        sink = lambda tr: self.sink(True, tr)['testLocation']['fileName']

        tr.test_name = "test-name"
        self.assertEqual(sink(tr), prefix + 'test-name')
        tr.test_name = "///test-name"
        self.assertEqual(sink(tr), prefix + '///test-name')

    def test_device_failure(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Failure
        tr.device_failed = True
        sent_data = self.sink(True, tr)

        # If the device failed, 'expected' and 'status' must be False and 'ABORT'
        self.assertEqual(sent_data['expected'], False)
        self.assertEqual(sent_data['status'], 'ABORT')

    def test_timeout(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Timeout
        sent_data = self.sink(True, tr)

        # Timeout is considered as 'ABORT'
        self.assertEqual(sent_data['status'], 'ABORT')

    def test_artifacts(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.artifacts.AddArtifact('test-image.png', '/tmp/test-image.png', True)
        tr.artifacts.AddArtifact('stdout', '/tmp/stdout', True)

        sent_data = self.sink(True, tr)
        self.assertDictEqual(
            sent_data['artifacts'], {
                'test-image.png': {
                    'filePath': '/tmp/test-image.png'
                },
                'stdout': {
                    'filePath': '/tmp/stdout'
                }
            })

    def test_artifacts_with_duplicate_paths(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.artifacts.AddArtifact('artifact', '/tmp/foo', False)
        tr.artifacts.AddArtifact('artifact', '/tmp/bar', False)

        sent_data = self.sink(True, tr)
        self.assertDictEqual(
            sent_data['artifacts'], {
                'artifact': {
                    'filePath': '/tmp/foo'
                },
                'artifact-1': {
                    'filePath': '/tmp/bar'
                }
            })
