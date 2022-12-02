# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining polymorphic builders."""

load("./builders.star", "builder", "defaults")
load("./nodes.star", "nodes")
load("//project.star", "settings")

_LAUNCHER = nodes.create_bucket_scoped_node_type("polymorphic-launcher")
_RUNNER = nodes.create_link_node_type("polymorphic-runner", _LAUNCHER, nodes.BUILDER)
_TARGET_BUILDER = nodes.create_link_node_type("polymorphic-target", _LAUNCHER, nodes.BUILDER)

def _builder_ref_to_builder_id(ref):
    bucket, builder = ref.split("/", 1)
    return dict(
        project = settings.project,
        bucket = bucket,
        builder = builder,
    )

def _target_builder(*, builder, dimensions = {}):
    """Details for a target builder for a polymorphic launcher.

    Args:
        builder: (str) The bucket-qualified reference to the builder that
            performs the polymoprhic runs.
        dimensions: (dict[str, str]) Additional dimensions to set for the target
            builder. Any dimensions specified here will override dimensions on
            the runner builder. An empty dimension value will remove the
            dimension when the runner builder is triggered for the target
            builder.
    """
    return struct(
        builder = builder,
        dimensions = dimensions,
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
            bucket-qualified reference to the target builder.
        **kwargs: Additional keyword arguments to be passed onto
            `builders.builder`.

    Returns:
        The lucicfg keyset for the builder
    """
    if not target_builders:
        fail("target_builders must not be empty")
    target_builders = [_target_builder(builder = t) if type(t) == type("") else t for t in target_builders]
    bucket = defaults.get_value_from_kwargs("bucket", kwargs)

    launcher_key = _LAUNCHER.add(bucket, name)

    # Create links to the runner and target builders. We don't actually do
    # anything with the links, but lucicfg will check that the nodes that are
    # linked to were actually added (i.e. that the referenced builders actually
    # exist).
    _RUNNER.link(launcher_key, runner)
    for t in target_builders:
        _TARGET_BUILDER.link(launcher_key, t.builder)

    def _target_builder_prop(t):
        p = {"builder_id": _builder_ref_to_builder_id(t.builder)}
        if t.dimensions:
            p["dimensions"] = t.dimensions
        return p

    properties = dict(kwargs.pop("properties", {}))
    properties.update({
        "runner_builder": _builder_ref_to_builder_id(runner),
        "target_builders": [_target_builder_prop(t) for t in target_builders],
    })

    kwargs.setdefault("executable", "recipe:chromium_polymorphic/launcher")
    kwargs.setdefault("resultdb_enable", False)

    return builder(
        name = name,
        properties = properties,
        **kwargs
    )

polymorphic = struct(
    launcher = _launcher,
    target_builder = _target_builder,
)
