# Copyright (c) 2009, Google Inc. All rights reserved.
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

import collections
import logging
import re

# pylint: disable=unused-import; `Build` is imported by other modules
from blinkpy.common.memoized import memoized
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.net.rpc import Build, ResultDBClient
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.web_tests.builder_list import BuilderList

_log = logging.getLogger(__name__)

RESULTS_SUMMARY_URL_BASE = 'https://storage.googleapis.com/chromium-layout-test-archives'


class TestResultsFetcher:
    """This class represents an interface to test results for particular builds.

    This includes fetching web test results from Google Storage;
    for more information about the web test result format, see:
        https://www.chromium.org/developers/the-json-test-results-format
    """

    _test_id_pattern = re.compile(
        r'ninja://\S*((blink|chrome)_(web|wpt)_tests|(chrome_public_wpt|trichrome_webview_wpt_64))/(?P<name>\S+)'
    )

    def __init__(self, web, luci_auth):
        self.web = web
        self._resultdb_client = ResultDBClient(web, luci_auth)

    @classmethod
    def from_host(cls, host):
        return cls(host.web, LuciAuth(host))

    @memoized
    def gather_results(self,
                       build: Build,
                       suite: str,
                       exclude_exonerated: bool = False,
                       only_unexpected: bool = True) -> WebTestResults:
        """Gather all web test results on a given build step from ResultDB."""
        assert build.build_id, f'{build} must set a build ID'
        assert not suite.endswith('(with patch)'), suite
        test_result_predicate = {
            # TODO(crbug.com/1428727): Using `read_mask` with
            # `VARIANTS_WITH_ONLY_UNEXPECTED_RESULTS` throws an internal server
            # error. Use `VARIANTS_WITH_UNEXPECTED_RESULTS` for now and filter
            # results client-side.
            'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS',
            'excludeExonerated': exclude_exonerated,
            'variant': {
                'contains': {
                    'def': {
                        'test_suite': suite,
                    },
                },
            },
        }
        read_mask = ['name', 'testId', 'tags', 'status', 'expected']
        test_results = self._resultdb_client.query_test_results(
            [build.build_id], test_result_predicate, read_mask)
        test_results_by_name = self._group_test_results_by_test_name(
            test_results)
        # TODO(crbug.com/1428727): Once the bug is fixed, use `expectancy` to
        # filter results server-side instead.
        if only_unexpected:
            test_results_by_name = {
                test: raw_results
                for test, raw_results in test_results_by_name.items()
                if not any(result.get('expected') for result in raw_results)
            }
        artifacts = self._resultdb_client.query_artifacts(
            [build.build_id], {
                'testResultPredicate': test_result_predicate,
                'artifactIdRegexp': 'actual_.*'
            })
        artifacts_by_run = self._group_artifacts_by_test_run(artifacts)
        return WebTestResults.from_rdb_responses(test_results_by_name,
                                                 artifacts_by_run,
                                                 step_name=suite,
                                                 build=build)

    def _group_test_results_by_test_name(self, test_results):
        test_results_by_name = collections.defaultdict(list)
        for test_result in test_results:
            test_id_match = self._test_id_pattern.fullmatch(
                test_result['testId'])
            if not test_id_match:
                continue
            test_results_by_name[test_id_match['name']].append(test_result)
        return test_results_by_name

    def _group_artifacts_by_test_run(self, artifacts):
        test_run_pattern = re.compile(
            r'invocations/[^/\s]+/tests/[^/\s]+/results/[^/\s]+')
        artifacts_by_run = collections.defaultdict(list)
        for artifact in artifacts:
            test_run_match = test_run_pattern.match(artifact['name'])
            if not test_run_match:
                continue
            artifacts_by_run[test_run_match[0]].append(artifact)
        return artifacts_by_run

    def get_full_builder_url(self, url_base: str, builder_name: str) -> str:
        """Returns the URL for the given builder's directory in Google Storage.

        Each builder has a directory in the GS bucket, and the directory
        name is the builder name transformed to be more URL-friendly by
        replacing all spaces, periods and parentheses with underscores.
        """
        return '%s/%s' % (url_base, re.sub('[ .()]', '_', builder_name))

    @memoized
    def fetch_retry_summary_json(self, build, test_suite):
        """Fetches and returns the text of the archived *test_results_summary.json file.

        This file is expected to contain the results of retrying web tests
        with and without a patch in a try job. It includes lists of tests
        that failed only with the patch ("failures"), and tests that failed
        both with and without ("ignored").
        """
        url_base = self.get_full_builder_url(RESULTS_SUMMARY_URL_BASE,
                                             build.builder_name)
        url_base = '%s/%s' % (url_base, build.build_number)
        # NOTE(crbug.com/1082907): We used to fetch retry_with_patch_summary.json from
        # test-results.appspot.com. The file has been renamed and can no longer be
        # accessed via test-results, so we download it from GCS directly.
        # There is still a bug in uploading this json file for other platforms than linux.
        # see https://crbug.com/1157202
        file_name = test_suite + '_' + 'test_results_summary.json'
        return self.web.get_binary('%s/%s' %
                                   (url_base, file_name),
                                   return_none_on_404=True)

    def fetch_wpt_report_urls(self, *build_ids):
        """Get a list of URLs pointing to a given build's wptreport artifacts.

        wptreports are a wptrunner log format used to store test results.

        The URLs look like:
            https://results.usercontent.cr.dev/invocations/ \
                task-chromium-swarm.appspot.com-58590ed6228fd611/ \
                artifacts/wpt_reports_android_webview_01.json \
                ?token=AXsiX2kiOiIxNjQx...

        Arguments:
            build_ids: Build IDs retrieved from Buildbucket.

        Returns:
            A list of URLs, sorted by (product, shard index). Note that the URLs
            contain a time-sensitive `token` query parameter required for
            access.
        """
        if not build_ids:
            return []
        artifacts = self._resultdb_client.query_artifacts(
            list(build_ids), {
                'followEdges': {
                    'includedInvocations': True,
                },
                'artifactIdRegexp': 'wpt_reports.json',
            })
        artifacts.sort(key=lambda artifact: artifact['artifactId'])
        return [artifact['fetchUrl'] for artifact in artifacts]


def filter_latest_builds(builds):
    """Filters Build objects to include only the latest for each builder.

    Args:
        builds: A collection of Build objects.

    Returns:
        A list of Build objects; only one Build object per builder name. If
        there are only Builds with no build number, then one is kept; if there
        are Builds with build numbers, then the one with the highest build
        number is kept.
    """
    latest_builds = {}
    for build in builds:
        builder = build.builder_name
        if builder not in latest_builds or (
                build.build_number
                and build.build_number > latest_builds[builder].build_number):
            latest_builds[builder] = build
    return sorted(latest_builds.values())
