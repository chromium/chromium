# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Load TestExpectations and testharness baselines into wptrunner.

This module provides a drop-in `wptrunner.testloader.TestLoader(...)`
replacement that translates Blink expectation formats into a WPT metadata
syntax tree that wptrunner understands.

Building the syntax tree is preferred to parsing one to avoid coupling the
translation to the metadata serialization format and dealing with character
escaping.
"""

import collections
import contextlib
import functools
import logging
from typing import Collection, List, Mapping, Optional, Tuple
from urllib.parse import urlsplit

from blinkpy.common import path_finder
from blinkpy.common.memoized import memoized
from blinkpy.w3c.wpt_results_processor import (
    RunInfo,
    TestType,
    WPTResult,
    chromium_to_wptrunner_statuses,
    normalize_statuses,
)
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.testharness_results import (
    LineType,
    Status,
    TestharnessLine,
    parse_testharness_baseline,
)
from blinkpy.web_tests.models.typ_types import (ExpectationType, ResultType)
from blinkpy.web_tests.port.base import Port

path_finder.bootstrap_wpt_imports()
from manifest.item import ManifestItem
from tools.manifest.manifest import Manifest
from wptrunner import manifestexpected, testloader, testrunner, wpttest
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends import static

_log = logging.getLogger(__name__)

InheritedMetadata = List[manifestexpected.DirectoryManifest]


class TestLoader(testloader.TestLoader):
    def __init__(self,
                 port: Port,
                 *args,
                 expectations: Optional[TestExpectations] = None,
                 include: Optional[Collection[str]] = None,
                 **kwargs):
        self._port = port
        self._expectations = expectations or TestExpectations(port)
        self._include = include
        # Invoking the superclass constructor will immediately load tests, so
        # set `_port` and `_expectations` first.
        super().__init__(*args, **kwargs)

    def _load_tests(self):
        # Override the default implementation to provide a faster one that only
        # handles test URLs (i.e., not files, directories, or globs).
        # `WebTestFinder` already resolves files/directories to URLs.
        self.tests, self.disabled_tests = {}, {}
        items_by_url = self._load_items_by_url()
        manifests_by_url_base = {
            manifest.url_base: manifest
            for manifest in self.manifests
        }

        for subsuite_name, subsuite in self.subsuites.items():
            self.tests[subsuite_name] = collections.defaultdict(list)
            self.disabled_tests[subsuite_name] = collections.defaultdict(list)

            test_urls = subsuite.include or self._include or []
            for test_url in test_urls:
                if not test_url.startswith('/'):
                    test_url = f'/{test_url}'
                item = items_by_url.get(test_url)
                # Skip items excluded by `run_wpt_tests.py --no-wpt-internal`.
                if not item:
                    continue
                manifest = manifests_by_url_base[item.url_base]
                test_root = self.manifests[manifest]
                inherit_metadata, test_metadata = self.load_metadata(
                    subsuite.run_info, manifest, test_root['metadata_path'],
                    item.path)
                test = self.get_test(manifest, item, inherit_metadata,
                                     test_metadata)
                # `WebTestFinder` should have already filtered out skipped
                # tests, but add to `disabled_tests` anyway just in case.
                tests = self.disabled_tests if test.disabled() else self.tests
                tests[subsuite_name][item.item_type].append(test)

    def _load_items_by_url(self) -> Mapping[str, ManifestItem]:
        items_by_url = {}
        for manifest, test_root in self.manifests.items():
            items = manifest.itertypes(*self.test_types)
            for test_type, test_path, tests in items:
                for test in tests:
                    items_by_url[test.id] = test
        return items_by_url

    def load_dir_metadata(
        self,
        run_info: RunInfo,
        test_manifest: Manifest,
        metadata_path: str,
        test_path: str,
    ) -> InheritedMetadata:
        # Ignore `__dir__.ini`.
        return []

    def load_metadata(
        self,
        run_info: RunInfo,
        test_manifest: Manifest,
        metadata_path: str,
        test_path: str,
    ) -> Tuple[InheritedMetadata, manifestexpected.ExpectedManifest]:
        test_file_ast = wptnode.DataNode()
        test_type: Optional[TestType] = None

        for item in test_manifest.iterpath(test_path):
            test_name = wpt_url_to_blink_test(item.id)
            virtual_suite = run_info.get('virtual_suite')
            if virtual_suite:
                test_name = f'virtual/{virtual_suite}/{test_name}'
            assert not test_type or test_type == item.item_type, item
            test_type = item.item_type
            exp_line = self._expectations.get_expectations(test_name)
            expected_text = self._port.expected_text(test_name)
            if expected_text:
                testharness_lines = parse_testharness_baseline(
                    expected_text.decode('utf-8', 'replace'))
            else:
                testharness_lines = []

            if exp_line.results == {ResultType.Pass} and not testharness_lines:
                continue
            test_file_ast.append(
                self._build_test_ast(item.item_type, exp_line,
                                     testharness_lines))

        if not test_file_ast.children:
            # AST is empty. Fast path for all-pass expectations.
            return [], None
        test_metadata = static.compile_ast(test_file_ast,
                                           run_info,
                                           data_cls_getter,
                                           test_path=test_path)
        test_metadata.set('type', test_type)
        return [], test_metadata

    def _build_test_ast(
        self,
        test_type: TestType,
        exp_line: ExpectationType,
        testharness_lines: List[TestharnessLine],
    ) -> wptnode.DataNode:
        test_statuses = chromium_to_wptrunner_statuses(
            exp_line.results - {ResultType.Skip}, test_type)
        harness_errors = {
            line
            for line in testharness_lines
            if line.line_type is LineType.HARNESS_ERROR
        }
        if not can_have_subtests(test_type):
            # Temporarily expect PASS so that unexpected passes don't contribute
            # to retries or build failures.
            test_statuses.add(Status.PASS.name)
        elif ResultType.Failure in exp_line.results or not harness_errors:
            # Add `OK` for `[ Failure ]` lines or no explicit harness error in
            # the baseline.
            test_statuses.update(
                chromium_to_wptrunner_statuses(frozenset([ResultType.Pass]),
                                               test_type))
        elif len(harness_errors) > 1:
            raise ValueError(
                f'testharness baseline for {exp_line.test!r} can only have up '
                f'to one harness error; found {harness_errors!r}.')
        else:
            error = harness_errors.pop()
            test_statuses = test_statuses & {'CRASH', 'TIMEOUT'}
            test_statuses.update(status.name for status in error.statuses)

        assert test_statuses, exp_line.to_string()
        test_ast = _build_expectation_ast(_test_basename(exp_line.test),
                                          normalize_statuses(test_statuses))
        # If `[ Failure ]` is expected, the baseline is allowed to be anything.
        # To mimic this, skip creating any explicit subtests, and rely on
        # implicit subtest creation.
        if ResultType.Failure in exp_line.results:
            expect_any = wptnode.KeyValueNode('expect_any_subtests')
            expect_any.append(wptnode.AtomNode(True))
            test_ast.append(expect_any)
            return test_ast

        for line in filter(lambda line: line.subtest, testharness_lines):
            test_ast.append(
                _build_expectation_ast(
                    line.subtest,
                    normalize_statuses(status.name
                                       for status in line.statuses),
                    line.message))
        return test_ast

    @classmethod
    def install(cls, port: Port, expectations: TestExpectations,
                include: List[str]):
        """Patch overrides into the wptrunner API (may be unstable)."""
        testloader.TestLoader = functools.partial(cls,
                                                  port,
                                                  expectations=expectations,
                                                  include=include)

        # Ideally, we would patch `executorchrome.*.convert_result`, but changes
        # to the executor classes here in the main process don't persist to
        # worker processes, which reload the module from source. Therefore,
        # patch `TestRunnerManager`, which runs in the main process.
        test_ended = testrunner.TestRunnerManager.test_ended

        @functools.wraps(test_ended)
        def wrapper(self, test, results):
            return test_ended(self, test,
                              allow_any_subtests_on_timeout(test, results))

        testrunner.TestRunnerManager.test_ended = wrapper


Results = Tuple[wpttest.Result, List[wpttest.SubtestResult]]


def allow_any_subtests_on_timeout(test: wpttest.Test,
                                  results: Results) -> Results:
    """On timeout, suppress all subtest mismatches with added expectations.

    When a test times out in `run_web_tests.py`, text mismatches don't affect
    pass/fail status or trigger retries. This prevents the test from being
    susceptible to flakiness due to variation in which subtest times out and
    lets build gardeners suppress failures with just a `[ Timeout ]` line in
    TestExpectations.

    This converter injects extra expectations to mimic the `run_web_tests.py`
    behavior. Note that, if the test expected to time out runs to completion,
    the subtest results are then checked as usual.

    See Also:
        https://github.com/web-platform-tests/wpt/pull/44134
    """
    harness_result, subtest_results = results
    if harness_result.status in {'CRASH', 'TIMEOUT'}:
        for result in subtest_results:
            result.expected, result.known_intermittent = result.status, []
            result.message = test.expected_fail_message(result.name)
    return harness_result, subtest_results


def _build_expectation_ast(name: str,
                           statuses: Collection[str],
                           message: Optional[str] = None) -> wptnode.DataNode:
    """Build an in-memory syntax tree representing part of a metadata file:

        [(sub)test]
          expected: [...]
          expected-fail-message: "..."

    Arguments:
        name: Everything including and after the basename of a test URL's path,
            or a subtest name.
        statuses: wptrunner statuses.
        message: An optional assertion message that should match.
    """
    test_ast = wptnode.DataNode(name)
    expected_key = wptnode.KeyValueNode('expected')
    expected_value = wptnode.ListNode()
    for status in statuses:
        expected_value.append(wptnode.ValueNode(status))
    expected_key.append(expected_value)
    test_ast.append(expected_key)
    if message:
        message_key = wptnode.KeyValueNode('expected-fail-message')
        message_key.append(wptnode.ValueNode(message))
        test_ast.append(message_key)
    return test_ast


def data_cls_getter(output_node, visited_node):
    if output_node is None:
        return manifestexpected.ExpectedManifest
    elif isinstance(output_node, manifestexpected.ExpectedManifest):
        return TestNode
    elif isinstance(output_node, TestNode):
        return manifestexpected.SubtestNode
    raise ValueError


class TestNode(manifestexpected.TestNode):
    def get_subtest(self, name: str):
        chromium_statuses = frozenset([ResultType.Pass])
        with contextlib.suppress(KeyError):
            # Create an implicit subtest that accepts any status. This supports
            # testharness tests marked with `[ Failure ]` without a checked-in
            # baseline, which would pass when run with `run_web_tests.py`.
            #
            # We still create PASS-only subtests in case the test can time out
            # overall (see below for adding TIMEOUT, NOTRUN).
            if self.get('expect_any_subtests'):
                chromium_statuses = frozenset(WPTResult.STATUSES)
        if name not in self.subtests and can_have_subtests(self.test_type):
            statuses = chromium_to_wptrunner_statuses(chromium_statuses,
                                                      self.test_type, True)
            subtest_ast = _build_expectation_ast(name,
                                                 normalize_statuses(statuses))
            self.node.append(subtest_ast)
            self.append(
                static.compile_ast(
                    subtest_ast,
                    expr_data={},
                    data_cls_getter=lambda x, y: manifestexpected.SubtestNode,
                    test_path=self.parent.test_path))
        return super().get_subtest(name)


def _test_basename(test_id: str) -> str:
    # The test "basename" is test path + query string + fragment
    path_parts = urlsplit(test_id).path.rsplit('/', maxsplit=1)
    if len(path_parts) == 1:
        return test_id
    return test_id[len(path_parts[0]) + 1:]


@memoized
def can_have_subtests(test_type: TestType) -> bool:
    return wpttest.manifest_test_cls[test_type].subtest_result_cls is not None


def wpt_url_to_blink_test(test: str) -> str:
    for wpt_dir, url_prefix in Port.WPT_DIRS.items():
        if test.startswith(url_prefix):
            return test.replace(url_prefix, wpt_dir + '/', 1)
    raise ValueError('no matching WPT roots found')
