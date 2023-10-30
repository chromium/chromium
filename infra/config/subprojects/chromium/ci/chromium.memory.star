# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.memory",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ordering = {
        None: ["win", "mac", "linux", "cros"],
        "*build-or-test*": consoles.ordering(short_names = ["bld", "tst"]),
        "linux|TSan v2": "*build-or-test*",
        "linux|asan lsan": "*build-or-test*",
        "linux|webkit": consoles.ordering(short_names = ["asn", "msn"]),
    },
)

# TODO(gbeaty) Find a way to switch testers to use ci.thin_tester while ensuring
# that the builders and testers targeting linux set the necessary notifies

def linux_memory_builder(*, name, **kwargs):
    kwargs["notifies"] = kwargs.get("notifies", []) + ["linux-memory"]
    return ci.builder(name = name, **kwargs)

linux_memory_builder(
    name = "Linux ASan LSan Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
)

linux_memory_builder(
    name = "Linux ASan LSan Tests (1)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux ASan LSan Builder"],
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
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    reclient_instance = None,
)

linux_memory_builder(
    name = "Linux TSan Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
)

linux_memory_builder(
    name = "Linux CFI",
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
        build_gs_bucket = "chromium-memory-archive",
    ),
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 5 * time.hour,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
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
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "bld",
    ),
    # TODO(crbug.com/1030593): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Tests (1)",
    triggered_by = ["Linux Chromium OS ASan LSan Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
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
        category = "cros|asan",
        short_name = "tst",
    ),
    reclient_instance = None,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    cores = 16,
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "bld",
    ),
    execution_timeout = 4 * time.hour,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Tests",
    triggered_by = ["Linux ChromiumOS MSan Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "tst",
    ),
    execution_timeout = 4 * time.hour,
    reclient_instance = None,
)

linux_memory_builder(
    name = "Linux MSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "bld",
    ),
)

linux_memory_builder(
    name = "Linux MSan Tests",
    triggered_by = ["Linux MSan Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "tst",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

linux_memory_builder(
    name = "linux-lacros-asan-lsan-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|asan",
        short_name = "asan",
    ),
)

ci.builder(
    name = "Mac ASan 64 Builder",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
    builder_spec = builder_config.builder_spec(
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
    builderless = False,
    cores = None,  # Swapping between 8 and 24
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
)

linux_memory_builder(
    name = "Linux TSan Tests",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux TSan Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Mac ASan 64 Tests (1)",
    triggered_by = ["Mac ASan 64 Builder"],
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
    builderless = False,
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "tst",
    ),
    reclient_instance = None,
)

ci.builder(
    name = "WebKit Linux ASAN",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "asan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "asn",
    ),
)

ci.builder(
    name = "WebKit Linux Leak",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "lk",
    ),
)

ci.builder(
    name = "WebKit Linux MSAN",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "asan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "msn",
    ),
)

ci.builder(
    name = "android-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-memory-archive",
    ),
    os = os.LINUX_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "asn",
    ),
)

ci.builder(
    name = "linux-ubsan-vptr",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    builderless = 1,
    cores = 32,
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "vpt",
    ),
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "asn",
    ),
    # This builder is normally using 2.5 hours to run with a cached builder. And
    # 1.5 hours additional setup time without cache, https://crbug.com/1311134.
    execution_timeout = 5 * time.hour,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "ios-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ios-asan",
            archive_subdir = "ios-asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    sheriff_rotations = args.ignore_default(sheriff_rotations.IOS),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "asn",
    ),
    xcode = xcode.x14main,
)
