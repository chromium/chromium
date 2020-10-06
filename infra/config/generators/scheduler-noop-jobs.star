# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generator for creating no-op scheduler jobs.

The triggering relationship is actually described in the configuration defined
in the recipes, which is shared between the versions of a builder for different
milestones. We don't always want to trigger the same set of builders on all of
the branches, so we create no-op jobs for the milestones where the builder is
not defined so that the recipe can issue a trigger for the non-existent builder
without error.
"""

# Don't make a habit of this - it isn't public API
load("@stdlib//internal/luci/proto.star", "scheduler_pb")
load("//project.star", "settings")

# For the chromium project (settings.is_master is True), we have bucket-based
# milestones for <=M85. We create a no-op job that prefixes the ci bucket name
# for those milestones. Combined with setting the bucketed_triggers property,
# this makes it safe to issue triggers for the builders that don't exist for the
# milestone.
# For the chromium milestone projects (settings.is_master is False), the
# milestone project will use the same bucket names, so we create a no-op job for
# the 'ci' bucket.
_BRANCH_NOOP_CONFIG = struct(
    buckets = ["ci-m85"],
    fmt = "{bucket}-{builder}",
) if settings.is_master else struct(
    buckets = ["ci"],
    fmt = "{builder}",
)

_NON_BRANCHED_TESTERS = (
    # This tester is triggered by 'Mac Builder', but it is an FYI builder and
    # not mirrored by any branched try builders, so we do not need to run it on
    # the branches
    "mac-osxbeta-rel",

    # This tester is triggered by 'mac-arm64-rel', but it is an FYI builder and
    # not mirrored by any branched try builders and we have limited test
    # capacity, so we do not need to run it on the branches
    "mac-arm64-rel-tests",

    # These testers are triggered by 'Win x64 Builder', but it is an FYI builder
    # and not mirrored by any branched try builders, so we do not need to run it
    # on the branches (crbug/990885)
    "Win10 Tests x64 1803",
    "Win10 Tests x64 1909",

    # These Android testers are triggered by 'Android arm Builder (dbg)', but we
    # don't have sufficient capacity of devices with older Android versions, so
    # we do not run them on the branches
    "Android WebView L (dbg)",
    "Lollipop Phone Tester",
    "Lollipop Tablet Tester",
    "Marshmallow Tablet Tester",
)

_TESTER_NOOP_JOBS = [scheduler_pb.Job(
    id = _BRANCH_NOOP_CONFIG.fmt.format(bucket = bucket, builder = builder),
    schedule = "triggered",
    acl_sets = [bucket],
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    )],
    noop = scheduler_pb.NoopTask(),
) for builder in _NON_BRANCHED_TESTERS for bucket in _BRANCH_NOOP_CONFIG.buckets]

def _add_noop_jobs(ctx):
    cfg = ctx.output["luci-scheduler.cfg"]
    for j in _TESTER_NOOP_JOBS:
        cfg.job.append(j)

lucicfg.generator(_add_noop_jobs)
