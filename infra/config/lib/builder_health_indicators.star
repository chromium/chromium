# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining builder health indicator thresholds.

See chromium/src -- //docs/infra/builder_health_indicators.md for more info.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//project.star", "settings")
load("./builder_exemptions.star", "exempted_from_contact_builders")
load("./nodes.star", "nodes")
load("./structs.star", "structs")

_HEALTH_SPEC = nodes.create_bucket_scoped_node_type("health_spec")

# See https://source.chromium.org/chromium/infra/infra/+/main:go/src/infra/cr_builder_health/src_config.go
# for all configurable thresholds.
_default_specs = {
    "Unhealthy": struct(
        score = 5,
        period_days = 7 if settings.project.startswith("chromium") else 14,
        # If any of these thresholds are exceeded, the builder will be deemed
        # unhealthy.
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
            p50_mins = 20 if settings.project.startswith("chromium") else 60,
        ),
    ),
    "Low Value": struct(
        score = 1,
        period_days = 90,
        # If any of these thresholds are met, the builder will be deemed
        # low-value and will be considered for deletion.
        # Setting a value of None will ignore that threshold
        fail_rate = struct(
            average = 0.99,
        ),
    ),
}

_blank_unhealthy_thresholds = struct(
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

blank_low_value_thresholds = struct(
    fail_rate = struct(
        average = None,
    ),
)

DEFAULT = {
    "Unhealthy": struct(
        score = 5,
        period_days = 7 if settings.project.startswith("chromium") else 14,
        _default = "_default",
    ),
    "Low Value": struct(
        score = 1,
        period_days = 90,
        _default = "_default",
    ),
}

# Users define the specs as {problem_name -> problem_spec} for aesthetic reasons
# So all user-exposed functions expect a dictionary.
# We then convert that into a list of [problem_specs] so the object encapsulates
# its own name, for ease of processing
def unhealthy_thresholds(
        fail_rate = struct(),
        infra_fail_rate = struct(),
        build_time = struct(),
        pending_time = struct()):
    thresholds = {"fail_rate": fail_rate, "infra_fail_rate": infra_fail_rate, "build_time": build_time, "pending_time": pending_time}
    fail_if_any_none_val(thresholds)

    return structs.evolve(_blank_unhealthy_thresholds, **thresholds)

def low_value_thresholds(
        fail_rate = struct()):
    thresholds = {"fail_rate": fail_rate}
    fail_if_any_none_val(thresholds)

    return structs.evolve(blank_low_value_thresholds, **thresholds)

def fail_if_any_none_val(vals):
    for k, v in vals.items():
        if v == None:
            fail(k + " threshold was None. Thresholds can't be None. Use an empty struct() instead")

def modified_default(modifications):
    return _merge_mods(_default_specs, modifications)

def _merge_mods(base, modifications):
    spec = dict(base)

    for mod_name, mod in modifications.items():
        mods_proto = structs.to_proto_properties(mod)
        if len(mods_proto) == 0:
            fail("Modifications for health spec \"{}\" were empty.".format(mod_name))

        if mod_name not in spec:
            spec[mod_name] = mod
        else:
            spec[mod_name] = structs.evolve(spec[mod_name], **mods_proto)

    return spec

def _exempted_from_contact(bucket, builder):
    return builder in exempted_from_contact_builders.get(bucket, [])

def register_health_spec(bucket, name, specs, contact_team_email):
    if not contact_team_email and not _exempted_from_contact(bucket, name):
        fail("Builder " + name + " must have a contact_team_email. All new builders must specify a team email for contact in case the builder stops being healthy or providing value.")
    elif contact_team_email and _exempted_from_contact(bucket, name):
        fail("Need to remove builder " + bucket + "/" + name + " from exempted_from_contact_builders")

    if specs:
        spec = struct(
            problem_specs = _convert_specs(specs),
            contact_team_email = contact_team_email,
        )
        health_spec_key = _HEALTH_SPEC.add(
            bucket,
            name,
            props = structs.to_proto_properties(spec),
            idempotent = True,
        )

        graph.add_edge(keys.project(), health_spec_key)

def _convert_specs(specs):
    """Users define the specs as {problem_name -> problem_spec} for aesthetic reasons,

    So all user-exposed functions expect a dictionary.
    We then convert that into a list of [problem_specs] so the object encapsulates its own name, for ease of processing
    """
    converted_specs = []
    for name, spec in specs.items():
        thresholds_spec = structs.to_proto_properties(spec)
        thresholds_spec.pop("score")
        thresholds_spec.pop("period_days")
        converted_specs.append(struct(
            name = name,
            score = spec.score,
            period_days = spec.period_days,
            thresholds = thresholds_spec,
        ))

    return converted_specs

def _generate_health_specs(ctx):
    specs = {}

    for node in graph.children(keys.project(), _HEALTH_SPEC.kind):
        bucket = node.key.container.id
        builder = node.key.id
        specs.setdefault(bucket, {})[builder] = node.props

    result = {
        "_default_specs": _convert_specs(_default_specs),
        "specs": specs,
    }

    ctx.output["health-specs/health-specs.json"] = json.indent(json.encode(result), indent = "  ")

health_spec = struct(
    DEFAULT = DEFAULT,
    unhealthy_thresholds = unhealthy_thresholds,
    low_value_thresholds = low_value_thresholds,
    modified_default = modified_default,
)

lucicfg.generator(_generate_health_specs)
