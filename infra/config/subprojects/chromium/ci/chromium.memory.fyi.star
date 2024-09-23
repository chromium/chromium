# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/builder_health_indicators.star", "blank_low_value_thresholds", "health_spec", "modified_default")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.memory.fyi",
)

# TODO(crbug.com/40248746): Remove this builder after burning down failures
# and measuring performance to see if we can roll UBSan into ASan.
ci.builder(
    name = "linux-ubsan-fyi-rel",
    schedule = "with 12h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "ubsan_no_recover",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = 1,
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "fyi",
    ),
    execution_timeout = 6 * time.hour,
    health_spec = modified_default({
        "Low Value": blank_low_value_thresholds,
    }),
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

# TODO(crbug.com/40223516): Remove this builder after burning down failures
# and measuring performance to see if we can roll LSan into ASan.
ci.builder(
    name = "mac-lsan-fyi-rel",
    description_html = "Runs basic Mac tests with is_lsan=true",
    schedule = "with 24h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "dcheck_always_on",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = 1,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac|lsan",
        short_name = "lsan",
    ),
    execution_timeout = 12 * time.hour,
    health_spec = modified_default({
        "Low Value": blank_low_value_thresholds,
    }),
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "mac-ubsan-fyi-rel",
    description_html = "Runs basic Mac tests with is_ubsan=true",
    schedule = "with 24h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ubsan_no_recover",
            "dcheck_always_on",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = 1,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac|ubsan",
        short_name = "ubsan",
    ),
    execution_timeout = 12 * time.hour,
    health_spec = modified_default({
        "Low Value": blank_low_value_thresholds,
    }),
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)
