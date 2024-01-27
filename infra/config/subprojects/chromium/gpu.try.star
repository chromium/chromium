# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    bucket = "try",
    executable = "recipe:chromium_trybot",
    pool = "luci.chromium.try",
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    contact_team_email = "chrome-gpu-infra@google.com",
    cq_group = "cq",
    execution_timeout = 6 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    subproject_list_view = "luci.chromium.try",
    task_template_canary_percentage = 5,
)

# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for GPU builders in each builder group where
# they appear: gpu_XXX_builder where XXX is the part after the last dot in the
# builder group
# Builder functions are defined for each builder group, with additional
# functions for specializing on OS: XXX_builder and XXX_YYY_builder where XXX is
# the part after the last dot in the builder group and YYY is the OS

def gpu_android_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.android",
        builderless = True,
        reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
        ssd = None,
        **kwargs
    )

gpu_android_builder(
    name = "gpu-fyi-try-android-m-nexus-5x-64",
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Release (Nexus 5X)",
    ],
    gn_args = "ci/GPU FYI Android arm64 Builder",
    pool = "luci.chromium.gpu.android.nexus5x.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-nvidia-shield-tv",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (NVIDIA Shield TV)",
    ],
    gn_args = "ci/GPU FYI Android arm Builder",
    pool = "luci.chromium.gpu.android.nvidia.shield.tv.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-p-pixel-2-32",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (Pixel 2)",
    ],
    gn_args = "ci/GPU FYI Android arm Builder",
    pool = "luci.chromium.gpu.android.pixel2.chromium.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-r-pixel-4-32",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (Pixel 4)",
    ],
    gn_args = "ci/GPU FYI Android arm Builder",
    pool = "luci.chromium.gpu.android.pixel4.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-pixel-6-64",
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Release (Pixel 6)",
    ],
    gn_args = "ci/GPU FYI Android arm64 Builder",
    pool = "luci.chromium.gpu.android.pixel6.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-pixel-6-64-exp",
    description_html = "Runs standard GPU tests on experimental Pixel 6 configs",
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Experimental Release (Pixel 6)",
    ],
    gn_args = "ci/GPU FYI Android arm64 Builder",
    pool = "luci.chromium.gpu.android.pixel6.try",
)

gpu_android_builder(
    name = "gpu-try-android-m-nexus-5x-64",
    mirrors = [
        "ci/Android Release (Nexus 5X)",
    ],
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "android_builder",
            "release_builder",
            "try_builder",
            "reclient",
            "arm64",
            "static_angle",
        ],
    ),
    pool = "luci.chromium.gpu.android.nexus5x.try",
)

def gpu_chromeos_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.chromiumos",
        builderless = True,
        reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
        ssd = None,
        **kwargs
    )

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-amd64-generic",
    mirrors = [
        "ci/ChromeOS FYI Release (amd64-generic)",
    ],
    gn_args = "ci/ChromeOS FYI Release (amd64-generic)",
    pool = "luci.chromium.gpu.chromeos.amd64.generic.try",
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-skylab-volteer",
    description_html = "Runs standard GPU tests on Skylab-hosted volteer devices",
    mirrors = [
        "ci/ChromeOS FYI Release Skylab (volteer)",
    ],
    gn_args = "ci/ChromeOS FYI Release Skylab (volteer)",
    pool = "luci.chromium.gpu.chromeos.volteer.try",
)

def gpu_linux_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.linux",
        builderless = True,
        reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
        ssd = None,
        **kwargs
    )

gpu_linux_builder(
    name = "gpu-fyi-try-lacros-amd-rel",
    mirrors = [
        "ci/GPU FYI Lacros x64 Builder",
        "ci/Lacros FYI x64 Release (AMD)",
    ],
    gn_args = "ci/GPU FYI Lacros x64 Builder",
    pool = "luci.chromium.gpu.linux.amd.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-lacros-intel-rel",
    mirrors = [
        "ci/GPU FYI Lacros x64 Builder",
        "ci/Lacros FYI x64 Release (Intel)",
    ],
    gn_args = "ci/GPU FYI Lacros x64 Builder",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-amd-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (AMD RX 5500 XT)",
    ],
    gn_args = "ci/GPU FYI Linux Builder",
    pool = "luci.chromium.gpu.linux.amd.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-exp",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Experimental Release (Intel UHD 630)",
    ],
    gn_args = "ci/GPU FYI Linux Builder",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (Intel UHD 630)",
    ],
    gn_args = "ci/GPU FYI Linux Builder",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-dbg",
    mirrors = [
        "ci/GPU FYI Linux Builder (dbg)",
        "ci/Linux FYI Debug (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Linux Builder (dbg)",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-exp",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Experimental Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Linux Builder",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Linux Builder",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-tsn",
    mirrors = [
        "ci/Linux FYI GPU TSAN Release",
    ],
    gn_args = "ci/Linux FYI GPU TSAN Release",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-dbg",
    mirrors = [
        "ci/GPU Linux Builder (dbg)",
        "ci/Linux Debug (NVIDIA)",
    ],
    gn_args = "ci/GPU Linux Builder (dbg)",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-rel",
    mirrors = [
        "ci/GPU Linux Builder",
        "ci/Linux Release (NVIDIA)",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/GPU Linux Builder",
            "no_symbols",
        ],
    ),
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

def gpu_mac_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        builderless = True,
        cores = None,
        # Builders can run on either Intel or Apple Silicon hadware.
        cpu = None,
        os = os.MAC_ANY,
        ssd = None,
        **kwargs
    )

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-pro-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac Pro FYI Release (AMD)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.pro.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-asan",
    mirrors = [
        "ci/GPU FYI Mac Builder (asan)",
        "ci/Mac FYI Retina ASAN (AMD)",
    ],
    gn_args = "ci/GPU FYI Mac Builder (asan)",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-dbg",
    mirrors = [
        "ci/GPU FYI Mac Builder (dbg)",
        "ci/Mac FYI Retina Debug (AMD)",
    ],
    gn_args = "ci/GPU FYI Mac Builder (dbg)",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Retina Release (AMD)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Retina Release (AMD)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m1-exp",
    mirrors = [
        "ci/GPU FYI Mac arm64 Builder",
        "ci/Mac FYI Experimental Release (Apple M1)",
    ],
    gn_args = "ci/GPU FYI Mac arm64 Builder",
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m1-rel",
    mirrors = [
        "ci/GPU FYI Mac arm64 Builder",
        "ci/Mac FYI Release (Apple M1)",
    ],
    gn_args = "ci/GPU FYI Mac arm64 Builder",
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m2-retina-rel",
    mirrors = [
        "ci/GPU FYI Mac arm64 Builder",
        "ci/Mac FYI Retina Release (Apple M2)",
    ],
    gn_args = "ci/GPU FYI Mac arm64 Builder",
    pool = "luci.chromium.gpu.mac.arm64.apple.m2.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-asan",
    mirrors = [
        "ci/GPU FYI Mac Builder (asan)",
        "ci/Mac FYI ASAN (Intel)",
    ],
    gn_args = "ci/GPU FYI Mac Builder (asan)",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-dbg",
    mirrors = [
        "ci/GPU FYI Mac Builder (dbg)",
        "ci/Mac FYI Debug (Intel)",
    ],
    gn_args = "ci/GPU FYI Mac Builder (dbg)",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Release (Intel)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Release (Intel)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Retina Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
    # This bot has one machine backing its tests at the moment.
    # If it gets more, the modified execution_timeout should be removed.
    execution_timeout = 12 * time.hour,
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Retina Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Mac Builder",
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-amd-retina-dbg",
    mirrors = [
        "ci/GPU Mac Builder (dbg)",
        "ci/Mac Retina Debug (AMD)",
    ],
    gn_args = "ci/GPU Mac Builder (dbg)",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-intel-dbg",
    mirrors = [
        "ci/GPU Mac Builder (dbg)",
        "ci/Mac Debug (Intel)",
    ],
    gn_args = "ci/GPU Mac Builder (dbg)",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

def gpu_win_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.win",
        builderless = True,
        os = os.WINDOWS_ANY,
        reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
        ssd = None,
        **kwargs
    )

gpu_win_builder(
    name = "gpu-fyi-try-win10-amd-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Release (AMD RX 5500 XT)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder",
    pool = "luci.chromium.gpu.win10.amd.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-exp-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Experimental Release (Intel)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder",
    pool = "luci.chromium.gpu.win10.intel.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Release (Intel)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder",
    pool = "luci.chromium.gpu.win10.intel.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dbg-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder (dbg)",
        "ci/Win10 FYI x64 Debug (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder (dbg)",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-dbg-64",
    mirrors = [
        "ci/GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
        "ci/Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 DX12 Vulkan Builder",
        "ci/Win10 FYI x64 DX12 Vulkan Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win x64 DX12 Vulkan Builder",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-exp-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Exp Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-32",
    mirrors = [
        "ci/GPU FYI Win Builder",
        "ci/Win10 FYI x86 Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win Builder",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Release (NVIDIA)",
    ],
    gn_args = "ci/GPU FYI Win x64 Builder",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win11-qualcomm-rel-64",
    description_html = "Triggers GPU tests on Windows arm64 devices",
    mirrors = [
        "ci/GPU FYI Win arm64 Builder",
        "ci/Win11 FYI arm64 Release (Qualcomm Adreno 690)",
    ],
    gn_args = "ci/GPU FYI Win arm64 Builder",
    pool = "luci.chromium.gpu.win11.qualcomm.try",
)

gpu_win_builder(
    name = "gpu-try-win-nvidia-dbg",
    mirrors = [
        "ci/GPU Win x64 Builder (dbg)",
        "ci/Win10 x64 Debug (NVIDIA)",
    ],
    gn_args = "ci/GPU Win x64 Builder (dbg)",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)
