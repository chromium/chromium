# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uploads Wpt test results from Chromium to wpt.fyi."""

import argparse
import base64
import gzip
import json
import logging
import os
import requests
import six
import tempfile

from blinkpy.common.net.rpc import BuildbucketClient
from blinkpy.common.system.log_utils import configure_logging

_log = logging.getLogger(__name__)


class WptReportUploader(object):
    def __init__(self, host):
        self._host = host
        self._bb_client = BuildbucketClient.from_host(host)
        self.options = None
        self._dry_run = False
        configure_logging(logging_level=logging.INFO, include_time=True)

    def main(self, argv=None):
        """Pull wpt_report.json from latest CI runs, merge the reports and
        upload that to wpt.fyi.

        Returns:
            A boolean: True if success, False if there were any failures.
        """
        self.options = self.parse_args(argv)
        if self.options.verbose:
            configure_logging(logging_level=logging.DEBUG, include_time=True)
        self._dry_run = self.options.dry_run

        rv = 0

        builders = [
            ("chromium", "ci", "android-webview-pie-x86-wpt-fyi-rel"),
            ("chromium", "ci", "android-chrome-pie-x86-wpt-fyi-rel"),
            ("chromium", "ci", "ios-wpt-fyi-rel"),
        ]
        for builder in builders:
            reports = []
            _log.info("Uploading report for %s" % builder[2])
            build = self.fetch_latest_complete_build(*builder)
            if build:
                _log.info("Find latest completed build %d" % build.get("number"))
                # pylint: disable=unsubscriptable-object
                urls = self._host.results_fetcher.fetch_wpt_report_urls(
                    build["id"])
                for url in urls:
                    _log.info("Fetching wpt report from %s" % url)
                    body = self._host.web.get_binary(url,
                                                     return_none_on_404=True)
                    if not body:
                        _log.error("Failed to fetch wpt report.")
                        continue
                    # Ignore retry results on subsequent lines.
                    initial_report, _, _ = body.partition(b'\n')
                    reports.append(json.loads(initial_report))
            merged_report = self.merge_reports(reports)
            if merged_report is None:
                _log.error("No result to upload, skip...")
            else:
                with tempfile.TemporaryDirectory() as tmpdir:
                    path = os.path.join(tmpdir, "reports.json.gz")
                    with gzip.open(path, 'wt', encoding="utf-8") as zipfile:
                        json.dump(merged_report, zipfile)
                    rv = rv | self.upload_report(path)
            _log.info(" ")

        return rv

    def fetch_latest_complete_build(self, project, bucket, builder_name):
        """Gets latest successful build from a CI builder.

        This uses the SearchBuilds RPC format specified in:
            https://cs.chromium.org/chromium/infra/go/src/go.chromium.org/luci/buildbucket/proto/builder_service.proto

        The 'builds' field of the response is a list of dicts of the following
        form:
            [
                {
                    "id": "8828280326907235505",
                    "builder": {
                        "builder": "android-webview-pie-x86-wpt-fyi-rel"
                    },
                    "status": "SUCCESS"
                },
                ... more builds,
            ]

        This method returns the latest finished build.
        """
        predicate = {
            "builder": {
                "project": project,
                "bucket": bucket,
                "builder": builder_name,
            },
            "status": "SUCCESS",
        }
        builds = self._bb_client.search_builds(
            predicate, ['builder.builder', 'number', 'status', 'id'], count=10)
        return builds[0] if builds else None

    def get_password(self):
        from google.cloud import kms
        import crcmod

        def crc32c(data):
            crc32c_fun = crcmod.predefined.mkPredefinedCrcFun('crc-32c')
            return crc32c_fun(six.ensure_binary(data))

        project_id = 'blink-kms'
        location_id = 'global'
        key_ring_id = 'chrome-official'
        key_id = 'autoroller_key'
        key_data = (b'CiQAcoZ22AXJttAoPI544QvH4C1jSnvVpe/XN+43vZan/RdbSmcSYyph'
                    b'ChQKDLy9d1hq3L5Vr0veUBDI7oTJDBIvCifABA4GBbd+dfwbhbFAuQ5R'
                    b'XZhu4Bl036JRYMtYZNrE4evBBMsO94YQ1qGnkggaGAoQ0eZ5gffcfN+M'
                    b'YBfWzGxvtxDy6KSYBw==')
        ciphertext = base64.b64decode(key_data)
        ciphertext_crc32c = crc32c(ciphertext)
        client = kms.KeyManagementServiceClient()
        key_name = client.crypto_key_path(project_id, location_id, key_ring_id, key_id)
        decrypt_response = client.decrypt(
            request={'name': key_name, 'ciphertext': ciphertext, 'ciphertext_crc32c': ciphertext_crc32c})
        if not decrypt_response.plaintext_crc32c == crc32c(decrypt_response.plaintext):
            raise Exception('The response received from the server was corrupted in-transit.')
        return decrypt_response.plaintext.decode('utf-8')

    def upload_report(self, path_to_report):
        """Upload the wpt report to wpt.fyi

        The Api is defined at:
        https://github.com/web-platform-tests/wpt.fyi/tree/main/api#results-creation
        """
        username = "chromium-ci-results-uploader"
        fqdn = "wpt.fyi"
        url = "https://%s/api/results/upload" % fqdn

        with open(path_to_report, 'rb') as fp:
            params = {'labels': 'master'}
            files = {'result_file': fp}
            if self._dry_run:
                _log.info("Dry run, no report uploaded.")
                return 0
            session = requests.Session()
            password = self.get_password()
            session.auth = (username, password)
            res = session.post(url=url, params=params, files=files)
            if res.status_code == 200:
                _log.info("Successfully uploaded wpt report with response: " + res.text.strip())
                report_id = res.text.split()[1]
                _log.info("Report uploaded to https://%s/results?run_id=%s" % (fqdn, report_id))
                return 0
            else:
                _log.error("Upload wpt report failed with status code: %d", res.status_code)
                return 1

    def merge_reports(self, reports):
        if not reports:
            return None

        merged_report = {}
        merged_report['run_info'] = reports[0]['run_info']
        merged_report['time_start'] = reports[0]['time_start']
        merged_report['results'] = []
        merged_report['time_end'] = reports[0]['time_end']
        for report in reports:
            merged_report['time_start'] = min(merged_report['time_start'],
                                              report['time_start'])
            merged_report['results'].extend(report['results'])
            merged_report['time_end'] = max(merged_report['time_end'],
                                            report['time_end'])
        if not merged_report['results']:
            return None
        return merged_report

    def parse_args(self, argv):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='See what would be done without actually uploading any report.')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with wpt.fyi credentials')
        return parser.parse_args(argv)
