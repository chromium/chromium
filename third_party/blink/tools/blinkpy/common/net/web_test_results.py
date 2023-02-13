# Copyright (c) 2010, Google Inc. All rights reserved.
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
import json
from typing import List, Optional

from blinkpy.common.memoized import memoized
from blinkpy.web_tests.layout_package import json_results_generator
from blinkpy.web_tests.models.typ_types import ResultType


class WebTestResult:
    def __init__(self, test_name, result_dict):
        self._test_name = test_name
        self._result_dict = result_dict

    def __repr__(self):
        return "WebTestResult(test_name=%s, result_dict=%s)" % \
            (repr(self._test_name), repr(self._result_dict))

    def suffixes_for_test_result(self):
        suffixes = set()
        artifact_names = self._result_dict.get('artifacts', {}).keys()
        # Add extensions for mismatches.
        if 'actual_text' in artifact_names:
            suffixes.add('txt')
        if 'actual_image' in artifact_names:
            suffixes.add('png')
        if 'actual_audio' in artifact_names:
            suffixes.add('wav')
        # Add extensions for missing baselines.
        if self.is_missing_text():
            suffixes.add('txt')
        if self.is_missing_image():
            suffixes.add('png')
        if self.is_missing_audio():
            suffixes.add('wav')
        return suffixes

    def result_dict(self):
        return self._result_dict

    def test_name(self):
        return self._test_name

    def did_pass_or_run_as_expected(self):
        return self.did_pass() or self.did_run_as_expected()

    def did_pass(self):
        return 'PASS' in self.actual_results()

    def did_run_as_expected(self):
        return not self._result_dict.get('is_unexpected', False)

    def is_missing_image(self):
        return self._result_dict.get('is_missing_image', False)

    def is_missing_text(self):
        return self._result_dict.get('is_missing_text', False)

    def is_missing_audio(self):
        return self._result_dict.get('is_missing_audio', False)

    @memoized
    def actual_results(self):
        return self._result_dict['actual'].split()

    def expected_results(self):
        return self._result_dict['expected']

    def last_retry_result(self):
        return self.actual_results()[-1]

    def has_non_reftest_mismatch(self):
        """Returns true if a test without reference failed due to mismatch.

        This happens when the actual output of a non-reftest does not match the
        baseline, including an implicit all-PASS testharness baseline (i.e. a
        previously all-PASS testharness test starts to fail)."""
        actual_results = self.actual_results()
        artifact_names = self._result_dict.get('artifacts', {}).keys()
        return ('FAIL' in actual_results and any(
            artifact_name.startswith('actual')
            for artifact_name in artifact_names)
                and 'reference_file_mismatch' not in artifact_names
                and 'reference_file_match' not in artifact_names)

    def is_missing_baseline(self):
        return (self.is_missing_image() or self.is_missing_text()
                or self.is_missing_audio())

    @property
    def attempts(self) -> int:
        return len(self.actual_results())


def _flatten_test_results_trie(trie, sep: str = '/'):
    if 'actual' in trie:
        yield '', trie
        return
    for component, child in trie.items():
        for suffix, leaf in _flatten_test_results_trie(child, sep=sep):
            test_name = component + sep + suffix if suffix else component
            yield test_name, leaf


# FIXME: This should be unified with ResultsSummary or other NRWT web tests code
# in the web_tests package.
# This doesn't belong in common.net, but we don't have a better place for it yet.
class WebTestResults:
    @classmethod
    def results_from_string(cls, string, step_name=None) -> 'WebTestResults':
        """Creates a WebTestResults object from a test result JSON string.

        Args:
            string: JSON string containing web test result.
        """
        if not string:
            return None
        content_string = json_results_generator.strip_json_wrapper(string)
        return cls.from_json(json.loads(content_string), step_name=step_name)

    @classmethod
    def from_json(cls, json_dict, **kwargs) -> 'WebTestResults':
        sep = json_dict.get('sep', '/')
        results = [
            WebTestResult(test_name, fields) for test_name, fields in
            _flatten_test_results_trie(json_dict['tests'], sep=sep)
        ]
        kwargs.setdefault('interrupted', json_dict.get('interrupted', False))
        kwargs.setdefault('builder_name', json_dict.get('builder_name'))
        kwargs.setdefault('chromium_revision',
                          json_dict.get('chromium_revision'))
        return cls(results, **kwargs)

    @classmethod
    def from_rdb_responses(cls,
                           test_results_by_name,
                           artifacts_by_name=None,
                           **kwargs):
        """Creates a WebTestResults object from raw ResultDB RPC response data.
        """
        artifacts_by_name = artifacts_by_name or collections.defaultdict(list)
        results = []
        for test_name, raw_results in test_results_by_name.items():
            actual = ' '.join(
                cls._rdb_to_web_test_statuses[raw_result['status']]
                for raw_result in raw_results)
            is_unexpected = any(not raw_result.get('expected')
                                for raw_result in raw_results)
            artifacts = collections.defaultdict(list)
            for artifact in artifacts_by_name[test_name]:
                artifacts[artifact['artifactId']].append(artifact['fetchUrl'])
            trie_leaf = {
                'actual': actual,
                'is_unexpected': is_unexpected,
                'artifacts': dict(artifacts),
            }
            results.append(WebTestResult(test_name, trie_leaf))
        return cls(results, **kwargs)

    _rdb_to_web_test_statuses = {
        'PASS': ResultType.Pass,
        'FAIL': ResultType.Failure,
        'CRASH': ResultType.Crash,
        'ABORT': ResultType.Timeout,
        'SKIP': ResultType.Skip,
    }

    def __init__(self,
                 results: List[WebTestResult],
                 chromium_revision: Optional[str] = None,
                 step_name: Optional[str] = None,
                 interrupted: bool = False,
                 builder_name: Optional[str] = None):
        self._results_by_name = collections.OrderedDict([
            (result.test_name(), result)
            for result in sorted(results, key=WebTestResult.test_name)
        ])
        self._chromium_revision = chromium_revision
        self._step_name = step_name
        self.interrupted = interrupted
        self.builder_name = builder_name

    def __iter__(self):
        yield from self._results_by_name.values()

    def step_name(self):
        return self._step_name

    @memoized
    def chromium_revision(self, git=None):
        """Returns the revision of the results in commit position number format."""
        revision = self._chromium_revision
        if not revision or not revision.isdigit():
            assert git, 'git is required if the original revision is a git hash.'
            revision = git.commit_position_from_git_commit(revision)
        return int(revision)

    def result_for_test(self, test):
        return self._results_by_name.get(test)

    def didnt_run_as_expected_results(self):
        return [
            result for result in self._results_by_name.values()
            if not result.did_run_as_expected()
        ]
