# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu.fyi builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.gpu.fyi",
    pool = ci.gpu.POOL,
    gardener_rotations = gardener_rotations.CHROMIUM_GPU,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = 6 * time.hour,
    health_spec = health_spec.DEFAULT,
    properties = {
        "perf_dashboard_machine_group": "ChromiumGPUFYI",
    },
    service_account = ci.gpu.SERVICE_ACCOUNT,
    shadow_service_account = ci.gpu.SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
        "swarming_containment_auto",
        "timeout_30m",
    ],
)

targets.settings_defaults.set(
    allow_script_tests = False,
)

consoles.console_view(
    name = "chromium.gpu.fyi",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
    ],
    ordering = {
        None: ["Windows", "Mac", "Linux"],
        "*builder*": ["Builder"],
        "*type*": consoles.ordering(short_names = ["rel", "dbg", "exp"]),
        "*cpu*": consoles.ordering(short_names = ["x86"]),
        "Windows": "*builder*",
        "Windows|Builder": ["Release", "dx12vk", "Debug"],
        "Windows|Builder|Release": "*cpu*",
        "Windows|Builder|dx12vk": "*type*",
        "Windows|Builder|Debug": "*cpu*",
        "Windows|10|x64|Intel": "*type*",
        "Windows|10|x64|Nvidia": "*type*",
        "Windows|10|x86|Nvidia": "*type*",
        "Windows|7|x64|Nvidia": "*type*",
        "Mac": "*builder*",
        "Mac|Builder": "*type*",
        "Mac|AMD|Retina": "*type*",
        "Mac|Intel": "*type*",
        "Mac|Nvidia": "*type*",
        "Linux": "*builder*",
        "Linux|Builder": "*type*",
        "Linux|Intel": "*type*",
        "Linux|Nvidia": "*type*",
        "Android": ["Builder", "L32", "M64", "P32", "R32", "S64"],
        "Wayland": "*builder*",
    },
)

def gpu_fyi_windows_builder(*, name, **kwargs):
    kwargs.setdefault("execution_timeout", ci.DEFAULT_EXECUTION_TIMEOUT)
    return ci.gpu.windows_builder(name = name, **kwargs)

ci.thin_tester(
    name = "Android FYI Release (NVIDIA Shield TV)",
    triggered_by = ["GPU FYI Android arm Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_shieldtv_gtests",
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_nvidia_shield_tv_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|P32|NVDA",
        short_name = "STV",
    ),
)

ci.thin_tester(
    name = "Android FYI Release (Nexus 5X)",
    triggered_by = ["GPU FYI Android arm64 Builder"],
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
                "download_xr_test_apks",
            ],
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_nexus5x_telemetry_tests",
            "android_webview_gpu_telemetry_tests",
        ],
        mixins = [
            "chromium_nexus_5x_oreo",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "angle_unittests": targets.remove(
                reason = "On Android, these are already run on the main waterfall.",
            ),
            "gl_unittests": targets.remove(
                reason = [
                    "On Android, these are already run on the main waterfall.",
                    "Run them on the one-off Android FYI bots, though.",
                ],
            ),
            # The browser is restarted after every test in this suite, which
            # includes re-applying permissions. Nexus 5Xs are very slow to apply
            # permissions compared to other devices, so increase sharding to
            # offset the increased runtime.
            "trace_test": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|M64|QCOM",
        short_name = "N5X",
    ),
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 2)",
    triggered_by = ["GPU FYI Android arm Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
    ),
    targets = targets.bundle(
        # We currently only want to run the WebGL 2.0 conformance tests on
        # this until additional Pixel 2 capacity is added.
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_fyi_android_webgl2_and_gold_telemetry_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "context_lost_validating_tests": targets.remove(
                reason = "TODO(crbug.com/40039565): Remove once there is capacity",
            ),
            "expected_color_pixel_validating_test": targets.remove(
                reason = "TODO(crbug.com/40039565): Remove once there is capacity",
            ),
            "gpu_process_launch_tests": targets.remove(
                reason = "TODO(crbug.com/40039565): Remove once there is capacity",
            ),
            "hardware_accelerated_feature_tests": targets.remove(
                reason = "TODO(crbug.com/40039565): Remove once there is capacity",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|P32|QCOM",
        short_name = "P2",
    ),
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 4)",
    triggered_by = ["GPU FYI Android arm Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_pixel_4_telemetry_tests",
            "android_webview_gpu_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_4_stable",
        ],
        per_test_modifications = {
            "expected_color_pixel_passthrough_test": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "expected_color_pixel_validating_test": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "pixel_skia_gold_passthrough_test": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "pixel_skia_gold_validating_test": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "screenshot_sync_passthrough_tests": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "screenshot_sync_validating_tests": targets.mixin(
                # Pixel 4s are weird in that they can output in different color spaces
                # simultaneously. The readback code for capturing a screenshot assumes
                # only one color space, so disable wide color gamut for the test to
                # work around the issue. See https://crbug.com/1166379 for more
                # information.
                args = [
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|R32|QCOM",
        short_name = "P4",
    ),
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 6)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["GPU FYI Android arm64 Builder"],
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
                "download_xr_test_apks",
            ],
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_pixel_6_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_6_stable",
        ],
        per_test_modifications = {
            "webgl2_conformance_gles_passthrough_tests": targets.remove(
                reason = [
                    "Currently not enough capacity to run these tests on this config.",
                    "TODO(crbug.com/40208926): Re-enable once more of the Pixel 6 capacity",
                    "is deployed.",
                ],
            ),
            "webgl2_conformance_validating_tests": targets.remove(
                reason = [
                    "Currently not enough capacity to run these tests on this config.",
                    "TODO(crbug.com/40208926): Re-enable once more of the Pixel 6 capacity",
                    "is deployed.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|S64|ARM",
        short_name = "P6",
    ),
)

ci.thin_tester(
    name = "Android FYI Experimental Release (Pixel 6)",
    description_html = "Runs standard GPU tests on experimental Pixel 6 configs",
    triggered_by = ["GPU FYI Android arm64 Builder"],
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
                "download_xr_test_apks",
            ],
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # If the experimental configuration is the same as stable, this should
        # only be running 'gpu_noop_sleep_telemetry_test'. Otherwise, this
        # should be running the same tests as 'Android FYI Release (Pixel 6)'.
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_pixel_6_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_6_experimental",
            "limited_capacity_bot",
        ],
        per_test_modifications = {
            "webgl2_conformance_gles_passthrough_tests": targets.remove(
                reason = [
                    "Currently not enough capacity to run these tests on this config.",
                    "TODO(crbug.com/40208926): Re-enable once more of the Pixel 6 capacity",
                    "is deployed.",
                ],
            ),
            "webgl2_conformance_validating_tests": targets.remove(
                reason = [
                    "Currently not enough capacity to run these tests on this config.",
                    "TODO(crbug.com/40208926): Re-enable once more of the Pixel 6 capacity",
                    "is deployed.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Android|S64|ARM",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Android FYI Release (Motorola Moto G Power 5G)",
    description_html = "Runs GPU tests on Motorola Moto G Power 5G phones",
    triggered_by = ["GPU FYI Android arm64 Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "motorola_moto_g_power_5g",
            "limited_capacity_bot",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Android|T64|IMG",
    #     short_name = "MGP",
    # ),
    list_view = "chromium.gpu.experimental",
)

# TODO(crbug.com/40282670): Add a trybot for this builder when there's capacity.
ci.thin_tester(
    name = "Android FYI Release (Samsung A13)",
    triggered_by = ["GPU FYI Android arm Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_samsung_a13_stable",
            "limited_capacity_bot",
        ],
        per_test_modifications = {
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.samsung_a13.gl_tests.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|S32|ARM",
        short_name = "A13",
    ),
)

# TODO(crbug.com/40282670): Add a trybot for this builder when there's capacity.
ci.thin_tester(
    name = "Android FYI Release (Samsung A23)",
    triggered_by = ["GPU FYI Android arm Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_samsung_a23_stable",
            "limited_capacity_bot",
        ],
        per_test_modifications = {
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.samsung_a23.gl_tests.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|S32|QCOM",
        short_name = "A23",
    ),
)

ci.thin_tester(
    name = "Android FYI Release (Samsung S23)",
    description_html = "Runs GPU tests on Samsung S23 phones",
    triggered_by = ["GPU FYI Android arm64 Builder"],
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_android_gtests",
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_samsung_s23_stable",
            "limited_capacity_bot",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|U64|QCOM",
        short_name = "S23",
    ),
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release (amd64-generic)",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = [
                "amd64-generic-vm",
            ],
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "chromeos_device",
            "amd64-generic-vm",
            "ozone_headless",
            "release_builder",
            "try_builder",
            "remoteexec",
            "dcheck_off",
            "no_symbols",
            "chromeos",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_chromeos_release_gtests",
            "gpu_fyi_chromeos_release_telemetry_tests",
        ],
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
        mixins = [
            "chromeos-generic-vm",
        ],
        per_test_modifications = {
            "info_collection_tests": targets.per_test_modification(
                mixins = targets.mixin(
                    # Swarming does not report a GPU since tests are run in a VM, but
                    # the VM does report that a GPU is present.
                    args = [
                        "--expected-vendor-id",
                        "1af4",
                        "--expected-device-id",
                        "1050",
                    ],
                ),
                replacements = targets.replacements(
                    # Magic substitution happens after regular replacement, so remove it
                    # now since we are manually applying the expected device ID above.
                    args = {
                        targets.magic_args.GPU_EXPECTED_VENDOR_ID: None,
                        targets.magic_args.GPU_EXPECTED_DEVICE_ID: None,
                    },
                ),
            ),
            "webgl2_conformance_gles_passthrough_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.CROS_CHROME,
        os_type = targets.os_type.CROS,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|LLVM",
        short_name = "gen",
    ),
    # Runs a lot of tests + VMs are slower than real hardware, so increase the
    # timeout.
    execution_timeout = 8 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release Skylab (volteer)",
    description_html = "Runs standard GPU tests on Skylab-hosted volteer devices",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "volteer",
            ],
        ),
        run_tests_serially = True,
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
            gs_extra = "chromeos_gpu",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "chromeos_device",
            "volteer",
            "ozone_headless",
            "release_builder",
            "try_builder",
            "remoteexec",
            "dcheck_off",
            "no_symbols",
            "is_skylab",
            "chromeos",
            "x64",
        ],
    ),
    # TODO(crbug.com/40942991): This config is experimental and currently
    # is too difficult for gardeners to keep green.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|Intel",
        short_name = "vlt",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU Flake Finder",
    executable = "recipe:chromium_expectation_files/expectation_file_scripts",
    # This will eventually be set up to run on a schedule, but only support
    # manual triggering for now until we get a successful build.
    schedule = "triggered",
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        short_name = "flk",
    ),
    properties = {
        "scripts": [
            {
                "step_name": "suppress_gpu_flakes",
                "script": "content/test/gpu/suppress_flakes.py",
                "script_type": "FLAKE_FINDER",
                "submit_type": "MANUAL",
                "reviewer_list": {
                    "reviewer": ["bsheedy@chromium.org"],
                },
                "cl_title": "Suppress flaky GPU tests",
                "args": [
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--no-prompt-for-user-input",
                ],
            },
        ],
    },
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
)

ci.gpu.linux_builder(
    name = "GPU FYI Android arm Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "try_builder",
            "remoteexec",
            "static_angle",
            "arm",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder",
        short_name = "arm",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU FYI Android arm64 Builder",
    branch_selector = branches.selector.ANDROID_BRANCHES,
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
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "try_builder",
            "remoteexec",
            "arm64",
            "static_angle",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder",
        short_name = "arm64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Wayland Builder",
    description_html = "Parent GPU builder for Linux Wayland builds",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "ozone_linux",
            "ozone_linux_non_x11",
            "release_builder",
            "try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Wayland|Builder",
        short_name = "rel",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "rel",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "dbg",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "Linux FYI GPU TSAN Release",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "tsan",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        # This bot doesn't run any browser-based tests (tab_capture_end2end_tests)
        targets = [
            "gpu_fyi_linux_debug_gtests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
        ],
    ),
    # This bot doesn't run any Telemetry-based tests so doesn't
    # need the browser_config parameter.
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "tsn",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "x64",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "rel",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder (asan)",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "asan",
            "x64",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "asn",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "debug_builder",
            "remoteexec",
            "x64",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "dbg",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac arm64 Builder",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "arm64",
            "mac",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "arm",
    ),
)

ci.thin_tester(
    name = "Linux Wayland FYI Release (AMD)",
    description_html = "Runs GPU tests on weston with Intel UHD 630",
    triggered_by = ["GPU FYI Linux Wayland Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_lacros_release_gtests",
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "linux_amd_rx_5500_xt",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LACROS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Wayland|AMD",
        short_name = "amd",
    ),
)

ci.thin_tester(
    name = "Linux Wayland FYI Release (Intel)",
    description_html = "Runs GPU tests on weston with AMD RX 5500 XT",
    triggered_by = ["GPU FYI Linux Wayland Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_lacros_release_gtests",
            "gpu_fyi_lacros_release_telemetry_tests",
        ],
        mixins = [
            "linux_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "webgl2_conformance_gles_passthrough_tests": targets.remove(
                reason = [
                    "Not enough CrOS hardware capacity to run both on anything other than",
                    "VMs. See https://crbug.com/1238070.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LACROS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Wayland|Intel",
        short_name = "int",
    ),
)

ci.thin_tester(
    name = "Linux FYI Debug (NVIDIA)",
    triggered_by = ["GPU FYI Linux Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_linux_debug_gtests",
            "gpu_fyi_linux_debug_telemetry_tests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "dbg",
    ),
)

ci.thin_tester(
    name = "Linux FYI Experimental Release (Intel UHD 630)",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            # If the experimental configuration is the same as stable, this should
            # only be running 'gpu_noop_sleep_telemetry_test'. Otherwise, this
            # should be running the same tests as 'Linux FYI Release (Intel UHD 630)'.
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "linux_intel_uhd_630_experimental",
        ],
        per_test_modifications = {
            # "gl_tests_passthrough": targets.mixin(
            #     args = [
            #         "--test-launcher-filter-file=../../testing/buildbot/filters/linux.uhd_630.gl_tests_passthrough.filter",
            #     ],
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Linux|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Linux FYI Experimental Release (NVIDIA)",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # If the experimental configuration is the same as stable, this should
        # only be running 'gpu_noop_sleep_telemetry_test'. Otherwise, this
        # should be running the same tests as 'Linux FYI Release (NVIDIA)'.
        targets = [
            "gpu_fyi_linux_release_gtests",
            "gpu_fyi_linux_release_vulkan_telemetry_tests",
        ],
        mixins = [
            "limited_capacity_bot",
            "linux_nvidia_gtx_1660_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Linux FYI Release (NVIDIA)",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_linux_release_gtests",
            "gpu_fyi_linux_release_vulkan_telemetry_tests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "tab_capture_end2end_tests": targets.remove(
                reason = "Disabled due to dbus crashes crbug.com/927465",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Linux FYI Release (AMD RX 5500 XT)",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_linux_release_gtests",
            "gpu_fyi_linux_release_telemetry_tests",
        ],
        mixins = [
            "linux_amd_rx_5500_xt",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|AMD",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Linux FYI Release (Intel UHD 630)",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_linux_release_gtests",
            "gpu_fyi_linux_release_telemetry_tests",
        ],
        mixins = [
            "linux_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "tab_capture_end2end_tests": targets.remove(
                reason = "Disabled due to dbus crashes crbug.com/927465",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Linux FYI Release (Intel UHD 770)",
    description_html = "Runs GPU tests on 12th gen Intel CPUs with UHD 770 GPUs",
    triggered_by = ["GPU FYI Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_linux_release_gtests",
            "gpu_fyi_linux_release_telemetry_tests",
        ],
        mixins = [
            "linux_intel_uhd_770_stable",
        ],
        per_test_modifications = {
            "gl_tests_passthrough": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/linux.uhd_770.gl_tests_passthrough.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "770",
    ),
)

ci.thin_tester(
    name = "Mac FYI Debug (Intel)",
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_debug_gtests",
            "gpu_common_gl_passthrough_ganesh_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "dbg",
    ),
)

ci.thin_tester(
    name = "Mac FYI Experimental Release (Apple M1)",
    triggered_by = ["GPU FYI Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Mac FYI Release (Apple M1)'.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "mac_arm64_apple_m1_gpu_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|Apple",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Mac FYI Experimental Release (Intel)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the combination of test_suites of 'Mac FYI Release (Intel)'
        # and 'Mac Release (Intel)' but with
        # 'gpu_fyi_only_mac_release_telemetry_tests' instead of
        # 'gpu_fyi_mac_release_telemetry_tests'.
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_mini_intel_gpu_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (AMD)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the combination of test_suites as 'Mac FYI Retina Release (AMD)'
        # and 'Mac Retina Release (AMD)'.
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_retina_amd_gpu_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (Apple M2)",
    description_html = "Runs standard GPU tests on experimental M2 configs",
    triggered_by = ["GPU FYI Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Mac FYI Retina Release (Apple
        # M2)'.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_arm64_apple_m2_retina_gpu_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|Apple",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (NVIDIA)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Mac FYI Retina Release (NVIDIA)'.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_retina_nvidia_gpu_experimental",
        ],
        per_test_modifications = {
            # "webgl2_conformance_metal_passthrough_graphite_tests": targets.remove(
            #     reason = "Not enough capacity.",
            # ),
            # "webgl_conformance_metal_passthrough_graphite_tests": targets.remove(
            #     reason = "crbug.com/1158857: re-enable when switching to Metal by default.",
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|Nvidia",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    execution_timeout = 12 * time.hour,
)

ci.thin_tester(
    name = "Mac FYI Release (Apple M1)",
    triggered_by = ["GPU FYI Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_arm64_apple_m1_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Apple",
        short_name = "m1",
    ),
)

ci.thin_tester(
    name = "Mac FYI Retina Release (Apple M2)",
    triggered_by = ["GPU FYI Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_arm64_apple_m2_retina_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Apple",
        short_name = "m2",
    ),
)

ci.thin_tester(
    name = "Mac FYI ASAN (Intel)",
    triggered_by = ["GPU FYI Mac Builder (asan)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
        per_test_modifications = {
            "webgl2_conformance_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            # "webgl2_conformance_metal_passthrough_graphite_tests": targets.mixin(
            #     args = [
            #         "--extra-browser-args=--disable-metal-shader-cache",
            #     ],
            # ),
            "webgl_conformance_metal_passthrough_ganesh_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            "webgl_conformance_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            # "webgl_conformance_metal_passthrough_graphite_tests": targets.mixin(
            #     args = [
            #         "--extra-browser-args=--disable-metal-shader-cache",
            #     ],
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "asn",
    ),
)

ci.thin_tester(
    name = "Mac FYI Release (Intel)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Mac FYI Retina ASAN (AMD)",
    triggered_by = ["GPU FYI Mac Builder (asan)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_only_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
        per_test_modifications = {
            "context_lost_metal_passthrough_ganesh_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "context_lost_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "expected_color_pixel_metal_passthrough_ganesh_test": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "expected_color_pixel_metal_passthrough_graphite_test": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "gpu_process_launch_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "hardware_accelerated_feature_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "info_collection_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "pixel_skia_gold_metal_passthrough_ganesh_test": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "pixel_skia_gold_metal_passthrough_graphite_test": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "screenshot_sync_metal_passthrough_ganesh_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "screenshot_sync_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "trace_test": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "webcodecs_metal_passthrough_ganesh_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "webcodecs_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1458020 for Mac Retina ASAN removal",
            ),
            "webgl2_conformance_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            # "webgl2_conformance_metal_passthrough_graphite_tests": targets.mixin(
            #     args = [
            #         "--extra-browser-args=--disable-metal-shader-cache",
            #     ],
            # ),
            "webgl_conformance_metal_passthrough_ganesh_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            "webgl_conformance_metal_passthrough_graphite_tests": targets.remove(
                reason = "crbug.com/1270755",
            ),
            # "webgl_conformance_metal_passthrough_graphite_tests": targets.mixin(
            #     args = [
            #         "--extra-browser-args=--disable-metal-shader-cache",
            #     ],
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "asn",
    ),
)

ci.thin_tester(
    name = "Mac FYI Retina Debug (AMD)",
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_debug_gtests",
            "gpu_common_gl_passthrough_ganesh_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "dbg",
    ),
)

ci.thin_tester(
    name = "Mac FYI Retina Release (AMD)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_mac_release_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Mac FYI Retina Release (NVIDIA)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_mac_nvidia_release_telemetry_tests",
        ],
        mixins = [
            "mac_retina_nvidia_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Mac Pro FYI Release (AMD)",
    triggered_by = ["GPU FYI Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_mac_release_gtests",
            "gpu_fyi_mac_pro_release_telemetry_tests",
        ],
        mixins = [
            "mac_pro_amd_gpu",
        ],
        per_test_modifications = {
            "services_unittests": targets.remove(
                reason = "The face and barcode detection tests fail on the Mac Pros.",
            ),
            "webgl2_conformance_metal_passthrough_graphite_tests": targets.per_test_modification(
                replacements = targets.replacements(
                    args = {
                        # Causes problems on older hardware. crbug.com/1499911.
                        "--enable-metal-debug-layers": None,
                    },
                ),
            ),
            "webgl_conformance_metal_passthrough_graphite_tests": targets.per_test_modification(
                replacements = targets.replacements(
                    args = {
                        # Causes problems on older hardware. crbug.com/1499911.
                        "--enable-metal-debug-layers": None,
                    },
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Pro",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Debug (NVIDIA)",
    triggered_by = ["GPU FYI Win x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_debug_telemetry_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "media_foundation_browser_tests": targets.remove(
                reason = [
                    "TODO(crbug.com/40912267): Enable Media Foundation browser tests on NVIDIA",
                    "gpu bots once the Windows OS supports HW secure decryption.",
                ],
            ),
            "tab_capture_end2end_tests": targets.remove(
                reason = "Run these only on Release bots.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "dbg",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)",
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "dbg",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Release (NVIDIA)",
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Experimental Release (Intel)",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as
        # 'Win10 FYI x64 Release (Intel)'
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_intel_uhd_630_experimental",
        ],
        per_test_modifications = {
            # "context_lost_passthrough_tests": targets.per_test_modification(
            #     mixins = targets.mixin(
            #         args = [
            #             # TODO(crbug.com/41496675): Apply this via magic subustitutions if it
            #             # helps with stability.
            #             "--jobs=2",
            #         ],
            #     ),
            #     replacements = targets.replacements(
            #         # Magic substitution happens after regular replacement, so remove it
            #         # now since we are manually applying the number of jobs above.
            #         args = {
            #             targets.magic_args.GPU_PARALLEL_JOBS: None,
            #         },
            #     ),
            # ),
            # "pixel_skia_gold_passthrough_test": targets.per_test_modification(
            #     mixins = targets.mixin(
            #         args = [
            #             # TODO(crbug.com/41496675): Apply this via magic subustitutions if it
            #             # helps with stability.
            #             "--jobs=2",
            #         ],
            #     ),
            #     replacements = targets.replacements(
            #         # Magic substitution happens after regular replacement, so remove it
            #         # now since we are manually applying the number of jobs above.
            #         args = {
            #             targets.magic_args.GPU_PARALLEL_JOBS: None,
            #         },
            #     ),
            # ),
            # "trace_test": targets.per_test_modification(
            #     mixins = targets.mixin(
            #         args = [
            #             # TODO(crbug.com/41496675): Apply this via magic subustitutions if it
            #             # helps with stability.
            #             "--jobs=2",
            #         ],
            #     ),
            #     replacements = targets.replacements(
            #         # Magic substitution happens after regular replacement, so remove it
            #         # now since we are manually applying the number of jobs above.
            #         args = {
            #             targets.magic_args.GPU_PARALLEL_JOBS: None,
            #         },
            #     ),
            # ),
            # "video_decode_accelerator_gl_unittest": targets.remove(
            #     reason = "Windows Intel doesn't have the GL extensions to support this test.",
            # ),
            # "webcodecs_tests": targets.per_test_modification(
            #     mixins = targets.mixin(
            #         args = [
            #             # TODO(crbug.com/41496675): Apply this via magic subustitutions if it
            #             # helps with stability.
            #             "--jobs=2",
            #         ],
            #     ),
            #     replacements = targets.replacements(
            #         # Magic substitution happens after regular replacement, so remove it
            #         # now since we are manually applying the number of jobs above.
            #         args = {
            #             targets.magic_args.GPU_PARALLEL_JOBS: None,
            #         },
            #     ),
            # ),
            # "xr_browser_tests": targets.mixin(
            #     args = [
            #         # TODO(crbug.com/40937024): Remove this once the flakes on Intel are
            #         # resolved.
            #         "--gtest_filter=-WebXrVrOpenXrBrowserTest.TestNoStalledFrameLoop",
            #     ],
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Windows|10|x64|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    # TODO(kbr): "Experimental" caused too-long path names pre-LUCI.
    # crbug.com/812000
    name = "Win10 FYI x64 Exp Release (NVIDIA)",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as
        # 'Win10 FYI x64 Release (NVIDIA)'
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_release_telemetry_tests",
            "gpu_fyi_win_optional_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_nvidia_gtx_1660_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (AMD RX 5500 XT)",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_amd_release_telemetry_tests",
        ],
        mixins = [
            "win10_amd_rx_5500_xt_stable",
        ],
        per_test_modifications = {
            "gl_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.amd.5500xt.gl_unittests.filter",
                ],
            ),
            "media_foundation_browser_tests": targets.remove(
                reason = [
                    "TODO(crbug.com/40912267): Enable Media Foundation browser tests on NVIDIA",
                    "gpu bots once the Windows OS supports HW secure decryption.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|AMD",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (Intel)",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_intel_release_telemetry_tests",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "xr_browser_tests": targets.mixin(
                args = [
                    # TODO(crbug.com/40937024): Remove this once the flakes on Intel are
                    # resolved.
                    "--gtest_filter=-WebXrVrOpenXrBrowserTest.TestNoStalledFrameLoop",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (Intel UHD 770)",
    description_html = "Runs GPU tests on 12th gen Intel CPUs with UHD 770 GPUs",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_intel_release_telemetry_tests",
        ],
        mixins = [
            "win10_intel_uhd_770_stable",
        ],
        per_test_modifications = {
            "gl_tests_passthrough": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.uhd_770.gl_tests_passthrough.filter",
                ],
            ),
            "xr_browser_tests": targets.mixin(
                args = [
                    # TODO(crbug.com/40937024): Remove this once the flakes on Intel are
                    # resolved.
                    "--gtest_filter=-WebXrVrOpenXrBrowserTest.TestNoStalledFrameLoop",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "770",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (NVIDIA)",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_release_telemetry_tests",
            "gpu_fyi_win_optional_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "media_foundation_browser_tests": targets.remove(
                reason = [
                    "TODO(crbug.com/40912267): Enable Media Foundation browser tests on NVIDIA",
                    "gpu bots once the Windows OS supports HW secure decryption.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (NVIDIA RTX 4070 Super)",
    description_html = "Runs GPU tests on NVIDIA RTX 4070 Super GPUs",
    triggered_by = ["GPU FYI Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_release_telemetry_tests",
            "gpu_fyi_win_optional_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_rtx_4070_super_stable",
            "limited_capacity_bot",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "4070",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x64 Release XR Perf (NVIDIA)",
    triggered_by = ["GPU FYI XR Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "win_specific_xr_perf_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "xr",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x86 Release (NVIDIA)",
    triggered_by = ["GPU FYI Win Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_optional_isolated_scripts",
            "gpu_fyi_win_release_telemetry_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "media_foundation_browser_tests": targets.remove(
                reason = [
                    "TODO(crbug.com/40912267): Enable Media Foundation browser tests on NVIDIA",
                    "gpu bots once the Windows OS supports HW secure decryption.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x86|Nvidia",
        short_name = "rel",
    ),
)

ci.thin_tester(
    name = "Win11 FYI arm64 Release (Qualcomm Adreno 690)",
    description_html = "Triggers GPU tests on Windows arm64 devices with Adreno 690 GPUs",
    triggered_by = ["GPU FYI Win arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_fyi_win_gtests",
            "gpu_fyi_win_release_telemetry_tests",
        ],
        mixins = [
            "win11_qualcomm_adreno_690_stable",
        ],
        per_test_modifications = {
            "context_lost_passthrough_graphite_tests": targets.remove(
                reason = "Test is not high priority and win11/arm has limited capacity.",
            ),
            "context_lost_passthrough_tests": targets.per_test_modification(
                mixins = targets.mixin(
                    # These devices have issues running these tests in parallel.
                    args = [
                        "--jobs=1",
                    ],
                ),
                replacements = targets.replacements(
                    # Magic substitution happens after regular replacement, so remove it
                    # now since we are manually applying the number of jobs above.
                    args = {
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
            "gl_unittests": targets.mixin(
                args = [
                    # crbug.com/1523061
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.win_arm64.gl_unittests.filter",
                ],
            ),
            "services_webnn_unittests": targets.mixin(
                args = [
                    # crbug.com/1522972
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.win_arm64.services_webnn_unittests.filter",
                ],
            ),
            "webcodecs_tests": targets.per_test_modification(
                mixins = targets.mixin(
                    # These devices have issues running these tests in parallel.
                    # TODO(crbug.com/346406092): Once addressed, remove this block.
                    args = [
                        "--jobs=1",
                    ],
                ),
                replacements = targets.replacements(
                    # Magic substitution happens after regular replacement, so remove it
                    # now since we are manually applying the number of jobs above.
                    args = {
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
            "webgl_conformance_d3d9_passthrough_tests": targets.remove(
                reason = "Per discussion on crbug.com/1523698, we aren't interested in testing D3D9 on this newer hardware.",
            ),
            "webgl_conformance_vulkan_passthrough_tests": targets.remove(
                reason = "Vulkan is not supported on these devices.",
            ),
            "xr_browser_tests": targets.remove(
                reason = "No Windows arm64 devices currently support XR features, so don't bother running related tests.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|11|arm64|Qualcomm",
        short_name = "rel",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win arm64 Builder",
    description_html = "Parent GPU builder for Windows arm64 release builds",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "win",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "a64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win Builder",
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
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "x86",
            "win",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x86",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            # Remove this once the decision to use cross-compilation or not in
            # crbug.com/1510985 is made.
            "win_cross",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "debug_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Debug",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "dx12vk",
            "release_builder",
            "try_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "rel",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "dx12vk",
            "debug_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "dbg",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu_fyi_windows_builder(
    name = "GPU FYI XR Win x64 Builder",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        # This causes the builder to upload isolates to a location where
        # Pinpoint can access them in addition to the usual isolate
        # server. This is necessary because "Win10 FYI x64 Release XR
        # perf (NVIDIA)", which is a child of this builder, uploads perf
        # results, and Pinpoint may trigger additional builds on this
        # builder during a bisect.
        perf_isolate_upload = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|XR",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)
