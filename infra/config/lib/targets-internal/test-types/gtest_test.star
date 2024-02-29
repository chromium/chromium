# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for gtest-based tests."""

load("@stdlib//internal/graph.star", "graph")
load("../common.star", _targets_common = "common")
load("../nodes.star", _targets_nodes = "nodes")

def gtest_test(*, name, binary = None, mixins = None, args = None):
    """Define a gtest-based test.

    A gtest test can be included in a basic suite to run the test for
    any builder that includes that basic suite.

    Args:
        name: The name that can be used to refer to the test in other
            starlark declarations. The step name of the test will be
            based on this name (additional components may be added by
            the recipe or when generating a test with a variant).
        binary: The test binary to use. There must be a defined binary
            with the given name. If none is provided, then an binary
            with the same name as the test must be defined.
        mixins: Mixins to apply when expanding the test.
        args: Arguments to be passed to the test binary.
    """
    key = _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(
            binary = binary,
            args = args,
        ),
        mixins = mixins,
    )

    # Make sure that the binary actually exists
    graph.add_edge(key, _targets_nodes.BINARY.key(binary or name))

    _targets_common.create_test(
        name = name,
        spec_handler = _targets_common.spec_handler_for_unimplemented_target_type("gtest_test"),
        spec_value = None,
    )
