# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os")
load("//lib/builder_config.star", "builder_config")

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
            groups = "project-chromium-admins",
        ),
    ],
)

luci.gitiles_poller(
    name = "chromium-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
)

luci.recipe.defaults.cipd_package.set(
    "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
)

luci.recipe.defaults.use_bbagent.set(True)

defaults.bucket.set("ci")
defaults.build_numbers.set(True)
defaults.builder_group.set("chromium.dev")
defaults.builderless.set(None)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set("recipe:swarming/staging")
defaults.execution_timeout.set(3 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set(
    "chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com",
)

def ci_builder(*, name, resultdb_bigquery_exports = None, **kwargs):
    resultdb_bigquery_exports = resultdb_bigquery_exports or []
    resultdb_bigquery_exports.extend([
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium_staging.ci_test_results",
        ),
        resultdb.export_text_artifacts(
            bq_table = "chrome-luci-data.chromium_staging.ci_text_artifacts",
        ),
    ])
    return builder(
        name = name,
        triggered_by = ["chromium-gitiles-trigger"],
        resultdb_bigquery_exports = resultdb_bigquery_exports,
        goma_backend = goma.backend.RBE_PROD,
        resultdb_index_by_timestamp = True,
        **kwargs
    )

ci_builder(
    name = "android-marshmallow-arm64-rel-swarming",
)

ci_builder(
    name = "linux-rel-swarming",
    description_html = "Test description. <b>Test HTML</b>.",
)

ci_builder(
    name = "linux-ssd-rel-swarming",
    description_html = "Ensures builders are using available local SSDs",
    builderless = False,
)

ci_builder(
    name = "mac-rel-swarming",
    os = os.MAC_DEFAULT,
)

ci_builder(
    name = "mac-arm-rel-swarming",
    cpu = cpu.ARM64,
    os = os.MAC_DEFAULT,
)

ci_builder(
    name = "win-rel-swarming",
    os = os.WINDOWS_10,
    goma_enable_ats = True,
)

ci_builder(
    name = "win11-rel-swarming",
    os = os.WINDOWS_11,
    goma_enable_ats = True,
)
