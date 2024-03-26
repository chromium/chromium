# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for gtest-based tests."""

load("@stdlib//internal/graph.star", "graph")
load("../common.star", _targets_common = "common")
load("../nodes.star", _targets_nodes = "nodes")

def _gtest_test_spec_init(node):
    binary_node = _targets_common.get_test_binary_node(node)
    return dict(
        name = node.key.id,
        test = binary_node.key.id,
        test_id_prefix = binary_node.props.test_id_prefix,
        args = node.props.details.args,
        # Tests will be swarmed by default, builders that don't want tests
        # swarmed will use a mixin to disable it
        swarming = _targets_common.swarming(enable = True),
        merge = None,
        use_isolated_scripts_api = None,
    )

def _gtest_test_spec_finalize(name, spec_value):
    spec_value = dict(spec_value)
    swarming = _targets_common.finalize_swarming(spec_value["swarming"])
    spec_value["swarming"] = swarming
    if swarming and not spec_value["merge"]:
        script = "standard_isolated_script_merge" if spec_value["use_isolated_scripts_api"] else "standard_gtest_merge"
        spec_value["merge"] = _targets_common.merge(
            script = "//testing/merge_scripts/{}.py".format(script),
        )
    spec_value["merge"] = _targets_common.finalize_merge(spec_value["merge"])
    return "gtest_tests", name, spec_value

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
        details = struct(
            args = args,
        ),
    )

    binary_key = _targets_nodes.BINARY.key(binary or name)
    graph.add_edge(legacy_test_key, binary_key)
    graph.add_edge(test_key, binary_key)
