# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")
load("//project.star", "settings")

# crbug/1408581 - The code coverage CI builders are expected to be triggered
# off the same ref every 24 hours. This poller is configured with a schedule
# to ensure this - setting schedules on the builder configuration does not
# guarantee that they are triggered off the same ref.
luci.gitiles_poller(
    name = "code-coverage-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
    # Trigger coverage jobs once a day at 4 am UTC(8 pm PST)
    schedule = "0 4 * * *",
)

# Use a separate poller to trigger the webview coverage builders.
luci.gitiles_poller(
    name = "code-coverage-webview-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
    # Trigger coverage jobs once a day at 10 am UTC(2 am PST)
    schedule = "0 10 * * *",
)

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.coverage",
    pool = ci_constants.DEFAULT_POOL,
    cores = 32,
    ssd = True,
    execution_timeout = 20 * time.hour,
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
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.coverage",
    title = "Code Coverage CI Builders",
)

def coverage_builder(**kwargs):
    return ci.builder(
        schedule = "triggered",
        triggered_by = ["code-coverage-gitiles-trigger"],
        # This should allow one to be pending should code coverage
        # builds take longer.
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = 2,
        ),
        **kwargs
    )

def coverage_webview_builder(**kwargs):
    return ci.builder(
        schedule = "triggered",
        triggered_by = ["code-coverage-webview-gitiles-trigger"],
        # This should allow one to be pending should code coverage
        # builds take longer.
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = 2,
        ),
        **kwargs
    )

coverage_builder(
    name = "android-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "resource_allowlisting",
            "static_angle",
            "android_fastbuild",
            "webview_google",
            "android_no_proguard",
            "use_java_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_coverage_instrumentation_tests",
            "chromium_junit_tests_scripts",
            "gtests_once",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
            "isolate_profile_data",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "arm64",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    generate_blame_list = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_webview_builder(
    name = "android-webview-code-coverage",
    description_html = "Builder for WebView java coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "resource_allowlisting",
            "static_angle",
            "android_fastbuild",
            "webview_google",
            "android_no_proguard",
            "use_java_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "webview_fyi_bot_all_gtests",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
            "isolate_profile_data",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "webview",
            short_name = "arm64",
        ),
    ],
    contact_team_email = "woa-engprod@google.com",
    coverage_test_types = ["overall", "unit"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_builder(
    name = "android-x86-code-coverage",
    description_html = "Builder for creating x86 Android code coverage builds.",
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
            "use_java_coverage",
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
            "gtests_once",
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
            # Keep this same as android-10-x86-rel
            "android_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.android_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            # Keep this same as android-10-x86-rel
            "android_sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),

            # Keep this same as android-10-x86-rel
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
            # Keep this same as android-10-x86-rel
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            # Keep this same as android-10-x86-rel
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
            # Keep this same as android-10-x86-rel
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/1084353",
            ),
            # Keep this same as android-10-x86-rel
            "content_shell_test_apk": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 6,
                ),
            ),
            # Keep this same as android-10-x86-rel
            "content_unittests": targets.mixin(
                ci_only = True,
            ),
            # Keep this same as android-10-x86-rel
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.gl_tests.filter",
                ],
            ),
            # Keep this same as android-10-x86-rel
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            # Keep this same as android-10-x86-rel
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.media_unittests.filter",
                ],
                ci_only = True,
            ),
            # Keep this same as android-10-x86-rel
            "services_unittests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            # Keep this same as android-10-x86-rel
            "system_webview_shell_layout_test_apk": targets.mixin(
                args = [
                    # TODO(crbug.com/390676579): Fix the failed test
                    "--gtest_filter=-org.chromium.webview_shell.test.WebViewLayoutTest.*",
                ],
                ci_only = True,
            ),
            # Keep this same as android-10-x86-rel
            "telemetry_chromium_minidump_unittests": targets.mixin(
                ci_only = True,
            ),
            # Keep this same as android-10-x86-rel
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
                ci_only = True,
            ),
            # Keep this same as android-10-x86-rel
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                swarming = targets.swarming(
                    shards = 18,
                ),
            ),
            # Keep this same as android-10-x86-rel
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
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "x86",
        ),
    ],
    contact_team_email = "clank-engprod@google.com",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_builder(
    name = "android-code-coverage-native",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "use_clang_coverage",
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    # No symbols to prevent linker file too large error on
    # android_webview_unittests target.
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "remoteexec",
            "arm64",
            "resource_allowlisting",
            "static_angle",
            "android_fastbuild",
            "webview_google",
            "android_no_proguard",
            "use_clang_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_android_gtests",
            "gtests_once",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "chrome_public_smoke_test": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "chrome_public_test_apk": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "chrome_public_test_vr_apk": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "chrome_public_unit_test_apk": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 25,
                ),
            ),
            "content_shell_test_apk": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "mojo_test_apk": targets.remove(
                reason = "Does not generate profraw data.",
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/41440830): Fix permission issue when creating tmp files",
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                swarming = targets.swarming(
                    # Shard number is increased for longer test execution time
                    # and added local coverage data merging time.
                    shards = 24,
                ),
            ),
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                swarming = targets.swarming(
                    # Shard number is increased for longer test execution time
                    # and added local coverage data merging time.
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "ann",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_webview_builder(
    name = "android-webview-code-coverage-native",
    description_html = "Builder for WebView clang coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "use_clang_coverage",
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    # No symbols to prevent linker file too large error on
    # android_webview_unittests target.
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "remoteexec",
            "arm64",
            "resource_allowlisting",
            "static_angle",
            "android_fastbuild",
            "webview_google",
            "android_no_proguard",
            "use_clang_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "webview_native_coverage_bot_gtests",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "webview_instrumentation_test_apk_mutations": targets.mixin(
                swarming = targets.swarming(
                    # Shard number is increased for longer test execution time
                    # and added local coverage data merging time.
                    shards = 60,
                ),
            ),
            "webview_instrumentation_test_apk_no_field_trial": targets.mixin(
                swarming = targets.swarming(
                    # Shard number is increased for longer test execution time
                    # and added local coverage data merging time.
                    shards = 30,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "webview",
            short_name = "awn",
        ),
    ],
    contact_team_email = "woa-engprod@google.com",
    coverage_test_types = ["overall", "unit"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "android-cronet-code-coverage-java",
    description_html = "Builder for Cronet java code coverage",
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    # No symbols to prevent linker file too large error on
    # android_webview_unittests target.
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "use_java_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "14-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = [
        consoles.console_view_entry(
            category = "cronet",
            short_name = "java",
        ),
    ],
    contact_team_email = "cronet-team@google.com",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_builder(
    name = "android-cronet-code-coverage-native",
    description_html = "Builder for Cronet clang coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "use_clang_coverage",
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
        android_config = builder_config.android_config(config = "base_config"),
    ),
    # No symbols to prevent linker file too large error on
    # android_webview_unittests target.
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "use_clang_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_cronet_clang_coverage_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "14-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    gardener_rotations = args.ignore_default(gardener_rotations.CRONET),
    console_view_entry = [
        consoles.console_view_entry(
            category = "cronet",
            short_name = "native",
        ),
    ],
    contact_team_email = "cronet-team@google.com",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

# fuschia runs outside of chromium, so we do not enable zoss for it.
coverage_builder(
    name = "fuchsia-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang",
            "fuchsia",
            "fuchsia_code_coverage",
            "no_symbols",
            "release_builder",
            "remoteexec",
            "use_clang_coverage",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "fuchsia_gtests",
            "gtests_once",
            targets.bundle(
                targets = "gpu_angle_fuchsia_unittests_isolated_scripts",
                mixins = "expand-as-isolated-script",
            ),
        ],
        mixins = [
            "fuchsia-code-coverage",
            "fuchsia-large-device-spec",
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "base_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "blink_platform_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "blink_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "cc_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.coverage.content_browsertests.filter",
                    "--test-launcher-jobs=1",
                ],
                swarming = targets.swarming(
                    shards = 41,
                ),
            ),
            "content_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "gfx_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "gpu_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "headless_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "media_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "mojo_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "net_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 14,
                ),
            ),
            "services_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 7,
                ),
            ),
            "web_engine_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 28,
                ),
            ),
            "web_engine_integration_tests": targets.mixin(
                args = [
                    "--test-launcher-jobs=1",
                ],
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuschia",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci|x64",
            short_name = "cov",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
)

coverage_builder(
    name = "ios-simulator-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "use_clang_coverage",
            "debug_static_builder",
            "remoteexec",
            "arm64",
            "ios_simulator",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios_code_coverage_tests",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_output_disabled_tests",
            "isolate_profile_data",
            "mac_default_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios",
            short_name = "sim",
        ),
    ],
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
    xcode = xcode.xcode_default,
)

coverage_builder(
    name = "linux-chromeos-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "remoteexec",
            "use_clang_coverage",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "linux_chromeos_gtests",
            "gtests_once",
        ],
        additional_compile_targets = [
            "gn_all",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 50,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "chromeos",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "linux-js-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "clang",
            "no_symbols",
            "use_javascript_coverage",
            "optimize_webui_off",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "js_code_coverage_browser_tests_suite",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "js",
        ),
    ],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_javascript_coverage = True,
)

coverage_builder(
    name = "chromeos-js-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "remoteexec",
            "use_javascript_coverage",
            "optimize_webui_off",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromeos_js_code_coverage_browser_tests_suite",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "js",
        ),
    ],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_javascript_coverage = True,
)

# Experimental builder. Does not export_coverage_to_zoss.
coverage_builder(
    name = "linux-fuzz-coverage",
    executable = "recipe:chromium/fuzz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "use_clang_coverage",
            "static",
            "mojo_fuzzer",
            "libfuzzer",
            "dcheck_off",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "release",
            "linux",
            "x64",
            "no_clang_modules",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux-fuzz",
            short_name = "lnx-fuzz",
        ),
    ],
    # TODO(crbug.com/449026537): Remove elevated timeout once performance
    # improves.
    execution_timeout = 32 * time.hour,
    notifies = ["chrome-fuzzing-core"],
    properties = {
        "collect_fuzz_coverage": True,
        "fuzz_engine": "libfuzzer",
    },
)

# Experimental builder. Does not export_coverage_to_zoss.
coverage_builder(
    name = "linux-centipede-fuzz-coverage",
    description_html = "This builder collects code coverage for centipede.",
    executable = "recipe:chromium/fuzz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "use_clang_coverage",
            "static",
            "mojo_fuzzer",
            "centipede",
            "dcheck_off",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "release",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux-fuzz",
            short_name = "centipede",
        ),
    ],
    contact_team_email = "chrome-fuzzing-core@google.com",
    # TODO(crbug.com/449026537): Remove elevated timeout once performance
    # improves.
    execution_timeout = 24 * time.hour,
    notifies = ["chrome-fuzzing-core"],
    properties = {
        "collect_fuzz_coverage": True,
        "fuzz_engine": "centipede",
    },
)

# Experimental builder. Does not export_coverage_to_zoss.
coverage_builder(
    name = "linux-x64-fuzzilli-coverage",
    description_html = "This builder collects code coverage for V8 Fuzzilli tests.",
    executable = "recipe:chromium/fuzz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "dcheck_always_on",
            "v8_backtrace",
            "v8_debug",
            "v8_heap",
            "v8_static",
            "use_clang_coverage",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux-fuzz",
            short_name = "fuzzlli-x64",
        ),
    ],
    contact_team_email = "v8-security@google.com",
    notifies = ["chrome-fuzzing-core"],
    properties = {
        "collect_fuzz_coverage": True,
        "fuzz_engine": "fuzzilli",
    },
)

coverage_builder(
    name = "linux-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "clang",
            "use_clang_coverage",
            "no_symbols",
            "chrome_with_codecs",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts_code_coverage",
            "gpu_dawn_webgpu_cts",
            "gtests_once",
            "chromium_linux_scripts",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_pytype": targets.remove(
                reason = "pytype isn't impacted by building with coverage",
            ),
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m%c.profraw",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m%c.profraw",
                ],
            ),
            "browser_tests": targets.mixin(
                args = [
                    "--no-sandbox",
                ],
                swarming = targets.swarming(
                    shards = 50,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--no-sandbox",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "gold_common_pytype": targets.remove(
                reason = "pytype isn't impacted by building with coverage",
            ),
            "gpu_pytype": targets.remove(
                reason = "pytype isn't impacted by building with coverage",
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
            "not_site_per_process_blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m%c.profraw",
                ],
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "testing_pytype": targets.remove(
                reason = "pytype isn't impacted by building with coverage",
            ),
            # These tests must run with a GPU.
            "webgpu_blink_web_tests": [
                "linux_nvidia_gtx_1660_stable",
            ],
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
            # These tests must run with a GPU.
            "webgpu_cts_tests": [
                "linux_nvidia_gtx_1660_stable",
            ],
            "webgpu_cts_with_validation_tests": targets.remove(
                reason = "Don't need validation layers on code coverage bots.",
            ),
            "webgpu_cts_dedicated_worker_tests": [
                "linux_nvidia_gtx_1660_stable",
            ],
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "Dedicated worker tests are probably sufficient.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "Dedicated worker tests are probably sufficient.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

coverage_builder(
    name = "mac-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "clang",
            "use_clang_coverage",
            "no_symbols",
            "chrome_with_codecs",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests",
            "chromium_mac_rel_isolated_scripts_code_coverage",
            # TODO(crbug.com/40249801): Enable gpu_dawn_webgpu_cts
            "gtests_once",
        ],
        mixins = [
            "isolate_profile_data",
            "mac_default_x64",
        ],
        per_test_modifications = {
            "browser_tests": targets.remove(
                reason = "https://crbug.com/1201386",
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--coverage-continuous-mode=1",
                ],
            ),
            "interactive_ui_tests": targets.mixin(
                args = [
                    "--coverage-continuous-mode=1",
                ],
            ),
            "sync_integration_tests": targets.mixin(
                args = [
                    "--coverage-continuous-mode=1",
                ],
            ),
        },
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "mac",
            short_name = "mac",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "win10-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "clang",
            "use_clang_coverage",
            "no_symbols",
            "chrome_with_codecs",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_gtests",
            "chromium_win_rel_isolated_scripts_code_coverage",
            "gpu_dawn_webgpu_cts",
            "gtests_once",
        ],
        mixins = [
            "isolate_profile_data",
            "win10",
        ],
        per_test_modifications = {
            "blink_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.coverage",
                        "ssd": "1",
                    },
                    shards = 40,
                ),
            ),
            "components_unittests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 5400,
                    shards = 6,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.coverage",
                        "ssd": "1",
                    },
                    shards = 40,
                ),
            ),
            "content_unittests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 5400,
                    shards = 2,
                ),
            ),
            "extensions_browsertests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.coverage",
                        "ssd": "1",
                    },
                    shards = 2,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.coverage",
                        "ssd": "1",
                    },
                    shards = 32,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.coverage",
                        "ssd": "1",
                    },
                    shards = 4,
                ),
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            # These tests must run with a GPU.
            "webgpu_blink_web_tests": [
                "win10_nvidia_gtx_1660_stable",
            ],
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "Don't need validation layers on code coverage bots.",
            ),
            "webgpu_cts_tests": [
                "win10_nvidia_gtx_1660_stable",
            ],
            "webgpu_cts_with_validation_tests": targets.remove(
                reason = "Don't need validation layers on code coverage bots.",
            ),
            "webgpu_cts_dedicated_worker_tests": [
                "win10_nvidia_gtx_1660_stable",
            ],
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "Dedicated worker tests are probably sufficient.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "Dedicated worker tests are probably sufficient.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "win",
            short_name = "win10",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)
