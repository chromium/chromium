# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for GPU telemetry tests."""

load("../common.star", _targets_common = "common")

# TODO(gbeaty) The args that are specified for webgl2?_conformance(.*)_tests
# in the basic suites are pretty formulaic, it would probably make sense to lift
# many of those values into this function
def gpu_telemetry_test(
        *,
        name,
        telemetry_test_name = None,
        args = None,
        mixins = None):
    """Define a GPU telemetry test.

    A GPU telemetry test can be included in a basic suite to run the
    test for any builder that includes that basic suite.

    Args:
        name: The name that can be used to refer to the test in other
            starlark declarations. The step name of the test will be
            based on this name (additional components may be added by
            the recipe or when generating a test with a variant).
        telemetry_test_name: The name of the telemetry benchmark to run.
        mixins: Mixins to apply when expanding the test.
    """
    _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(
            telemetry_test_name = telemetry_test_name,
            args = args,
        ),
        mixins = mixins,
    )
    _targets_common.create_test(
        name = name,
        spec_type = _targets_common.spec_type_for_unimplemented_target_type("gpu_telemetry_test"),
        spec_value = None,
    )
