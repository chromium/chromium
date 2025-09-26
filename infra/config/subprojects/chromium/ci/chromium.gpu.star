# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "cpu")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.gpu",
    pool = gpu.ci.POOL,
    gardener_rotations = gardener_rotations.CHROMIUM_GPU,
    tree_closing = True,
    tree_closing_notifiers = gpu.ci.TREE_CLOSING_NOTIFIERS,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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
    name = "chromium.gpu",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
    ordering = {
        None: [
            "Windows",
            "Mac",
            "Linux",
        ],
    },
)

gpu.ci.linux_builder(
    name = "Android Release (Nexus 5X)",
    description_html = "Runs a subset of release GPU tests on stable Nexus 5X configs",
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
        build_gs_bucket = "chromium-gpu-archive",
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
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "chromium_nexus_5x_oreo",
        ],
        per_test_modifications = {
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
        category = "Android",
        short_name = "N5X",
    ),
    cq_mirrors_console_view = "mirrors",
)

gpu.ci.linux_builder(
    name = "Android Release (Pixel 2)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Runs a subset of release GPU tests on stable Pixel 2 configs",
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
        build_gs_bucket = "chromium-gpu-archive",
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
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_android_telemetry_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android",
        short_name = "P2",
    ),
    cq_mirrors_console_view = "mirrors",
)

gpu.ci.linux_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Builds release Linux x64 binaries for GPU testing",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Linux Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

gpu.ci.linux_builder(
    name = "GPU Linux Builder (dbg)",
    description_html = "Builds debug Linux x64 binaries for GPU testing",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
)

gpu.ci.mac_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Builds release Mac x64 binaries for GPU testing",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Mac Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "x64",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

gpu.ci.mac_builder(
    name = "GPU Mac Builder (dbg)",
    description_html = "Builds debug Mac x64 binaries for GPU testing",
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "x64",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    cores = None,
    cpu = cpu.ARM64,
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
)

gpu.ci.windows_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Builds release x64 Windows binaries for GPU testing",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Mac Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
            ],
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "resource_allowlisting",
            "win",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
)

gpu.ci.windows_builder(
    name = "GPU Win x64 Builder (dbg)",
    description_html = "Builds debug Windows x64 binaries for GPU testing",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    # TODO(crbug.com/413285147): Restore this to the default once sync/compile
    # times are reduced.
    execution_timeout = 6 * time.hour,
)

ci.thin_tester(
    name = "Linux Debug (NVIDIA)",
    description_html = "Runs a subset of debug GPU tests on stable Linux/NVIDIA GTX 1660 configs",
    parent = "GPU Linux Builder (dbg)",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_linux_telemetry_tests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
            # TODO(crbug.com/331756538): Specify the puppet_production mixin
            # once testing is moved to Ubuntu 22.
        ],
        per_test_modifications = {
            "tab_capture_end2end_tests": targets.remove(
                reason = "Run these only on Release bots.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.LINUX,
    ),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
)

ci.thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Runs a subset of release GPU tests on stable Linux/NVIDIA GTX 1660 configs",
    parent = "ci/GPU Linux Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_linux_telemetry_tests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
            # TODO(crbug.com/331756538): Specify the puppet_production mixin
            # once testing is moved to Ubuntu 22.
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
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac Debug (Intel)",
    description_html = "Runs a subset of debug GPU tests on stable Mac/Intel UHD 630 Mac Mini configs",
    parent = "GPU Mac Builder (dbg)",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_metal_passthrough_graphite_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
            "puppet_production",
        ],
        per_test_modifications = {
            "tab_capture_end2end_tests": targets.remove(
                reason = "Run these only on Release bots.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.MAC,
    ),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
)

ci.thin_tester(
    name = "Mac Release (Intel)",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Runs a subset of release GPU tests on stable Mac/Intel UHD 630 Mac Mini configs",
    parent = "ci/GPU Mac Builder",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_metal_passthrough_graphite_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
            "puppet_production",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac Retina Debug (AMD)",
    description_html = "Runs a subset of debug GPU tests on stable Mac/AMD Macbook Pro configs",
    parent = "GPU Mac Builder (dbg)",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_metal_passthrough_graphite_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
            "puppet_production",
        ],
        per_test_modifications = {
            "tab_capture_end2end_tests": targets.remove(
                reason = "Run these only on Release bots.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG,
        os_type = targets.os_type.MAC,
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
)

ci.thin_tester(
    name = "Mac Retina Release (AMD)",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Runs a subset of release GPU tests on stable Mac/AMD Macbook Pro configs",
    parent = "ci/GPU Mac Builder",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_desktop_passthrough_gtests",
            "gpu_common_metal_passthrough_graphite_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
            "puppet_production",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Win10 x64 Debug (NVIDIA)",
    description_html = "Runs a subset of debug GPU tests on stable Windows 10/NVIDIA GTX 1660 configs",
    parent = "GPU Win x64 Builder (dbg)",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_win_gtests",
            "gpu_common_win_telemetry_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
            "puppet_production",
        ],
        per_test_modifications = {
            "pixel_skia_gold_passthrough_test": targets.per_test_modification(
                mixins = targets.mixin(
                    args = [
                        # TODO(crbug.com/382422293): Remove when fixed
                        "--jobs=1",
                    ],
                ),
                replacements = targets.replacements(
                    args = {
                        # Magic substitution happens after regular replacement, so remove it
                        # now since we are manually applying the number of jobs above.
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.DEBUG_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
)

ci.thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Runs a subset of release GPU tests on stable Windows 10/NVIDIA GTX 1660 configs",
    parent = "ci/GPU Win x64 Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
            ],
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_win_gtests",
            "gpu_common_win_telemetry_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
            "puppet_production",
        ],
        per_test_modifications = {
            "pixel_skia_gold_passthrough_test": targets.per_test_modification(
                mixins = targets.mixin(
                    args = [
                        # TODO(crbug.com/382422293): Remove when fixed
                        "--jobs=1",
                    ],
                ),
                replacements = targets.replacements(
                    args = {
                        # Magic substitution happens after regular replacement, so remove it
                        # now since we are manually applying the number of jobs above.
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
)
