# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator to output information about blocking CQ builders.

This generator copies commit-queue.cfg, removing builders that are experimental
or includable_only and removing other fields that don't impact when the CQ is
triggered or what the CQ triggers. The resultant file is output as
cq-usage/default.cfg. This enables applying limited owners for changes that
would impact the CQ for all users.
"""

load("@stdlib//internal/luci/proto.star", "cq_pb")
load("//lib/try.star", "location_filters_without_defaults")
load("//subprojects/chromium/fallback-cq.star", "fallback_cq")

def _remove_none(l):
    return [e for e in l if e != None]

def _trim_builder(builder, include_path_based):
    if builder.includable_only or builder.experiment_percentage:
        return None

    if builder.mode_allowlist and "FULL_RUN" not in builder.mode_allowlist:
        return None

    # The majority of CQ builders will exclude something because we don't
    # trigger most builders on changes to non-code directories (e.g. docs), so
    # only consider them path-based if they have an include filter
    if not include_path_based and location_filters_without_defaults(builder):
        return None
    trimmed = cq_pb.Verifiers.Tryjob.Builder(
        name = builder.name,
        disable_reuse = builder.disable_reuse,
    )
    if include_path_based:
        trimmed.location_filters = builder.location_filters
    return trimmed

def _trim_tryjob(tryjob, include_path_based):
    builders = _remove_none([_trim_builder(b, include_path_based) for b in tryjob.builders])
    if not builders:
        return None
    tryjob = proto.clone(tryjob)
    tryjob.builders = builders
    return tryjob

def _trim_verifiers(verifiers, include_path_based):
    if not proto.has(verifiers, "tryjob"):
        return None
    tryjob = _trim_tryjob(verifiers.tryjob, include_path_based)
    if not tryjob:
        return None
    return cq_pb.Verifiers(tryjob = tryjob)

def _trim_config_group(config_group, include_path_based):
    verifiers = _trim_verifiers(config_group.verifiers, include_path_based)
    if not verifiers:
        return None
    return cq_pb.ConfigGroup(
        name = config_group.name,
        verifiers = verifiers,
        gerrit = config_group.gerrit,
    )

def _generate_cq_usage(ctx):
    cfg = ctx.output["luci/commit-queue.cfg"]
    ctx.output["cq-usage/default.cfg"] = cq_pb.Config(config_groups = _remove_none(
        [
            _trim_config_group(g, include_path_based = False)
            for g in cfg.config_groups
            if g.name != fallback_cq.GROUP
        ],
    ))
    ctx.output["cq-usage/full.cfg"] = cq_pb.Config(config_groups = _remove_none(
        [
            _trim_config_group(g, include_path_based = True)
            for g in cfg.config_groups
            if g.name != fallback_cq.GROUP
        ],
    ))

lucicfg.generator(_generate_cq_usage)
