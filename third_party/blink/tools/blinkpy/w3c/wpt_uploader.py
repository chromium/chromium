# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uploads Wpt test results from Chromium to wpt.fyi."""

import argparse
import gzip
import json
import logging
import os
import requests
import tempfile

from blinkpy.common.net.rpc import Rpc
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.common import read_credentials

_log = logging.getLogger(__name__)


class WptReportUploader(object):
    def __init__(self, host):
        self._host = host
        self._rpc = Rpc(host)
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
        ]
        for builder in builders:
            reports = []
            _log.info("Uploading report for %s" % builder[2])
            build = self.fetch_latest_complete_build(*builder)
            if build:
                _log.info("Find latest completed build %d" % build.get("number"))
                urls = self.fetch_wpt_report_urls(build.get("id"))
                for url in urls:
                    _log.info("Fetching wpt report from %s" % url)
                    res = self._host.web.request("GET", url)
                    if res.getcode() == 200:
                        body = res.read()
                        reports.append(json.loads(body))
                    else:
                        _log.error("Failed to fetch wpt report.")

            merged_report = self.merge_reports(reports)

            with tempfile.TemporaryDirectory() as tmpdir:
                path = os.path.join(tmpdir, "reports.json.gz")
                with gzip.open(path, 'wt', encoding="utf-8") as zipfile:
                    json.dump(merged_report, zipfile)
                rv = rv | self.upload_report(path)
            _log.info(" ")

        return rv

    def fetch_wpt_report_urls(self, build_id):
        """Get a list of fetchUrl for wpt-report from given build.

        This uses the QueryArtifacts rpc format specified in
        https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/proto/v1/resultdb.proto

        The response is a list of dicts of the following form:

        {
            'artifacts': [
                {
                    'name': 'some name',
                    'artifactId': 'wpt_reports_dada.json',
                    'fetchUrl': 'https://something...',
                    'fetchUrlExpiration': 'some future time',
                    'sizeBytes': '8472164'
                },
                ... more artifacts
            ]
        }

        An example of the url as below:
        https://results.usercontent.cr.dev/invocations/ \
        task-chromium-swarm.appspot.com-58590ed6228fd611/ \
        artifacts/wpt_reports_android_webview_01.json? \
        token=AXsiX2kiOiIxNjQxNzYyNzU0MDkxIiwiX3giOiIzNjAwMDAwIn24WM72ciT_oYJG0hGx6MShOXu8SyVxfB_fw

        Returns a sorted(based on shard number) list of URLs for wpt report
        """

        invocation = "invocations/build-%s" % build_id
        data = {
            "invocations": [invocation],
            "predicate": {
                "followEdges": {"includedInvocations": True}
            }
        }
        url = 'https://results.api.cr.dev/prpc/luci.resultdb.v1.ResultDB/QueryArtifacts'
        res = self._rpc.luci_rpc(url, data)
        artifacts = res.get("artifacts") if res else None
        if not artifacts:
            return []

        rv = []
        for artifact in artifacts:
            if artifact.get("artifactId").startswith("wpt_reports"):
                rv.append(artifact.get("fetchUrl"))

        if len(rv) > 0:
            pos = rv[0].find("wpt_reports")
            rv.sort(key=lambda x: x[pos:])

        return rv

    def fetch_latest_complete_build(self, project, bucket, builder_name):
        """Gets latest successful build from a CI builder.

        This uses the SearchBuilds rpc format specified in
        https://cs.chromium.org/chromium/infra/go/src/go.chromium.org/luci/buildbucket/proto/rpc.proto

        The response is a list of dicts of the following form:
        {
           "builds": [
               {
                   "id": "8828280326907235505",
                   "builder": {
                       "builder": "android-webview-pie-x86-wpt-fyi-rel"
                   },
                   "status": "SUCCESS"
               },
               ... more builds
        }

        This method returns the latest finished build.
        """
        data = {
            "predicate": {
                "builder": {
                    "project": project,
                    "bucket": bucket,
                    "builder": builder_name
                },
                "status": "SUCCESS"
            },
            "fields": "builds.*.builder.builder,builds.*.number,builds.*.status,builds.*.id",
            "pageSize": 10
        }
        url = 'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/SearchBuilds'
        raw_results_json = self._rpc.luci_rpc(url, data)
        if 'builds' not in raw_results_json:
            return None
        builds = raw_results_json['builds']
        return builds[0] if builds else None

    def upload_report(self, path_to_report):
        """Upload the wpt report to wpt.fyi

        The Api is defined at:
        https://github.com/web-platform-tests/wpt.fyi/tree/main/api#results-creation
        """
        username = "chromium-ci-results-uploader"
        url = "https://staging.wpt.fyi/api/results/upload"

        with open(path_to_report, 'rb') as fp:
            files = {'result_file': fp}
            if self._dry_run:
                _log.info("Dry run, no report uploaded.")
                return 0
            session = requests.Session()
            credentials = read_credentials(self._host, self.options.credentials_json)
            if not credentials.get('GH_TOKEN'):
                _log.error("No password available, can not upload wpt reports.")
                return 1

            password = credentials['GH_TOKEN'][0:16]
            session.auth = (username, password)
            res = session.post(url=url, files=files)
            if res.status_code == 200:
                _log.info("Successfully uploaded wpt report with response: " + res.text.strip())
                report_id = res.text.split()[1]
                _log.info("Report uploaded to https://staging.wpt.fyi/results?run_id=%s" % report_id)
                return 0
            else:
                _log.error("Upload wpt report failed with status code: %d", res.status_code)
                return 1

    def merge_reports(self, reports):
        if not reports:
            return {}

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
