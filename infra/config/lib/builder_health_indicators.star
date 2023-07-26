# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining builder health indicator thresholds."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./nodes.star", "nodes")
load("./structs.star", "structs")

_HEALTH_SPEC = nodes.create_bucket_scoped_node_type("health_spec")

_default_thresholds = struct(
    # If any of these threholds are exceeded, the builder will be deemed unhealthy.
    infra_fail_rate = struct(
        average = 0.05,
    ),
    fail_rate = struct(
        average = 0.2,
    ),
    build_time = struct(
        p50_mins = 60,
    ),
    pending_time = struct(
        p50_mins = 20,
    ),
)

DEFAULT_HEALTH_SPEC = struct(_default = "_default")

def health_spec(**kwargs):
    return structs.evolve(_default_thresholds, **kwargs)

def register_health_spec(bucket, name, spec):
    if spec:
        health_spec_key = _HEALTH_SPEC.add(
            bucket,
            name,
            props = structs.to_proto_properties(spec),
            idempotent = True,
        )

        graph.add_edge(keys.project(), health_spec_key)

def _generate_health_specs(ctx):
    specs = {}

    for node in graph.children(keys.project(), _HEALTH_SPEC.kind):
        bucket = node.key.container.id
        builder = node.key.id
        specs.setdefault(bucket, {})[builder] = node.props

    result = {
        "_default": _default_thresholds,
        "thresholds": specs,
    }

    ctx.output["health-specs/health-specs.json"] = json.indent(json.encode(result), indent = "  ")

lucicfg.generator(_generate_health_specs)
