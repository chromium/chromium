# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.memory.fyi",
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
ci.builder(
    name = "linux-exp-asan-lsan-fyi-rel",
    schedule = "with 6h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "lsan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = 1,
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "experimental|linux",
        short_name = "asan lsan",
    ),
    execution_timeout = 6 * time.hour,
    reclient_jobs = reclient.jobs.DEFAULT,
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
        ),
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
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
ci.builder(
    name = "linux-exp-tsan-fyi-rel",
    schedule = "with 6h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tsan2",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = 1,
    console_view_entry = consoles.console_view_entry(
        category = "experimental|linux",
        short_name = "tsan",
    ),
    execution_timeout = 4 * time.hour,
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
        ),
    ),
    builderless = 1,
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "fyi",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1320449): Remove this builder after burning down failures
# and measuring performance to see if we can roll LSan into ASan.
ci.builder(
    name = "mac-lsan-fyi-rel",
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
        ),
        run_tests_serially = True,
    ),
    builderless = 1,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac|lsan",
        short_name = "lsan",
    ),
    execution_timeout = 12 * time.hour,
    reclient_jobs = reclient.jobs.DEFAULT,
)
