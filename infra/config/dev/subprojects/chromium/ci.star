# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os")

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                "luci-scheduler-dev@appspot.gserviceaccount.com",
                "chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "google/luci-task-force@google.com",
        ),
    ],
)

luci.gitiles_poller(
    name = "master-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
)

luci.recipe.defaults.cipd_package.set(
    "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
)

defaults.bucket.set("ci")
defaults.build_numbers.set(True)
defaults.builder_group.set("chromium.dev")
defaults.builderless.set(None)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = "swarming/staging"))
defaults.execution_timeout.set(3 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set(
    "chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com",
)
defaults.swarming_tags.set(["vpython:native-python-wrapper"])

def ci_builder(*, name, **kwargs):
    return builder(
        name = name,
        triggered_by = ["master-gitiles-trigger"],
        resultdb_bigquery_exports = [resultdb.export_test_results(
            bq_table = "luci-resultdb-dev.chromium.ci_test_results",
        )],
        isolated_server = "https://isolateserver-dev.appspot.com",
        goma_backend = goma.backend.RBE_PROD,
        **kwargs
    )

ci_builder(
    name = "android-lollipop-arm-rel-swarming",
)

ci_builder(
    name = "android-marshmallow-arm64-rel-swarming",
)

ci_builder(
    name = "linux-rel-swarming",
    description_html = "Test description. <b>Test HTML</b>.",
)

ci_builder(
    name = "mac-rel-swarming",
    os = os.MAC_DEFAULT,
)

ci_builder(
    name = "win-rel-swarming",
    os = os.WINDOWS_DEFAULT,
)

## builders using swarming staging instance

def ci_builder_staging(**kwargs):
    return ci_builder(
        swarming_host = "chromium-swarm-staging.appspot.com",
        **kwargs
    )

ci_builder_staging(
    name = "linux-rel-swarming-staging",
)

ci_builder_staging(
    name = "win-rel-swarming-staging",
    os = os.WINDOWS_DEFAULT,
)
