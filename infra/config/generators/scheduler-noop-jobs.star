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

_NON_BRANCHED_TESTERS = (
    # This tester is triggered by 'Mac Builder', but it is an FYI builder and
    # not mirrored by any branched try builders, so we do not need to run it on
    # the branches
    "mac-osxbeta-rel",

    # This tester is also triggered by 'Mac Builder', but we don't have enough
    # capacity on Mac 11 to run this on the branches yet.
    # TODO(crbug.com/1206401): Restore this when we do have capacity.
    "Mac11 Tests",

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
    id = builder,
    schedule = "triggered",
    acl_sets = ["ci"],
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    )],
    noop = scheduler_pb.NoopTask(),
) for builder in _NON_BRANCHED_TESTERS]

def _add_noop_jobs(ctx):
    if settings.is_main:
        return
    cfg = ctx.output["luci-scheduler.cfg"]
    for j in _TESTER_NOOP_JOBS:
        cfg.job.append(j)

lucicfg.generator(_add_noop_jobs)
