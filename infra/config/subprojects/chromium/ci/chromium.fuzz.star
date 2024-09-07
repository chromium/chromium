# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "cpu", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")
load("//lib/xcode.star", "xcode")
load("//project.star", "settings")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuzz",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    notifies = ["chrome-fuzzing-core"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

consoles.console_view(
    name = "chromium.fuzz",
    branch_selector = [
        branches.selector.LINUX_BRANCHES,
        branches.selector.WINDOWS_BRANCHES,
    ],
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
            target_platform = builder_config.target_platform.WIN,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "debug_builder",
            "remoteexec",
            "x64",
            "linux",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "dbg",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "v8_heap",
            "debug_builder",
            "remoteexec",
            "v8_hybrid",
            "x86",
            "linux",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "dbg",
    ),
    contact_team_email = "v8-infra@google.com",
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "fuzzer",
            "v8_heap",
            "release_builder",
            "remoteexec",
            "x64",
            "linux",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "v8_heap",
            "release_builder",
            "remoteexec",
            "v8_hybrid",
            "x86",
            "linux",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "rel",
    ),
    contact_team_email = "v8-infra@google.com",
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            archive_subdir = "media",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "v8_heap",
            "chromeos_codecs",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "med",
    ),
    siso_remote_jobs = 250,
)

ci.builder(
    name = "Centipede Upload Linux ASan",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ) if settings.is_main else None,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "centipede",
            "asan",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "shared",
            "release",
            "remoteexec",
            "disable_seed_corpus",
            "mojo_fuzzer",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "centipede",
        short_name = "centipede",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-centipede",
        "upload_directory": "asan",
        "archive_prefix": "centipede",
    },
)

ci.builder(
    name = "Centipede High End Upload Linux ASan",
    description_html = """This builder uploads centipede high end fuzzers.\
Those fuzzers require more resources to run correctly.\
""",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "centipede",
            "asan",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "shared",
            "release",
            "remoteexec",
            "disable_seed_corpus",
            "high_end_fuzzer_targets",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "centipede",
        short_name = "centipede high end",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-centipede",
        "upload_directory": "asan",
        "archive_prefix": "centipede-high-end",
    },
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan-v8-arm",
            archive_subdir = "v8-arm-media",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "v8_heap",
            "chromeos_codecs",
            "release_builder",
            "remoteexec",
            "v8_hybrid",
            "linux",
            "x86",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "med",
    ),
    contact_team_email = "v8-infra@google.com",
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            archive_subdir = "chromeos",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos",
            "asan",
            "lsan",
            "fuzzer",
            "v8_heap",
            "release_builder",
            "remoteexec",
            "chromeos",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros asan",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "msan-chained-origins",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-msan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "msan",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "org",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "msan-no-origins",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-msan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "msan_no_origins",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "linux msan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.MAC,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "v8_heap",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac asan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            pending_time = struct(),  # exception added because this builder has a pool of 1 machine and 2 concurrent invocations
        ),
    }),
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
            target_platform = builder_config.target_platform.MAC,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            archive_subdir = "media",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "v8_heap",
            "chrome_with_codecs",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "tsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-tsan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "tsan",
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "dbg",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "tsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-tsan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "tsan",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux tsan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ubsan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-ubsan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "ubsan",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ubsan-vptr",
            archive_subdir = "vptr",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-ubsan",
        ),
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
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux UBSan",
        short_name = "vpt",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = 250,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang",
            "asan",
            "fuzzer",
            "v8_heap",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "asan",
            archive_subdir = "media",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang",
            "asan",
            "fuzzer",
            "v8_heap",
            "chrome_with_codecs",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = ["chromium_builder_asan"],
        mixins = ["chromium-tester-service-account"],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "chromeos_with_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "disable_seed_corpus",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "chromeos-asan",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 6 * time.hour,
    properties = {
        "archive_prefix": "libfuzzer-chromeos",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "chromeos-asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload iOS Catalyst Debug",
    executable = "recipe:chromium/fuzz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["ios"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mac_toolchain",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "compile_only",
            "debug_static_builder",
            "remoteexec",
            "ios",
            "ios_catalyst",
            "x64",
            "asan",
            "libfuzzer",
            "no_dsyms",
            "no_remoting",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "ios",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 4 * time.hour,
    properties = {
        "archive_prefix": "libfuzzer-ios",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "ios-catalyst-debug",
        "ios_targets_only": True,
    },
    xcode = xcode.xcode_default,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ) if settings.is_main else None,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "mojo_fuzzer",
            "shared",
            "release",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 4 * time.hour,
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "debug_builder",
            "remoteexec",
            "shared",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "disable_seed_corpus",
            "linux",
            "x64",
        ],
    ),
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-dbg",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 4 * time.hour,
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux MSan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
                "msan",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "msan",
            "shared",
            "release",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "disable_seed_corpus",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-msan",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "msan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux UBSan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "ubsan_security_non_vptr",
            "release_builder",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "disable_seed_corpus",
            "shared",
            "linux",
            "x64",
        ],
    ),
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux-ubsan",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 5 * time.hour,
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "ubsan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "v8_simulate_arm64",
            "disable_seed_corpus",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64",
    ),
    contact_team_email = "v8-infra@google.com",
    properties = {
        "archive_prefix": "libfuzzer-v8-arm64",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan-arm64-sim",
        "v8_targets_only": True,
    },
)

ci.builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "debug_builder",
            "remoteexec",
            "shared",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "v8_simulate_arm64",
            "disable_seed_corpus",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm64-dbg",
    ),
    contact_team_email = "v8-infra@google.com",
    properties = {
        "archive_prefix": "libfuzzer-v8-arm64",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan-arm64-sim",
        "v8_targets_only": True,
    },
)

ci.builder(
    name = "Libfuzzer Upload Linux32 ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "x86",
            "disable_seed_corpus",
            "linux",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux32",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "v8_simulate_arm",
            "disable_seed_corpus",
            "linux",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm",
    ),
    contact_team_email = "v8-infra@google.com",
    properties = {
        "archive_prefix": "libfuzzer-v8-arm",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan-arm-sim",
        "v8_targets_only": True,
    },
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "debug_builder",
            "remoteexec",
            "shared",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "v8_simulate_arm",
            "disable_seed_corpus",
            "linux",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "arm-dbg",
    ),
    contact_team_email = "v8-infra@google.com",
    properties = {
        "archive_prefix": "libfuzzer-v8-arm",
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan-arm-sim",
        "v8_targets_only": True,
    },
)

ci.builder(
    name = "Libfuzzer Upload Mac ASan",
    executable = "recipe:chromium/fuzz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "chrome_with_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "mac",
            "x64",
        ],
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "mac-asan",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 4 * time.hour,
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
)

ci.builder(
    name = "Libfuzzer Upload Windows ASan",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    executable = "recipe:chromium/fuzz",
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ) if settings.is_main else None,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    # Note that because of optimize_for_fuzzing, Windows cannot share a config
    # with other libFuzzer builds. optimize_for_fuzzing is used by the other
    # libFuzzer build configs but it does not work on Windows.
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "pdf_xfa",
            "minimal_symbols",
            "mojo_fuzzer",
            "win",
            "x64",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "win-asan",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    # crbug.com/1175182: Temporarily increase timeout
    # crbug.com/1372531: Increase timeout again
    execution_timeout = 8 * time.hour,
    experiments = {
        "chromium.use_per_builder_build_dir_name": 100,
    },
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)
