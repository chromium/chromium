# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "defaults", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/swarming.star", swarming_lib = "swarming")

luci.bucket(
    name = "findit",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "googlers",
            users = "findit-builder@chops-service-accounts.iam.gserviceaccount.com",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "findit-tryjob-access",
            users = "luci-scheduler@appspot.gserviceaccount.com",
        ),
    ],
)

# Define the shadow bucket of `findit`.
luci.bucket(
    name = "findit.shadow",
    shadows = "findit",
    # Only the builds with allowed pool and service account can be created
    # in this bucket.
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.findit"],
        service_accounts = ["findit-builder@chops-service-accounts.iam.gserviceaccount.com"],
    ),
    bindings = [
        # for led permissions.
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "project-findit-owners",
        ),
    ],
    dynamic = True,
)

consoles.list_view(
    name = "findit",
)

# FindIt builders use a separate pool with a dedicated set of permissions.
swarming_lib.pool_realm(name = "pools/findit")

# Allow FindIt admins to run tasks directly to debug issues.
swarming_lib.task_triggerers(
    builder_realm = "findit",
    pool_realm = "pools/findit",
    groups = ["project-findit-owners"],
)

defaults.set(
    bucket = "findit",
    pool = "luci.chromium.findit",
    builderless = True,
    ssd = True,
    list_view = "findit",
    auto_builder_dimension = False,
    build_numbers = True,
    execution_timeout = 8 * time.hour,
    service_account = "findit-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_enabled = True,
)

# Builders are defined in lexicographic order by name

# LUCI Bisection builder to verify a culprit (go/luci-bisection-design-doc).
builder(
    name = "gofindit-culprit-verification",
    executable = "recipe:gofindit/chromium/single_revision",
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

# Builder to run a test for a single revision.
builder(
    name = "test-single-revision",
    executable = "recipe:gofindit/chromium/test_single_revision",
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)
