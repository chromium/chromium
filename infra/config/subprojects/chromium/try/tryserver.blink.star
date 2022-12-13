# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.blink builder group."""

load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/builder_config.star", "builder_config")
load("//lib/branches.star", "branches")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.blink",
    executable = try_.DEFAULT_EXECUTABLE,
    cores = 8,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
)

consoles.list_view(
    name = "tryserver.blink",
    branch_selector = branches.STANDARD_MILESTONE,
)

def blink_mac_builder(*, name, **kwargs):
    kwargs.setdefault("branch_selector", branches.STANDARD_MILESTONE)
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("goma_backend", goma.backend.RBE_PROD)
    kwargs.setdefault("os", os.MAC_ANY)
    kwargs.setdefault("ssd", True)
    return try_.builder(
        name = name,
        **kwargs
    )

try_.builder(
    name = "linux-blink-rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    os = os.LINUX_DEFAULT,
    main_list_view = "try",
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    tryjob = try_.job(
        location_filters = [
            "cc/.+",
            "third_party/blink/renderer/core/paint/.+",
            "third_party/blink/renderer/core/svg/.+",
            "third_party/blink/renderer/platform/graphics/.+",
        ],
    ),
)

try_.builder(
    name = "win10.20h2-blink-rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "win11-blink-rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

blink_mac_builder(
    name = "mac10.13-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
)

blink_mac_builder(
    name = "mac10.14-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
)

blink_mac_builder(
    name = "mac10.15-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
)

blink_mac_builder(
    name = "mac11.0-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    builderless = False,
)

blink_mac_builder(
    name = "mac11.0.arm64-blink-rel",
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
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
)

blink_mac_builder(
    name = "mac12.0-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
)

blink_mac_builder(
    name = "mac12.0.arm64-blink-rel",
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
        ),
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
)
