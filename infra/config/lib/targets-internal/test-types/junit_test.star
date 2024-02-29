# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for JUnit-based tests."""

load("../common.star", _targets_common = "common")

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

    # We don't need to reuse the test binary for multiple junit tests, so just
    # define the compile target and isolate entry as part of the test
    # declaration
    _targets_common.create_compile_target(
        name = name,
    )
    _targets_common.create_label_mapping(
        name = name,
        type = "generated_script",
        label = label,
        skip_usage_check = skip_usage_check,
    )

    _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(),
    )

    _targets_common.create_test(
        name = name,
        spec_handler = _targets_common.spec_handler_for_unimplemented_target_type("junit_test"),
        spec_value = None,
    )
