# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A validator to enforce that triggers do not cross builder groups."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys", "triggerer")

# These triggers cross builder groups but predate us restricting triggers to
# within a builder group, we should attempt to remove these where feasible
_LEGACY_CROSS_BUILDER_GROUP_TRIGGERS = {
    ("ci", "Android x64 Builder (dbg)"): [
        ("ci", "android-12-x64-dbg-tests"),
        ("ci", "android-webview-12-x64-dbg-tests"),
        ("ci", "android-webview-13-x64-dbg-tests"),
    ],
    ("ci", "Mac Builder (dbg)"): [
        ("ci", "mac-osxbeta-rel"),
    ],
    ("ci", "Win x64 Builder"): [
        ("ci", "win-network-sandbox-tester"),
    ],
    ("ci", "android-12-x64-rel"): [
        ("ci", "android-12-x64-fyi-rel"),
    ],
    ("ci", "mac-arm64-rel"): [
        ("ci", "mac-fieldtrial-tester"),
    ],
}

def _check_trigger_builder_groups(ctx):
    cfg = ctx.output["luci/cr-buildbucket.cfg"]

    builder_group_by_builder = {}

    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        for builder in bucket.swarming.builders:
            builder_group = json.decode(builder.properties).get("builder_group")
            if builder_group != None:
                builder_group_by_builder[(bucket.name, builder.name)] = builder_group

    # Traverse the graph for triggering. If builder X triggers Y, the nodes will
    # be: BUILDER(X) -> TRIGGERER(X) -> BUILDER_REF(Y) -> BUILDER(Y)
    # The TRIGGERER nodes abstract things that can trigger (builders or pollers)
    # The BUILDER_REF nodes abstract out referring to a builder by simple name
    # (e.g. mac-rel) or bucket-qualified name (e.g. try/mac-rel)
    bad_triggers = []
    for (bucket_name, builder_name), builder_group in builder_group_by_builder.items():
        builder_node = graph.node(keys.builder(bucket_name, builder_name))
        legacy_triggers = _LEGACY_CROSS_BUILDER_GROUP_TRIGGERS.get((bucket_name, builder_name), [])
        for n in triggerer.targets(builder_node):
            child_bucket_name = n.key.container.id
            child_builder_name = n.key.id
            child_group = builder_group_by_builder.get((child_bucket_name, child_builder_name))
            if child_group == None:
                fail("A builder without a group is being triggered: {}/{} ({}) -> {}/{}"
                    .format(bucket_name, builder_name, builder_group, child_bucket_name, child_builder_name))
            if child_group == builder_group:
                continue
            if (child_bucket_name, child_builder_name) in legacy_triggers:
                continue

            bad_triggers.append("* {}/{} ({}) -> {}/{} ({})"
                .format(bucket_name, builder_name, builder_group, child_bucket_name, child_builder_name, child_group))

    if bad_triggers:
        fail("The following triggers cross builder groups:\n{}".format("\n".join(sorted(bad_triggers))))

lucicfg.generator(_check_trigger_builder_groups)
