# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
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
    name = "android-chrome-pie-x86-wpt-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
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
            "webview_monochrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chrome_public_wpt_suite",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "pie-x86-emulator",
            "emulator-8-cores",
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
        short_name = "p-x86",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-chrome-13-x64-wpt-android-specific",
    description_html = "Run wpt tests on Chrome Android in Android 13 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            "no_secondary_abi",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "wpt_web_tests_android",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "13-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|chrome",
        short_name = "13-x64",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
)

ci.builder(
    name = "android-webview-13-x64-wpt-android-specific",
    description_html = "Run wpt tests on Android Webview in Android 13 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "wpt_web_tests_webview",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "13-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|webview",
        short_name = "13-x64",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
)

ci.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
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
            "webview_monochrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "system_webview_wpt_suite",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "pie-x86-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "system_webview_wpt": targets.mixin(
                args = [
                    "--use-upstream-wpt",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|webview",
        short_name = "p-x86",
    ),
)

# TODO(crbug.com/1022533#c40): Remove this builder once there are no associated
# disabled tests.
ci.builder(
    name = "android-pie-x86-fyi-rel",
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x86|rel",
        short_name = "P",
    ),
)

# TODO(crbug.com/40152686): This and android-12-x64-fyi-rel
# are being kept around so that build links in the related
# bugs are accessible
# Remove these once the bugs are closed
ci.builder(
    name = "android-11-x86-fyi-rel",
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder_mb"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x86|rel",
        short_name = "11",
    ),
)

ci.builder(
    name = "android-12-x64-fyi-rel",
    triggered_by = ["ci/android-12-x64-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "android_12_emulator_gtests",
        ],
        mixins = [
            "12-google-atd-x64-emulator",
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
        category = "emulator|x64|rel",
        short_name = "12",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

# TODO(crbug.com/40263601): Remove after experimental is done.
ci.builder(
    name = "android-12l-x64-fyi-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_12l_emulator_gtests",
        ],
        mixins = [
            "12l-fyi-x64-emulator",
            "emulator-8-cores",
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
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
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
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
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
        category = "emulator|x64|dbg",
        short_name = "12L",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

# TODO(crbug.com/40263601): Remove after experimental is done.
ci.builder(
    name = "android-13-x64-fyi-rel",
    description_html = "Run chromium tests on Android 13 emulators for experimental.",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_13_emulator_gtests",
        ],
        mixins = [
            "13-swangle-x64-emulator",
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
                ci_only = True,
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
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
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
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "13",
    ),
    contact_team_email = "clank-engprod@google.com",
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-x64-fyi-rel",
    description_html = "Run chromium tests on Android 14 emulators for experimental.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_emulator_gtests",
        ],
        mixins = [
            "14-swangle-x64-emulator",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.base_unittests.filter",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.content_shell_test_apk.filter",
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
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.gwp_asan_unittests.filter",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.unit_tests.filter",
                ],
            ),
            "webkit_unit_tests": targets.mixin(
                args = [
                    # https://crbug.com/352586409
                    "--gtest_filter=-All/HTMLPreloadScannerLCPPLazyLoadImageTest.TokenStreamMatcherWithLoadingLazy/*",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.14.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-arm64-fyi-rel",
    description_html = "Run chromium tests on Android 14 devices",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
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
            "android_14_device_fyi_gtests",
            "chromium_android_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "panther_on_14",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64|rel",
        short_name = "14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-tablet-14-arm64-fyi-rel",
    description_html = "Run chromium tests on Android 14 tablets.",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
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
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.14.webview_instrumentation_test_apk.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64|rel",
        short_name = "tablet-14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-15-x64-fyi-rel",
    description_html = "Run chromium tests on Android 15 emulators.",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_15_emulator_fyi_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
            ),
            "15-swangle-x64-emulator",
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
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.content_shell_test_apk.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "net_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/362292404): Fix the failed test
                    "--gtest_filter=-TrafficStatsAndroidTest.*",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "15",
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
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
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

# TODO(crbug.com/40216047): Move to non-FYI once the tester works fine.
ci.thin_tester(
    name = "android-webview-12-x64-dbg-tests",
    triggered_by = ["Android x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
    triggered_by = ["Android x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "webview_trichrome_64_cts_gtests",
            "webview_trichrome_64_32_cts_tests_suite",
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
    triggered_by = ["Android x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
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
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
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
            "cronet_perf_test_apk",
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
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
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
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm64",
        short_name = "mte",
    ),
    contact_team_email = "chrome-mte@google.com",
    execution_timeout = 20 * time.hour,
)
