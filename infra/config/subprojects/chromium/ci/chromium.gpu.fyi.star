# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "sheriff_rotations")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.gpu.fyi",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.gpu.POOL,
    properties = {
        "perf_dashboard_machine_group": "ChromiumGPUFYI",
    },
    service_account = ci.gpu.SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM_GPU,
    thin_tester_cores = 2,
)

consoles.console_view(
    name = "chromium.gpu.fyi",
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
        "Lacros": "*builder*",
    },
)

def gpu_fyi_windows_builder(*, name, **kwargs):
    kwargs.setdefault("execution_timeout", ci.DEFAULT_EXECUTION_TIMEOUT)
    return ci.gpu.windows_builder(name = name, **kwargs)

ci.thin_tester(
    name = "Android FYI Release (NVIDIA Shield TV)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|P32|NVDA",
        short_name = "STV",
    ),
    triggered_by = ["GPU FYI Android arm Builder"],
)

ci.thin_tester(
    name = "Android FYI Release (Nexus 5)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|L32",
        short_name = "N5",
    ),
    triggered_by = ["GPU FYI Android arm Builder"],
)

ci.thin_tester(
    name = "Android FYI Release (Nexus 5X)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|M64|QCOM",
        short_name = "N5X",
    ),
    triggered_by = ["GPU FYI Android arm64 Builder"],
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 2)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|P32|QCOM",
        short_name = "P2",
    ),
    triggered_by = ["GPU FYI Android arm Builder"],
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 4)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|R32|QCOM",
        short_name = "P4",
    ),
    triggered_by = ["GPU FYI Android arm Builder"],
)

ci.thin_tester(
    name = "Android FYI Release (Pixel 6)",
    # TODO(crbug.com/1280418): Revert this to the default once more Pixel 6
    # capacity is deployed.
    execution_timeout = 8 * time.hour,
    console_view_entry = consoles.console_view_entry(
        category = "Android|S64|ARM",
        short_name = "P6",
    ),
    triggered_by = ["GPU FYI Android arm64 Builder"],
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release (amd64-generic)",
    # Runs a lot of tests + VMs are slower than real hardware, so increase the
    # timeout.
    execution_timeout = 8 * time.hour,
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|LLVM",
        short_name = "gen",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "gpu-fyi-chromeos-jacuzzi-exp",
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|ARM",
        short_name = "jcz",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release (kevin)",
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|ARM",
        short_name = "kvn",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "gpu-fyi-chromeos-octopus-exp",
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|Intel",
        short_name = "oct",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.gpu.linux_builder(
    name = "gpu-fyi-chromeos-zork-exp",
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["zork"],
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|AMD",
        short_name = "zrk",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.gpu.linux_builder(
    name = "GPU FYI Android arm Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder",
        short_name = "arm",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "GPU FYI Android arm64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder",
        short_name = "arm64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "GPU FYI Lacros x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Lacros|Builder",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "dbg",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "Linux FYI GPU TSAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "tsn",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "rel",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder (asan)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "asn",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "dbg",
    ),
)

ci.gpu.mac_builder(
    name = "GPU FYI Mac arm64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder",
        short_name = "arm",
    ),
)

ci.thin_tester(
    name = "Lacros FYI x64 Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Lacros|AMD",
        short_name = "amd",
    ),
    triggered_by = ["GPU FYI Lacros x64 Builder"],
)

ci.thin_tester(
    name = "Lacros FYI x64 Release (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Lacros|Intel",
        short_name = "int",
    ),
    triggered_by = ["GPU FYI Lacros x64 Builder"],
)

ci.thin_tester(
    name = "Linux FYI Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Linux Builder (dbg)"],
)

ci.thin_tester(
    name = "Linux FYI Experimental Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Experimental Release (NVIDIA)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Linux|Nvidia",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Release (AMD RX 5500 XT)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Release (Intel UHD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "uhd",
    ),
    # TODO(https://crbug.com/986939): Remove this increased timeout once more
    # devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Mac FYI Debug (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.thin_tester(
    name = "Mac FYI Experimental Release (Intel)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (AMD)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|AMD|Retina",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (NVIDIA)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Mac|Nvidia",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Release (Apple M1)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Apple",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac arm64 Builder"],
)

ci.thin_tester(
    name = "Mac FYI ASAN (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "asn",
    ),
    triggered_by = ["GPU FYI Mac Builder (asan)"],
)

ci.thin_tester(
    name = "Mac FYI Release (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Retina ASAN (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "asn",
    ),
    triggered_by = ["GPU FYI Mac Builder (asan)"],
)

ci.thin_tester(
    name = "Mac FYI Retina Debug (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.thin_tester(
    name = "Mac FYI Retina Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.thin_tester(
    name = "Mac FYI Retina Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Retina Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac Pro FYI Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Pro",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Win x64 Builder (dbg)"],
)

ci.thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder (dbg)"],
)

ci.thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Exp Release (Intel HD 630)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Windows|10|x64|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Exp Release (NVIDIA)",
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Windows|10|x64|Nvidia",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (AMD RX 5500 XT)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Release XR Perf (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "xr",
    ),
    triggered_by = ["GPU FYI XR Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x86 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x86|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x86",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Debug",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "dbg",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

gpu_fyi_windows_builder(
    name = "GPU FYI XR Win x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|XR",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)
