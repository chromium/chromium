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
import urllib.parse

# pylint: disable=unused-import; `Build` is imported by other modules
from blinkpy.common.memoized import memoized
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.net.rpc import Build, ResultDBClient
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.web_tests.builder_list import BuilderList

_log = logging.getLogger(__name__)

TEST_RESULTS_SERVER = 'https://test-results.appspot.com'
RESULTS_URL_BASE = '%s/data/layout_results' % TEST_RESULTS_SERVER
RESULTS_SUMMARY_URL_BASE = 'https://storage.googleapis.com/chromium-layout-test-archives'


class TestResultsFetcher:
    """This class represents an interface to test results for particular builds.

    This includes fetching web test results from Google Storage;
    for more information about the web test result format, see:
        https://www.chromium.org/developers/the-json-test-results-format
    """

    _test_id_pattern = re.compile(
        r'ninja://\S*blink_(web|wpt)_tests/(?P<name>\S+)')

    def __init__(self, web, luci_auth, builders=None):
        self.web = web
        self._resultdb_client = ResultDBClient(web, luci_auth)
        self.builders = builders or BuilderList.load_default_builder_list(
            FileSystem())

    @classmethod
    def from_host(cls, host):
        return cls(host.web, LuciAuth(host), host.builders)

    def results_url(self, builder_name, build_number=None, step_name=None):
        """Returns a URL for one set of archived web test results.

        If a build number is given, this will be results for a particular run;
        otherwise it will be the accumulated results URL, which should have
        the latest results.
        """
        if build_number:
            assert str(build_number).isdigit(), \
                'expected numeric build number, got %s' % build_number
            url_base = self.builder_results_url_base(builder_name)
            if step_name:
                return '%s/%s/%s/layout-test-results' % (
                    url_base, build_number, urllib.parse.quote(step_name))
            return '%s/%s/layout-test-results' % (url_base, build_number)
        return self.accumulated_results_url_base(builder_name)

    @memoized
    def gather_results(self,
                       build: Build,
                       step_name: str,
                       exclude_exonerated: bool = True,
                       only_unexpected: bool = True) -> WebTestResults:
        """Gather all web test results on a given build step from ResultDB."""
        assert build.build_id, '%s must set a build ID' % build
        suite = step_name
        if suite.endswith('(with patch)'):
            suite = suite[:-len('(with patch)')].strip()

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
        return WebTestResults.from_rdb_responses(
            test_results_by_name,
            artifacts_by_run,
            step_name=step_name,
            builder_name=build.builder_name)

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

    def get_full_builder_url(self, url_base, builder_name):
        """ Returns the url for a builder directory in google storage.

        Each builder has a directory in the GS bucket, and the directory
        name is the builder name transformed to be more URL-friendly by
        replacing all spaces, periods and parentheses with underscores.
        """
        return '%s/%s' % (url_base, re.sub('[ .()]', '_', builder_name))

    def builder_results_url_base(self, builder_name):
        """Returns the URL for the given builder's directory in Google Storage.
        """
        return self.get_full_builder_url(RESULTS_URL_BASE, builder_name)

    def builder_retry_results_url_base(self, builder_name):
        """Returns the URL for the given builder's directory in Google Storage.

        This is used for fetching the retry data which is now contained in
        test_results_summary.json which cannot be fetched from
        https://test-results.appspot.com anymore. Migrating this tool to use
        resultDB is the ideal solution.
        """
        return self.get_full_builder_url(RESULTS_SUMMARY_URL_BASE,
                                         builder_name)

    @memoized
    def fetch_retry_summary_json(self, build, test_suite):
        """Fetches and returns the text of the archived *test_results_summary.json file.

        This file is expected to contain the results of retrying web tests
        with and without a patch in a try job. It includes lists of tests
        that failed only with the patch ("failures"), and tests that failed
        both with and without ("ignored").
        """
        url_base = '%s/%s' % (self.builder_retry_results_url_base(
            build.builder_name), build.build_number)
        # NOTE(crbug.com/1082907): We used to fetch retry_with_patch_summary.json from
        # test-results.appspot.com. The file has been renamed and can no longer be
        # accessed via test-results, so we download it from GCS directly.
        # There is still a bug in uploading this json file for other platforms than linux.
        # see https://crbug.com/1157202
        file_name = test_suite + '_' + 'test_results_summary.json'
        return self.web.get_binary('%s/%s' %
                                   (url_base, file_name),
                                   return_none_on_404=True)

    def accumulated_results_url_base(self, builder_name):
        return self.builder_results_url_base(
            builder_name) + '/results/layout-test-results'

    @memoized
    def fetch_results(self, build, full=False, step_name=None):
        """Returns a WebTestResults object for results from a given Build.
        Uses full_results.json if full is True, otherwise failing_results.json.
        """
        if not build.builder_name or not build.build_number:
            _log.debug('Builder name or build number is None')
            return None
        return self.fetch_web_test_results(
            self.results_url(
                build.builder_name,
                build.build_number,
                step_name=step_name), full, step_name)

    @memoized
    def get_layout_test_step_names(self, build):
        if build.builder_name is None:
            _log.debug('Builder name is None')
            return []

        return self.builders.step_names_for_builder(build.builder_name)

    @memoized
    def fetch_web_test_results(self, results_url, full=False, step_name=None):
        """Returns a WebTestResults object for results fetched from a given URL.
        Uses full_results.json if full is True, otherwise failing_results.json.
        """
        base_filename = 'full_results.json' if full else 'failing_results.json'
        results_file = self.web.get_binary(
            '%s/%s' % (results_url, base_filename), return_none_on_404=True)
        if results_file is None:
            _log.debug('Got 404 response from:\n%s/%s', results_url,
                       base_filename)
            return None
        return WebTestResults.results_from_string(results_file, step_name)

    def fetch_webdriver_test_results(self, build, master):
        if not build.builder_name or not build.build_number or not master:
            _log.debug('Builder name or build number or master is None')
            return None

        url = '%s/testfile?%s' % (TEST_RESULTS_SERVER,
                                  urllib.parse.urlencode(
                                      [('buildnumber', build.build_number),
                                       ('master', master),
                                       ('builder', build.builder_name),
                                       ('testtype',
                                        'webdriver_tests_suite (with patch)'),
                                       ('name', 'full_results.json')]))

        data = self.web.get_binary(url, return_none_on_404=True)
        if not data:
            _log.debug('Got 404 response from:\n%s', url)
            return None
        return WebTestResults.results_from_string(data)

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
            })
        filename_pattern = re.compile(r'wpt_reports_(.*)\.json')
        url_to_index = {}
        for artifact in artifacts:
            filename_match = filename_pattern.fullmatch(artifact['artifactId'])
            if filename_match:
                url_to_index[artifact['fetchUrl']] = filename_match[0]
        return sorted(url_to_index, key=url_to_index.get)


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
