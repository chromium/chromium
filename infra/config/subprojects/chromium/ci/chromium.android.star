# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
        # Android emulator tasks often flake during emulator start-up, which
        # leads to the whole shard being marked as invalid.
        retry_invalid_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.ANDROID,
    tree_closing_notifiers = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

targets.settings_defaults.set(
    os_type = targets.os_type.ANDROID,
)

consoles.console_view(
    name = "chromium.android",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    ordering = {
        None: ["cronet", "builder", "tester"],
        "*cpu*": ["arm", "arm64", "x86", "x64"],
        "cronet": "*cpu*",
        "builder": "*cpu*",
        "builder|det": consoles.ordering(short_names = ["rel", "dbg"]),
        "tester": ["phone", "tablet"],
        "builder_tester|arm64": consoles.ordering(short_names = ["M proguard"]),
        "cast": ["arm", "arm64"],
    },
)

ci.builder(
    name = "Android arm Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "debug_builder",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_junit_tests_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    free_space = builders.free_space.high,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Android arm64 Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "arm64",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "android_lint",
    ),
    # The 'All' version of this builder below provides the same build coverage
    # but cycles much faster due to beefier machine resources. So any regression
    # that this bot would close the tree on would always be caught by the 'All'
    # bot much faster.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# We want to confirm that we can compile everything.
# Android however has some non standard buildchains
# which cause gn analyze to not filter out our compile targets
# when running a try bot.
# This means that our trybots would result in compile times of
# 5+ hours. So instead we have this bot which will compile all on CI.
# It should match "Android arm64 Builder (dbg)"
# History: crbug.com/1246468
ci.builder(
    name = "Android arm64 Builder All Targets (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "arm64",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    builderless = False,
    cores = None,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# This builder should be used for trybot mirroring when no need to compile all.
# See the builder "Android x64 Builder All Targets (dbg)" for more details.
# Don't enable this for branches unless the triggered testers are enabled for
# branches, "Android x64 Builder All Targets (dbg)" is already building all.
ci.builder(
    name = "Android x64 Builder (dbg)",
    builder_spec = builder_config.copy_from("ci/Android x64 Builder All Targets (dbg)"),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(),
    builderless = False,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# Similar to crbug.com/1246468#c34, as android has some non standard
# buildchains, `mb analyze` will not be able to filter out compile targets when
# running a trybot and thus tries to compile everythings. If the builder
# specifies `all` target, the compile time can take 5+ hours.
# So this builder matches "Android x64 Builder (dbg)", but compiles everything.
ci.builder(
    name = "Android x64 Builder All Targets (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "64-all",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
    # prevent from bot died by OOM. https://crbug.com/425441534
    siso_experiments = [
        "oom-score-adj",
    ],
    # enable remote link to mitigate bot died https://crbug.com/418817397
    siso_output_local_strategy = "greedy",
    siso_remote_linking = True,
)

ci.builder(
    name = "Android x86 Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "remoteexec",
            "x86",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    ssd = True,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
)

ci.builder(
    name = "android-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.thin_tester(
    name = "android-webview-10-x86-rel-tests",
    parent = "ci/android-x86-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "webview_trichrome_10_cts_tests_gtest",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "10-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|x86",
        short_name = "10",
    ),
    contact_team_email = "woa-engprod@google.com",
)

ci.builder(
    name = "android-cast-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_debug",
            "cast_java_debug",
            "android_builder",
            "android_with_static_analysis",
            "clang",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "and32dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_java_release",
            "cast_release",
            "disable_jni_multiplexing",
            "android_builder",
            "android_with_static_analysis",
            "clang",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "and32rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_debug",
            "cast_java_debug",
            "android_builder",
            "android_with_static_analysis",
            "clang",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm64_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "and64dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM64 Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_java_release",
            "cast_release",
            "disable_jni_multiplexing",
            "android_builder",
            "android_with_static_analysis",
            "clang",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm64_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "and64rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "Deterministic Android",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm",
        ],
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
    notifies = ["Deterministic Android"],
)

ci.builder(
    name = "Deterministic Android (dbg)",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_builder",
            "remoteexec",
            "arm",
        ],
    ),
    builderless = False,
    cores = 16,
    ssd = True,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "dbg",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Android"],
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-10-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium and XR tests on Android 10 devices.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_wpr_tests",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "enable_android_secondary_abi",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_10_rel_gtests",
        ],
        additional_compile_targets = [
            "check_chrome_static_initializers",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "walleye",
            "10_fleet",
            "retry_only_failed_tests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "10",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-x64-dbg-tests",
    description_html = "Run Chromium tests on Android 12l tablet-flavor emulator.",
    parent = "ci/Android x64 Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "android_lff_emulator_gtests",
        ],
        mixins = [
            "12l-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    ("--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*:" +
                     # https://crbug.com/1468262
                     "All/PaymentHandlerEnforceFullDelegationTest.WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject/1"),
                ],
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.remove(
                reason = "TODO(crbug.com/40263601): Temporarily remove it from android-12L builder due to infra flakiness.",
            ),
            # "content_browsertests": targets.mixin(
            #     # TODO(crbug.com/40265619): Remove experiment and ci_only
            #     # once the test suite is stable.
            #     ci_only = True,
            #     experiment_percentage 100,
            #     args = [
            #         '--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.content_browsertests.filter',
            #     ],
            #     swarming = targets.swarming(
            #         shards = 40,
            #     ),
            # ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.content_shell_test_apk.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
        short_name = "12L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-landscape-x64-dbg-tests",
    description_html = "Run Chromium tests on Android 12l tablet-flavor " +
                       "emulator in Landscape Mode.",
    parent = "ci/Android x64 Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "android_lff_landscape_emulator_gtests",
        ],
        mixins = [
            "12l-landscape-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l_landscape.chrome_public_test_apk.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
        short_name = "12L-L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-x64-rel-cq",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run CQ-specific Chromium test suites on Android 12l " +
                       "tablet-flavor emulator.",
    builder_spec = builder_config.copy_from("ci/android-13-x64-rel"),
    gn_args = "ci/android-13-x64-rel",
    targets = targets.bundle(
        targets = "android_12l_rel_cq_gtests",
        mixins = [
            "12l-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "retry_only_failed_tests",
            "x86-64",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "12L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-arm64-proguard-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "enable_android_secondary_abi",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_marshmallow_gtests",
            "chromium_junit_tests_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "chrome_public_test_apk": targets.mixin(
                swarming = targets.swarming(
                    shards = 25,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.arm64_proguard_rel.crashpad_tests.filter",
                ],
            ),
            "gin_unittests": targets.mixin(
                args = [
                    # https://crbug.com/1404782
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_10.proguard_rel.gin_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.arm64_proguard_rel.gl_tests.filter",  # https://crbug.com/1034007
                ],
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/931138): Fix permission issue when creating tmp files",
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "M proguard",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 8 * time.hour,
)

ci.builder(
    name = "android-bfcache-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "bfcache_android_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "pie-x86-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "bf_cache_content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "bf_cache_android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-binary-size-generator",
    executable = "recipe:binary_size_generator_tot",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "chrome_with_codecs",
            "remoteexec",
            "minimal_symbols",
            "official_optimize",
            "stable_channel",
            "v8_release_branch",
            # Allows the bot to measure low-end arm32 and high-end arm64 using
            # a single build.
            "android_low_end_secondary_toolchain",
        ],
    ),
    builderless = False,
    cores = 32,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|other",
        short_name = "size",
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "arm",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "release_java",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "arm",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-riscv64-dbg",
    description_html = "Verifies building Cronet against RISC-V64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "riscv64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-riscv64-rel",
    description_html = "Verifies building Cronet against RISC-V64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "riscv64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x86-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x86",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_smoketests_apk",
        ],
        per_test_modifications = {
            # Do not run cronet_sizes on CQ builders (binaries are instrumented
            # for code coverage)
            "cronet_sizes": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_smoketests_apk",
        ],
        per_test_modifications = {
            # Do not run cronet_sizes on CQ builders (binaries are instrumented
            # for code coverage)
            "cronet_sizes": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-12-tests",
    parent = "ci/android-cronet-x64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "12",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-13-tests",
    parent = "ci/android-cronet-x64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "13",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-14-tests",
    description_html = "Tests Cronet against Android 14",
    parent = "ci/android-cronet-x64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "14-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "14",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-15-tests",
    description_html = "Tests Cronet against Android 15",
    parent = "ci/android-cronet-x64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "15",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-16-tests",
    description_html = "Tests Cronet against Android 16",
    parent = "ci/android-cronet-x64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "16-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "16",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-marshmallow-tests",
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "marshmallow-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "m",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-nougat-tests",
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "nougat-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "n",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-oreo-tests",
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "oreo-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "o",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-pie-tests",
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "pie-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                # crbug.com/1046060
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "p",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "10-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "10",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-11-tests",
    parent = "ci/android-cronet-x86-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "11-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "11",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
            "cronet_smoketests_apk",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_smoketests_apk",
        ],
    ),
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-10-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 10 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "enable_wpr_tests",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            targets.bundle(
                targets = [
                    "android_10_emulator_gtests",
                    "android_10_isolated_scripts",
                ],
                mixins = targets.mixin(
                    args = [
                        "--use-persistent-shell",
                    ],
                ),
            ),
            "chromium_android_scripts",
        ],
        additional_compile_targets = [
            "chrome_nocompile_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "10-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "base_unittests_android_death_tests": targets.mixin(
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "android_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.android_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "android_sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),

            # If you change this, make similar changes in android-x86-code-coverage
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.chrome_public_test_apk.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/1084353",
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_shell_test_apk": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 6,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_unittests": targets.mixin(
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.gl_tests.filter",
                ],
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.media_unittests.filter",
                ],
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "services_unittests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "system_webview_shell_layout_test_apk": targets.mixin(
                args = [
                    # TODO(crbug.com/390676579): Fix the failed test
                    "--gtest_filter=-org.chromium.webview_shell.test.WebViewLayoutTest.*",
                ],
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "telemetry_chromium_minidump_unittests": targets.mixin(
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                swarming = targets.swarming(
                    shards = 18,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                # Only multiple process tests run in CQ.
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x86",
        short_name = "10",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-10-x86-nofieldtrial-rel",
    description_html = "Run chromium tests on Android 10 emulators without fieldtrials.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "enable_wpr_tests",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            targets.bundle(
                targets = [
                    "android_10_emulator_gtests",
                    "android_10_isolated_scripts",
                ],
                mixins = targets.mixin(
                    args = [
                        "--use-persistent-shell",
                    ],
                ),
            ),
            "chromium_android_scripts",
        ],
        additional_compile_targets = [
            "chrome_nocompile_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "10-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.android_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "android_sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests_android_death_tests": targets.mixin(
                ci_only = True,
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.chrome_public_test_apk.filter",
                    "--disable-field-trial-config",
                    "--skia-gold-consider-unsupported",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--disable-field-trial-config",
                    "--skia-gold-consider-unsupported",
                ],
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/1084353",
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--disable-field-trial-config",
                    "--skia-gold-consider-unsupported",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 6,
                ),
            ),
            "content_unittests": targets.mixin(
                ci_only = True,
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.gl_tests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.media_unittests.filter",
                ],
                ci_only = True,
            ),
            "services_unittests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "system_webview_shell_layout_test_apk": targets.mixin(
                args = [
                    # TODO(crbug.com/390676579): Fix the failed test
                    "--gtest_filter=-org.chromium.webview_shell.test.WebViewLayoutTest.*",
                ],
                ci_only = True,
            ),
            "telemetry_chromium_minidump_unittests": targets.mixin(
                ci_only = True,
            ),
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
                ci_only = True,
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                    "--disable-field-trial-config",
                    "--skia-gold-consider-unsupported",
                ],
                swarming = targets.swarming(
                    shards = 18,
                ),
            ),
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                    "--disable-field-trial-config",
                    "--skia-gold-consider-unsupported",
                ],
                # Only multiple process tests run in CQ.
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "10-nft",
    ),
    contact_team_email = "clank-engprod@google.com",
)

# TODO(crbug.com/40152686): Update the console view config once on CQ
ci.builder(
    name = "android-11-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_11_emulator_gtests",
        ],
        mixins = [
            "11-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
            ),
            "cc_unittests": targets.mixin(
                # https://crbug.com/1039860
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11_12.cc_unittests.filter",
                ],
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "components_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40174843): Fix the test failure
                    "--gtest_filter=-FieldFormatterTest.DifferentLocales",
                ],
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.content_shell_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
            ),
            "content_browsertests": targets.remove(
                reason = "TODO(crbug.com/40152686): Temporarily remove it from android-11 ci builder until it is stable.",
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/41440830): Fix permission issue when creating tmp files",
            ),
            "services_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40203477): Fix the failed tests
                    "--gtest_filter=-PacLibraryTest.ActualPacMyIpAddress*",
                ],
            ),
            "viz_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.viz_unittests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    # TODO(crbug.com/40173842) Enable this test once the issue is fixed.
                    "--gtest_filter=-org.chromium.net.NetworkChangeNotifierTest.testNetworkChangeNotifierJavaObservers",
                ],
            ),
            "webview_ui_test_app_test_apk": targets.remove(
                reason = "crbug.com/1165280",
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "11",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-12-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 12 emulator.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_12_emulator_gtests",
            "private_code_failure_test",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
            ),
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
            ),
            "android_browsertests_no_fieldtrial": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "cc_unittests": targets.mixin(
                # https://crbug.com/1039860
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11_12.cc_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.chrome_public_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.chrome_public_unit_test_apk.filter",
                ],
                ci_only = True,
            ),
            "components_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40174843): Fix the test failure
                    "--gtest_filter=-FieldFormatterTest.DifferentLocales",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.content_browsertests.filter",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gl_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.gl_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40203477): Fix the failed tests
                    "--gtest_filter=-PacLibraryTest.ActualPacMyIpAddress*",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "12",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-13-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 13 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_13_emulator_gtests",
            "android_rel_isolated_scripts",
        ],
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1414886
                    "--gtest_filter=-OfferNotificationControllerAndroidBrowserTestForMessagesUi.MessageShown",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "android_chrome_wpt_tests": targets.remove(
                reason = "Only run this step on Android 15 for now.",
            ),
            "android_webdriver_wpt_tests": targets.remove(
                reason = "Only run this step on Android 15 for now.",
            ),
            "android_webview_wpt_tests": targets.remove(
                reason = "Only run this step on Android 15 for now.",
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.chrome_public_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.content_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
                # TODO(crbug.com/337935399): Remove experiment after the bug is fixed.
                experiment_percentage = 100,
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "13",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-14-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 14 devices.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_device_ci_only_gtests",
            "android_14_device_gtests",
            "chromium_android_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "panther_on_14",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "cc_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.cc_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14.chrome_public_test_apk.filter",
                ],
                ci_only = True,
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14.content_browsertests.filter",
                ],
                # TODO(crbug.com/410638690): Re-enable on CQ once the high
                # pending time is gone.
                ci_only = True,
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "14",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-x64-rel",
    description_html = "Run chromium tests on Android 14 emulators.",
    # TODO(crbug.com/40286106): Enable on branches once stable
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_emulator_gtests",
        ],
        mixins = [
            "14-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/361042311
                    "--gtest_filter=-All/SharedStorageChromeBrowserTest.CrossOriginWorklet_SelectURL_Success/*",
                ],
            ),
            "android_sync_integration_tests": targets.mixin(
                args = [
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
                # https://crbug.com/345579530
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.base_unittests.filter",
                ],
            ),
            "blink_unittests": targets.mixin(
                args = [
                    # https://crbug.com/352586409
                    "--gtest_filter=-All/HTMLPreloadScannerLCPPLazyLoadImageTest.TokenStreamMatcherWithLoadingLazy/*",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.chrome_public_test_apk.filter",
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.chrome_public_unit_test_apk.filter",
                ],
            ),
            "components_browsertests": targets.mixin(
                args = [
                    # TODO(crbug.com/40746860): Fix the test failure
                    "--gtest_filter=-V8ContextTrackerTest.AboutBlank",
                ],
            ),
            "components_unittests": targets.mixin(
                args = [
                    # crbug.com/361638641
                    "--gtest_filter=-BrowsingTopicsStateTest.EpochsForSite_FourEpochs_SwitchTimeArrived",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.content_shell_test_apk.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.unit_tests.filter",
                ],
            ),
            "viz_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/338436747): Fix the failed test
                    "--gtest_filter=-SkiaOutputSurfaceImplTest.EndPaintReleaseFence",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-automotive-landscape-x64-rel",
    description_html = "Run chromium tests on Android 14 automotive landscape emulators.",
    # TODO(crbug.com/40286106): Enable on branches once stable
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_automotive_landscape_emulator_gtests",
        ],
        mixins = [
            "14-automotive-landscape-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_automotive_landscape.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_automotive_landscape.chrome_public_unit_test_apk.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    gardener_rotations = args.ignore_default(None),
    #tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "14A",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-tablet-landscape-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 14 tablets in Landscape Mode.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_tablet_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "tangorpro",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/375086487
                    "--gtest_filter=-InstallableManagerBrowserTest.CheckManifestWithIconThatIsTooSmall",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.tangorpro.base_unittests.filter",
                ],
            ),
            "cc_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.cc_unittests.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.content_unittests.filter",
                ],
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.content_shell_test_apk.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14_tablet.chrome_public_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14_tablet.content_browsertests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14_tablet.unit_tests.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "14T-L",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 3 * time.hour,
    # crbug.com/372192123 - downloading with "minimum" strategy doesn't work
    # well for Android builds because some steps have additional inputs/outputs
    # they are not configured in the build graph.
    siso_output_local_strategy = "greedy",
    siso_remote_linking = True,
)

ci.builder(
    name = "android-15-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 15 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_15_emulator_gtests",
            "android_rel_isolated_scripts",
            "gtests_once",
        ],
        mixins = [
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/361042311
                    "--gtest_filter=-All/SharedStorageChromeBrowserTest.CrossOriginWorklet_SelectURL_Success/*",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.base_unittests.filter",
                ],
            ),
            "base_unittests_android_death_tests": targets.mixin(
                ci_only = True,
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.chrome_public_test_apk.filter",
                    "--emulator-debug-tags=all",
                ],
                swarming = targets.swarming(
                    shards = 50,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.content_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "15",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-15-tablet-landscape-x64-rel",
    # TODO(crbug.com/376748979 ): Enable on branches once tests are stable
    # branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 15 tablet landscape emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_lff_landscape_emulator_gtests",
        ],
        mixins = [
            "15-tablet-landscape-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_tablet_landscape.chrome_public_test_apk.filter",
                    "--skia-gold-consider-unsupported",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--skia-gold-consider-unsupported",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "15T-L",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-15-tablet-x64-rel",
    # TODO(crbug.com/376748979 ): Enable on branches once tests are stable
    # branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 15 tablet emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_lff_emulator_gtests",
        ],
        mixins = [
            "15-tablet-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.remove(
                reason = "TODO(crbug.com/388919418): Temporarily remove it from builder due to flakiness.",
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.base_unittests.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_tablet.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_tablet.chrome_public_test_apk.filter",
                    "--emulator-debug-tags=all",
                    "--skia-gold-consider-unsupported",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_tablet.chrome_public_unit_test_apk.filter",
                    "--skia-gold-consider-unsupported",
                ],
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--skia-gold-consider-unsupported",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.content_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.gwp_asan_unittests.filter",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--skia-gold-consider-unsupported",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "15T",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-16-x64-dbg-tests",
    description_html = "Run chromium tests on Android 16 emulators.",
    parent = "ci/Android x64 Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "android_16_emulator_gtests",
        ],
        mixins = [
            "16-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "android_browsertests_no_fieldtrial": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.base_unittests.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_16.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.content_shell_test_apk.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.content_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "tester|x64",
        short_name = "16",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-16-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 16 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_16_emulator_gtests",
            "android_rel_isolated_scripts",
            "gtests_once",
        ],
        mixins = [
            "16-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.base_unittests.filter",
                ],
            ),
            "base_unittests_android_death_tests": targets.mixin(
                ci_only = True,
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 23,
                ),
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--emulator-debug-tags=all",
                ],
                swarming = targets.swarming(
                    shards = 47,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_16.content_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15_16.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15_16.unit_tests.filter",
                ],
            ),
            "webview_ui_test_app_test_apk_no_field_trial": targets.mixin(
                ci_only = True,
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "16",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-webview-13-x64-hostside-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = (
        "Runs WebView host-driven CTS on Android 13 emulator."
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = "webview_trichrome_64_cts_hostside_gtests",
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "webview_trichrome_64_cts_hostside_tests full_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "13-hs",
    ),
    contact_team_email = "woa-engprod@google.com",
)

ci.builder(
    name = "android-mte-arm64-rel",
    description_html = (
        "Run chromium tests with MTE SYNC mode enabled on Android."
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "full_mte",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "shiba",
        ],
        per_test_modifications = {
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.mte.base_unittests.filter",
                ],
            ),
            "components_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.mte.components_unittests.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.mte.content_browsertests.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.mte.crashpad_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    # TODO(crbug.com/40268661): Enable gardening once tests are stable
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "mte",
    ),
    contact_team_email = "chrome-mte@google.com",
    execution_timeout = 5 * time.hour,
)
