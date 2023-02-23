# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "defaults", "goma", "reclient")
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

defaults.auto_builder_dimension.set(False)
defaults.bucket.set("findit")
defaults.build_numbers.set(True)
defaults.builderless.set(True)
defaults.list_view.set("findit")
defaults.ssd.set(True)
defaults.execution_timeout.set(8 * time.hour)
defaults.pool.set("luci.chromium.findit")
defaults.service_account.set("findit-builder@chops-service-accounts.iam.gserviceaccount.com")

# Builders are defined in lexicographic order by name

# Same as findit_variable, except now with a specified recipe, as this is no
# longer overridable with Buildbucket V2
builder(
    name = "findit-rerun",
    executable = "recipe:findit/chromium/single_revision",
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

# GoFindit builder to verify a culprit (go/gofindit-design-doc)
builder(
    name = "gofindit-culprit-verification",
    executable = "recipe:gofindit/chromium/single_revision",
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)
