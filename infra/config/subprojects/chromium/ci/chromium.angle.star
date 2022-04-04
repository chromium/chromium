# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.angle builder group."""

load("//lib/builders.star", "goma", "xcode")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.angle",
    executable = "recipe:angle_chromium",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.gpu.POOL,
    properties = {
        "perf_dashboard_machine_group": "ChromiumANGLE",
    },
    service_account = ci.gpu.SERVICE_ACCOUNT,
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
    name = "android-angle-arm64-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder|ANGLE",
        short_name = "arm64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.thin_tester(
    name = "android-angle-arm64-nexus5x",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Nexus5X|ANGLE",
        short_name = "arm64",
    ),
    triggered_by = ["android-angle-arm64-builder"],
)

ci.gpu.linux_builder(
    name = "android-angle-chromium-arm64-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder|Chromium",
        short_name = "arm64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.thin_tester(
    name = "android-angle-chromium-arm64-nexus5x",
    console_view_entry = consoles.console_view_entry(
        category = "Android|Nexus5X|Chromium",
        short_name = "arm64",
    ),
    triggered_by = ["android-angle-chromium-arm64-builder"],
)

ci.gpu.linux_builder(
    name = "fuchsia-angle-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Fuchsia|Builder|ANGLE",
        short_name = "x64",
    ),
)

ci.gpu.linux_builder(
    name = "linux-angle-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder|ANGLE",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.thin_tester(
    name = "linux-angle-intel",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["linux-angle-builder"],
)

ci.thin_tester(
    name = "linux-angle-nvidia",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|NVIDIA|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["linux-angle-builder"],
)

ci.gpu.linux_builder(
    name = "linux-angle-chromium-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder|Chromium",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.thin_tester(
    name = "linux-angle-chromium-intel",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["linux-angle-chromium-builder"],
)

ci.thin_tester(
    name = "linux-angle-chromium-nvidia",
    console_view_entry = consoles.console_view_entry(
        category = "Linux|NVIDIA|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["linux-angle-chromium-builder"],
)

ci.gpu.mac_builder(
    name = "mac-angle-chromium-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder|Chromium",
        short_name = "x64",
    ),
)

ci.thin_tester(
    name = "mac-angle-chromium-amd",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["mac-angle-chromium-builder"],
)

ci.thin_tester(
    name = "mac-angle-chromium-intel",
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["mac-angle-chromium-builder"],
)

ci.gpu.mac_builder(
    name = "ios-angle-builder",
    xcode = xcode.x13main,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Builder|ANGLE",
        short_name = "x64",
    ),
)

ci.thin_tester(
    name = "ios-angle-intel",
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Intel|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["ios-angle-builder"],
)

ci.gpu.windows_builder(
    name = "win-angle-chromium-x64-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-intel",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Intel|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["win-angle-chromium-x64-builder"],
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-nvidia",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|NVIDIA|Chromium",
        short_name = "x64",
    ),
    triggered_by = ["win-angle-chromium-x64-builder"],
)

ci.gpu.windows_builder(
    name = "win-angle-chromium-x86-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x86",
    ),
)

ci.thin_tester(
    name = "win7-angle-chromium-x86-amd",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Win7-AMD|Chromium",
        short_name = "x86",
    ),
    triggered_by = ["win-angle-chromium-x86-builder"],
)

ci.gpu.windows_builder(
    name = "win-angle-x64-builder",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|ANGLE",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.thin_tester(
    name = "win7-angle-x64-nvidia",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Win7-NVIDIA|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["win-angle-x64-builder"],
)

ci.thin_tester(
    name = "win10-angle-x64-intel",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Intel|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["win-angle-x64-builder"],
)

ci.thin_tester(
    name = "win10-angle-x64-nvidia",
    console_view_entry = consoles.console_view_entry(
        category = "Windows|NVIDIA|ANGLE",
        short_name = "x64",
    ),
    triggered_by = ["win-angle-x64-builder"],
)
