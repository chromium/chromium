# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code for generating targets spec files.

A builder can set its tests in starlark by setting the targets value in the
declaration. To do so also requires setting the builder_spec attribute.

When the register_builder_config function from the builder_config lib is called,
it will call the register_targets function to record the requested targets for
the builder. When the builder_config lib's generator is executed to generate the
builder config properties for the builders, it will call
get_targets_spec_generator to get an object that can be used to generate the
targets spec files for the builder.
"""

load("@stdlib//internal/graph.star", "graph")
load("//lib/args.star", args_lib = "args")
load("//lib/chrome_settings.star", "targets_config")
load("//lib/structs.star", "structs")
load("./common.star", _targets_common = "common")
load("./nodes.star", _targets_nodes = "nodes")

def register_targets(*, parent_key, builder_group, builder_name, name, targets, settings):
    """Register the targets for a builder.

    This will create the necessary nodes and edges so that the targets spec for
    the builder can be generated via get_targets_spec_generator.

    Args:
        parent_key: The graph key of the parent node to register the targets
            for.
        name: The name to use for the registered bundle. This will allow for
            other builders to specify their targets in terms of another
            builder's.
        targets: The targets for the builder. Can take the form of the name of a
            separately-declared bundle, an unnamed targets.bundle instance or a
            list of such elements.
        settings: The targets.settings instance to use for expanding the tests
            for the builder. If None, then a default targets.setting instance
            will be used.
    """
    targets_key = _targets_common.create_bundle(
        name = name,
        builder_group = builder_group,
        builder_name = builder_name,
        targets = args_lib.listify(targets),
        mixins = _targets_common.builder_defaults.mixins.get(),
        settings = settings or _targets_common.settings(),
    )

    graph.add_edge(parent_key, targets_key)

_OS_SPECIFIC_ARGS = set([
    "android_args",
    "chromeos_args",
    "lacros_args",
])

_OS_SPECIFIC_SWARMING = set([
    "android_swarming",
])

def _apply_mixin(spec, mixin_values):
    invalid_mixin_values = set([k for k in mixin_values if k not in spec.value])
    if "args" in spec.value:
        invalid_mixin_values -= _OS_SPECIFIC_ARGS
    if "swarming" in spec.value:
        invalid_mixin_values -= _OS_SPECIFIC_SWARMING
    if invalid_mixin_values:
        # Return the original spec in the case of an error so that the caller
        # doesn't have to save the original value
        return spec, "unsupported mixin values: {}".format(sorted(invalid_mixin_values))

    spec_value = dict(spec.value)
    mixin_values = dict(mixin_values)

    # TODO: crbug.com/40258588 Implement support for the os-specific mixin values
    for a in _OS_SPECIFIC_ARGS | _OS_SPECIFIC_SWARMING:
        mixin_values.pop(a, None)

    args_mixin = mixin_values.pop("args", None)
    if args_mixin:
        spec_value["args"] = args_lib.listify(spec_value["args"], args_mixin) or None

    swarming_mixin = mixin_values.pop("swarming", None)
    if swarming_mixin:
        spec_value["swarming"] = _targets_common.merge_swarming(spec_value["swarming"], swarming_mixin)

    spec_value.update(mixin_values)

    return structs.evolve(spec, value = spec_value), None

def _test_expansion(*, spec, source):
    return struct(
        spec = spec,
        source = source,
    )

def _get_bundle_resolver():
    def resolved_bundle(*, additional_compile_targets, test_expansion_by_name):
        return struct(
            additional_compile_targets = additional_compile_targets,
            test_expansion_by_name = test_expansion_by_name,
        )

    def visitor(_, children):
        return [c for c in children if c.key.kind == _targets_nodes.BUNDLE.kind]

    resolved_bundle_by_bundle_node_by_settings = {}

    def resolve(bundle_node, settings):
        resolved_bundle_by_bundle_node = resolved_bundle_by_bundle_node_by_settings.setdefault(settings, {})
        for n in graph.descendants(bundle_node.key, visitor = visitor, topology = graph.DEPTH_FIRST):
            if n in resolved_bundle_by_bundle_node:
                continue

            # TODO: crbug.com/1420012 - Update the handling of conflicting defs
            # so that more context is provided about where the error is
            # resulting from
            additional_compile_targets = set([t.key.id for t in graph.children(n.key, _targets_nodes.COMPILE_TARGET.kind)])

            test_expansion_by_name = {}
            for test in graph.children(n.key, kind = _targets_nodes.TEST.kind):
                spec_handler = test.props.spec_handler
                spec_value = spec_handler.init(test, settings)
                spec = struct(handler = spec_handler, value = spec_value)

                # The order that mixins are declared is significant,
                # DEFINITION_ORDER preserves the order that the edges were added
                # from the parent to the child
                for m in graph.children(test.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                    spec, error = _apply_mixin(spec, m.props.mixin_values)
                    if error:
                        fail("modifying {} {} with {} failed: {}"
                            .format(spec.handler.type_name, test.key.id, m, error))
                test_expansion_by_name[test.key.id] = _test_expansion(
                    spec = spec,
                    source = n.key,
                )

            for child in graph.children(n.key, kind = _targets_nodes.BUNDLE.kind):
                child_resolved_bundle = resolved_bundle_by_bundle_node[child]
                additional_compile_targets = additional_compile_targets | child_resolved_bundle.additional_compile_targets
                for name, test_expansion in child_resolved_bundle.test_expansion_by_name.items():
                    if name in test_expansion_by_name:
                        existing_test_expansion = test_expansion_by_name[name]
                        if existing_test_expansion.spec != test_expansion.spec:
                            fail("target {} has conflicting definitions in deps of {}\n  {}: {}\n  {}: {}".format(
                                name,
                                n.key,
                                existing_test_expansion.source,
                                existing_test_expansion.spec,
                                test_expansion.source,
                                test_expansion.spec,
                            ))
                    test_expansion_by_name[name] = test_expansion

            def update_spec_with_mixin(test_name, test_expansion, mixin, *, ignore_error = False):
                spec = test_expansion.spec
                new_spec, error = _apply_mixin(spec, mixin.props.mixin_values)
                if error:
                    if ignore_error:
                        return
                    fail(
                        "modifying {} {} with {} failed: {}"
                            .format(spec.handler.type_name, test_name, mixin, error),
                        trace = n.props.stacktrace,
                    )
                test_expansion_by_name[test_name] = structs.evolve(test_expansion, spec = new_spec, source = n.key)

            for name in n.props.tests_to_remove:
                if name not in test_expansion_by_name:
                    fail(
                        "attempting to remove test '{}' that is not contained in the bundle"
                            .format(name),
                        trace = n.props.stacktrace,
                    )
                test_expansion_by_name.pop(name)

            # The order that mixins are declared is significant,
            # DEFINITION_ORDER preserves the order that the edges were added
            # from the parent to the child
            for mixin in graph.children(n.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                for name, test_expansion in test_expansion_by_name.items():
                    # We don't care if a mixin applied at bundle level doesn't
                    # apply to every test, so ignore errors
                    update_spec_with_mixin(name, test_expansion, mixin, ignore_error = True)
            for per_test_modification in graph.children(n.key, kind = _targets_nodes.PER_TEST_MODIFICATION.kind):
                name = per_test_modification.key.id
                if name not in test_expansion_by_name:
                    fail(
                        "attempting to modify test '{}' that is not contained in the bundle"
                            .format(name),
                        trace = n.props.stacktrace,
                    )

                # The order that mixins are declared is significant,
                # DEFINITION_ORDER preserves the order that the edges were added
                # from the parent to the child
                for mixin in graph.children(per_test_modification.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                    update_spec_with_mixin(name, test_expansion_by_name[name], mixin)

            resolved_bundle_by_bundle_node[n] = resolved_bundle(
                additional_compile_targets = additional_compile_targets,
                test_expansion_by_name = test_expansion_by_name,
            )

        resolved = resolved_bundle_by_bundle_node[bundle_node]
        return (
            resolved.additional_compile_targets,
            {name: test_expansion.spec for name, test_expansion in resolved.test_expansion_by_name.items()},
        )

    return resolve

def _resolve_magic_args(builder_name, settings, spec_value):
    new_args = []
    for arg in spec_value["args"]:
        if type(arg) == type(struct()):
            new_args.extend(arg.function(builder_name, settings, spec_value))
        else:
            new_args.append(arg)
    spec_value["args"] = new_args

# flag to merge -> inter-value separator
_FLAGS_TO_MERGE = {
    "--enable-features=": ",",
    "--extra-browser-args=": " ",
    "--test-launcher-filter-file=": ";",
    "--extra-app-args=": ",",
}

def _merge_args(spec_value):
    new_args = []
    merged = {}
    for arg in spec_value["args"]:
        found_flag = False
        for flag in _FLAGS_TO_MERGE:
            # Add a placeholder, recording the index and the flag's value. Later
            # instances of the flag will add their value to the list without
            # updating new_args. After all arguments have been examined, the
            # placeholders will be replaced with the flag with combined values.
            if arg.startswith(flag):
                value = arg.removeprefix(flag)
                if flag not in merged:
                    merged[flag] = len(new_args), [value]
                    new_args.append(None)
                else:
                    _, values = merged[flag]
                    values.append(value)
                found_flag = True
                break
        if not found_flag:
            new_args.append(arg)
    for flag, (idx, values) in merged.items():
        separator = _FLAGS_TO_MERGE[flag]
        new_args[idx] = flag + separator.join(values)
    spec_value["args"] = new_args

def get_targets_spec_generator():
    """Get a generator for builders' targets specs.

    Returns:
        A function that can be used to get the targets specs for a builder. The
        function takes a single argument that is a node. If the node corresponds
        to a builder that has tests registered using register_targets, then a
        dict will be returned with the target specs for the builder. Otherwise,
        None will be returned.
    """
    bundle_resolver = _get_bundle_resolver()
    autoshard_exceptions = targets_config().autoshard_exceptions

    def get_targets_spec(parent_node):
        bundle_nodes = graph.children(parent_node.key, _targets_nodes.BUNDLE.kind)
        if not bundle_nodes:
            return None
        if len(bundle_nodes) > 1:
            fail("internal error: there should be at most 1 targets_spec")
        bundle_node = bundle_nodes[0]

        settings = bundle_node.props.settings
        if not settings:
            fail("internal error: settings should be set for bundle_node")
        builder_group = bundle_node.props.builder_group
        if not builder_group:
            fail("internal error: builder_group should be set for bundle_node")
        builder_name = bundle_node.props.builder_name
        if not builder_name:
            fail("internal error: builder_name should be set for bundle_node")

        current_autoshard_exceptions = autoshard_exceptions.get(builder_group, {}).get(builder_name, {})

        additional_compile_targets, test_spec_by_name = bundle_resolver(bundle_node, settings)
        sort_key_and_specs_by_type_key = {}
        for name, spec in test_spec_by_name.items():
            spec_value = dict(spec.value)
            type_key, sort_key, spec_value = spec.handler.finalize(name, settings, spec_value)
            if "args" in spec_value:
                _resolve_magic_args(builder_name, settings, spec_value)

                # Merge args after resolving magic args since that could produce
                # additional args that should be merged
                _merge_args(spec_value)
            if name in current_autoshard_exceptions:
                spec_value["swarming"]["shards"] = current_autoshard_exceptions[name]
            finalized_spec = {k: v for k, v in spec_value.items() if v not in ([], None)}
            sort_key_and_specs_by_type_key.setdefault(type_key, []).append((sort_key, finalized_spec))

        specs_by_type_key = {}
        if additional_compile_targets:
            specs_by_type_key["additional_compile_targets"] = sorted(additional_compile_targets)
        for type_key, sort_key_and_specs in sorted(sort_key_and_specs_by_type_key.items()):
            specs_by_type_key[type_key] = [spec for _, spec in sorted(sort_key_and_specs)]

        return specs_by_type_key

    return get_targets_spec
