# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/structs.star", "structs")
load("//console-header.star", "HEADER")

luci.bucket(
    name = "reclient",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-ci-schedulers",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
)

ci.defaults.set(
    bucket = "reclient",
    executable = "recipe:chromium",
    triggered_by = ["chromium-gitiles-trigger"],
    builder_group = "chromium.reclient.fyi",
    pool = "luci.chromium.ci",
    cores = 8,
    cpu = cpu.X86_64,
    free_space = builders.free_space.standard,
    build_numbers = True,
    execution_timeout = 3 * time.hour,
    goma_backend = None,
    service_account = (
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
    ),
)

consoles.console_view(
    name = "chromium.reclient.fyi",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    include_experimental_builds = True,
)

def fyi_reclient_staging_builder(
        *,
        name,
        console_view_category,
        reclient_instance = "rbe-chromium-%s",
        untrusted_service_account = (
            "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com"
        ),
        reclient_version = "staging",
        reclient_scandeps_server = False,
        **kwargs):
    trusted_instance = reclient_instance % "trusted"
    unstrusted_instance = reclient_instance % "untrusted"
    reclient_bootstrap_env = kwargs.pop("reclient_bootstrap_env", {})

    # Use goma deps cache with scan deps server
    if not reclient_scandeps_server:
        # TODO(b/233275188) remove once reproxy 0.83.0 is rolled out
        reclient_bootstrap_env.update({
            "RBE_experimental_goma_deps_cache": "True",
            "RBE_ip_reset_min_delay": "-1s",
            "RBE_deps_cache_mode": "reproxy",
        })

    reclient_bootstrap_env.update({
        # TODO(b/258210757) remove once long term breakpad plans are dertermined
        "GOMA_COMPILER_PROXY_ENABLE_CRASH_DUMP": "true" if reclient_scandeps_server else "false",
    })
    return [
        ci.builder(
            name = name,
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, trusted_instance),
            reclient_instance = trusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbe|" + console_view_category,
                short_name = "rcs",
            ),
            reclient_bootstrap_env = reclient_bootstrap_env,
            reclient_scandeps_server = reclient_scandeps_server,
            **kwargs
        ),
        ci.builder(
            name = name + " untrusted",
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, unstrusted_instance),
            reclient_instance = unstrusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbecq|" + console_view_category,
                short_name = "rcs",
            ),
            service_account = untrusted_service_account,
            reclient_bootstrap_env = reclient_bootstrap_env,
            reclient_scandeps_server = reclient_scandeps_server,
            **kwargs
        ),
    ]

def fyi_reclient_test_builder(
        *,
        name,
        console_view_category,
        **kwargs):
    return fyi_reclient_staging_builder(
        name = name,
        console_view_category = console_view_category,
        reclient_instance = "rbe-chromium-%s-test",
        reclient_version = "test",
        untrusted_service_account = ci.DEFAULT_SERVICE_ACCOUNT,
        **kwargs
    )

fyi_reclient_staging_builder(
    name = "Linux Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "Linux Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
    reclient_scandeps_server = True,
)

fyi_reclient_staging_builder(
    name = "Mac Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
)

fyi_reclient_test_builder(
    name = "Mac Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_profiler_service = "reclient-mac",
)

fyi_reclient_test_builder(
    name = "Mac Builder reclient scandeps test",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_profiler_service = "reclient-mac",
    reclient_scandeps_server = True,
)

fyi_reclient_staging_builder(
    name = "Win x64 Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_category = "win",
    execution_timeout = 5 * time.hour,
)

fyi_reclient_test_builder(
    name = "Win x64 Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_category = "win",
    execution_timeout = 5 * time.hour,
)

fyi_reclient_staging_builder(
    name = "Simple Chrome Builder reclient staging",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_staging", "checkout_lacros_sdk"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["amd64-generic"],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "Simple Chrome Builder reclient test",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_test", "checkout_lacros_sdk"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["amd64-generic"],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "ios-simulator reclient test",
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "ios",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    xcode = xcode.x14main,
)

fyi_reclient_test_builder(
    name = "ios-simulator reclient scandeps test",
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "ios",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_scandeps_server = True,
    xcode = xcode.x14main,
)

fyi_reclient_staging_builder(
    name = "ios-simulator reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "ios",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    xcode = xcode.x14main,
)

fyi_reclient_staging_builder(
    name = "mac-arm64-rel reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/mac-arm64-rel",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
)

fyi_reclient_test_builder(
    name = "mac-arm64-rel reclient test",
    builder_spec = builder_config.copy_from(
        "ci/mac-arm64-rel",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
)

fyi_reclient_test_builder(
    name = "mac-arm64-rel reclient scandeps test",
    builder_spec = builder_config.copy_from(
        "ci/mac-arm64-rel",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_scandeps_server = True,
)

ci.builder(
    name = "Comparison Linux (reclient vs reclient remote links)",
    executable = "recipe:reclient_reclient_comparison",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_clang_depscan_archive": "true",
    },
    reclient_cache_silo = "Comparison Linux remote links - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Comparison Linux (reclient vs reclient remote links)(small)",
    executable = "recipe:reclient_reclient_comparison",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_clang_depscan_archive": "true",
        "RBE_use_unified_uploads": "false",
        "RBE_experimental_sysroot_do_not_upload": "true",
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
    },
    reclient_cache_silo = "Comparison Linux remote links - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

# The following 2 builders use the untrusted RBE instance because each instance has its own
# rewrapper configs and the trusted instance uses native windows rewrapper configs but the
# untrusted instance uses cross compile windows rewrapper configs.
# TODO(b/260228493) Remove once CI backend is switched
ci.builder(
    name = "Windows Cross deterministic",
    description_html = "verify artifacts. should be removed after the migration. b/260228493",
    executable = "recipe:swarming/deterministic_build",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "detcross",
    ),
    execution_timeout = 12 * time.hour,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com",
)

# TODO(b/260228493) Remove once CI backend is switched
ci.builder(
    name = "Win x64 Cross Builder (reclient compare)",
    description_html = "verify artifacts. should be removed after the migration. b/260228493",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage", "reclient_test"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "compcross",
    ),
    execution_timeout = 12 * time.hour,
    reclient_disable_bq_upload = True,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    service_account = "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com",
)
