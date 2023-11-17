# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.angle builder group."""

load("//lib/builders.star", "cpu", "os", "reclient")
load("//lib/builder_config.star", "builder_config")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/try.star", "try_")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.angle",
    pool = try_.DEFAULT_POOL,
    builderless = False,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.gpu.SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.angle",
)

try_.builder(
    name = "android-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/android-angle-chromium-arm64-builder",
        "ci/android-angle-chromium-arm64-nexus5x",
    ],
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/android-angle-chromium-arm64-builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "fuchsia-angle-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/fuchsia-angle-builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-angle-builder",
            "no_symbols",
            "use_dummy_lastchange",
        ],
    ),
)

try_.builder(
    name = "linux-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/linux-angle-chromium-builder",
        "ci/linux-angle-chromium-intel",
        "ci/linux-angle-chromium-nvidia",
    ],
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/linux-angle-chromium-builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "mac-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/mac-angle-chromium-amd",
        "ci/mac-angle-chromium-builder",
        "ci/mac-angle-chromium-intel",
    ],
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    gn_args = gn_args.config(
        configs = [
            "ci/mac-angle-chromium-builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "win-angle-chromium-x64-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/win-angle-chromium-x64-builder",
        "ci/win10-angle-chromium-x64-intel",
        "ci/win10-angle-chromium-x64-nvidia",
    ],
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    os = os.WINDOWS_ANY,
    gn_args = gn_args.config(
        configs = [
            "ci/win-angle-chromium-x64-builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "win-angle-chromium-x86-try",
    executable = "recipe:angle_chromium_trybot",
    mirrors = [
        "ci/win-angle-chromium-x86-builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
        retry_failed_shards = False,
    ),
    os = os.WINDOWS_ANY,
    gn_args = gn_args.config(
        configs = [
            "ci/win-angle-chromium-x86-builder",
            "no_symbols",
        ],
    ),
)
