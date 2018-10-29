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
import urllib
import urllib2

from blinkpy.common.memoized import memoized
from blinkpy.common.net.layout_test_results import LayoutTestResults
from blinkpy.common.net.network_transaction import NetworkTransaction
from blinkpy.web_tests.layout_package import json_results_generator

_log = logging.getLogger(__name__)

TEST_RESULTS_SERVER = 'https://test-results.appspot.com'
RESULTS_URL_BASE = '%s/data/layout_results' % TEST_RESULTS_SERVER


class Build(collections.namedtuple('Build', ('builder_name', 'build_number'))):
    """Represents a combination of builder and build number.

    If build number is None, this represents the latest build
    for a given builder.
    """
    def __new__(cls, builder_name, build_number=None):
        return super(Build, cls).__new__(cls, builder_name, build_number)


class BuildBot(object):
    """This class represents an interface to BuildBot-related functionality.

    This includes fetching layout test results from Google Storage;
    for more information about the layout test result format, see:
        https://www.chromium.org/developers/the-json-test-results-format
    """

    def results_url(self, builder_name, build_number=None, step_name=None):
        """Returns a URL for one set of archived layout test results.

        If a build number is given, this will be results for a particular run;
        otherwise it will be the accumulated results URL, which should have
        the latest results.
        """
        if build_number:
            assert str(build_number).isdigit(), 'expected numeric build number, got %s' % build_number
            url_base = self.builder_results_url_base(builder_name)
            if step_name is None:
                step_name = self.get_layout_test_step_name(Build(builder_name, build_number))
            if step_name:
                return '%s/%s/%s/layout-test-results' % (
                    url_base, build_number, urllib.quote(step_name))
            return '%s/%s/layout-test-results' % (url_base, build_number)
        return self.accumulated_results_url_base(builder_name)

    def builder_results_url_base(self, builder_name):
        """Returns the URL for the given builder's directory in Google Storage.

        Each builder has a directory in the GS bucket, and the directory
        name is the builder name transformed to be more URL-friendly by
        replacing all spaces, periods and parentheses with underscores.
        """
        return '%s/%s' % (RESULTS_URL_BASE, re.sub('[ .()]', '_', builder_name))

    @memoized
    def fetch_retry_summary_json(self, build):
        """Fetches and returns the text of the archived retry_summary file.

        This file is expected to contain the results of retrying layout tests
        with and without a patch in a try job. It includes lists of tests
        that failed only with the patch ("failures"), and tests that failed
        both with and without ("ignored").
        """
        url_base = '%s/%s' % (self.builder_results_url_base(build.builder_name), build.build_number)
        return NetworkTransaction(return_none_on_404=True).run(
            lambda: self.fetch_file('%s/%s' % (url_base, 'retry_summary.json')))

    def accumulated_results_url_base(self, builder_name):
        return self.builder_results_url_base(builder_name) + '/results/layout-test-results'

    @memoized
    def fetch_results(self, build, full=False):
        """Returns a LayoutTestResults object for results from a given Build.
        Uses full_results.json if full is True, otherwise failing_results.json.
        """
        return self.fetch_layout_test_results(
            self.results_url(build.builder_name, build.build_number,
                             step_name=self.get_layout_test_step_name(build)),
            full)

    @memoized
    def get_layout_test_step_name(self, build):
        url = '%s/testfile?%s' % (TEST_RESULTS_SERVER, urllib.urlencode({
            'builder': build.builder_name,
            'buildnumber': build.build_number,
            'name': 'full_results.json',
            # This forces the server to gives us JSON rather than an HTML page.
            'callback': json_results_generator.JSON_CALLBACK,
        }))
        data = NetworkTransaction(return_none_on_404=True).run(
            lambda: self.fetch_file(url))
        if not data:
            _log.debug('Got 404 response from:\n%s', url)
            return None

        # Strip out the callback
        data = json.loads(json_results_generator.strip_json_wrapper(data))
        suites = [
            entry['TestType'] for entry in data
            # Some suite names are like 'webkit_layout_tests on Intel GPU (with
            # patch)'. Only make sure it starts with webkit_layout_tests and
            # runs with a patch. This should be changed eventually to use actual
            # structured data from the test results server.
            if (entry['TestType'].startswith('webkit_layout_tests') and
                entry['TestType'].endswith('(with patch)'))
        ]
        # In manual testing, I sometimes saw results where the same suite was
        # repeated twice. De-duplicate here to try to catch this.
        suites = list(set(suites))
        if len(suites) != 1:
            raise Exception(
                'build %s on builder %s expected to only have one layout test '
                'step, instead has %s' % (
                    build.build_number, build.builder_name, suites))

        return suites[0]

    @memoized
    def fetch_layout_test_results(self, results_url, full=False):
        """Returns a LayoutTestResults object for results fetched from a given URL.
        Uses full_results.json if full is True, otherwise failing_results.json.
        """
        base_filename = 'full_results.json' if full else 'failing_results.json'
        results_file = NetworkTransaction(return_none_on_404=True).run(
            lambda: self.fetch_file('%s/%s' % (results_url, base_filename)))
        if results_file is None:
            _log.debug('Got 404 response from:\n%s/%s', results_url, base_filename)
            return None
        revision = NetworkTransaction(return_none_on_404=True).run(
            lambda: self.fetch_file('%s/LAST_CHANGE' % results_url))
        if revision is None:
            _log.debug('Got 404 response from:\n%s/LAST_CHANGE', results_url)
        return LayoutTestResults.results_from_string(results_file, revision)

    def fetch_file(self, url):
        # It seems this can return None if the url redirects and then returns 404.
        # FIXME: This could use Web instead of using urllib2 directly.
        result = urllib2.urlopen(url)
        if not result:
            return None
        # urlopen returns a file-like object which sometimes works fine with str()
        # but sometimes is a addinfourl object.  In either case calling read() is correct.
        return result.read()


def current_build_link(host):
    """Returns a link to the current job if running on buildbot, or None."""
    master_name = host.environ.get('BUILDBOT_MASTERNAME')
    builder_name = host.environ.get('BUILDBOT_BUILDERNAME')
    build_number = host.environ.get('BUILDBOT_BUILDNUMBER')
    if not (master_name and builder_name and build_number):
        return None
    return ('https://ci.chromium.org/buildbot/%s/%s/%s' %
            (master_name, builder_name, build_number))


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
        if builder not in latest_builds or build.build_number > latest_builds[builder].build_number:
            latest_builds[builder] = build
    return sorted(latest_builds.values())
