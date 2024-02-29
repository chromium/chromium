# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions needed for targets implementation."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//lib/args.star", "args")
load("./nodes.star", _targets_nodes = "nodes")

def _create_compile_target(*, name):
    _targets_nodes.COMPILE_TARGET.add(name)

def _create_label_mapping(
        *,
        name,
        type,
        label,
        label_type = None,
        executable = None,
        executable_suffix = None,
        script = None,
        skip_usage_check = False,
        args = None):
    mapping_key = _targets_nodes.LABEL_MAPPING.add(name, props = dict(
        type = type,
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
    ))
    graph.add_edge(keys.project(), mapping_key)

def _basic_suite_test_config(
        *,
        script = None,
        binary = None,
        telemetry_test_name = None,
        args = None):
    """The details for the test included when included in a basic suite.

    When generating test_suites.pyl, these values will be written out
    for a test in any basic suite that includes it.

    Args:
        script: The name of the file within the //testing/scripts
            directory to run as the test. Only applicable to script tests.
        binary: The name of the binary to run as the test. Only
            applicable to gtests, isolated script tests and junit tests.
        telemetry_test_name: The telemetry test to run. Only applicable
            to telemetry test types.
        args: Arguments to be passed to the test binary.
    """
    return struct(
        script = script,
        binary = binary,
        telemetry_test_name = telemetry_test_name,
        args = args,
    )

def _create_legacy_test(*, name, basic_suite_test_config, mixins = None):
    test_key = _targets_nodes.LEGACY_TEST.add(name, props = dict(
        basic_suite_test_config = basic_suite_test_config,
    ))
    for m in args.listify(mixins):
        graph.add_edge(test_key, _targets_nodes.MIXIN.key(m))
    return test_key

def _create_bundle(*, name, additional_compile_targets = [], targets = [], builder_group = None, test_spec_by_name = {}, modifications_by_name = {}):
    key = _targets_nodes.BUNDLE.add(name, props = dict(
        builder_group = builder_group,
        test_spec_by_name = test_spec_by_name,
        modifications_by_name = modifications_by_name,
    ))

    for t in additional_compile_targets:
        graph.add_edge(key, _targets_nodes.COMPILE_TARGET.key(t))
    for t in targets:
        graph.add_edge(key, _targets_nodes.BUNDLE.key(t))
    return key

def _create_test(*, name, spec_handler, spec_value):
    return _create_bundle(
        name = name,
        test_spec_by_name = {
            name: struct(
                handler = spec_handler,
                spec_value = spec_value,
            ),
        },
    )

def _spec_handler(*, finalize):
    """Create a spec handler for a target type.

    The spec handler is responsible for producing the final value of the spec
    once all mixins have been applied.

    Args:
        finalize: Produce the final value of a spec for a target. The function
            will be passed the name of the test and the spec value that has been
            modified by all applicable mixins. The function should return a
            3-tuple:
            * The test_suites key that the spec should be added to in the output
                json file (one of "gtest_tests", "isolated_scripts",
                "junit_tests", "scripts" or "skylab_tests").
            * The sort key used to order tests for a given test_suites key. The
                format is up to the spec handler, but all sort keys for a given
                test_suites key must be comparable.
            * The final value of the spec. This must be some object that can be
                encoded to json with json.encode
                (https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#json.encode).

    Returns:
        An object that can be passed to the spec_handler argument of
        common.create_test.
    """
    return struct(
        finalize = finalize,
    )

# TODO: crbug.com/1420012 - Update the handling of unimplemented test types so
# that more context is provided about where the error is resulting from
def _spec_handler_for_unimplemented_target_type(type_name):
    def unimplemented():
        fail("support for {} targets is not yet implemented".format(type_name))

    return _spec_handler(
        finalize = (lambda name, spec: unimplemented()),
    )

common = struct(
    # Functions used for creating nodes by functions that define targets
    create_compile_target = _create_compile_target,
    create_label_mapping = _create_label_mapping,
    basic_suite_test_config = _basic_suite_test_config,
    create_legacy_test = _create_legacy_test,
    create_test = _create_test,
    create_bundle = _create_bundle,

    # Functions for defining target spec types
    spec_handler = _spec_handler,
    spec_handler_for_unimplemented_target_type = _spec_handler_for_unimplemented_target_type,
)
