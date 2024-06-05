# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for registering builders to gardener_rotations."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./nodes.star", "nodes")
load("//project.star", "settings")

_SHERIFF_ROTATION = nodes.create_unscoped_node_type("sheriff_rotation")

_SHERIFFED_BUILDER = nodes.create_bucket_scoped_node_type("sheriffed_builder")

def register_gardener_builder(bucket, builder, gardener_rotations):
    """Register a builder with its sheriff rotations.

    Args:
        bucket: The name of the bucket of the builder.
        builder: The name of the builder.
        gardener_rotations: A list of the names of sheriff rotations the builder
            is a part of.
    """
    sheriffed_builder_key = _SHERIFFED_BUILDER.add(bucket, builder)
    for s in gardener_rotations:
        sheriff_rotation_key = _SHERIFF_ROTATION.add(s, idempotent = True)
        graph.add_edge(sheriff_rotation_key, sheriffed_builder_key)
        graph.add_edge(keys.project(), sheriff_rotation_key)

def get_gardener_rotations(bucket, builder):
    sheriffed_builder = _SHERIFFED_BUILDER.get(bucket, builder)
    if sheriffed_builder:
        return graph.parents(sheriffed_builder.key)
    return []

def _generate_gardener_rotations_files(ctx):
    if not settings.is_main:
        return

    for sheriff_rotation_node in graph.children(keys.project(), _SHERIFF_ROTATION.kind):
        sheriffed_builders = []
        for sheriffed_builder_node in graph.children(sheriff_rotation_node.key, _SHERIFFED_BUILDER.kind):
            key = sheriffed_builder_node.key
            sheriffed_builders.append("{}/{}".format(key.container.id, key.id))
        sheriff_rotation_file = "sheriff-rotations/{}.txt".format(sheriff_rotation_node.key.id)
        ctx.output[sheriff_rotation_file] = "".join(["{}\n".format(b) for b in sorted(sheriffed_builders)])

lucicfg.generator(_generate_gardener_rotations_files)
