# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A common library for using WPT metadata in Blink."""

from typing import Any, Dict, Optional, Set
from blinkpy.common import path_finder

path_finder.bootstrap_wpt_imports()
from wptrunner.wptmanifest import node as wptnode
from wptrunner.manifestexpected import TestNode, SubtestNode

RunInfo = Dict[str, Any]


def fill_implied_expectations(test: TestNode,
                              extra_subtests: Optional[Set[str]] = None,
                              test_type: str = 'testharness'):
    """Populate a test result with implied OK/PASS expectations.

    This is a helper for diffing WPT results.
    """
    default_test_status = 'OK' if test_type == 'testharness' else 'PASS'
    _ensure_expectation(test, default_test_status)
    for subtest in test.subtests:
        _ensure_expectation(test.get_subtest(subtest), 'PASS')
    missing_subtests = (extra_subtests or set()) - set(test.subtests)
    for subtest in missing_subtests:
        subtest_node = SubtestNode(wptnode.DataNode(subtest))
        # Append to both the test container and the underlying AST.
        test.append(subtest_node)
        test.node.append(subtest_node.node)
        _ensure_expectation(subtest_node, 'PASS')


def _ensure_expectation(test: TestNode, default_status: str):
    if not test.has_key('expected'):
        test.set('expected', default_status)


# TODO(crbug.com/1299650): Move shared code in `update-metadata`, `lint-wpt` to
# this module.
