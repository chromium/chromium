# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "gardener_rotations", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/builder_health_indicators.star", "health_spec")

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
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "exp",
    ),
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
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Windows|10|x64|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
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
