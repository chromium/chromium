# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.rpc_mock import MockRpc
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.wpt_uploader import WptReportUploader


class WptReportUploaderTest(LoggingTestCase):
    def setUp(self):
        super(WptReportUploaderTest, self).setUp()
        self.host = MockHost()

    def test_fetch_wpt_report_urls(self):
        uploader = WptReportUploader(self.host)
        uploader._rpc = MockRpc(self.host)
        res = {'artifacts': [{'name': 'report1',
                              'artifactId': 'wpt_reports_dada.json',
                              'fetchUrl': 'https://a.b.c/report1.json',
                              'sizeBytes': '8472164'},
                             {'name': 'report2',
                              'artifactId': 'wpt_reports_dada.json',
                              'fetchUrl': 'https://a.b.c/report2.json',
                              'sizeBytes': '8455564'},
                             {'name': 'other',
                              'artifactId': 'other_dada.json',
                              'fetchUrl': 'https://a.b.c/other.json',
                              'sizeBytes': '9845'}]}
        uploader._rpc.set_response(res)
        self.assertEqual(uploader.fetch_wpt_report_urls("31415926535"),
                         ['https://a.b.c/report1.json',
                          'https://a.b.c/report2.json'])

        res = {'artifacts': [{'name': 'other',
                              'artifactId': 'other_dada.json',
                              'fetchUrl': 'https://a.b.c/other.json',
                              'sizeBytes': '9845'}]}
        uploader._rpc.set_response(res)
        self.assertEqual(uploader.fetch_wpt_report_urls("31415926535"), [])

        res = {}
        uploader._rpc.set_response(res)
        self.assertEqual(uploader.fetch_wpt_report_urls("31415926535"), [])

    def test_fetch_latest_complete_build(self):
        uploader = WptReportUploader(self.host)
        uploader._rpc = MockRpc(self.host)
        builder = ("chromium", "ci", "test_builder")
        expected = {"id": "31415926535",
                    "builder": {"builder": "test_builder"},
                    "status": "SUCCESS",
                    "number": "100"}
        res = {"builds": [expected,
                          {"id": "89793238462",
                           "builder": {"builder": "test_builder"},
                           "status": "SUCCESS",
                           "number": "99"},
                          {"id": "64338327950",
                           "builder": {"builder": "test_builder"},
                           "status": "SUCCESS",
                           "number": "98"}]}
        uploader._rpc.set_response(res)
        build = uploader.fetch_latest_complete_build(*builder)
        self.assertEqual(build, expected)

        res = {"builds": []}
        uploader._rpc.set_response(res)
        build = uploader.fetch_latest_complete_build(*builder)
        self.assertIsNone(build)

    def test_merge_reports(self):
        uploader = WptReportUploader(self.host)
        report0 = {"run_info": {"os": "linux",
                                "processor": "x86_64",
                                "product": "android_webview",
                                "revision": "1408b119ac563b427a3e00a5514eef697c8da268"},
                   "time_start": 4248,
                   "results": [{"test": "foo1.html",
                                "status": "PASS",
                                "duration": 2100},
                               {"test": "foo2.html",
                                "status": "FAIL",
                                "duration": 300}],
                   "time_end": 7293}
        self.assertEqual(uploader.merge_reports([report0]), report0)

        report1 = {"run_info": {"os": "linux",
                                "processor": "x86_64",
                                "product": "android_webview",
                                "revision": "1408b119ac563b427a3e00a5514eef697c8da268"},
                   "time_start": 4200,
                   "results": [{"test": "bar.html",
                                "status": "PASS",
                                "duration": 990}],
                   "time_end": 7200}

        report2 = {"run_info": {"os": "linux",
                                "processor": "x86_64",
                                "product": "android_webview",
                                "revision": "1408b119ac563b427a3e00a5514eef697c8da268"},
                   "time_start": 5200,
                   "results": [{"test": "test.html",
                                "status": "PASS",
                                "duration": 990}],
                   "time_end": 7999}

        _expect = {"run_info": {"os": "linux",
                                "processor": "x86_64",
                                "product": "android_webview",
                                "revision": "1408b119ac563b427a3e00a5514eef697c8da268"},
                   "time_start": 4200,
                   "results": [{"test": "foo1.html",
                                "status": "PASS",
                                "duration": 2100},
                               {"test": "foo2.html",
                                "status": "FAIL",
                                "duration": 300},
                               {"test": "bar.html",
                                "status": "PASS",
                                "duration": 990},
                               {"test": "test.html",
                                "status": "PASS",
                                "duration": 990}],
                   "time_end": 7999}
        self.assertEqual(uploader.merge_reports([report0, report1, report2]),
                         _expect)
