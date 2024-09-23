# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "fail_on_san_warnings",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_enabled = True,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_project = None,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "tsan",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cfi_full",
            "cfi_icall",
            "cfi_diag",
            "thin_lto",
            "release",
            "static",
            "dcheck_always_on",
            "remoteexec",
            "linux",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "chromeos",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x64",
        ],
    ),
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "bld",
    ),
    # TODO(crbug.com/40661942): Builds take more than 3 hours sometimes. Remove
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "tst",
    ),
    siso_project = None,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos",
            "msan",
            "release_builder",
            "remoteexec",
            "x64",
        ],
    ),
    cores = 16,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "tst",
    ),
    execution_timeout = 4 * time.hour,
    siso_project = None,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "msan",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    # Requires dedicated extra memory builder (crbug.com/352281723).
    builderless = False,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "tst",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "minimal_symbols",
            "release_builder",
            "remoteexec",
            "dcheck_always_on",
            "mac",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.MAC,
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
    siso_project = None,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "release_builder_blink",
            "remoteexec",
            "linux",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "remoteexec",
            "linux",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "msan",
            "release_builder_blink",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "clang",
            "asan",
            "release_builder",
            "remoteexec",
            "strip_debug_info",
            "minimal_symbols",
            "arm",
        ],
    ),
    os = os.LINUX_DEFAULT,
    gardener_rotations = args.ignore_default(None),
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "ubsan_vptr",
            "ubsan_vptr_no_recover_hack",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = 1,
    cores = 32,
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "vpt",
    ),
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "static",
            "v8_heap",
            "minimal_symbols",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
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
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "ios-asan",
    description_html = (
        "Builds the open-source version of Chrome for iOS with " +
        "AddressSanitizer (ASan) and runs unit tests for detecting memory " +
        "errors."
    ),
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
    gn_args = gn_args.config(
        configs = [
            "ios_simulator",
            "x64",
            "release_builder",
            "remoteexec",
            "asan",
            "xctest",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    gardener_rotations = args.ignore_default(gardener_rotations.IOS),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "asn",
    ),
    xcode = xcode.xcode_default,
)

ci.builder(
    name = "linux-codeql-generator",
    description_html = "Compiles a CodeQL database on a Linux host and uploads the result.",
    executable = "recipe:chrome_codeql_database_builder",
    # Run once daily at 5am Pacific/1 PM UTC
    schedule = "0 13 * * *",
    cores = 32,
    ssd = True,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = [
        consoles.console_view_entry(
            category = "codeql-linux",
            short_name = "cdql-lnx",
        ),
    ],
    contact_team_email = "chrome-memory-safety-team@google.com",
    execution_timeout = 18 * time.hour,
    notifies = ["codeql-infra"],
    properties = {
        "codeql_version": "version:3@2.18.1",
    },
)

ci.builder(
    name = "linux-codeql-query-runner",
    description_html = "Runs a set of CodeQL queries against a CodeQL database on a Linux host and uploads the result.",
    executable = "recipe:chrome_codeql_query_runner",
    # Run once daily at 5am Pacific/1 PM UTC
    schedule = "0 13 * * *",
    cores = 32,
    ssd = True,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = [
        consoles.console_view_entry(
            category = "codeql-linux-queries",
            short_name = "cdql-lnx-qrs",
        ),
    ],
    contact_team_email = "chrome-memory-safety-team@google.com",
    execution_timeout = 18 * time.hour,
    notifies = ["codeql-infra"],
    properties = {
        "codeql_version": "version:3@2.18.1",
    },
)
