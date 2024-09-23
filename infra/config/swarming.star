# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Permissions for Chromium main swarming pools (CI, try, tests).

They are actually shared with a bunch other projects.
"""

load("//lib/swarming.star", "swarming")
load("//project.star", "ACTIVE_MILESTONES")

# Set up permissions that apply to all Chromium pools.
swarming.root_permissions()

# Task accounts for isolated tests.
#
# For simplicity of configuration we allow *any* task in the project (in any
# realm) to run as any of these accounts. This is fine since all CI and Try
# builders trigger isolated tasks in an identical way, using identical accounts
# for isolated tests anyway.
#
# Note that this is declared on all branches, since task accounts "live" in a
# project they are defined in, so we need to declare them for per-milestone
# projects as well.
swarming.task_accounts(
    realm = "@root",  # i.e. inherit by all realms
    groups = [
        "project-chromium-test-task-accounts",
    ],
    users = [
        # TODO(crbug.com/40554235): Migrate uses of this account to a dedicated
        # public test task account that's part of the group above, then delete
        # this.
        "ios-isolated-tester@chops-service-accounts.iam.gserviceaccount.com",
    ],
)

# LED users that can trigger tasks in *any* realm in *any* pool.
#
# This should be used relatively sparingly. Prefer to configure the permissions
# more precisely. E.g. see "chromium-led-users" below.
swarming.task_triggerers(
    builder_realm = "@root",
    pool_realm = "@root",
    groups = [
        "mdb/chrome-browser-infra",
    ],
)

# Realm with bots that run CI builds (aka main waterfall bots).
#
# The tasks here are triggered via Buildbucket (which authenticates as
# "project:<project that defines the bucket>"), so we enumerate projects
# (besides "project:chromium" itself) that are allowed to use Chromium CI pools
# in their Buildbucket configs (which are currently only per-milestone Chromium
# projects).
swarming.pool_realm(
    name = "pools/ci",
    user_projects = [details.project for details in ACTIVE_MILESTONES.values()],
    owner_groups = [
        "mdb/chrome-infra-eng",
    ],
)

swarming.task_triggerers(
    builder_realm = "ci",
    pool_realm = "pools/ci",
    groups = [
        "mdb/chrome-build-access-sphinx",
    ],
    users = [
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",

        # Used by Findit to re-run swarming tasks for bisection purposes.
        "findit-for-me@appspot.gserviceaccount.com",
    ],
)

# Realm with bots that run try builds.
#
# The tasks here are also triggered via Buildbucket. See comment above.
swarming.pool_realm(
    name = "pools/try",
    user_projects = [details.project for details in ACTIVE_MILESTONES.values()],
    owner_groups = [
        "mdb/chrome-infra-eng",
    ],
)

# LED users that can trigger try builds via LED.
swarming.task_triggerers(
    builder_realm = "try",
    pool_realm = "pools/try",
    groups = [
        "mdb/chrome-build-access-sphinx",
        # Prefer the above sphinx group for led access. But if folks outside
        # Chrome need access, can add them to chromium-led-users.
        "chromium-led-users",
    ],
    users = [
        # Build Recipes Tester launches orchestrator led builds which needs to
        # trigger compilator led builds
        "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
        # An account used by "Build Recipes Tester" builder infra/try bucket
        # used to tests changes to Chromium recipes using LED before commit.
        "infra-try-recipes-tester@chops-service-accounts.iam.gserviceaccount.com",
    ],
)

# Realm with bots that run isolated tests.
#
# Tasks here are triggered directly on Swarming (not via Buildbucket) by various
# CI and Try builder (not only Chromium ones!) and also directly by users.
swarming.pool_realm(
    name = "pools/tests",
    user_groups = [
        # Various Chromium CI and Try LUCI builders that trigger isolated tests.
        "project-chromium-ci-task-accounts",
        "project-chromium-findit-task-accounts",
        "project-chromium-try-task-accounts",

        # DevTools uses Chrome pools for Layout tests.
        "project-devtools-frontend-ci-task-accounts",
        "project-devtools-frontend-try-task-accounts",

        # V8 are reusing Chrome pools for isolated tests too.
        "project-v8-ci-task-accounts",
        "project-v8-try-task-accounts",

        # ... and WebRTC.
        "project-webrtc-ci-task-accounts",
        "project-webrtc-try-task-accounts",

        # ... and Angle.
        "project-angle-ci-task-accounts",
        "project-angle-try-task-accounts",

        # Used by Pinpoint to trigger bisect jobs on machines in the Chrome-GPU pool.
        "service-account-chromeperf",
    ],
    user_users = [
        # Skia uses this pool directly.
        "skia-external-ct-skps@skia-swarming-bots.iam.gserviceaccount.com",
        # TODO(borenet): Remove the below after we're fully switched to Kitchen.
        "chromium-swarm-bots@skia-swarming-bots.iam.gserviceaccount.com",
    ],
    owner_groups = [
        "mdb/chrome-infra-eng",
    ],
)

# Anyone with Chromium tryjob access can use isolate testers pool directly.
#
# We assume isolated tests triggered from workstation go to the "try" realm,
# just like tasks triggered by try jobs.
swarming.task_triggerers(
    builder_realm = "try",
    pool_realm = "pools/tests",
    groups = ["project-chromium-tryjob-access"],
)
