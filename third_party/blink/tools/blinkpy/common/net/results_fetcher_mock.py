# Copyright (C) 2011 Google Inc. All rights reserved.
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

from collections import namedtuple

from blinkpy.common.net.rpc import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.net.results_fetcher import TestResultsFetcher

BuilderStep = namedtuple('BuilderStep', ['build', 'step_name'])

# TODO(qyearsley): To be consistent with other fake ("mock") classes, this
# could be changed so it's not a subclass of TestResultsFetcher.
class MockTestResultsFetcher(TestResultsFetcher):
    def __init__(self, web, luci_auth, builders=None):
        super(MockTestResultsFetcher, self).__init__(web, luci_auth, builders)
        self._canned_results = {}
        self._canned_artifacts_resultdb = {}
        self._canned_retry_summary_json = {}
        self._webdriver_results = {}
        self.fetched_builds = []
        self.fetched_webdriver_builds = []

    def set_results(self, build, results, step_name=None):
        step_name = step_name or results.step_name()
        step = BuilderStep(build=build, step_name=step_name)
        self._canned_results[step] = results

    def make_results_from_raw_rdb(self, test_results, artifacts,
                                  **kwargs) -> WebTestResults:
        return WebTestResults.from_rdb_responses(
            self._group_test_results_by_test_name(test_results),
            self._group_artifacts_by_test_name(artifacts),
            **kwargs,
        )

    def gather_results(self, build: Build, step_name: str) -> WebTestResults:
        return self.fetch_results(build, step_name=step_name)

    def fetch_results(self, build, full=False, step_name=None):
        step = BuilderStep(build=build, step_name=step_name)
        self.fetched_builds.append(step)
        return self._canned_results.get(step)

    def set_results_to_resultdb(self, build, results):
        # The step name is not relevant for ResultDB, so just set it to None.
        step = BuilderStep(build, step_name=None)
        self._canned_results[step] = results

    def fetch_results_from_resultdb(self, builds, predicate):
        rv = []
        for build in builds:
            step = BuilderStep(build, step_name=None)
            results = self._canned_results.get(step)
            if results:
                rv.extend(results)
        return rv

    def set_webdriver_test_results(self, build, m, results):
        self._webdriver_results[(build, m)] = results

    def fetch_webdriver_test_results(self, build, m):
        self.fetched_webdriver_builds.append((build, m))
        return self._webdriver_results.get((build, m))

    def set_retry_sumary_json(self, build, content):
        self._canned_retry_summary_json[build] = content

    def fetch_retry_summary_json(self, build, test_suite):
        return self._canned_retry_summary_json.get(build)

    def get_layout_test_step_names(self, build):
        return [
            step.step_name for step in self._canned_results
            if build == step.build
        ]
