# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.memory.fyi",
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
ci.builder(
    name = "linux-exp-msan-fyi-rel",
    schedule = "with 6h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "msan",
            "fail_on_san_warnings",
            "release_builder",
            "reclient",
        ],
    ),
    builderless = 1,
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "experimental|linux",
        short_name = "msan",
    ),
    execution_timeout = 6 * time.hour,
    health_spec = modified_default({
        "Low Value": blank_low_value_thresholds,
    }),
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1394755): Remove this builder after burning down failures
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
            "release_builder",
            "reclient",
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
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1320449): Remove this builder after burning down failures
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
            "reclient",
        ],
    ),
    builderless = 1,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac|lsan",
        short_name = "lsan",
    ),
    execution_timeout = 12 * time.hour,
    health_spec = modified_default({
        "Low Value": blank_low_value_thresholds,
    }),
    reclient_jobs = reclient.jobs.DEFAULT,
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
            "reclient",
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
    reclient_jobs = reclient.jobs.DEFAULT,
)
