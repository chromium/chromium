# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for isolated script tests."""

load("@stdlib//internal/graph.star", "graph")
load("../common.star", _targets_common = "common")
load("../nodes.star", _targets_nodes = "nodes")

def _isolated_script_test_spec_init(node, settings, **kwargs):
    return _targets_common.spec_init(node, settings, **kwargs)

def _isolated_script_test_spec_finalize(builder_name, test_name, settings, spec_value):
    default_merge_script = "standard_isolated_script_merge"
    spec_value = _targets_common.spec_finalize(builder_name, settings, spec_value, default_merge_script)
    return "isolated_scripts", test_name, spec_value

def create_isolated_script_test_spec_handler(type_name):
    """Create spec handler for test type implemented via isolated scripts.

    The isolated script interface is the common interface all tests should
    implement, but ideally we would not allow directly configuring arbitrary
    isolated scripts and instead require a more-specific test type. This
    function allows other test types to be implemented that use the isolated
    script interface at run time. It should not be used by files outside of this
    directory.
    """
    return _targets_common.spec_handler(
        type_name = type_name,
        init = _isolated_script_test_spec_init,
        finalize = _isolated_script_test_spec_finalize,
    )

def isolated_script_test_details(*, args = None, additional_fields = {}):
    return struct(
        args = args,
        **additional_fields
    )

_isolated_script_test_spec_handler = create_isolated_script_test_spec_handler("isolated script")

def isolated_script_test(*, name, binary = None, mixins = None, args = None):
    """Define an isolated script test.

    An isolated script test can be included in a basic suite to run the
    test for any builder that includes that basic suite.

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
        spec_handler = _isolated_script_test_spec_handler,
        details = isolated_script_test_details(
            args = args,
        ),
        mixins = mixins,
    )

    binary_key = _targets_nodes.BINARY.key(binary or name)
    graph.add_edge(legacy_test_key, binary_key)
    graph.add_edge(test_key, binary_key)
