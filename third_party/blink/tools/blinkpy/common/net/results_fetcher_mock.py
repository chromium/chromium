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

from blinkpy.common.net.results_fetcher import TestResultsFetcher


# TODO(qyearsley): To be consistent with other fake ("mock") classes, this
# could be changed so it's not a subclass of TestResultsFetcher.
class MockTestResultsFetcher(TestResultsFetcher):

    def __init__(self):
        super(MockTestResultsFetcher, self).__init__()
        self._canned_results = {}
        self._canned_retry_summary_json = {}
        self._webdriver_results = {}
        self.fetched_builds = []
        self.fetched_webdriver_builds = []
        self._layout_test_step_name = 'webkit_layout_tests (with patch)'

    def set_results(self, build, results):
        self._canned_results[build] = results

    def fetch_results(self, build, full=False):
        self.fetched_builds.append(build)
        return self._canned_results.get(build)

    def set_webdriver_test_results(self, build, master, results):
        self._webdriver_results[(build, master)] = results

    def fetch_webdriver_test_results(self, build, master):
        self.fetched_webdriver_builds.append((build, master))
        return self._webdriver_results.get((build, master))

    def set_retry_sumary_json(self, build, content):
        self._canned_retry_summary_json[build] = content

    def fetch_retry_summary_json(self, build):
        return self._canned_retry_summary_json.get(build)

    def set_layout_test_step_name(self, name):
        self._layout_test_step_name = name

    def get_layout_test_step_name(self, build):
        return self._layout_test_step_name
