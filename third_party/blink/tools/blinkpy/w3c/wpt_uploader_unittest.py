# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import gzip

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.wpt_uploader import WptReportUploader


class WptReportUploaderTest(LoggingTestCase):
    def setUp(self):
        super(WptReportUploaderTest, self).setUp()
        self.host = MockHost()

    def test_fetch_latest_complete_build(self):
        uploader = WptReportUploader(self.host)
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
        self.host.results_fetcher.web.append_prpc_response(res)
        build = uploader.fetch_latest_complete_build(*builder)
        self.assertEqual(build, expected)

        res = {"builds": []}
        self.host.results_fetcher.web.append_prpc_response(res)
        build = uploader.fetch_latest_complete_build(*builder)
        self.assertIsNone(build)

    def test_encode_result_files(self):
        uploader = WptReportUploader(self.host)
        files = uploader.encode_result_files(
            [b'{"time_start":0}', b'{"time_start":1}'],
            [b'data:image/png;base64,iVBORw\n'])
        self.assertEqual(3, len(files), files)

        form_field, (filename, payload, content_type) = files[0]
        self.assertEqual('result_file', form_field)
        self.assertEqual('result_0.json.gz', filename)
        self.assertEqual({
            'time_start': 0,
        }, json.loads(gzip.decompress(payload)))
        self.assertEqual('application/gzip', content_type)

        form_field, (filename, payload, content_type) = files[1]
        self.assertEqual('result_file', form_field)
        self.assertEqual('result_1.json.gz', filename)
        self.assertEqual({
            'time_start': 1,
        }, json.loads(gzip.decompress(payload)))
        self.assertEqual('application/gzip', content_type)

        form_field, (filename, payload, content_type) = files[2]
        self.assertEqual('screenshot_file', form_field)
        self.assertEqual('screenshots.txt.gz', filename)
        self.assertEqual(b'data:image/png;base64,iVBORw\n',
                         gzip.decompress(payload))
        self.assertEqual('application/gzip', content_type)

    def test_merge_screenshots(self):
        uploader = WptReportUploader(self.host)
        screenshots = [b'a\nb\n', b'c\n']
        self.assertEqual(b'a\nb\nc\n',
                         uploader.merge_screenshots(screenshots, size_limit=6))
        self.assertEqual(b'a\nb\n',
                         uploader.merge_screenshots(screenshots, size_limit=5))
        self.assertEqual(b'a\nb\n',
                         uploader.merge_screenshots(screenshots, size_limit=4))
        self.assertEqual(b'a\n',
                         uploader.merge_screenshots(screenshots, size_limit=3))
