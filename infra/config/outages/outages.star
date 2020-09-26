# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("./config.star", "config")
load("@stdlib//internal/luci/proto.star", "cq_pb")

def _disable_cq_experiments(ctx):
    if not config.disable_cq_experiments:
        return

    for c in ctx.output["commit-queue.cfg"].config_groups:
        if c.verifiers.tryjob == cq_pb.Verifiers.Tryjob():
            # Accessing the tryjob field where it wasn't set causes it to be set
            # to an empty message and added to the output, setting to None
            # prevents the change to the output
            c.verifiers.tryjob = None
            continue
        for b in c.verifiers.tryjob.builders:
            if not b.experiment_percentage:
                continue
            project, bucket, builder = b.name.split("/", 2)
            if project == "chromium" and bucket in ("try", "try-m85"):
                b.includable_only = True
                b.experiment_percentage = 0
                b.location_regexp.clear()
                b.location_regexp_exclude.clear()

lucicfg.generator(_disable_cq_experiments)
