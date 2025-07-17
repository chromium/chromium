# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/args.star", "args")
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

def ci_builder(
        max_concurrent_invocations = None,
        chromium_config = None,
        gclient_apply_configs = None,
        clusterfuzz_archive = None,
        gn_extra_configs = [],
        console_category = None,
        console_short_name = None,
        **kwargs):
    gn_configs = ["remoteexec"] + gn_extra_configs

    if chromium_config.build_config == builder_config.build_config.DEBUG:
        gn_configs.append("debug_builder")
        default_console_short_name = "dbg"
    elif chromium_config.build_config == builder_config.build_config.RELEASE:
        gn_configs.append("release_builder")
        default_console_short_name = "rel"

    if chromium_config.target_bits == 32:
        gn_configs.append("x86")
    elif chromium_config.target_bits == 64:
        gn_configs.append("x64")

    target_platform = chromium_config.target_platform
    if target_platform == builder_config.target_platform.CHROMEOS:
        gn_configs.append("chromeos")
    elif target_platform == builder_config.target_platform.LINUX:
        gn_configs.append("linux")
    elif target_platform == builder_config.target_platform.MAC:
        gn_configs.append("mac")
    elif target_platform == builder_config.target_platform.WIN:
        gn_configs.append("win")

    return ci.builder(
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = max_concurrent_invocations,
        ),
        builder_spec = builder_config.builder_spec(
            gclient_config = builder_config.gclient_config(
                config = "chromium",
                apply_configs = gclient_apply_configs,
            ),
            chromium_config = chromium_config,
            clusterfuzz_archive = clusterfuzz_archive,
        ),
        gn_args = gn_args.config(configs = gn_configs),
        console_view_entry = consoles.console_view_entry(
            category = console_category,
            short_name = console_short_name or default_console_short_name,
        ),
        **kwargs
    )

def browser_builder(
        max_concurrent_invocations = 4,
        clusterfuzz_archive_name_prefix = None,
        clusterfuzz_archive_subdir = None,
        clusterfuzz_gs_bucket = None,
        **kwargs):
    return ci_builder(
        max_concurrent_invocations = max_concurrent_invocations,
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = clusterfuzz_archive_name_prefix,
            archive_subdir = clusterfuzz_archive_subdir,
            gs_acl = "public-read",
            gs_bucket = clusterfuzz_gs_bucket,
        ),
        targets = targets.bundle(
            additional_compile_targets = ["chromium_builder_asan"],
            mixins = ["chromium-tester-service-account"],
        ),
        **kwargs
    )

def browser_asan_builder(
        chromium_config_name = "chromium_asan",
        build_config = None,
        target_bits = None,
        target_platform = None,
        clusterfuzz_archive_name_prefix = "asan",
        gn_extra_configs = [],
        console_category = "linux asan",
        **kwargs):
    return browser_builder(
        chromium_config = builder_config.chromium_config(
            config = chromium_config_name,
            apply_configs = [
                "mb",
                "clobber",
            ],
            build_config = build_config,
            target_bits = target_bits,
            target_platform = target_platform,
        ),
        gn_extra_configs = ["asan"] + gn_extra_configs,
        clusterfuzz_archive_name_prefix = clusterfuzz_archive_name_prefix,
        clusterfuzz_gs_bucket = "chromium-browser-asan",
        console_category = console_category,
        **kwargs
    )

browser_asan_builder(
    name = "ASAN Debug",
    build_config = builder_config.build_config.DEBUG,
    target_bits = 64,
    target_platform = builder_config.target_platform.LINUX,
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    gn_extra_configs = [
        "lsan",
    ],
    siso_remote_jobs = 250,
)

browser_asan_builder(
    name = "ASan Debug (32-bit x86 with V8-ARM)",
    build_config = builder_config.build_config.DEBUG,
    target_bits = 32,
    target_platform = builder_config.target_platform.LINUX,
    clusterfuzz_archive_name_prefix = "asan-v8-arm",
    clusterfuzz_archive_subdir = "v8-arm",
    console_category = "linux asan|x64 v8-ARM",
    contact_team_email = "v8-infra@google.com",
    gn_extra_configs = [
        "v8_heap",
        "v8_hybrid",
    ],
)

browser_asan_builder(
    name = "ASAN Release",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.LINUX,
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    gn_extra_configs = [
        "lsan",
        "fuzzer",
        "v8_heap",
    ],
    max_concurrent_invocations = 5,
    siso_remote_jobs = 250,
)

browser_asan_builder(
    name = "ASan Release (32-bit x86 with V8-ARM)",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 32,
    target_platform = builder_config.target_platform.LINUX,
    clusterfuzz_archive_name_prefix = "asan-v8-arm",
    clusterfuzz_archive_subdir = "v8-arm",
    console_category = "linux asan|x64 v8-ARM",
    contact_team_email = "v8-infra@google.com",
    gn_extra_configs = [
        "fuzzer",
        "v8_heap",
        "v8_hybrid",
    ],
)

browser_asan_builder(
    name = "ASAN Release Media",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.LINUX,
    clusterfuzz_archive_subdir = "media",
    console_short_name = "med",
    gn_extra_configs = [
        "lsan",
        "v8_heap",
        "chromeos_codecs",
    ],
    siso_remote_jobs = 250,
)

ci.builder(
    name = "ASAN Release V8 Sandbox Testing",
    description_html = "This builder produces an ASan Chromium build in the V8 Sandbox Testing configuration.",
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
            archive_name_prefix = "asan-v8-sandbox-testing",
            archive_subdir = "v8-sandbox-testing",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "fuzzer",
            "v8_sandbox_testing",
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
    # TODO(saelo): remove this once we've verified that the builder works.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "linux asan",
        short_name = "sbxtst",
    ),
    contact_team_email = "v8-infra@google.com",
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
        short_name = "cent",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    execution_timeout = 4 * time.hour,
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
            "mojo_fuzzer",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "centipede",
        short_name = "cent high",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-centipede",
        "upload_directory": "asan",
        "archive_prefix": "centipede-high-end",
    },
)

ci.builder(
    name = "Centipede High End Upload Linux ASan DCheck",
    description_html = """This builder uploads centipede high end fuzzers \
in release mode with dcheck_always_on.\
""",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(),
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
            "asan",
            "centipede",
            "disable_seed_corpus",
            "high_end_fuzzer_targets",
            "linux",
            "optimize_for_fuzzing",
            "release_with_dchecks",
            "remoteexec",
            "shared",
            "x64",
        ],
    ),
    # TODO(crbug.com/399002817): add this to the gardener_rotations.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "centipede",
        short_name = "cent high dc",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-centipede",
        "upload_directory": "asan",
        "archive_prefix": "centipede-high-end-dcheck",
    },
)

ci.builder(
    name = "Libfuzzer High End Upload Linux ASan",
    description_html = """This builder uploads centipede high end fuzzers.\
Those fuzzers require more resources to run correctly.\
""",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(),
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
            "shared",
            "release",
            "remoteexec",
            "disable_seed_corpus",
            "high_end_fuzzer_targets",
            "linux",
            "x64",
            "mojo_fuzzer",
        ],
    ),
    # TODO(crbug.com/399002817): add this to the gardener_rotations.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux high end",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
        "archive_prefix": "libfuzzer-high-end",
    },
)

ci.builder(
    name = "Libfuzzer High End Upload Linux ASan Debug",
    description_html = """This builder uploads centipede high end fuzzers.\
Those fuzzers require more resources to run correctly.\
""",
    executable = "recipe:chromium/fuzz",
    triggering_policy = scheduler.greedy_batching(),
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
            "high_end_fuzzer_targets",
            "linux",
            "x64",
        ],
    ),
    # TODO(crbug.com/399002817): add this to the gardener_rotations.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "libfuzz",
        short_name = "linux high dbg",
    ),
    contact_team_email = "chrome-deet-core@google.com",
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
        "archive_prefix": "libfuzzer-high-end",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

browser_asan_builder(
    name = "ASan Release Media (32-bit x86 with V8-ARM)",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 32,
    target_platform = builder_config.target_platform.LINUX,
    clusterfuzz_archive_name_prefix = "asan-v8-arm",
    clusterfuzz_archive_subdir = "v8-arm-media",
    console_category = "linux asan|x64 v8-ARM",
    console_short_name = "med",
    contact_team_email = "v8-infra@google.com",
    gn_extra_configs = [
        "fuzzer",
        "v8_heap",
        "chromeos_codecs",
        "v8_hybrid",
    ],
)

browser_asan_builder(
    name = "ChromiumOS ASAN Release",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.CHROMEOS,
    clusterfuzz_archive_subdir = "chromeos",
    console_category = "cros asan",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    gclient_apply_configs = ["chromeos"],
    gn_extra_configs = [
        "lsan",
        "fuzzer",
        "v8_heap",
    ],
    max_concurrent_invocations = 6,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

def browser_msan_builder(**kwargs):
    return browser_builder(
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
        clusterfuzz_gs_bucket = "chromium-browser-msan",
        os = os.LINUX_FOCAL,
        console_category = "linux msan",
        contact_team_email = "chrome-sanitizer-builder-owners@google.com",
        siso_remote_jobs = 250,
        **kwargs
    )

browser_msan_builder(
    name = "MSAN Release (chained origins)",
    clusterfuzz_archive_name_prefix = "msan-chained-origins",
    console_short_name = "org",
    gn_extra_configs = [
        "msan",
    ],
)

browser_msan_builder(
    name = "MSAN Release (no origins)",
    clusterfuzz_archive_name_prefix = "msan-no-origins",
    gn_extra_configs = [
        "msan_no_origins",
    ],
)

def browser_asan_mac_builder(
        gn_extra_configs = [],
        **kwargs):
    return browser_asan_builder(
        max_concurrent_invocations = 2,
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.MAC,
        gn_extra_configs = [
            "fuzzer",
            "v8_heap",
        ] + gn_extra_configs,
        os = os.MAC_DEFAULT,
        console_category = "mac asan",
        **kwargs
    )

browser_asan_mac_builder(
    name = "Mac ASAN Release",
    builderless = True,
    cpu = cpu.ARM64,
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            pending_time = struct(),  # exception added because this builder has a pool of 1 machine and 2 concurrent invocations
        ),
    }),
)

browser_asan_mac_builder(
    name = "Mac ASAN Release Media",
    builderless = False,
    cores = 12,
    clusterfuzz_archive_subdir = "media",
    console_short_name = "med",
    gn_extra_configs = [
        "chrome_with_codecs",
    ],
)

def browser_tsan_builder(
        build_config = None,
        **kwargs):
    return browser_builder(
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
                "tsan2",
                "clobber",
            ],
            build_config = build_config,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive_name_prefix = "tsan",
        clusterfuzz_gs_bucket = "chromium-browser-tsan",
        gn_extra_configs = [
            "tsan",
        ],
        console_category = "linux tsan",
        contact_team_email = "chrome-sanitizer-builder-owners@google.com",
        **kwargs
    )

browser_tsan_builder(
    name = "TSAN Debug",
    build_config = builder_config.build_config.DEBUG,
)

browser_tsan_builder(
    name = "TSAN Release",
    build_config = builder_config.build_config.RELEASE,
    max_concurrent_invocations = 3,
)

def browser_ubsan_builder(
        chromium_config_name = None,
        **kwargs):
    return browser_builder(
        chromium_config = builder_config.chromium_config(
            config = chromium_config_name,
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        console_category = "linux UBSan",
        clusterfuzz_gs_bucket = "chromium-browser-ubsan",
        contact_team_email = "chrome-sanitizer-builder-owners@google.com",
        siso_remote_jobs = 250,
        **kwargs
    )

browser_ubsan_builder(
    name = "UBSan Release",
    chromium_config_name = "chromium_linux_ubsan",
    clusterfuzz_archive_name_prefix = "ubsan",
    gn_extra_configs = [
        "ubsan",
    ],
)

browser_ubsan_builder(
    name = "UBSan vptr Release",
    chromium_config_name = "chromium_linux_ubsan_vptr",
    clusterfuzz_archive_name_prefix = "ubsan-vptr",
    clusterfuzz_archive_subdir = "vptr",
    console_short_name = "vpt",
    gn_extra_configs = [
        "ubsan_vptr",
        "ubsan_vptr_no_recover_hack",
    ],
)

def browser_asan_win_builder(
        gn_extra_configs = [],
        **kwargs):
    return browser_asan_builder(
        chromium_config_name = "chromium_win_clang_asan",
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.WIN,
        gn_extra_configs = [
            "clang",
            "fuzzer",
            "v8_heap",
        ] + gn_extra_configs,
        builderless = False,
        os = os.WINDOWS_DEFAULT,
        console_category = "win asan",
        contact_team_email = "chrome-sanitizer-builder-owners@google.com",
        siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
        **kwargs
    )

browser_asan_win_builder(
    name = "Win ASan Release",
    max_concurrent_invocations = 7,
)

browser_asan_win_builder(
    name = "Win ASan Release Media",
    clusterfuzz_archive_subdir = "media",
    console_short_name = "med",
    gn_extra_configs = [
        "chrome_with_codecs",
    ],
    max_concurrent_invocations = 6,
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
    triggering_policy = scheduler.greedy_batching(),
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
    triggering_policy = scheduler.greedy_batching(),
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
    triggering_policy = scheduler.greedy_batching(),
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
    triggering_policy = scheduler.greedy_batching(),
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
            target_bits = 32,
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
            "mojo_fuzzer",
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
    properties = {
        "upload_bucket": "chromium-browser-libfuzzer",
        "upload_directory": "asan",
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)
