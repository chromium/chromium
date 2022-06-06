# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.memory",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    os = os.LINUX_DEFAULT,
    main_console_view = "main",
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.memory",
    branch_selector = branches.STANDARD_MILESTONE,
    ordering = {
        None: ["win", "mac", "linux", "cros"],
        "*build-or-test*": consoles.ordering(short_names = ["bld", "tst"]),
        "linux|TSan v2": "*build-or-test*",
        "linux|asan lsan": "*build-or-test*",
        "linux|webkit": consoles.ordering(short_names = ["asn", "msn"]),
    },
)

def linux_memory_builder(*, name, **kwargs):
    kwargs["notifies"] = kwargs.get("notifies", []) + ["linux-memory"]
    return ci.builder(name = name, **kwargs)

linux_memory_builder(
    name = "Linux ASan LSan Builder",
    branch_selector = branches.STANDARD_MILESTONE,
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
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.LINUX_BIONIC,
    ssd = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

linux_memory_builder(
    name = "Linux ASan LSan Tests (1)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
        build_gs_bucket = "chromium-memory-archive",
    ),
    triggered_by = ["ci/Linux ASan LSan Builder"],
    os = os.LINUX_BIONIC,
)

linux_memory_builder(
    name = "Linux ASan Tests (sandboxed)",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "sbx",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Linux ASan LSan Builder"],
)

linux_memory_builder(
    name = "Linux TSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tsan2",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

linux_memory_builder(
    name = "Linux CFI",
    console_view_entry = consoles.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 5 * time.hour,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Builder",
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "bld",
    ),
    # TODO(crbug.com/1030593): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
    ssd = True,
    cores = 16,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Tests (1)",
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "tst",
    ),
    triggered_by = ["Linux Chromium OS ASan LSan Builder"],
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Builder",
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "bld",
    ),
    execution_timeout = 4 * time.hour,
    ssd = True,
    cores = 16,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Tests",
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "tst",
    ),
    execution_timeout = 4 * time.hour,
    triggered_by = ["Linux ChromiumOS MSan Builder"],
)

linux_memory_builder(
    name = "Linux MSan Builder",
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

linux_memory_builder(
    name = "Linux MSan Tests",
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "tst",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    triggered_by = ["Linux MSan Builder"],
)

ci.builder(
    name = "Mac ASan 64 Builder",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
    goma_debug = True,  # TODO(hinoka): Remove this after debugging.
    goma_jobs = None,
    cores = None,  # Swapping between 8 and 24
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

linux_memory_builder(
    name = "Linux TSan Tests",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tsan2",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Linux TSan Builder"],
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Mac ASan 64 Tests (1)",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "tst",
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    triggered_by = ["Mac ASan 64 Builder"],
)

ci.builder(
    name = "WebKit Linux ASAN",
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "asn",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "WebKit Linux Leak",
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "lk",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "WebKit Linux MSAN",
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "msn",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "android-asan",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "asn",
    ),
    os = os.LINUX_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-ubsan-vptr",
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "vpt",
    ),
    builderless = 1,
    cores = 32,
    tree_closing = False,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win-asan",
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "asn",
    ),
    cores = 32,
    # This builder is normally using 2.5 hours to run with a cached builder. And
    # 1.5 hours additional setup time without cache, https://crbug.com/1311134.
    execution_timeout = 5 * time.hour,
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)
