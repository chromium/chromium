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

def launcher(
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
        target_builders: (list[str]) Bucket-qualified references to the target
            builders.
        **kwargs: Additional keyword arguments to be passed onto
            `builders.builder`.

    Returns:
        The lucicfg keyset for the builder
    """
    if not target_builders:
        fail("target_builders must not be empty")
    bucket = defaults.get_value_from_kwargs("bucket", kwargs)

    launcher_key = _LAUNCHER.add(bucket, name)

    # Create links to the runner and target builders. We don't actually do
    # anything with the links, but lucicfg will check that the nodes that are
    # linked to were actually added (i.e. that the referenced builders actually
    # exist).
    _RUNNER.link(launcher_key, runner)
    for t in target_builders:
        _TARGET_BUILDER.link(launcher_key, t)

    properties = dict(kwargs.pop("properties", {}))
    properties.update({
        "runner_builder": _builder_ref_to_builder_id(runner),
        "target_builders": [
            {"builder_id": _builder_ref_to_builder_id(t)}
            for t in target_builders
        ],
    })

    kwargs.setdefault("executable", "recipe:chromium_polymorphic/launcher")
    kwargs.setdefault("resultdb_enable", False)

    return builder(
        name = name,
        properties = properties,
        **kwargs
    )

polymorphic = struct(
    launcher = launcher,
)
