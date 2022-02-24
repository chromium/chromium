# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "goma", "os")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    cores = 8,
    cpu = cpu.X86_64,
    cq_group = "cq",
    executable = "recipe:chromium_trybot",
    execution_timeout = 6 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.try",
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
        goma_backend = goma.backend.RBE_PROD,
        os = os.LINUX_BIONIC_REMOVE,
        ssd = None,
        **kwargs
    )

gpu_android_builder(
    name = "gpu-fyi-try-android-l-nexus-5-32",
    pool = "luci.chromium.gpu.android.nexus5.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-m-nexus-5x-64",
    pool = "luci.chromium.gpu.android.nexus5x.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-nvidia-shield-tv",
    pool = "luci.chromium.gpu.android.nvidia.shield.tv.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-p-pixel-2-32",
    pool = "luci.chromium.gpu.android.pixel2.chromium.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-r-pixel-4-32",
    pool = "luci.chromium.gpu.android.pixel4.try",
)

gpu_android_builder(
    name = "gpu-fyi-try-android-pixel-6-64",
    pool = "luci.chromium.gpu.android.pixel6.try",
)

gpu_android_builder(
    name = "gpu-try-android-m-nexus-5x-64",
    pool = "luci.chromium.gpu.android.nexus5x.try",
)

def gpu_chromeos_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.chromiumos",
        builderless = True,
        goma_backend = goma.backend.RBE_PROD,
        os = os.LINUX_BIONIC_REMOVE,
        ssd = None,
        **kwargs
    )

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-amd64-generic",
    pool = "luci.chromium.gpu.chromeos.amd64.generic.try",
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-jacuzzi-exp",
    pool = "luci.chromium.gpu.chromeos.jacuzzi.try",
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-kevin",
    pool = "luci.chromium.gpu.chromeos.kevin.try",
)

gpu_chromeos_builder(
    name = "gpu-fyi-try-chromeos-octopus-exp",
    pool = "luci.chromium.gpu.chromeos.octopus.try",
)

def gpu_linux_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.linux",
        builderless = True,
        goma_backend = goma.backend.RBE_PROD,
        os = os.LINUX_BIONIC_REMOVE,
        ssd = None,
        **kwargs
    )

gpu_linux_builder(
    name = "gpu-fyi-try-lacros-amd-rel",
    pool = "luci.chromium.gpu.linux.amd.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-lacros-intel-rel",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-amd-rel",
    pool = "luci.chromium.gpu.linux.amd.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-exp",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-intel-rel",
    pool = "luci.chromium.gpu.linux.intel.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-dbg",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-exp",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-rel",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-fyi-try-linux-nvidia-tsn",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-dbg",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

gpu_linux_builder(
    name = "gpu-try-linux-nvidia-rel",
    pool = "luci.chromium.gpu.linux.nvidia.try",
)

def gpu_mac_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        builderless = True,
        cores = None,
        goma_backend = goma.backend.RBE_PROD,
        os = os.MAC_ANY,
        ssd = None,
        **kwargs
    )

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-pro-rel",
    pool = "luci.chromium.gpu.mac.pro.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-asan",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-dbg",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-exp",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-amd-retina-rel",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-arm64-apple-m1-rel",
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-asan",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-dbg",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-exp",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-intel-rel",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-dbg",
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-exp",
    # This bot has one machine backing its tests at the moment.
    # If it gets more, the modified execution_timeout should be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
)

gpu_mac_builder(
    name = "gpu-fyi-try-mac-nvidia-retina-rel",
    pool = "luci.chromium.gpu.mac.retina.nvidia.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-amd-retina-dbg",
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

gpu_mac_builder(
    name = "gpu-try-mac-intel-dbg",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

def gpu_win_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        builder_group = "tryserver.chromium.win",
        builderless = True,
        goma_backend = goma.backend.RBE_PROD,
        os = os.WINDOWS_ANY,
        ssd = None,
        **kwargs
    )

gpu_win_builder(
    name = "gpu-fyi-try-win10-amd-rel-64",
    pool = "luci.chromium.gpu.win10.amd.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-exp-64",
    pool = "luci.chromium.gpu.win10.intel.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-intel-rel-64",
    pool = "luci.chromium.gpu.win10.intel.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dbg-64",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-dbg-64",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-dx12vk-rel-64",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-exp-64",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-32",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win10-nvidia-rel-64",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win7-amd-rel-32",
    pool = "luci.chromium.gpu.win7.amd.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win7-nvidia-rel-32",
    pool = "luci.chromium.gpu.win7.nvidia.try",
)

gpu_win_builder(
    name = "gpu-fyi-try-win7-nvidia-rel-64",
    pool = "luci.chromium.gpu.win7.nvidia.try",
)

gpu_win_builder(
    name = "gpu-try-win10-nvidia-rel",
    pool = "luci.chromium.gpu.win10.nvidia.try",
)
