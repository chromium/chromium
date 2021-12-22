# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu.fyi builder group."""

load("//lib/builders.star", "goma", "sheriff_rotations")
load("//lib/ci.star", "ci")
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
        "Android": ["L32", "M64", "N64", "P32", "vk", "skgl", "skv"],
        "Android|M64": ["QCOM"],
    },
)

def gpu_fyi_windows_builder(*, name, **kwargs):
    kwargs.setdefault("execution_timeout", ci.DEFAULT_EXECUTION_TIMEOUT)
    return ci.gpu.windows_builder(name = name, **kwargs)

ci.gpu.linux_builder(
    name = "Android FYI Release (NVIDIA Shield TV)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|N64|NVDA",
        short_name = "STV",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI Release (Nexus 5)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|L32",
        short_name = "N5",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI Release (Nexus 5X)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|M64|QCOM",
        short_name = "N5X",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI Release (Nexus 9)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|M64|NVDA",
        short_name = "N9",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI Release (Pixel 2)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|P32|QCOM",
        short_name = "P2",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI Release (Pixel 4)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|R32|QCOM",
        short_name = "P4",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI SkiaRenderer GL (Nexus 5X)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|skgl|M64",
        short_name = "N5X",
    ),
)

ci.gpu.linux_builder(
    name = "Android FYI SkiaRenderer Vulkan (Pixel 2)",
    console_view_entry = consoles.console_view_entry(
        category = "Android|skv|P32",
        short_name = "P2",
    ),
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release (amd64-generic)",
    # Runs a lot of tests + VMs are slower than real hardware, so increase the
    # timeout.
    execution_timeout = 8 * time.hour,
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|amd64|generic",
        short_name = "x64",
    ),
)

ci.gpu.linux_builder(
    name = "ChromeOS FYI Release (kevin)",
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|arm|kevin",
        short_name = "kvn",
    ),
)

ci.gpu.linux_builder(
    name = "GPU FYI Lacros x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Lacros|Builder",
        short_name = "rel",
    ),
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "rel",
    ),
)

ci.gpu.linux_builder(
    name = "GPU FYI Linux Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder",
        short_name = "dbg",
    ),
)

ci.gpu.linux_builder(
    name = "Linux FYI GPU TSAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "tsn",
    ),
)

# Builder + tester.
ci.gpu.linux_builder(
    name = "Linux FYI SkiaRenderer Dawn Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "skd",
    ),
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
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI Experimental Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "exp",
    ),
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
    name = "Linux FYI SkiaRenderer Vulkan (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel",
        short_name = "skv",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.thin_tester(
    name = "Linux FYI SkiaRenderer Vulkan (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "skv",
    ),
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
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.thin_tester(
    name = "Mac FYI Experimental Retina Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "exp",
    ),
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
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.thin_tester(
    name = "Win10 FYI x64 Exp Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "exp",
    ),
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

# Builder + tester.
gpu_fyi_windows_builder(
    name = "Win10 FYI x64 SkiaRenderer Dawn Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "skd",
    ),
)

ci.thin_tester(
    name = "Win10 FYI x86 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|10|x86|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.thin_tester(
    name = "Win7 FYI Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|7|x86|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.thin_tester(
    name = "Win7 FYI Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|7|x86|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.thin_tester(
    name = "Win7 FYI x64 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|7|x64|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x86",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x64",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Debug",
        short_name = "x64",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "rel",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "dbg",
    ),
)

gpu_fyi_windows_builder(
    name = "GPU FYI XR Win x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|XR",
        short_name = "x64",
    ),
)
