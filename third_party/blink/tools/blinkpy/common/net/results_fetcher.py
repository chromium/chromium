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
import json
import re
import six.moves.urllib.request
import six.moves.urllib.parse
import six.moves.urllib.error
import time

from blinkpy.common.memoized import memoized
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.net.web import Web
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.layout_package import json_results_generator

_log = logging.getLogger(__name__)

TEST_RESULTS_SERVER = 'https://test-results.appspot.com'
RESULTS_URL_BASE = '%s/data/layout_results' % TEST_RESULTS_SERVER
RESULTS_SUMMARY_URL_BASE = 'https://storage.googleapis.com/chromium-layout-test-archives'

PREDICATE_UNEXPECTED_RESULTS = {
    "expectancy": "VARIANTS_WITH_ONLY_UNEXPECTED_RESULTS",
    "excludeExonerated": True
}


class Build(collections.namedtuple('Build', ('builder_name', 'build_number',
                                             'build_id'))):
    """Represents a combination of builder and build number.

    If build number is None, this represents the latest build
    for a given builder.
    """

    def __new__(cls, builder_name, build_number=None, build_id=None):
        return super(Build, cls).__new__(cls, builder_name,
                                         build_number, build_id)


class TestResultsFetcher(object):
    """This class represents an interface to test results for particular builds.

    This includes fetching web test results from Google Storage;
    for more information about the web test result format, see:
        https://www.chromium.org/developers/the-json-test-results-format
    """

    def __init__(self):
        self.web = Web()
        self.builders = BuilderList.load_default_builder_list(FileSystem())
        self._webtest_results_resultdb = None

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
            if step_name is None:
                step_name = self.get_layout_test_step_name(
                    Build(builder_name, build_number))
            if step_name:
                return '%s/%s/%s/layout-test-results' % (
                    url_base, build_number,
                    six.moves.urllib.parse.quote(step_name))
            return '%s/%s/layout-test-results' % (url_base, build_number)
        return self.accumulated_results_url_base(builder_name)

    def get_artifact_list_for_test(self, host, result_name):
        """Fetches the list of artifacts for a test-result.
        """
        luci_token = LuciAuth(host).get_access_token()

        url = 'https://results.api.cr.dev/prpc/luci.resultdb.v1.ResultDB/ListArtifacts'
        header = {
            'Authorization': 'Bearer ' + luci_token,
            'Accept': 'application/json',
            'Content-Type': 'application/json',
        }

        data = {
            "parent": result_name,
        }

        req_body = json.dumps(data).encode("utf-8")
        response = self.do_request_with_retries('POST', url, req_body, header)
        if response is None:
            _log.warning("Failed to get baseline artifacts")
        if response.getcode() == 200:
            response_body = response.read()

        RESPONSE_PREFIX = b")]}'"
        if response_body.startswith(RESPONSE_PREFIX):
            response_body = response_body[len(RESPONSE_PREFIX):]
        res = json.loads(response_body)
        return res['artifacts']

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
    def fetch_retry_summary_json(self, build):
        """Fetches and returns the text of the archived test_results_summary.json file.

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
        return self.web.get_binary('%s/%s' %
                                   (url_base, 'test_results_summary.json'),
                                   return_none_on_404=True)

    def accumulated_results_url_base(self, builder_name):
        return self.builder_results_url_base(
            builder_name) + '/results/layout-test-results'

    def get_invocation(self, build):
        """Returns the invocation for a build
        """
        return "invocations/build-%s" % build.build_id

    def do_request_with_retries(self, method, url, data, headers):
        for i in range(5):
            try:
                response = self.web.request(method, url, data=data, headers=headers)
                return response
            except six.moves.urllib.error.URLError:
                _log.warning("Meet URLError...")
                if i < 4:
                    time.sleep(10)
        _log.error("Http request failed for %s" % data)
        return None

    @memoized
    def fetch_results_from_resultdb_layout_tests(self, host, build,
                                                 unexpected_results):
        rv = []
        if unexpected_results:
            predicate = PREDICATE_UNEXPECTED_RESULTS
        else:
            predicate = ""
        rv = self.fetch_results_from_resultdb(host, [build], predicate)
        self._webtest_results_resultdb = WebTestResults.results_from_resultdb(
            rv)
        return self._webtest_results_resultdb

    def fetch_results_from_resultdb(self, host, builds, predicate):
        """Returns a list of test results from ResultDB
        """
        luci_token = LuciAuth(host).get_access_token()

        url = 'https://results.api.cr.dev/prpc/luci.resultdb.v1.ResultDB/QueryTestResults'
        header = {
            'Authorization': 'Bearer ' + luci_token,
            'Accept': 'application/json',
            'Content-Type': 'application/json',
        }
        rv = []
        page_token = None
        request_more = True
        invocations = [self.get_invocation(build) for build in builds]
        data = {
            "invocations": invocations,
        }
        if predicate:
            data.update({"predicate": predicate})
        while request_more:
            request_more = False
            if page_token:
                data.update({"pageToken": page_token})
            req_body = json.dumps(data).encode("utf-8")
            _log.debug("Sending QueryTestResults request. Url: %s with Body: %s" %
                       (url, req_body))

            response = self.do_request_with_retries('POST', url, req_body, header)
            if response is None:
                continue

            if response.getcode() == 200:
                response_body = response.read()

                # This string always appear at the beginning of the RPC response
                # from ResultDB.
                RESPONSE_PREFIX = b")]}'"
                if response_body.startswith(RESPONSE_PREFIX):
                    response_body = response_body[len(RESPONSE_PREFIX):]
                res = json.loads(response_body)
                if res:
                    rv.extend(res['testResults'])
                    page_token = res.get('nextPageToken')
                    if page_token:
                        request_more = True
            else:
                _log.error(
                    "Failed to get test results from ResultDB (status=%s)" %
                    response.status)
                _log.debug("Full QueryTestResults response: %s" % str(response))
        return rv

    @memoized
    def fetch_results(self, build, full=False, step_name=None):
        """Returns a WebTestResults object for results from a given Build.
        Uses full_results.json if full is True, otherwise failing_results.json.
        """
        if not build.builder_name or not build.build_number:
            _log.debug('Builder name or build number is None')
            return None
        step_name = step_name or self.get_layout_test_step_name(build)
        return self.fetch_web_test_results(
            self.results_url(
                build.builder_name,
                build.build_number,
                step_name=step_name), full, step_name)

    @memoized
    def get_layout_test_step_name(self, build):
        if not build.builder_name or not build.build_number:
            _log.debug('Builder name or build number is None')
            return None

        # We were not able to retrieve step name for some builders from
        # https://test-results.appspot.com. Read from config file instead
        step_name = self.builders.step_name_for_builder(build.builder_name)
        if step_name:
            return step_name

        url = '%s/testfile?%s' % (
            TEST_RESULTS_SERVER,
            six.moves.urllib.parse.urlencode([
                ('buildnumber', build.build_number),
                # This forces the server to gives us JSON rather than an HTML page.
                ('callback', json_results_generator.JSON_CALLBACK),
                ('builder', build.builder_name),
                ('name', 'full_results.json')
            ]))
        data = self.web.get_binary(url, return_none_on_404=True)
        if not data:
            _log.debug('Got 404 response from:\n%s', url)
            return None

        # Strip out the callback
        data = json.loads(json_results_generator.strip_json_wrapper(data))
        suites = [
            entry['TestType'] for entry in data
            # Some suite names are like 'blink_web_tests on Intel GPU (with
            # patch)'. Only make sure it starts with blink_web_tests and
            # runs with a patch. This should be changed eventually to use actual
            # structured data from the test results server.
            if re.match(
                r'(blink_web_tests|wpt_tests_suite|high_dpi_blink_web_tests).*\(with patch\)$',
                entry['TestType'])
        ]
        # In manual testing, I sometimes saw results where the same suite was
        # repeated twice. De-duplicate here to try to catch this.
        suites = list(set(suites))
        if len(suites) != 1:
            raise Exception(
                'build %s on builder %s expected to only have one web test '
                'step, instead has %s' % (build.build_number,
                                          build.builder_name, suites))
        return suites[0]

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
                                  six.moves.urllib.parse.urlencode(
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
