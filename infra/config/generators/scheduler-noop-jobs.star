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
load("@stdlib//internal/luci/proto.star", "realms_pb", "scheduler_pb")
load("//lib/branches.star", "branches")
load("//project.star", "settings")

_NON_BRANCHED_TESTERS = {
    # This tester is triggered by 'Win x64 Builder', but it is an FYI builder
    # and not mirrored by any branched try builders, so we do not need to run it
    # on the branches
    "Win11 Tests x64": branches.DESKTOP_EXTENDED_STABLE_MILESTONE,

    # These Android testers are triggered by 'Android arm Builder (dbg)', but we
    # don't have sufficient capacity of devices with older Android versions, so
    # we do not run them on the branches
    "Marshmallow Tablet Tester": branches.STANDARD_MILESTONE,

    # These Android testers are triggered by 'Android x64 Builder (dbg)', but
    # they are FYI testers so we do not run them on the branches
    "android-12-x64-dbg-tests": branches.STANDARD_MILESTONE,
    "android-webview-12-x64-dbg-tests": branches.STANDARD_MILESTONE,
}

_TESTER_NOOP_JOBS = [scheduler_pb.Job(
    id = builder,
    schedule = "triggered",
    acl_sets = ["ci"],
    realm = "ci",
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    )],
    noop = scheduler_pb.NoopTask(),
) for builder, selector in _NON_BRANCHED_TESTERS.items() if branches.matches(selector)]

def _add_noop_jobs(ctx):
    if settings.is_main:
        return
    cfg = ctx.output["luci/luci-scheduler.cfg"]
    for j in _TESTER_NOOP_JOBS:
        cfg.job.append(j)
    for realm in ctx.output["luci/realms.cfg"].realms:
        if realm.name == "ci":
            realm.bindings.append(realms_pb.Binding(
                role = "role/scheduler.triggerer",
                principals = [
                    "user:chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
                ],
                conditions = [
                    realms_pb.Condition(
                        restrict = realms_pb.Condition.AttributeRestriction(
                            attribute = "scheduler.job.name",
                            values = sorted(set([j.id for j in _TESTER_NOOP_JOBS])),
                        ),
                    ),
                ],
            ))
            break

lucicfg.generator(_add_noop_jobs)
