# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "goma", "os", "reclient")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    executable = "recipe:chromium_trybot",
    pool = "luci.chromium.try",
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    cq_group = "cq",
    execution_timeout = 6 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
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
    pool = "luci.chromium.gpu.android.nexus5x.try",
    goma_backend = None,
)

gpu_android_builder(
    name = "gpu-fyi-try-android-nvidia-shield-tv",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (NVIDIA Shield TV)",
    ],
    pool = "luci.chromium.gpu.android.nvidia.shield.tv.try",
    goma_backend = None,
)

gpu_android_builder(
    name = "gpu-fyi-try-android-p-pixel-2-32",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (Pixel 2)",
    ],
    pool = "luci.chromium.gpu.android.pixel2.chromium.try",
    goma_backend = None,
)

gpu_android_builder(
    name = "gpu-fyi-try-android-r-pixel-4-32",
    mirrors = [
        "ci/GPU FYI Android arm Builder",
        "ci/Android FYI Release (Pixel 4)",
    ],
    pool = "luci.chromium.gpu.android.pixel4.try",
    goma_backend = None,
)

gpu_android_builder(
    name = "gpu-fyi-try-android-pixel-6-64",
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Release (Pixel 6)",
    ],
    pool = "luci.chromium.gpu.android.pixel6.try",
    goma_backend = None,
)

gpu_android_builder(
    name = "gpu-try-android-m-nexus-5x-64",
    mirrors = [
        "ci/Android Release (Nexus 5X)",
    ],
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
    pool = "luci.chromium.gpu.chromeos.amd64.generic.try",
    goma_backend = None,
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-jacuzzi-exp",
    mirrors = [
        "ci/gpu-fyi-chromeos-jacuzzi-exp",
    ],
    pool = "luci.chromium.gpu.chromeos.jacuzzi.try",
    goma_backend = None,
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-kevin",
    mirrors = [
        "ci/ChromeOS FYI Release (kevin)",
    ],
    pool = "luci.chromium.gpu.chromeos.kevin.try",
    goma_backend = None,
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-octopus-exp",
    mirrors = [
        "ci/gpu-fyi-chromeos-octopus-exp",
    ],
    pool = "luci.chromium.gpu.chromeos.octopus.try",
    goma_backend = None,
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-zork-exp",
    mirrors = ["ci/gpu-fyi-chromeos-zork-exp"],
    pool = "luci.chromium.gpu.chromeos.zork.try",
    goma_backend = None,
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
    pool = "luci.chromium.gpu.linux.amd.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-lacros-intel-rel",
    mirrors = [
        "ci/GPU FYI Lacros x64 Builder",
        "ci/Lacros FYI x64 Release (Intel)",
    ],
    pool = "luci.chromium.gpu.linux.intel.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-amd-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (AMD RX 5500 XT)",
    ],
    pool = "luci.chromium.gpu.linux.amd.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-exp",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Experimental Release (Intel UHD 630)",
    ],
    pool = "luci.chromium.gpu.linux.intel.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (Intel UHD 630)",
    ],
    pool = "luci.chromium.gpu.linux.intel.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-dbg",
    mirrors = [
        "ci/GPU FYI Linux Builder (dbg)",
        "ci/Linux FYI Debug (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-exp",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Experimental Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-rel",
    mirrors = [
        "ci/GPU FYI Linux Builder",
        "ci/Linux FYI Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-tsn",
    mirrors = [
        "ci/Linux FYI GPU TSAN Release",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
    goma_backend = None,
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-dbg",
    mirrors = [
        "ci/GPU Linux Builder (dbg)",
        "ci/Linux Debug (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-rel",
    mirrors = [
        "ci/GPU Linux Builder",
        "ci/Linux Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

def gpu_mac_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        builderless = True,
        cores = None,
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
    pool = "luci.chromium.gpu.mac.pro.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-asan",
    mirrors = [
        "ci/GPU FYI Mac Builder (asan)",
        "ci/Mac FYI Retina ASAN (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-dbg",
    mirrors = [
        "ci/GPU FYI Mac Builder (dbg)",
        "ci/Mac FYI Retina Debug (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Retina Release (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Retina Release (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m1-exp",
    mirrors = [
        "ci/GPU FYI Mac arm64 Builder",
        "ci/Mac FYI Experimental Release (Apple M1)",
    ],
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m1-rel",
    mirrors = [
        "ci/GPU FYI Mac arm64 Builder",
        "ci/Mac FYI Release (Apple M1)",
    ],
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-asan",
    mirrors = [
        "ci/GPU FYI Mac Builder (asan)",
        "ci/Mac FYI ASAN (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-dbg",
    mirrors = [
        "ci/GPU FYI Mac Builder (dbg)",
        "ci/Mac FYI Debug (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Release (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Release (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-exp",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Experimental Retina Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
    # This bot has one machine backing its tests at the moment.
    # If it gets more, the modified execution_timeout should be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-rel",
    mirrors = [
        "ci/GPU FYI Mac Builder",
        "ci/Mac FYI Retina Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-amd-retina-dbg",
    mirrors = [
        "ci/GPU Mac Builder (dbg)",
        "ci/Mac Retina Debug (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-intel-dbg",
    mirrors = [
        "ci/GPU Mac Builder (dbg)",
        "ci/Mac Debug (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
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
    pool = "luci.chromium.gpu.win10.amd.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-exp-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Exp Release (Intel HD 630)",
    ],
    pool = "luci.chromium.gpu.win10.intel.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Release (Intel HD 630)",
    ],
    pool = "luci.chromium.gpu.win10.intel.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dbg-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder (dbg)",
        "ci/Win10 FYI x64 Debug (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-dbg-64",
    mirrors = [
        "ci/GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
        "ci/Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 DX12 Vulkan Builder",
        "ci/Win10 FYI x64 DX12 Vulkan Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-exp-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Exp Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-32",
    mirrors = [
        "ci/GPU FYI Win Builder",
        "ci/Win10 FYI x86 Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-64",
    mirrors = [
        "ci/GPU FYI Win x64 Builder",
        "ci/Win10 FYI x64 Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    goma_backend = None,
)
