# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for gtest-based tests."""

load("@stdlib//internal/graph.star", "graph")
load("//lib/args.star", args_lib = "args")
load("../common.star", _targets_common = "common")
load("../nodes.star", _targets_nodes = "nodes")
load("./isolated_script_test.star", "create_isolated_script_test_spec_handler", "isolated_script_test_details")

_isolated_script_test_spec_handler = create_isolated_script_test_spec_handler("gtest")

def _gtest_test_spec_init(node, settings):
    return _targets_common.spec_init(node, settings, additional_fields = dict(
        use_isolated_scripts_api = None,
        expand_as_isolated_script = False,
    ))

def _gtest_test_spec_finalize(builder_name, test_name, settings, spec_value):
    expand_as_isolated_script = spec_value.pop("expand_as_isolated_script")
    if expand_as_isolated_script:
        return _isolated_script_test_spec_handler.finalize(builder_name, test_name, settings, spec_value)

    use_isolated_scripts_api = spec_value["use_isolated_scripts_api"]
    if (settings.is_android and spec_value["swarming"].enable and not use_isolated_scripts_api):
        # TODO(crbug.com/40725094) make Android presentation work with
        # isolated scripts in test_results_presentation.py merge script
        _targets_common.update_spec_for_android_presentation(settings, spec_value)
        spec_value["args"] = args_lib.listify(spec_value["args"], "--recover-devices")
    default_merge_script = "standard_isolated_script_merge" if use_isolated_scripts_api else "standard_gtest_merge"
    spec_value = _targets_common.spec_finalize(builder_name, settings, spec_value, default_merge_script)
    return "gtest_tests", test_name, spec_value

_gtest_test_spec_handler = _targets_common.spec_handler(
    type_name = "gtest",
    init = _gtest_test_spec_init,
    finalize = _gtest_test_spec_finalize,
)

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
    legacy_test_key = _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(
            binary = binary,
            args = args,
        ),
        mixins = mixins,
    )

    test_key = _targets_common.create_test(
        name = name,
        spec_handler = _gtest_test_spec_handler,
        details = isolated_script_test_details(
            args = args,
        ),
        mixins = mixins,
    )

    binary_key = _targets_nodes.BINARY.key(binary or name)
    graph.add_edge(legacy_test_key, binary_key)
    graph.add_edge(test_key, binary_key)
