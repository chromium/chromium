# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.angle builder group."""

load("//lib/builders.star", "cpu", "gardener_rotations", "os", "siso")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = "recipe:angle_chromium",
    builder_group = "chromium.angle",
    pool = ci.gpu.POOL,
    gardener_rotations = gardener_rotations.ANGLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    properties = {
        "perf_dashboard_machine_group": "ChromiumANGLE",
    },
    service_account = ci.gpu.SERVICE_ACCOUNT,
    shadow_service_account = ci.gpu.SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
)

consoles.console_view(
    name = "chromium.angle",
    ordering = {
        None: ["Android", "Fuchsia", "Linux", "Mac", "iOS", "Windows", "Perf"],
        "*builder*": ["Builder"],
        "Android": "*builder*",
        "Fuchsia": "*builder*",
        "Linux": "*builder*",
        "Mac": "*builder*",
        "iOS": "*builder*",
        "Windows": "*builder*",
        "Perf": "*builder*",
    },
)

ci.gpu.linux_builder(
    name = "android-angle-chromium-arm64-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder_without_codecs",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "arm64",
            "static_angle",
            "android_fastbuild",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder|Chromium",
        short_name = "arm64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "android-angle-chromium-arm64-pixel2",
    description_html = "Running Angle chromium tests on Pixel 2",
    triggered_by = ["android-angle-chromium-arm64-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Pixel2|Chromium",
        short_name = "arm64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.linux_builder(
    name = "fuchsia-angle-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "fuchsia",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "fuchsia",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Fuchsia|Builder|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.linux_builder(
    name = "linux-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "linux-angle-chromium-intel",
    triggered_by = ["linux-angle-chromium-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "linux-angle-chromium-nvidia",
    triggered_by = ["linux-angle-chromium-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|NVIDIA|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.mac_builder(
    name = "mac-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "mac",
            "x64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "mac-angle-chromium-amd",
    triggered_by = ["mac-angle-chromium-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "mac-angle-chromium-intel",
    triggered_by = ["mac-angle-chromium-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.mac_builder(
    name = "ios-angle-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "ios_simulator",
            "x64",
            "xctest",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Builder|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
    xcode = xcode.xcode_default,
)

ci.thin_tester(
    name = "ios-angle-intel",
    triggered_by = ["ios-angle-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Intel|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.windows_builder(
    name = "win-angle-chromium-x64-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-intel",
    triggered_by = ["win-angle-chromium-x64-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-nvidia",
    triggered_by = ["win-angle-chromium-x64-builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
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
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|NVIDIA|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.gpu.windows_builder(
    name = "win-angle-chromium-x86-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
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
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x86",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x86",
    ),
    contact_team_email = "angle-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)
