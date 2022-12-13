# Copyright 2020 The Chromium Authors
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

luci.bucket(
    name = "ci.shadow",
    shadows = "ci",
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.ci"],
        service_accounts = ["chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com"],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "mdb/chrome-troopers",
        ),
    ],
    dynamic = True,
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
    name = "android-pie-arm64-rel-swarming",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
    ),
)

ci_builder(
    name = "linux-rel-swarming",
    description_html = "Test description. <b>Test HTML</b>.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
)

ci_builder(
    name = "linux-ssd-rel-swarming",
    description_html = "Ensures builders are using available local SSDs",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    builderless = False,
)

ci_builder(
    name = "mac-rel-swarming",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    os = os.MAC_DEFAULT,
)

ci_builder(
    name = "mac-arm-rel-swarming",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

ci_builder(
    name = "win-rel-swarming",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    os = os.WINDOWS_10,
    goma_enable_ats = True,
)

ci_builder(
    name = "win11-rel-swarming",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    os = os.WINDOWS_11,
    goma_enable_ats = True,
)
