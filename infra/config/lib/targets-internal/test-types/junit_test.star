# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for JUnit-based tests."""

load("@stdlib//internal/graph.star", "graph")
load("../common.star", _targets_common = "common")
load("./isolated_script_test.star", "create_isolated_script_test_spec_handler", "isolated_script_test_details")

_junit_test_spec_handler = create_isolated_script_test_spec_handler("junit test")

def junit_test(*, name, label, skip_usage_check = False):
    """Define a junit test.

    A junit test is a test using the JUnit test framework. A junit test
    can be included in a basic suite to run the test for any builder
    that includes that basic suite.

    Args:
        name: The name that can be used to refer to the test in other
            starlark declarations. The step name of the test will be
            based on this name (additional components may be added by
            the recipe or when generating a test with a variant).
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
    """
    binary_key = _targets_common.create_binary(
        name = name,
        type = "generated_script",
        label = label,
        skip_usage_check = skip_usage_check,
    )

    legacy_test_key = _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(),
    )

    test_key = _targets_common.create_test(
        name = name,
        spec_handler = _junit_test_spec_handler,
        details = isolated_script_test_details(),
    )

    graph.add_edge(legacy_test_key, binary_key)
    graph.add_edge(test_key, binary_key)
