# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//project.star", "settings")

fallback_cq = struct(
    GROUP = "fallback-cq",
)

# TODO(crbug.com/41460531) Run a generator to set the fallback field for
# the empty CQ group until it's exposed in lucicfg or there is a better way to
# create a CQ group for all of the canary branches
def _generate_cq_group_fallback(ctx):
    if not settings.is_main:
        return

    cq_cfg = ctx.output["luci/commit-queue.cfg"]

    for c in cq_cfg.config_groups:
        if c.name == fallback_cq.GROUP:
            c.fallback = 1  # YES
            return

    fail("Could not find fallback CQ group")

lucicfg.generator(_generate_cq_group_fallback)
