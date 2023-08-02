# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "os", "reclient", "sheriff_rotations", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuzz",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    notifies = ["chromesec-lkgr-failures"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.fuzz",
    ordering = {
        None: [
            "centipede",
            "win asan",
            "mac asan",
            "cros asan",
            "linux asan",
            "libfuzz",
            "linux msan",
            "linux tsan",
        ],
        "*config*": consoles.ordering(short_names = ["dbg", "rel"]),
        "win asan": "*config*",
        "mac asan": "*config*",
        "linux asan": "*config*",
        "linux asan|x64 v8-ARM": "*config*",
        "libfuzz": consoles.ordering(short_names = [
            "chromeos-asan",
            "linux32",
            "linux32-dbg",
            "linux",
            "linux-dbg",
            "linux-msan",
            "linux-ubsan",
            "mac-asan",
            "win-asan",
        ]),
    },
)

ci.builder(
    name = "ASAN Debug",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "dbg",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "ASan Debug (32-bit x86 with V8-ARM)",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "ASAN Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "ASan Release (32-bit x86 with V8-ARM)",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "rel",
    ),
)

ci.builder(
    name = "ASAN Release Media",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chrome-test-builds/media",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "med",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "Centipede Upload Linux ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "centipede",
        short_name = "centipede",
    ),
)

ci.builder(
    name = "ASan Release Media (32-bit x86 with V8-ARM)",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
            gs_acl = "public-read",
            gs_bucket = "chrome-test-builds/media",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "med",
    ),
)

ci.builder(
    name = "ChromiumOS ASAN Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            archive_subdir = "chromeos",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros asan",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "MSAN Release (chained origins)",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
                "msan",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "msan-chained-origins",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-msan",
        ),
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "org",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "MSAN Release (no origins)",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
                "msan",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "msan-no-origins",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-msan",
        ),
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "Mac ASAN Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    builderless = False,
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "rel",
    ),
)

ci.builder(
    name = "Mac ASAN Release Media",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chrome-test-builds/media",
        ),
    ),
    builderless = False,
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "med",
    ),
)

ci.builder(
    name = "TSAN Debug",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
                "tsan2",
                "clobber",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "tsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-tsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "dbg",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "TSAN Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
                "tsan2",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "tsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-tsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "UBSan Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_linux_ubsan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ubsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-ubsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "UBSan vptr Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_linux_ubsan_vptr",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ubsan-vptr",
            archive_subdir = "vptr",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-ubsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "vpt",
    ),
    reclient_jobs = 250,
)

ci.builder(
    name = "Win ASan Release",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Win ASan Release Media",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan",
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chrome-test-builds/media",
        ),
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "chromeos-asan",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload iOS Catalyst Debug",
    executable = "recipe:chromium/fuzz",
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "ios",
    ),
    execution_timeout = 4 * time.hour,
    xcode = xcode.x14main,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-dbg",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux MSan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-msan",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux UBSan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-ubsan",
    ),
    execution_timeout = 5 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64",
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64-dbg",
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm",
    ),
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm-dbg",
    ),
)

ci.builder(
    name = "Libfuzzer Upload Mac ASan",
    executable = "recipe:chromium/fuzz",
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "mac-asan",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "Libfuzzer Upload Windows ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "win-asan",
    ),
    # crbug.com/1175182: Temporarily increase timeout
    # crbug.com/1372531: Increase timeout again
    execution_timeout = 6 * time.hour,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)
