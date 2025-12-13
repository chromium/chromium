# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.angle builder group."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "builders", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = "recipe:angle_chromium_trybot",
    builder_group = "tryserver.chromium.angle",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    ssd = None,
    contact_team_email = "angle-team@google.com",
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    max_concurrent_builds = 5,
    service_account = gpu.try_.SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.angle",
)

def base_angle_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        # For some reason, this cannot be set via try_.defaults.set(), as it
        # complains about it being set twice.
        free_space = None,
        **kwargs
    )

base_angle_builder(
    name = "android-angle-chromium-try",
    description_html = "Builds and tests ANGLE on arm64 Android using ToT ANGLE and a known good Chromium revision.",
    mirrors = [
        "ci/android-angle-chromium-arm64-builder",
        "ci/android-angle-chromium-arm64-pixel2",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/android-angle-chromium-arm64-builder",
            "no_symbols",
        ],
    ),
    contact_team_email = "angle-team@google.com",
)

base_angle_builder(
    name = "fuchsia-angle-try",
    description_html = "Builds ANGLE on x64 Fuchsia using ToT ANGLE and a known good Chromium revision. Compile-only.",
    mirrors = [
        "ci/fuchsia-angle-builder",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-angle-builder",
            "no_symbols",
        ],
    ),
)

base_angle_builder(
    name = "linux-angle-chromium-try",
    description_html = "Builds and tests ANGLE on x64 Linux using ToT ANGLE and a known good Chromium revision.",
    mirrors = [
        "ci/linux-angle-chromium-builder",
        "ci/linux-angle-chromium-intel",
        "ci/linux-angle-chromium-nvidia",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/linux-angle-chromium-builder",
            "no_symbols",
        ],
    ),
)

def angle_mac_builder(**kwargs):
    return base_angle_builder(
        cores = None,
        os = os.MAC_ANY,
        cpu = None,
        **kwargs
    )

angle_mac_builder(
    name = "mac-angle-chromium-try",
    description_html = "Builds and tests ANGLE on x64 Mac using ToT ANGLE and a known good Chromium revision.",
    mirrors = [
        "ci/mac-angle-chromium-amd",
        "ci/mac-angle-chromium-builder",
        "ci/mac-angle-chromium-intel",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/mac-angle-chromium-builder",
            "no_symbols",
        ],
    ),
)

def angle_win_builder(**kwargs):
    return base_angle_builder(
        os = os.WINDOWS_ANY,
        ssd = builders.with_expiration(True, expiration = 5 * time.minute),
        **kwargs
    )

angle_win_builder(
    name = "win-angle-chromium-x64-try",
    description_html = "Builds and tests ANGLE on x64 Windows using ToT ANGLE and a known good Chromium revision.",
    mirrors = [
        "ci/win-angle-chromium-x64-builder",
        "ci/win10-angle-chromium-x64-intel",
        "ci/win10-angle-chromium-x64-nvidia",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/win-angle-chromium-x64-builder",
            "no_symbols",
        ],
    ),
)

angle_win_builder(
    name = "win-angle-chromium-x86-try",
    description_html = "Builds and tests ANGLE on x86 Windows using ToT ANGLE and a known good Chromium revision.",
    mirrors = [
        "ci/win-angle-chromium-x86-builder",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/win-angle-chromium-x86-builder",
            "no_symbols",
        ],
    ),
)
