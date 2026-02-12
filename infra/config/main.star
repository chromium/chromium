#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

"""Entrypoint for `lucicfg generate infra/config/main.star`."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//chromium_luci.star", "chromium_luci")
load("@chromium-luci//targets.star", "targets")
load(
    "//lib/builder_exemptions.star",
    "exempted_from_contact_builders",
    "exempted_from_description_builders",
    "exempted_gardened_mirrors_in_cq_builders",
    "mega_cq_excluded_builders",
    "mega_cq_excluded_gardener_rotations",
    "standalone_trybot_excluded_builder_groups",
    "standalone_trybot_excluded_builders",
)
load("//project.star", "settings")

lucicfg.check_version(
    min = "1.44.1",
    message = "Update depot_tools",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "builders/*/*/*",
        "builders/*/*/*/*",
        "builders/alerting-builders.txt",
        "builders/gn_args_locations.json",
        "builder-owners/*.txt",
        "cq-builders.md",
        "cq-usage/cq-tests.md",
        "cq-usage/default.cfg",
        "cq-usage/full.cfg",
        "cq-usage/mega_cq_bots.txt",
        "health-specs/health-specs.json",
        "luci/commit-queue.cfg",
        "luci/cr-buildbucket.cfg",
        "luci/luci-analysis.cfg",
        "luci/luci-bisection.cfg",
        "luci/luci-logdog.cfg",
        "luci/luci-milo.cfg",
        "luci/luci-notify.cfg",
        "luci/luci-notify/email-templates/*.template",
        "luci/luci-scheduler.cfg",
        "luci/project.cfg",
        "luci/realms.cfg",
        "luci/testhaus.cfg",
        "outages.pyl",
        "sheriff-rotations/*.txt",
        "project.pyl",
        "testing/*.pyl",
    ],
    fail_on_warnings = True,
)

# Just copy Testhaus config to generated outputs.
lucicfg.emit(
    dest = "luci/testhaus.cfg",
    data = io.read_file("testhaus.cfg"),
)

# Just copy LUCI Analysis config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-analysis.cfg",
    data = io.read_file("luci-analysis.cfg"),
)

# Just copy LUCI Bisection config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-bisection.cfg",
    data = io.read_file("luci-bisection.cfg"),
)

luci.project(
    name = settings.project,
    config_dir = "luci",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            groups = [
                "project-chromium-try-task-accounts",
                "project-chromium-ci-task-accounts",
            ],
        ),
        # Roles for LUCI Analysis.
        luci.binding(
            roles = "role/analysis.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/analysis.queryUser",
            groups = "authenticated-users",
        ),
        luci.binding(
            roles = "role/analysis.editor",
            groups = ["project-chromium-committers", "googlers"],
        ),
        # Role for builder health indicators
        luci.binding(
            roles = "role/buildbucket.healthUpdater",
            users = ["generate-builder@cr-builder-health-indicators.iam.gserviceaccount.com"],
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = "chromium-cq-status.appspot.com",
    honor_gerrit_linked_accounts = True,
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg",
)

luci.notify(
    tree_closing_enabled = True,
)

chromium_luci.configure_project(
    name = settings.project,
    ref = settings.ref,
    is_main = settings.is_main,
    platforms = settings.platforms,
    experiments = [
        "targets.generate_pyl_entry_default_off",
        "targets.module_name_without_slash",
        "targets.module_scheme_generator",
        "targets.module_scheme_junit_tests",
        "targets.module_scheme_regex",
        "targets.module_scheme_script_tests",
    ],
)

chromium_luci.configure_per_builder_outputs(
    root_dir = "builders",
)

chromium_luci.configure_builder_config(
    exempted_gardened_mirrors_in_cq_builders = exempted_gardened_mirrors_in_cq_builders,
    mega_cq_excluded_builders = mega_cq_excluded_builders,
    mega_cq_excluded_gardener_rotations = mega_cq_excluded_gardener_rotations,
    standalone_trybot_excluded_builder_groups = standalone_trybot_excluded_builder_groups,
    standalone_trybot_excluded_builders = standalone_trybot_excluded_builders,
    cq_groups_to_generate_test_coverage_files = {
        "cq": "cq-usage/cq-tests.md",
    },
)

chromium_luci.configure_builder_health_indicators(
    unhealthy_period_days = 7,
    pending_time_p50_min = 20,
    exempted_from_contact_builders = exempted_from_contact_builders,
)

chromium_luci.configure_builders(
    enable_alerts_configuration = True,
    os_dimension_overrides = {
        os.LINUX_DEFAULT: chromium_luci.os_dimension_overrides(
            default = os.LINUX_JAMMY,
            overrides = json.decode(io.read_file("//lib/linux-default.json")),
        ),
        os.MAC_DEFAULT: "Mac-15|Mac-26",
        os.MAC_BETA: "Mac-26",
        os.WINDOWS_DEFAULT: os.WINDOWS_10,
    },
    exempted_from_description_builders = exempted_from_description_builders,
)

chromium_luci.configure_ci(
    main_console_view = "main" if not settings.is_main else None,
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
)

chromium_luci.configure_gardener_rotations(
    rotation_files_path = "sheriff-rotations",
)

chromium_luci.configure_targets(
    generate_pyl_files = chromium_luci.pyl_generation_configuration(
        mixin_overrides = {
            # The following mixins are referenced via
            # //testing/buildbot/waterfalls.pyl
            "11-x86-emulator": True,
            "amd_radeon_rx_5500_xt": True,
            "android": True,
            "blink_tests_write_run_histories": True,
            "chrome-finch-swarming-pool": True,
            "chrome-flame-fleet-pool": True,
            "chrome-intelligence-swarming-pool": True,
            "chrome-swarming-pool": True,
            "chrome-tester-service-account": True,
            "chromeos-betty-finch": True,
            "crosier-no-arc": True,
            "experiments": True,
            "gce": True,
            "gpu_enable_metal_debug_layers": True,
            "gpu_force_angle_d3d11": True,
            "gpu_force_angle_d3d9": True,
            "gpu_force_angle_gl": True,
            "gpu_force_angle_gles": True,
            "gpu_force_angle_metal": True,
            "gpu_force_angle_swiftshader": True,
            "gpu_force_angle_vulkan": True,
            "gpu_force_command_decoder_passthrough": True,
            "gpu_force_command_decoder_validating": True,
            "gpu_force_high_performance_gpu": True,
            "gpu_force_high_performance_gpu_for_webgl_metal": True,
            "gpu_force_skia_ganesh": True,
            "gpu_force_skia_graphite": True,
            "gpu_gtest_common_args": True,
            "gpu_integration_test_common_args": True,
            "gpu_integration_test_expected_color_args": True,
            "gpu_integration_test_pixel_args": True,
            "gpu_integration_test_screenshot_sync_args": True,
            "gpu_integration_test_webgl1_args": True,
            "gpu_integration_test_webgl2_args": True,
            "has_native_resultdb_integration": True,
            "intel_uhd_630_or_770": True,
            "ios_runtime_cache_18_2": True,
            "long_skylab_timeout": True,
            "mac_14_x64": True,
            "mac_default_arm64": True,
            "mac_default_x64": True,
            "mac_toolchain": True,
            "non-gce": True,
            "nvidia_geforce_gtx_1660": True,
            "out_dir_arg": True,
            "skia_gold_test": True,
            "skylab-20-tests-per-shard": True,
            "skylab-40-tests-per-shard": True,
            "skylab-50-tests-per-shard": True,
            "skylab-rdb-gtest": True,
            "skylab-rdb-native": True,
            "skylab-rdb-tast": True,
            "vaapi_unittest_args": True,
            "win-arm64": True,
            "win10": True,
            "win10-any": True,
            "xcode_26_main": True,
            "xctest": True,

            # The following mixins need to always be generated in mixins.pyl
            # because they are used by //content/test/gpu/find_bad_machines.py
            "chromium_nexus_5x_oreo": targets.IGNORE_UNUSED,
            "chromium_pixel_2_pie": targets.IGNORE_UNUSED,
            "chromium_pixel_2_q": targets.IGNORE_UNUSED,
            "gpu_intel_arc_140v_linux_experimental": targets.IGNORE_UNUSED,
            "gpu_nvidia_shield_tv_stable": targets.IGNORE_UNUSED,
            "gpu_pixel_10_stable": targets.IGNORE_UNUSED,
            "gpu_pixel_4_stable": targets.IGNORE_UNUSED,
            "gpu_pixel_6_experimental": targets.IGNORE_UNUSED,
            "gpu_pixel_6_stable": targets.IGNORE_UNUSED,
            "gpu_samsung_a13_stable": targets.IGNORE_UNUSED,
            "gpu_samsung_a23_stable": targets.IGNORE_UNUSED,
            "gpu_samsung_s23_stable": targets.IGNORE_UNUSED,
            "gpu_samsung_s24_stable": targets.IGNORE_UNUSED,
            "gpu_win11_intel_arc_140v_experimental": targets.IGNORE_UNUSED,
            "linux_amd_780m_experimental": targets.IGNORE_UNUSED,
            "linux_amd_890m_experimental": targets.IGNORE_UNUSED,
            "linux_amd_rx_5500_xt": targets.IGNORE_UNUSED,
            "linux_amd_rx_7600_stable": targets.IGNORE_UNUSED,
            "linux_intel_uhd_630_experimental": targets.IGNORE_UNUSED,
            "linux_intel_uhd_630_stable": targets.IGNORE_UNUSED,
            "linux_intel_uhd_770_stable": targets.IGNORE_UNUSED,
            "linux_nvidia_gtx_1660_experimental": targets.IGNORE_UNUSED,
            "linux_nvidia_gtx_1660_stable": targets.IGNORE_UNUSED,
            "linux_nvidia_rtx_4070_super_stable": targets.IGNORE_UNUSED,
            "mac_arm64_apple_m1_gpu_experimental": targets.IGNORE_UNUSED,
            "mac_arm64_apple_m1_gpu_stable": targets.IGNORE_UNUSED,
            "mac_arm64_apple_m2_retina_gpu_experimental": targets.IGNORE_UNUSED,
            "mac_arm64_apple_m2_retina_gpu_stable": targets.IGNORE_UNUSED,
            "mac_arm64_apple_m3_retina_gpu_stable": targets.IGNORE_UNUSED,
            "mac_mini_intel_gpu_experimental": targets.IGNORE_UNUSED,
            "mac_mini_intel_gpu_stable": targets.IGNORE_UNUSED,
            "mac_pro_amd_gpu": targets.IGNORE_UNUSED,
            "mac_retina_amd_555x_gpu_stable": targets.IGNORE_UNUSED,
            "mac_retina_amd_gpu_experimental": targets.IGNORE_UNUSED,
            "mac_retina_amd_gpu_stable": targets.IGNORE_UNUSED,
            "win10_intel_uhd_630_experimental": targets.IGNORE_UNUSED,
            "win10_intel_uhd_630_stable": targets.IGNORE_UNUSED,
            "win10_intel_uhd_770_stable": targets.IGNORE_UNUSED,
            "win10_nvidia_gtx_1660_experimental": targets.IGNORE_UNUSED,
            "win10_nvidia_gtx_1660_stable": targets.IGNORE_UNUSED,
            "win11_amd_780m_experimental": targets.IGNORE_UNUSED,
            "win11_amd_890m_experimental": targets.IGNORE_UNUSED,
            "win11_amd_rx_5500_xt_experimental": targets.IGNORE_UNUSED,
            "win11_amd_rx_5500_xt_stable": targets.IGNORE_UNUSED,
            "win11_amd_rx_7600_stable": targets.IGNORE_UNUSED,
            "win11_nvidia_rtx_4070_super_experimental": targets.IGNORE_UNUSED,
            "win11_nvidia_rtx_4070_super_stable": targets.IGNORE_UNUSED,
            "win11_qualcomm_adreno_690_stable": targets.IGNORE_UNUSED,
            "win11_qualcomm_snapdragon_x_elite_stable": targets.IGNORE_UNUSED,

            # The angle repo uses a script that wraps generate_buildbot_json.py
            # which consumes //testing/buildbot/mixins.pyl (via the
            # chromium/src/testing subtree repo) and adds additional mixins, the
            # following mixins need to always be generated in mixins.pyl because
            # they are used by the angle pyl files
            "chromium-tester-service-account": targets.IGNORE_UNUSED,
            "gpu_linux_gce_stable": targets.IGNORE_UNUSED,
            "gpu_win_gce_stable": targets.IGNORE_UNUSED,
            "gpu-swarming-pool": targets.IGNORE_UNUSED,
            "limited_capacity_bot": targets.IGNORE_UNUSED,
            "linux-jammy": targets.IGNORE_UNUSED,
            "no_gpu": targets.IGNORE_UNUSED,
            "no_tombstones": targets.IGNORE_UNUSED,
            "swarming_containment_auto": targets.IGNORE_UNUSED,
            "timeout_15m": targets.IGNORE_UNUSED,
            "very_limited_capacity_bot": targets.IGNORE_UNUSED,
            "win10_gce_gpu_pool": targets.IGNORE_UNUSED,
            "x86-64": targets.IGNORE_UNUSED,
        },
        variant_overrides = {
            # The following variants are referenced via
            # //testing/buildbot/waterfalls.pyl
            "AMD_RADEON_RX_5500_XT": True,
            "CHANNEL_BETA": True,
            "CHANNEL_DEV": True,
            "CHANNEL_STABLE": True,
            "CROS_GPU_BRYA_RELEASE_LKGM": True,
            "CROS_GPU_CORSOLA_RELEASE_LKGM": True,
            "CROS_GPU_SKYRIM_RELEASE_LKGM": True,
            "CROS_LKGM": True,
            "CROS_JACUZZI_RELEASE_LKGM": True,
            "INTEL_UHD_630_OR_770": True,
            "IPHONE_13": True,
            "NVIDIA_GEFORCE_GTX_1660": True,
            "SIM_IPHONE_14_18_2": True,
        },
    ),
    autoshard_exceptions_file = "//autoshard_exceptions.json",
)

chromium_luci.configure_try(
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
    additional_default_exclude_path_regexps = ["docs/.+"],
)

# An all-purpose public realm.
luci.realm(
    name = "public",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-tryjob-access",
        ),
        # Allow everyone to view Turbo CI workflows
        luci.binding(
            roles = "role/turboci.nodeReaderExternal",
            groups = "all",
        ),
        # Other roles are inherited from @root which grants them to group:all.
    ],
)

luci.realm(
    name = "ci",
    bindings = [
        # Allow CI builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.realm(
    name = "try",
    bindings = [
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = [
                "project-chromium-try-task-accounts",
                # In order to be able to reproduce test tasks that have
                # ResultDB enabled (at this point that should be all
                # tests), a realm must be provided. The ability to
                # trigger machines in the test pool is associated with
                # the try realm, so allow those who can trigger swarming
                # tasks in that pool tasks to create invocations.
                "chromium-led-users",
                "project-chromium-tryjob-access",
            ],
        ),
        # Allow everyone to view Turbo CI workflows
        luci.binding(
            roles = "role/turboci.nodeReaderExternal",
            groups = "all",
        ),
    ],
)

# Allows builders to write baselines and query ResultDB for new tests.
# TODO(crbug.com/40276195) @project is not available, and @root should inherit into
# project so we'll do this for now until @project is supported.
luci.realm(
    name = "@root",
    bindings = [
        luci.binding(
            roles = "role/resultdb.baselineWriter",
            groups = [
                "project-chromium-ci-task-accounts",
                "project-chromium-try-task-accounts",
            ],
            users = [
                "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/resultdb.baselineReader",
            groups = [
                "project-chromium-try-task-accounts",
            ],
            users = [
                "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ],
)

luci.realm(
    name = "@project",
    bindings = [
        # Allow everyone (including non-logged-in users) to see chromium tree status.
        luci.binding(
            roles = "role/treestatus.limitedReader",
            groups = [
                "all",
            ],
        ),
        # Only allow Googlers to see PII.
        luci.binding(
            roles = "role/treestatus.reader",
            groups = [
                "googlers",
            ],
            users = [
                "chromium-status-hr@appspot.gserviceaccount.com",
                "luci-notify@appspot.gserviceaccount.com",
                "luci-bisection@appspot.gserviceaccount.com",
            ],
        ),
        # Only allow Googlers and service accounts.
        luci.binding(
            roles = "role/treestatus.writer",
            groups = [
                "googlers",
            ],
            users = [
                "luci-notify@appspot.gserviceaccount.com",
                "luci-bisection@appspot.gserviceaccount.com",
            ],
        ),
    ],
)

luci.realm(
    name = "webrtc",
    bindings = [
        # Allow WebRTC builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.builder.defaults.test_presentation.set(resultdb.test_presentation(grouping_keys = ["status", "v.test_suite"]))

exec("//swarming.star")

exec("//recipes.star")
exec("//gn_args/gn_args.star")

exec("@chromium-targets//declarations.star")

exec("//notifiers.star")

exec("//subprojects/build/subproject.star")
exec("//subprojects/chrome/subproject.star")
exec("//subprojects/chromium/subproject.star")
exec("//subprojects/infra/subproject.star")
branches.exec("//subprojects/codesearch/subproject.star")
branches.exec("//subprojects/findit/subproject.star")
branches.exec("//subprojects/flakiness/subproject.star")
branches.exec("//subprojects/reviver/subproject.star")
branches.exec("//subprojects/webrtc/subproject.star")

exec("//generators/cq-usage.star")
branches.exec("//generators/cq-builders-md.star")

exec("//generators/builder-owners.star")
exec("//generators/sort-consoles.star")

# Execute validators after eveything except the outage file so that we're
# validating the final non-outages configuration
exec("//validators/builder-group-triggers.star")
exec("//validators/builders-in-consoles.star")

# Notify findit about completed builds for code coverage purposes
luci.buildbucket_notification_topic(
    name = "projects/findit-for-me/topics/buildbucket_notification",
)

# Execute this file last so that any configuration changes needed for handling
# outages gets final say
exec("//outages/outages.star")
