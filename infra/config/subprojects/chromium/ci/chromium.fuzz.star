# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "xcode")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.fuzz",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    notifies = ["chromesec-lkgr-failures"],
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
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
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ASan Debug (32-bit x86 with V8-ARM)",
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ASAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ASan Release (32-bit x86 with V8-ARM)",
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ASAN Release Media",
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Afl Upload Linux ASan",
    console_view_entry = consoles.console_view_entry(
        category = "afl",
        short_name = "afl",
    ),
    executable = "recipe:chromium_afl",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ASan Release Media (32-bit x86 with V8-ARM)",
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "ChromiumOS ASAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "cros asan",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.builder(
    name = "ChromiumOS ASAN Release (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "cros asan",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
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
                "mb",
                "clobber",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    notifies = args.ignore_default(None),
)

ci.builder(
    name = "MSAN Release (chained origins)",
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "org",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "MSAN Release (no origins)",
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Mac ASAN Release",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "rel",
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.builder(
    name = "Mac ASAN Release Media",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "med",
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.builder(
    name = "TSAN Debug",
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "TSAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "UBSan Release",
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "UBSan vptr Release",
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "vpt",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    goma_backend = None,
    reclient_jobs = 250,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Win ASan Release",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Win ASan Release Media",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.LOW_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "chromeos-asan",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.builder(
    name = "Libfuzzer Upload iOS Catalyst Debug",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "ios",
    ),
    cores = 4,
    executable = "recipe:chromium_libfuzzer",
    execution_timeout = 4 * time.hour,
    os = os.MAC_11,
    xcode = xcode.x13main,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-dbg",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "Libfuzzer Upload Linux MSan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-msan",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux UBSan",
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-ubsan",
    ),
    executable = "recipe:chromium_libfuzzer",
    execution_timeout = 5 * time.hour,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64-dbg",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan Debug",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32-dbg",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm-dbg",
    ),
    executable = "recipe:chromium_libfuzzer",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Mac ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "mac-asan",
    ),
    cores = 24,
    executable = "recipe:chromium_libfuzzer",
    execution_timeout = 4 * time.hour,
    os = os.MAC_DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Windows ASan",
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "win-asan",
    ),
    # crbug.com/1175182: Temporarily increase timeout
    execution_timeout = 4 * time.hour,
    executable = "recipe:chromium_libfuzzer",
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)
