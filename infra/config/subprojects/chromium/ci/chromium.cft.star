# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions for the chromium.cft (chrome for testing) builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.cft",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    sheriff_rotations = sheriff_rotations.CFT,
    tree_closing = False,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_remote_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

def builder_spec(*, target_platform, build_config, is_arm64 = False, additional_configs = None):
    additional_configs = additional_configs or []
    if is_arm64:
        additional_configs.append("arm64")
    return builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = additional_configs,
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = build_config,
            target_arch = builder_config.target_arch.ARM if is_arm64 else None,
            target_bits = 64,
            target_platform = target_platform,
        ),
    )

consoles.console_view(
    name = "chromium.cft",
)

ci.builder(
    name = "mac-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.MAC,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
            "x64",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        short_name = "mac-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
)

ci.builder(
    name = "linux-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.LINUX,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        short_name = "linux-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
)

ci.builder(
    name = "win-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.WIN,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        short_name = "win-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
    execution_timeout = 6 * time.hour,
)
