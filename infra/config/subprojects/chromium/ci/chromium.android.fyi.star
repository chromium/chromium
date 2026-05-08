# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.fyi builder group."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android.fyi",
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    priority = ci_constants.DEFAULT_FYI_PRIORITY,
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

consoles.console_view(
    name = "chromium.android.fyi",
    ordering = {
        None: ["android", "memory", "webview"],
    },
)

ci.builder(
    name = "android-15-chrome-wpt-fyi-rel",
    description_html = "This builder runs upstream web platform tests for {}.".format(
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests.md#wpt_fyi-integration",
            "reporting results to wpt.fyi",
        ),
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
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
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chrome_public_wpt_suite",
        ],
        mixins = [
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_wpt": targets.mixin(
                args = [
                    "--use-upstream-wpt",
                    "--timeout-multiplier=4",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|chrome",
        short_name = "15",
    ),
    contact_team_email = "chrome-product-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-15-webview-wpt-fyi-rel",
    description_html = "This builder runs upstream web platform tests for {}.".format(
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests.md#wpt_fyi-integration",
            "reporting results to wpt.fyi",
        ),
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
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
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_webview_wpt_tests",
        ],
        mixins = [
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_webview_wpt_tests": targets.mixin(
                args = [
                    "--use-upstream-wpt",
                ],
                swarming = targets.swarming(
                    shards = 36,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|webview",
        short_name = "15",
    ),
    contact_team_email = "chrome-product-engprod@google.com",
)

ci.builder(
    name = "android-11-x86-fyi-rel",
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
            "android_11_emulator_fyi_gtests",
        ],
        mixins = [
            "11-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.chrome_public_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
                # TODO(crbug.com/40210655) Move to CI builder once stable
                swarming = targets.swarming(
                    dimensions = {
                        "cores": "8",
                    },
                ),
            ),
            "content_browsertests": targets.mixin(
                # TODO(crbug.com/40210655) Move to CI builder once stable
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 30,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x86|rel",
        short_name = "11",
    ),
)

ci.builder(
    name = "android-15-tablet-x64-fyi-rel",
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
            "android_browsertests_fyi",
        ],
        mixins = [
            "15-tablet-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/375086487
                    "--gtest_filter=-InstallableManagerBrowserTest.CheckManifestWithIconThatIsTooSmall",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "15T",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-16-x64-fyi-rel",
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
            apply_configs = ["mb"],
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
            "android_16_emulator_fyi_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--emulator-debug-tags=all",
                ],
            ),
            "16-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "16",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-16-x64-leakcanary-fyi-rel",
    description_html = "Runs Java instrumentation tests with LeakCanary enabled.",
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
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_leakcanary_tests",
        ],
        mixins = [
            "16-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "leak",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-17-x64-fyi-rel",
    description_html = "Run chromium tests on Android 17 Beta emulators.",
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
            "android_emulator_fyi_gtests",
            "android_fyi_isolated_scripts",
        ],
        mixins = [
            "17-beta-x64-emulator",
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
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--emulator-debug-tags=all",
                ],
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/40131701",
            ),
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "17",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-annotator-rel",
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
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "test_traffic_annotation_auditor_script",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "and",
    ),
    notifies = ["annotator-rel"],
)

ci.builder(
    name = "android-canary-x64-fyi-rel",
    description_html = "Run chromium tests on Android Emulators with Canary Image",
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
            "android_emulator_fyi_gtests",
            "android_fyi_isolated_scripts",
        ],
        mixins = [
            "canary-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
            ),
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/40131701",
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "canary",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

# TODO(crbug.com/40216047): Move to non-FYI once the tester works fine.
ci.thin_tester(
    name = "android-webview-12-x64-dbg-tests",
    parent = "Android x64 Builder (dbg)",
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
            "webview_fyi_bot_all_gtests",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "12",
    ),
)

ci.thin_tester(
    name = "android-webview-13-x64-dbg-tests",
    parent = "Android x64 Builder (dbg)",
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
            "webview_64_cts_gtests",
            "webview_64_32_cts_tests_suite",
        ],
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "13",
    ),
    notifies = [],
)

# TODO(crbug.com/40216047): Move to non-FYI once the tester works fine.
ci.thin_tester(
    name = "android-12-x64-dbg-tests",
    parent = "Android x64 Builder (dbg)",
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
            "android_12_dbg_emulator_gtests",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "12",
    ),
)

ci.builder(
    name = "android-cronet-asan-x86-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.COMPILE_AND_TEST,
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
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "cronet_android",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "clang",
            "asan",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        additional_compile_targets = [
            "cronet_package",
        ],
        mixins = [
            "marshmallow-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
    contact_team_email = "cronet-team@google.com",
)
