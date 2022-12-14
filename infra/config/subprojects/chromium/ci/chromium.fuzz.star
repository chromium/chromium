# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.fuzz",
    executable = ci.DEFAULT_EXECUTABLE,
    cores = 8,
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    sheriff_rotations = sheriff_rotations.CHROMIUM_FUZZ,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    notifies = ["chromesec-lkgr-failures"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

consoles.console_view(
    name = "chromium.fuzz",
    ordering = {
        None: [
            "afl",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "dbg",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "ASan Debug (32-bit x86 with V8-ARM)",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "ASAN Release",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.builder(
    name = "ASan Release (32-bit x86 with V8-ARM)",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "ASAN Release Media",
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
            gs_bucket = "chrome-test-builds/media",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "med",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "Afl Upload Linux ASan",
    executable = "recipe:chromium_afl",
    cores = 16,
    console_view_entry = consoles.console_view_entry(
        category = "afl",
        short_name = "afl",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "ASan Release Media (32-bit x86 with V8-ARM)",
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
            gs_bucket = "chrome-test-builds/media",
            gs_acl = "public-read",
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "ChromiumOS ASAN Release",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
            archive_subdir = "chromeos",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros asan",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.builder(
    name = "MSAN Release (chained origins)",
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
            gs_bucket = "chromium-browser-msan",
            gs_acl = "public-read",
            archive_name_prefix = "msan-chained-origins",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "org",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "MSAN Release (no origins)",
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
            gs_bucket = "chromium-browser-msan",
            gs_acl = "public-read",
            archive_name_prefix = "msan-no-origins",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    os = os.LINUX_FOCAL,
)

ci.builder(
    name = "Mac ASAN Release",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    builderless = False,
    cores = 4,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.builder(
    name = "Mac ASAN Release Media",
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
            gs_bucket = "chrome-test-builds/media",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    builderless = False,
    cores = 4,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.builder(
    name = "TSAN Debug",
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
            gs_bucket = "chromium-browser-tsan",
            gs_acl = "public-read",
            archive_name_prefix = "tsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "dbg",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "TSAN Release",
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
            gs_bucket = "chromium-browser-tsan",
            gs_acl = "public-read",
            archive_name_prefix = "tsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.builder(
    name = "UBSan Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_linux_ubsan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            gs_bucket = "chromium-browser-ubsan",
            gs_acl = "public-read",
            archive_name_prefix = "ubsan",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "rel",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "UBSan vptr Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium_linux_ubsan_vptr",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            gs_bucket = "chromium-browser-ubsan",
            gs_acl = "public-read",
            archive_name_prefix = "ubsan-vptr",
            archive_subdir = "vptr",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "vpt",
    ),
    reclient_jobs = 250,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.builder(
    name = "Win ASan Release",
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
            gs_bucket = "chromium-browser-asan",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
)

ci.builder(
    name = "Win ASan Release Media",
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
            gs_bucket = "chrome-test-builds/media",
            gs_acl = "public-read",
            archive_name_prefix = "asan",
        ),
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "chromeos-asan",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.builder(
    name = "Libfuzzer Upload iOS Catalyst Debug",
    executable = "recipe:chromium_libfuzzer",
    cores = 4,
    os = os.MAC_12,
    xcode = xcode.x14main,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "ios",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-dbg",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux MSan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-msan",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux UBSan",
    executable = "recipe:chromium_libfuzzer",
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-ubsan",
    ),
    execution_timeout = 5 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan Debug",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32-dbg",
    ),
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm",
    ),
    reclient_jobs = reclient.jobs.DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    executable = "recipe:chromium_libfuzzer",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.builder(
    name = "Libfuzzer Upload Mac ASan",
    executable = "recipe:chromium_libfuzzer",
    cores = 24,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "mac-asan",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "Libfuzzer Upload Windows ASan",
    executable = "recipe:chromium_libfuzzer",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "win-asan",
    ),
    # crbug.com/1175182: Temporarily increase timeout
    # crbug.com/1372531: Increase timeout again
    execution_timeout = 6 * time.hour,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)
