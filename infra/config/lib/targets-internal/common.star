# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions needed for targets implementation."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//lib/args.star", args_lib = "args")
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

def _binary_test_config(*, results_handler = None, merge = None, resultdb = None):
    """The details for a test provided by the test's binary.

    When test_suites.pyl is generated, tests that are using the binary
    will have these values written into the test's entry in the basic suite.

    Args:
        results_handler: The name of the results handler to use for the
            test.
        merge: A targets.merge describing the invocation to merge the
            results from the test's tasks.
        resultdb: A targets.resultdb describing the ResultDB integration
            for the test.
    """
    return struct(
        results_handler = results_handler,
        merge = merge,
        resultdb = resultdb,
    )

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
    for m in args_lib.listify(mixins):
        graph.add_edge(test_key, _targets_nodes.MIXIN.key(m))
    return test_key

def _create_bundle(
        *,
        name,
        additional_compile_targets = [],
        targets = [],
        builder_group = None,
        mixins = [],
        per_test_modifications = {}):
    bundle_key = _targets_nodes.BUNDLE.add(name, props = dict(
        builder_group = builder_group,
        # Record the stacktrace so that failures actually point out the failing
        # definition (this is especially important for unnamed bundles since
        # they won't have a useful name to search for)
        stacktrace = stacktrace(skip = 3),
    ))

    for t in additional_compile_targets:
        graph.add_edge(bundle_key, _targets_nodes.COMPILE_TARGET.key(t))
    for t in targets:
        graph.add_edge(bundle_key, _targets_nodes.BUNDLE.key(t))
    for m in mixins:
        graph.add_edge(bundle_key, _targets_nodes.MIXIN.key(m))
    for test_name, mods in per_test_modifications.items():
        # Use bundle_key.id here instead of name because an inline bundle will
        # have None for name
        modification_key = _targets_nodes.PER_TEST_MODIFICATION.add(bundle_key.id, test_name)
        graph.add_edge(bundle_key, modification_key)
        for m in args_lib.listify(mods):
            graph.add_edge(modification_key, _targets_nodes.MIXIN.key(m))
    return bundle_key

def _create_test(*, name, spec_handler, details = None, mixins = None):
    test_key = _targets_nodes.TEST.add(name, props = dict(
        spec_handler = spec_handler,
        details = details,
    ))
    bundle_key = _create_bundle(
        name = name,
    )
    graph.add_edge(bundle_key, test_key)
    for m in args_lib.listify(mixins):
        graph.add_edge(test_key, _targets_nodes.MIXIN.key(m))
    return test_key

def _get_test_binary_node(node):
    binary_nodes = graph.children(node.key, _targets_nodes.BINARY.kind)
    if len(binary_nodes) != 1:
        fail("internal error: test node {} should have link to exactly 1 binary node, got {}".format(node, binary_nodes))
    binary_node = binary_nodes[0]
    return binary_node

def _spec_handler(*, type_name, init, finalize):
    """Declare a spec handler for a target type.

    The node that is added for each test (type _target_nodes.TEST) will store a
    spec handler. When creating the initial spec for a test, the init function
    of the handler will be called. After the spec has been modified by all
    applicable mixins, the finalize function of the handler will be called to
    get the (mosty-)final value of the spec.

    Args:
        type_name: The name of the test type. This will be used in error
            messages.
        init: The function to create the initial value of the spec. The
            function will be called with the test node (type _target_nodes.TEST)
            and should return a dict with all keys populated that are supported
            by the type.
        finalize: The function that produces the (mostly-)final value of a spec
            for a target. The function will be passed the name of the test and
            the spec value (dict) that has been modified by all applicable
            mixins. The function should return a 3-tuple:
            * The test_suites key that the spec should be added to in the output
                json file (one of "gtest_tests", "isolated_scripts",
                "junit_tests", "scripts" or "skylab_tests").
            * The sort key used to order tests for a given test_suites key. The
                format is up to the spec handler, but all sort keys for a given
                test_suites key must be comparable.
            * The final value of the spec. This must be a dict with string keys.
                Any items in the dict where the value is None or [] will not be
                emitted in the final json.

    Returns:
        An object that can be passed to the spec_handler argument of
        common.create_test.
    """
    return struct(
        type_name = type_name,
        init = init,
        finalize = finalize,
    )

# TODO: crbug.com/1420012 - Update the handling of unimplemented test types so
# that more context is provided about where the error is resulting from
def _spec_handler_for_unimplemented_target_type(type_name):
    def unimplemented():
        fail("support for {} targets is not yet implemented".format(type_name))

    return _spec_handler(
        type_name = type_name,
        init = (lambda node: unimplemented()),
        finalize = (lambda name, spec: unimplemented()),
    )

def _merge(
        *,
        script,
        args = None):
    """Define a merge script to be used for a swarmed test.

    Args:
        script: GN-format path (e.g. //foo/bar/script.py) to the script
            to use to merge results from the shard tasks.
        args: Any args to pass to the merge script, in addition to any
            arguments supplied by the recipe.

    Returns:
        A struct that can be passed to the merge argument of
        `targets.mixin`.
    """
    return struct(
        script = script,
        args = args,
    )

def _finalize_merge(merge):
    if not merge:
        return None
    d = {a: getattr(merge, a) for a in dir(merge)}
    return {k: v for k, v in d.items() if v != None}

def _swarming(
        *,
        enable = None,
        dimensions = None,
        optional_dimensions = None,
        containment_type = None,
        cipd_packages = None,
        expiration_sec = None,
        hard_timeout_sec = None,
        io_timeout_sec = None,
        shards = None,
        idempotent = None,
        service_account = None,
        named_caches = None):
    """Define the swarming details for a test.

    When specified as a mixin, fields will overwrites the test's values
    unless otherwise indicated.

    Args:
        enable: Whether swarming should be enabled for the test.
        dimensions: A dict of dimensions to apply to all dimension sets
            for the test. This can only be specified in a mixin. After
            any dimension sets from the mixin are added to the test, the
            dimensions will be applied to all of the dimension sets on
            the test. If there are no dimension sets on the test, a
            single dimension set with these dimensions will be added.
        optional_dimensions: Optional dimensions to add to each
            dimension set.
        containment_type: The containment type to use for the swarming
            task(s). See ContainmentType enum in
            https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/swarming/proto/api/swarming.proto
        cipd_packages: A list of targets.cipd_package that detail CIPD
            packages to be downloaded for the test.
        expiration_sec: The time that each task for the test should wait
            to be scheduled.
        hard_timeout_sec: The maximum time each task for the test can
            take after starting.
        io_timeout_sec: The maximum time that can elapse between output
            from tasks for the test.
        shards: The number of tasks to split the test into.
        idempotent: Whether the test task should be considered
            idempotent.
        service_account: The service account used to run the test's
            tasks.
        named_caches: A list of swarming.cache that detail the named
            caches that should be mounted for the test's tasks.
    """
    return struct(
        enable = enable,
        dimensions = dimensions,
        optional_dimensions = optional_dimensions,
        containment_type = containment_type,
        cipd_packages = cipd_packages,
        expiration_sec = expiration_sec,
        hard_timeout_sec = hard_timeout_sec,
        io_timeout_sec = io_timeout_sec,
        shards = shards,
        idempotent = idempotent,
        service_account = service_account,
        named_caches = named_caches,
    )

def _finalize_swarming(swarming):
    if not swarming or not swarming.enable:
        return None
    d = {a: getattr(swarming, a) for a in dir(swarming) if a != "enable"}
    return {k: v for k, v in d.items() if v != None}

def _finalize_resultdb(resultdb):
    if not resultdb:
        return None
    d = {a: getattr(resultdb, a) for a in dir(resultdb)}
    return {k: v for k, v in d.items() if v != None}

def _spec_init(node, **kwargs):
    """Init for gtest and isolated script test specs."""
    binary_node = _get_test_binary_node(node)
    binary_test_config = binary_node.props.test_config or _binary_test_config()
    return dict(
        name = node.key.id,
        test = binary_node.key.id,
        test_id_prefix = binary_node.props.test_id_prefix,
        args = node.props.details.args,
        # Tests will be swarmed by default, builders that don't want tests
        # swarmed will use a mixin to disable it
        swarming = _swarming(enable = True),
        merge = binary_test_config.merge,
        resultdb = binary_test_config.resultdb,
        results_handler = binary_test_config.results_handler,
        **kwargs
    )

def _spec_finalize(spec_value, default_merge_script):
    spec_value = dict(spec_value)
    swarming = _finalize_swarming(spec_value["swarming"])
    spec_value["swarming"] = swarming
    if swarming and not spec_value["merge"]:
        spec_value["merge"] = _merge(
            script = "//testing/merge_scripts/{}.py".format(default_merge_script),
        )
    spec_value["merge"] = _finalize_merge(spec_value["merge"])
    spec_value["resultdb"] = _finalize_resultdb(spec_value["resultdb"])
    return spec_value

common = struct(
    # Functions used for creating objects that are part of the public API that
    # need to be used internally as well
    merge = _merge,
    swarming = _swarming,

    # Functions used for creating nodes by functions that define targets
    binary_test_config = _binary_test_config,
    create_compile_target = _create_compile_target,
    create_label_mapping = _create_label_mapping,
    basic_suite_test_config = _basic_suite_test_config,
    create_legacy_test = _create_legacy_test,
    create_test = _create_test,
    create_bundle = _create_bundle,

    # Functions for defining target spec types
    spec_handler = _spec_handler,
    spec_handler_for_unimplemented_target_type = _spec_handler_for_unimplemented_target_type,

    # Functions for implementing spec handlers
    spec_init = _spec_init,
    spec_finalize = _spec_finalize,
)
