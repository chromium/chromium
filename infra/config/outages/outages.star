# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("./config.star", "DEFAULT_CONFIG", "config")
load("@stdlib//internal/luci/proto.star", "cq_pb")

def _generate_outages_file(ctx):
    config_to_write = {}
    for a in dir(config):
        value = getattr(config, a)
        if value != getattr(DEFAULT_CONFIG, a):
            config_to_write[a] = value
    ctx.output["outages.pyl"] = "\n".join([
        "# This is a non-LUCI generated file",
        "# This details the current configuration modifications for outages settings",
        repr(config_to_write),
        "",
    ])

lucicfg.generator(_generate_outages_file)

def _disable_cq_experiments(ctx):
    if not config.disable_cq_experiments:
        return

    for c in ctx.output["luci/commit-queue.cfg"].config_groups:
        if c.verifiers.tryjob == cq_pb.Verifiers.Tryjob():
            # Accessing the tryjob field where it wasn't set causes it to be set
            # to an empty message and added to the output
            c.verifiers.tryjob = None
            continue
        for b in c.verifiers.tryjob.builders:
            if not b.experiment_percentage:
                continue
            project, bucket, _ = b.name.split("/", 2)
            if project == "chromium" and bucket == "try":
                b.includable_only = True
                b.experiment_percentage = 0
                b.location_filters.clear()

lucicfg.generator(_disable_cq_experiments)
