# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining builder health indicator thresholds.

See //docs/infra/builder_health_indicators.md for more info.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./nodes.star", "nodes")
load("./structs.star", "structs")

_HEALTH_SPEC = nodes.create_bucket_scoped_node_type("health_spec")

# See https://source.chromium.org/chromium/infra/infra/+/main:go/src/infra/cr_builder_health/thresholds.go?q=f:thresholds.go%20%22type%20BuilderThresholds%22
# for all configurable thresholds.
_default_thresholds = struct(
    # If any of these threholds are exceeded, the builder will be deemed unhealthy.
    # Setting a value of None will ignore that threshold
    infra_fail_rate = struct(
        average = 0.05,
    ),
    fail_rate = struct(
        average = 0.2,
    ),
    build_time = struct(
        p50_mins = None,
    ),
    pending_time = struct(
        p50_mins = 20,
    ),
)

_blank_thresholds = struct(
    infra_fail_rate = struct(
        average = None,
    ),
    fail_rate = struct(
        average = None,
    ),
    build_time = struct(
        p50_mins = None,
    ),
    pending_time = struct(
        p50_mins = None,
    ),
)

DEFAULT = struct(_default = "_default")

def spec(**kwargs):
    return structs.evolve(_blank_thresholds, **kwargs)

def modified_default(**kwargs):
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

health_spec = struct(
    DEFAULT = DEFAULT,
    spec = spec,
    modified_default = modified_default,
)

lucicfg.generator(_generate_health_specs)
