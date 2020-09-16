# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "defaults", "goma", "os")
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

# FindIt builders use a separate pool with a dedicated set of permissions.
swarming_lib.pool_realm(name = "pools/findit")

# Allow FindIt admins to run tasks directly to debug issues.
swarming_lib.task_triggerers(
    builder_realm = "findit",
    pool_realm = "pools/findit",
    groups = ["project-findit-owners"],
)

defaults.auto_builder_dimension.set(False)
defaults.bucket.set("findit")
defaults.build_numbers.set(True)
defaults.builderless.set(True)
defaults.ssd.set(True)
defaults.configure_kitchen.set(True)
defaults.execution_timeout.set(8 * time.hour)
defaults.pool.set("luci.chromium.findit")
defaults.service_account.set("findit-builder@chops-service-accounts.iam.gserviceaccount.com")
defaults.swarming_tags.set(["vpython:native-python-wrapper"])

defaults.caches.set([
    swarming.cache(
        name = "win_toolchain",
        path = "win_toolchain",
    ),
])

# Builders are defined in lexicographic order by name

# Same as findit_variable, except now with a specified recipe, as this is no
# longer overridable with Buildbucket V2
builder(
    name = "findit-rerun",
    executable = "recipe:findit/chromium/single_revision",
    goma_backend = goma.backend.RBE_PROD,
)

# Dimensionless trybot for findit.
#
# Findit will add appropriate dimensions and properties as needed based on
# the waterfall builder being analyzed.
#
# TODO(robertocn): Remove _variable trybot builders from "try" bucket
#   after they have been configured to use this generic builder, as well as
#   the findit 'mixin'.
builder(
    name = "findit_variable",
    # Findit app specifies these for each build it schedules. The reason why
    # we specify them here is to pass validation of the buildbucket config.
    # Also, to illustrate the typical use case of this bucket.
    executable = "recipe:findit/chromium/compile",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "linux_chromium_bot_db_exporter",
    executable = "recipe:findit/chromium/export_bot_db",
    os = os.LINUX_DEFAULT,
    properties = {
        "gs_bucket": "findit-for-me",
        "gs_object": "bot_db.json",
    },
    schedule = "0 0,6,12,18 * * *",
)
