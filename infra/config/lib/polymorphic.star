# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining polymorphic builders."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "builder_ref", "keys")
load("./builders.star", "builder", "defaults")
load("./nodes.star", "nodes")
load("//project.star", "settings")

_LAUNCHER = nodes.create_bucket_scoped_node_type("polymorphic-launcher")
_RUNNER = nodes.create_link_node_type("polymorphic-runner", _LAUNCHER, nodes.BUILDER)
_TARGET_BUILDER = nodes.create_link_node_type("polymorphic-target", _LAUNCHER, nodes.BUILDER)
_TARGET_TESTER = nodes.create_link_node_type("polymorphic-target-tester", nodes.BUILDER, nodes.BUILDER)

def _builder_ref_to_builder_id(ref):
    bucket, builder = ref.split("/", 1)
    return dict(
        project = settings.project,
        bucket = bucket,
        builder = builder,
    )

def _target_builder(*, builder, dimensions = None, testers = None):
    """Details for a target builder for a polymorphic launcher.

    Args:
        builder: (str) The bucket-qualified reference to the builder that
            performs the polymoprhic runs.
        dimensions: (dimensions.dimensions) Additional dimensions to set for the
            target builder. Any dimensions specified here will override
            dimensions on the runner builder. An empty dimension value will
            remove the dimension when the runner builder is triggered for the
            target builder.
        testers: (list[str]) An optional list of testers to restrict the
            operation to. If not specified, then the operation will include all
            testers that are triggered by the target builder.
    """
    if dimensions:
        dimensions = dimensions.resolve(*builder.split("/"))
    return struct(
        builder = builder,
        dimensions = dimensions,
        testers = testers,
    )

def _launcher(
        *,
        name,
        runner,
        target_builders,
        **kwargs):
    """Define a polymorphic launcher builder.

    The executable will default to the `chromium_polymorphic/launcher` recipe
    and the properties will be updated to set the `runner_builder` and
    `target_builder` properties as required by the recipe.

    Args:
        name: (str) The name of the builder.
        runner: (str) Bucket-qualified reference to the builder that performs
            the polymorphic runs.
        target_builders: (list[str|target_builder]) The target builders that the
            runner builder should be triggered for. Can either be an object
            returned by polymorphic.target_builder or a string with the
            bucket-qualified reference to the target builder. It should be noted
            that an empty list has different behavior from the default: none of
            the triggered testers will be included in the operation.
        **kwargs: Additional keyword arguments to be passed onto
            `builders.builder`.

    Returns:
        The lucicfg keyset for the builder
    """
    if not target_builders:
        fail("target_builders must not be empty")
    target_builders = [_target_builder(builder = t) if type(t) == type("") else t for t in target_builders]
    bucket = defaults.get_value_from_kwargs("bucket", kwargs)

    launcher_key = _LAUNCHER.add(bucket, name, props = dict(
        runner = runner,
        target_builders = target_builders,
    ))
    graph.add_edge(keys.project(), launcher_key)

    # Create links to the runner and target builders. We don't actually do
    # anything with the links, but lucicfg will check that the nodes that are
    # linked to were actually added (i.e. that the referenced builders actually
    # exist).
    _RUNNER.link(launcher_key, runner)
    for t in target_builders:
        _TARGET_BUILDER.link(launcher_key, t.builder)
        if t.testers != None:
            for tester in t.testers:
                _TARGET_TESTER.link(launcher_key, tester)

    kwargs.setdefault("executable", "recipe:chromium_polymorphic/launcher")
    kwargs.setdefault("resultdb_enable", False)

    return builder(
        name = name,
        **kwargs
    )

polymorphic = struct(
    launcher = _launcher,
    target_builder = _target_builder,
)

def _get_tester_group_and_name(context_node, builder_proto_by_key, tester_ref):
    builder_ref_node = graph.node(keys.builder_ref(tester_ref))
    builder_node = builder_ref.follow(builder_ref_node, context_node)
    builder_proto = builder_proto_by_key[builder_node.key]
    builder_group = json.decode(builder_proto.properties)["builder_group"]
    return {
        "group": builder_group,
        "builder": builder_node.key.id,
    }

def _target_builder_prop(context_node, builder_proto_by_key, target_builder):
    p = {"builder_id": _builder_ref_to_builder_id(target_builder.builder)}
    if target_builder.dimensions:
        p["dimensions"] = target_builder.dimensions
    if target_builder.testers != None:
        testers = []
        p["tester_filter"] = {"testers": testers}
        for t in target_builder.testers:
            testers.append(_get_tester_group_and_name(context_node, builder_proto_by_key, t))
    return p

def _generate_launcher_properties(ctx):
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to update properties")

    builder_proto_by_key = {}
    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            builder_proto_by_key[keys.builder(bucket_name, builder_name)] = builder

    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            node = _LAUNCHER.get(bucket_name, builder_name)
            if not node:
                continue

            properties = json.decode(builder.properties)

            properties.update({
                "runner_builder": _builder_ref_to_builder_id(node.props.runner),
                "target_builders": [_target_builder_prop(node, builder_proto_by_key, t) for t in node.props.target_builders],
            })

            builder.properties = json.encode(properties)

lucicfg.generator(_generate_launcher_properties)
