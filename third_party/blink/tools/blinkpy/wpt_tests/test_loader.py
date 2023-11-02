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

import functools
import logging
from typing import Container, List, Optional, Tuple
from urllib.parse import urlsplit

from blinkpy.common import path_finder
from blinkpy.w3c import wpt_metadata
from blinkpy.w3c.wpt_results_processor import (
    WPTResult,
    chromium_to_wptrunner_statuses,
    normalize_statuses,
)
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.testharness_results import (
    TestharnessLine,
    LineType,
    parse_testharness_baseline,
)
from blinkpy.web_tests.models.typ_types import Expectation, ResultType
from blinkpy.web_tests.port.base import Port

path_finder.bootstrap_wpt_imports()
from tools.manifest.manifest import Manifest
from wptrunner import manifestexpected, testloader
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends import static

_log = logging.getLogger(__name__)

InheritedMetadata = List[manifestexpected.DirectoryManifest]


class TestLoader(testloader.TestLoader):
    def __init__(self,
                 port: Port,
                 *args,
                 expectations: Optional[TestExpectations] = None,
                 **kwargs):
        self._port = port
        self._expectations = expectations or TestExpectations(port)
        # Invoking the superclass constructor will immediately load tests, so
        # set `_port` and `_expectations` first.
        super().__init__(*args, **kwargs)

    def load_dir_metadata(
        self,
        run_info: wpt_metadata.RunInfo,
        test_manifest: Manifest,
        metadata_path: str,
        test_path: str,
    ) -> InheritedMetadata:
        # Ignore `__dir__.ini`.
        return []

    def load_metadata(
        self,
        run_info: wpt_metadata.RunInfo,
        test_manifest: Manifest,
        metadata_path: str,
        test_path: str,
    ) -> Tuple[InheritedMetadata, manifestexpected.ExpectedManifest]:
        test_file_ast = wptnode.DataNode()
        test_type: Optional[wpt_metadata.TestType] = None

        for item in test_manifest.iterpath(test_path):
            test_name = wpt_metadata.wpt_url_to_exp_test(item.id)
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

            # A test file may be split between skipped and non-skipped variants.
            # Since `WPTAdapter` should never pass skipped tests to wptrunner,
            # there's no need to create their expectations here.
            if exp_line.results == {ResultType.Skip}:
                continue
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
        test_type: wpt_metadata.TestType,
        exp_line: Expectation,
        testharness_lines: List[TestharnessLine],
    ) -> wptnode.DataNode:
        test_statuses = chromium_to_wptrunner_statuses(exp_line.results,
                                                       test_type)
        harness_errors = {
            line
            for line in testharness_lines
            if line.line_type is LineType.HARNESS_ERROR
        }
        if len(harness_errors) > 1:
            raise ValueError(
                f'testharness baseline for {exp_line.test!r} can only have up '
                f'to one harness error; found {harness_errors!r}.')
        elif harness_errors:
            error = harness_errors.pop()
            test_statuses = test_statuses & {'CRASH', 'TIMEOUT'}
            test_statuses.update(status.name for status in error.statuses)
        elif wpt_metadata.can_have_subtests(
                test_type) and exp_line.results == {ResultType.Failure}:
            # The `[ Failure ]` line for this test was only masking subtest
            # failures, but the harness is actually OK.
            test_statuses.add('OK')

        assert test_statuses, exp_line.to_string()
        test_ast = _build_expectation_ast(_test_basename(exp_line.test),
                                          normalize_statuses(test_statuses))
        # If a `[ Failure ]` line exists, the baseline is allowed to be
        # anything. To mimic this, skip creating any explicit subtests, and rely
        # on implicit subtest creation.
        if ResultType.Failure in exp_line.results:
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
    def install(cls, port: Port, expectations: TestExpectations):
        testloader.TestLoader = functools.partial(cls,
                                                  port,
                                                  expectations=expectations)


def _build_expectation_ast(name: str,
                           statuses: Container[str],
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
        if name not in self.subtests and wpt_metadata.can_have_subtests(
                self.test_type):
            # Create an implicit subtest that accepts any status. This supports
            # testharness tests marked with `[ Failure ]` without a checked-in
            # baseline, which would pass when run with `run_web_tests.py`.
            statuses = chromium_to_wptrunner_statuses(
                frozenset(WPTResult.status_priority), self.test_type, True)
            subtest_ast = _build_expectation_ast(name,
                                                 normalize_statuses(statuses))
            self.node.append(subtest_ast)
            self.append(
                static.compile_ast(
                    subtest_ast,
                    expr_data={},
                    data_cls_getter=lambda x, y: manifestexpected.SubtestNode,
                    test_path=self.parent.test_path))
        subtest = super().get_subtest(name)
        if 'TIMEOUT' in {self.expected, *self.known_intermittent}:
            expected = [subtest.expected, *subtest.known_intermittent]
            # Ignore which subtest timed out.
            for status in ['TIMEOUT', 'NOTRUN']:
                if status not in expected:
                    expected.append(status)
            subtest.set('expected', expected)
        return subtest


def _test_basename(test_id: str) -> str:
    # The test "basename" is test path + query string + fragment
    path_parts = urlsplit(test_id).path.rsplit('/', maxsplit=1)
    if len(path_parts) == 1:
        return test_id
    return test_id[len(path_parts[0]) + 1:]
