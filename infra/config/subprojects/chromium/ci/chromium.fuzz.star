# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")
load("//project.star", "settings")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuzz",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    notifies = ["chrome-fuzzing-core"],
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

_DEFAULT_CONSOLE_ORDERING = consoles.ordering(short_names = ["dbg", "rel"])

_LIBFUZZER_CONSOLE_ORDERING = consoles.ordering(short_names = [
    "linux",
    "linux-dbg",
    "linux high dbg",
    "linux high end",
    "linux32",
    "linux-msan",
    "linux-ubsan",
    "chromeos-asan",
    "mac-asan",
    "mac-arm64-asan",
    "win-asan",
    "arm",
    "arm-dbg",
    "arm64",
    "arm64-dbg",
    "ios",
    "android-asan",
    "android-arm64",
])

consoles.console_view(
    name = "chromium.fuzz",
    branch_selector = [
        branches.selector.LINUX_BRANCHES,
        branches.selector.WINDOWS_BRANCHES,
    ],
    ordering = {
        None: [
            "linux asan",
            "win asan",
            "mac asan",
            "cros asan",
            "linux msan",
            "linux tsan",
            "libfuzzer",
            "libfuzzer-tests",
            "centipede",
            "centipede-tests",
        ],
        "win asan": _DEFAULT_CONSOLE_ORDERING,
        "mac asan": _DEFAULT_CONSOLE_ORDERING,
        "linux asan": _DEFAULT_CONSOLE_ORDERING,
        "linux asan|x64 v8-ARM": _DEFAULT_CONSOLE_ORDERING,
        "libfuzzer": _LIBFUZZER_CONSOLE_ORDERING,
        "libfuzzer-tests": _LIBFUZZER_CONSOLE_ORDERING,
    },
)

_PLATFORM_SHORT_NAMES = {
    builder_config.target_platform.CHROMEOS: "chromeos",
    builder_config.target_platform.LINUX: "linux",
    builder_config.target_platform.MAC: "mac",
    builder_config.target_platform.WIN: "win",
    builder_config.target_platform.ANDROID: "android",
}

_BUILD_CONFIG_SHORT_NAMES = {
    builder_config.build_config.DEBUG: "dbg",
    builder_config.build_config.RELEASE: "rel",
}

def _arch_short_name(target_arch, target_bits):
    if target_arch == None or target_arch == builder_config.target_arch.INTEL:
        if target_bits == 32:
            return "x86"
        if target_bits == 64:
            return "x64"

    if target_arch == builder_config.target_arch.ARM and target_bits == 64:
        return "arm64"

    fail("Unsupported architecture:", str(target_bits) + "-bit", target_arch)

def ci_builder(
        max_concurrent_invocations = None,
        chromium_config_name = None,
        build_config = None,
        target_bits = None,
        target_arch = None,
        target_platform = None,
        chromium_extra_apply_configs = [],
        gclient_apply_configs = None,
        use_component_build = False,
        dcheck_always_on = False,
        clusterfuzz_archive = None,
        gn_extra_configs = [],
        console_category = None,
        console_short_name = None,
        **kwargs):
    gn_configs = ["remoteexec"] + gn_extra_configs

    if build_config == builder_config.build_config.DEBUG:
        gn_configs.append("debug")
        gn_configs.append("minimal_symbols")
    elif build_config == builder_config.build_config.RELEASE:
        gn_configs.append("release")

    if use_component_build:
        gn_configs.append("shared")
    else:
        gn_configs.append("static")

    if dcheck_always_on:
        gn_configs.append("dcheck_always_on")

    gn_configs.append(_arch_short_name(target_arch, target_bits))

    platform_short_name = _PLATFORM_SHORT_NAMES.get(target_platform)
    if platform_short_name:
        gn_configs.append(platform_short_name)

    return ci.builder(
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = max_concurrent_invocations,
        ),
        builder_spec = builder_config.builder_spec(
            gclient_config = builder_config.gclient_config(
                config = "chromium",
                apply_configs = gclient_apply_configs,
            ),
            chromium_config = builder_config.chromium_config(
                config = chromium_config_name,
                apply_configs = sorted([
                    "mb",
                ] + chromium_extra_apply_configs),
                build_config = build_config,
                target_arch = target_arch,
                target_bits = target_bits,
                target_platform = target_platform,
            ),
            clusterfuzz_archive = clusterfuzz_archive,
        ),
        gn_args = gn_args.config(configs = gn_configs),
        console_view_entry = consoles.console_view_entry(
            category = console_category,
            short_name = console_short_name,
        ),
        **kwargs
    )

def browser_builder(
        max_concurrent_invocations = 4,
        build_config = None,
        clusterfuzz_archive_name_prefix = None,
        clusterfuzz_archive_subdir = None,
        clusterfuzz_gs_bucket = None,
        console_short_name = None,
        **kwargs):
    if build_config == builder_config.build_config.DEBUG:
        default_console_short_name = "dbg"
        use_component_build = True
    elif build_config == builder_config.build_config.RELEASE:
        default_console_short_name = "rel"
        use_component_build = False

    return ci_builder(
        max_concurrent_invocations = max_concurrent_invocations,
        build_config = build_config,
        use_component_build = use_component_build,
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
        console_short_name = console_short_name or default_console_short_name,
        **kwargs
    )

def browser_asan_builder(
        chromium_config_name = "chromium_asan",
        chromium_extra_apply_configs = [],
        clusterfuzz_archive_name_prefix = "asan",
        gn_extra_configs = [],
        console_category = "linux asan",
        **kwargs):
    return browser_builder(
        chromium_config_name = chromium_config_name,
        chromium_extra_apply_configs = [
            "clobber",
        ] + chromium_extra_apply_configs,
        gn_extra_configs = ["asan"] + gn_extra_configs,
        clusterfuzz_archive_name_prefix = clusterfuzz_archive_name_prefix,
        clusterfuzz_gs_bucket = "chromium-browser-asan",
        console_category = console_category,
        **kwargs
    )

def fuzz_target_builder(
        name = None,
        test_builder_name = None,
        build_config = None,
        target_bits = None,
        target_platform = None,
        target_arch = None,
        swarming_mixins = None,
        builderless = True,
        fuzzing_engine = None,
        sanitizer = None,
        branch_selector = None,
        max_concurrent_invocations = None,
        gn_extra_configs = [],
        use_component_build = True,
        chromium_extra_apply_configs = [],
        clusterfuzz_archive_name_prefix = None,
        clusterfuzz_archive_subdir = None,
        clusterfuzz_ios_targets_only = None,
        clusterfuzz_v8_targets_only = None,
        contact_team_email = "chrome-fuzzing-core@google.com",
        **kwargs):
    if not name and not test_builder_name:
        fail("Must specify at least one of name or test_builder_name.")

    gn_configs = [
        fuzzing_engine,
        "shared",
    ] + gn_extra_configs

    properties = {
        "upload_bucket": "chromium-browser-" + fuzzing_engine,
        "upload_directory": clusterfuzz_archive_subdir or sanitizer,
    }

    if clusterfuzz_archive_name_prefix != None:
        properties["archive_prefix"] = clusterfuzz_archive_name_prefix

    if clusterfuzz_ios_targets_only != None:
        properties["ios_targets_only"] = clusterfuzz_ios_targets_only

    if clusterfuzz_v8_targets_only != None:
        properties["v8_targets_only"] = clusterfuzz_v8_targets_only

    # Creating a dict in this manner will result in an error if a named
    # argument we provide collides with a value already specified in `kwargs`,
    # which is desirable.
    kwargs = dict(
        build_config = build_config,
        target_bits = target_bits,
        target_platform = target_platform,
        target_arch = target_arch,
        chromium_config_name = "chromium_clang",
        chromium_extra_apply_configs = [
            "clobber",
        ] + chromium_extra_apply_configs,
        gn_extra_configs = gn_configs,
        use_component_build = use_component_build,
        contact_team_email = contact_team_email,
        **kwargs
    )

    if name:
        ci_builder(
            name = name,
            max_concurrent_invocations = max_concurrent_invocations,
            executable = "recipe:chromium/fuzz",
            # Branch selector only applies to the non-tests builder for now,
            # since the tests builder is not gardened.
            branch_selector = branch_selector,
            builderless = builderless,
            console_category = fuzzing_engine,
            properties = properties,
            **kwargs
        )

    if not test_builder_name:
        return

    # Ensure that the test builder names follow a strict convention, but let
    # the caller specify the literal string for codesearchability.
    expected_name = "-".join([
        _PLATFORM_SHORT_NAMES[target_platform],
        _arch_short_name(target_arch, target_bits),
        fuzzing_engine,
        sanitizer,
        _BUILD_CONFIG_SHORT_NAMES[build_config],
        "tests",
    ])
    if test_builder_name != expected_name:
        fail("Unexpected fuzz target test builder name: got " +
             test_builder_name + ", expected " + expected_name)

    description = "Builds and runs fuzz target tests."
    if name:
        description += " Mirrors the build configuration of \"" + name + "\"."

    ci_builder(
        name = test_builder_name,
        description_html = description,
        # We have 1 machine per builder.
        max_concurrent_invocations = 1,
        # Use the builderless machine pool.
        builderless = True,
        # Build and run fuzzing unit tests.
        targets = targets.bundle(
            targets = ["fuzzing_unittests"],
            mixins = [
                "chromium-tester-service-account",
                targets.mixin(args = ["--asan-detect-odr-violation=0"]),
            ] + swarming_mixins,
        ),
        # TODO(https://crbug.com/432407787): Add to a gardening rotation
        # once the bots are proven green enough.
        gardener_rotations = args.ignore_default(None),
        console_category = fuzzing_engine + "-tests",
        **kwargs
    )

def libfuzzer_builder(**kwargs):
    return fuzz_target_builder(fuzzing_engine = "libfuzzer", **kwargs)

def libfuzzer_linux_builder(
        # Allow overriding despite the name for ChromeOS and Android builder.
        target_platform = builder_config.target_platform.LINUX,
        gn_extra_configs = [],
        swarming_mixins = [],
        **kwargs):
    gn_configs = [
        "chromeos_codecs",
        "optimize_for_fuzzing",
        "pdf_xfa",
    ] + gn_extra_configs

    return libfuzzer_builder(
        swarming_mixins = ["linux-jammy"] + swarming_mixins,
        target_platform = target_platform,
        gn_extra_configs = gn_configs,
        **kwargs
    )

def libfuzzer_linux_asan_builder(
        gn_extra_configs = [],
        **kwargs):
    gn_configs = ["asan"] + gn_extra_configs
    return libfuzzer_linux_builder(
        gn_extra_configs = gn_configs,
        sanitizer = "asan",
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

def centipede_linux_asan_builder(
        gn_extra_configs = [],
        **kwargs):
    return fuzz_target_builder(
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.LINUX,
        contact_team_email = "chrome-fuzzing-core@google.com",
        fuzzing_engine = "centipede",
        sanitizer = "asan",
        gn_extra_configs = [
            "asan",
            "optimize_for_fuzzing",
            "disable_seed_corpus",
        ] + gn_extra_configs,
        **kwargs
    )

centipede_linux_asan_builder(
    name = "Centipede Upload Linux ASan",
    branch_selector = branches.selector.LINUX_BRANCHES,
    clusterfuzz_archive_name_prefix = "centipede",
    console_short_name = "cent",
    execution_timeout = 5 * time.hour,
    gn_extra_configs = [
        "chromeos_codecs",
        "pdf_xfa",
        "mojo_fuzzer",
    ],
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    max_concurrent_invocations = 4 if settings.is_main else None,
    swarming_mixins = ["linux-jammy"],
    test_builder_name = "linux-x64-centipede-asan-rel-tests",
)

centipede_linux_asan_builder(
    name = "Centipede High End Upload Linux ASan",
    description_html = """This builder uploads centipede high end fuzzers.\
Those fuzzers require more resources to run correctly.\
""",
    clusterfuzz_archive_name_prefix = "centipede-high-end",
    console_short_name = "cent high",
    gn_extra_configs = [
        "chromeos_codecs",
        "pdf_xfa",
        "high_end_fuzzer_targets",
        "mojo_fuzzer",
    ],
    max_concurrent_invocations = 4,
)

centipede_linux_asan_builder(
    name = "Centipede High End Upload Linux ASan DCheck",
    description_html = """This builder uploads centipede high end fuzzers \
in release mode with dcheck_always_on.\
""",
    # TODO(crbug.com/399002817): add this to the gardener_rotations.
    gardener_rotations = args.ignore_default(None),
    clusterfuzz_archive_name_prefix = "centipede-high-end-dcheck",
    console_short_name = "cent high dc",
    dcheck_always_on = True,
    gn_extra_configs = [
        "high_end_fuzzer_targets",
    ],
)

def libfuzzer_linux_asan_high_end_builder(
        gn_extra_configs = [],
        **kwargs):
    gn_configs = [
        "high_end_fuzzer_targets",
        "disable_seed_corpus",
    ] + gn_extra_configs

    return libfuzzer_linux_asan_builder(
        description_html = """This builder uploads libfuzzer high end fuzzers.\
Those fuzzers require more resources to run correctly.\
""",
        # TODO(crbug.com/399002817): add this to the gardener_rotations.
        gardener_rotations = args.ignore_default(None),
        target_bits = 64,
        clusterfuzz_archive_name_prefix = "libfuzzer-high-end",
        gn_extra_configs = gn_configs,
        **kwargs
    )

libfuzzer_linux_asan_high_end_builder(
    name = "Libfuzzer High End Upload Linux ASan",
    build_config = builder_config.build_config.RELEASE,
    console_short_name = "linux high end",
    gn_extra_configs = ["mojo_fuzzer"],
)

libfuzzer_linux_asan_high_end_builder(
    name = "Libfuzzer High End Upload Linux ASan Debug",
    build_config = builder_config.build_config.DEBUG,
    console_short_name = "linux high dbg",
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
        chromium_config_name = "chromium_clang",
        chromium_extra_apply_configs = ["clobber", "msan"],
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.LINUX,
        clusterfuzz_gs_bucket = "chromium-browser-msan",
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
        max_concurrent_invocations = 2,
        **kwargs):
    return browser_asan_builder(
        max_concurrent_invocations = max_concurrent_invocations,
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

browser_asan_mac_builder(
    name = "Mac ARM64 ASAN Release",
    description_html = "ASAN build of chrome for Mac ARM64.",
    builderless = True,
    cpu = cpu.ARM64,
    # TODO(https://crbug.com/431089339): Add to gardening rotation once the build
    # is proven green.
    gardener_rotations = args.ignore_default(None),
    target_arch = builder_config.target_arch.ARM,
    # Full subdir: `mac-release-arm64`
    clusterfuzz_archive_subdir = "arm64",
    console_short_name = "arm64-rel",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    # We requested a single machine in https://crbug.com/432473774.
    max_concurrent_invocations = 1,
)

def browser_tsan_builder(**kwargs):
    return browser_builder(
        chromium_config_name = "chromium_clang",
        chromium_extra_apply_configs = ["clobber", "tsan2"],
        target_bits = 64,
        target_platform = builder_config.target_platform.LINUX,
        clusterfuzz_archive_name_prefix = "tsan",
        clusterfuzz_gs_bucket = "chromium-browser-tsan",
        gn_extra_configs = ["tsan"],
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

def browser_ubsan_builder(**kwargs):
    return browser_builder(
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.LINUX,
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

libfuzzer_linux_builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.CHROMEOS,
    clusterfuzz_archive_name_prefix = "libfuzzer-chromeos",
    clusterfuzz_archive_subdir = "chromeos-asan",
    console_short_name = "chromeos-asan",
    execution_timeout = 6 * time.hour,
    gclient_apply_configs = [
        "chromeos",
    ],
    gn_extra_configs = [
        "asan",
        "disable_seed_corpus",
    ],
    max_concurrent_invocations = 3,
    sanitizer = "asan",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    swarming_mixins = ["x86-64"],  # Avoid running on ARM bots.
    test_builder_name = "chromeos-x64-libfuzzer-asan-rel-tests",
)

libfuzzer_builder(
    name = "Libfuzzer Upload iOS Catalyst Debug",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    build_config = builder_config.build_config.DEBUG,
    target_arch = builder_config.target_arch.INTEL,
    target_bits = 64,
    target_platform = builder_config.target_platform.IOS,
    chromium_extra_apply_configs = ["mac_toolchain"],
    clusterfuzz_archive_name_prefix = "libfuzzer-ios",
    clusterfuzz_archive_subdir = "ios-catalyst-debug",
    clusterfuzz_ios_targets_only = True,
    console_short_name = "ios",
    execution_timeout = 4 * time.hour,
    gclient_apply_configs = ["ios"],
    gn_extra_configs = [
        "compile_only",
        "ios_catalyst",
        "asan",
        "no_dsyms",
        "no_remoting",
    ],
    use_component_build = False,
    xcode = xcode.xcode_default,
)

libfuzzer_linux_asan_builder(
    name = "Libfuzzer Upload Linux ASan",
    branch_selector = branches.selector.LINUX_BRANCHES,
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    console_short_name = "linux",
    execution_timeout = 4 * time.hour,
    gn_extra_configs = [
        "mojo_fuzzer",
    ],
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    max_concurrent_invocations = 5 if settings.is_main else None,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    test_builder_name = "linux-x64-libfuzzer-asan-rel-tests",
)

libfuzzer_linux_asan_builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    free_space = builders.free_space.high,
    build_config = builder_config.build_config.DEBUG,
    target_bits = 64,
    console_short_name = "linux-dbg",
    execution_timeout = 4 * time.hour,
    gn_extra_configs = [
        "disable_seed_corpus",
    ],
    max_concurrent_invocations = 5,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    test_builder_name = "linux-x64-libfuzzer-asan-dbg-tests",
)

libfuzzer_linux_builder(
    name = "Libfuzzer Upload Linux MSan",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    chromium_extra_apply_configs = ["msan"],
    console_short_name = "linux-msan",
    gn_extra_configs = [
        "msan",
        "disable_seed_corpus",
    ],
    max_concurrent_invocations = 5,
    sanitizer = "msan",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    test_builder_name = "linux-x64-libfuzzer-msan-rel-tests",
)

libfuzzer_linux_builder(
    name = "Libfuzzer Upload Linux UBSan",
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    console_short_name = "linux-ubsan",
    execution_timeout = 5 * time.hour,
    gn_extra_configs = [
        "ubsan_security_non_vptr",
        "disable_seed_corpus",
    ],
    max_concurrent_invocations = 5,
    sanitizer = "ubsan",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    test_builder_name = "linux-x64-libfuzzer-ubsan-rel-tests",
)

def libfuzzer_linux_v8_arm64_builder(**kwargs):
    return libfuzzer_linux_asan_builder(
        target_bits = 64,
        clusterfuzz_archive_name_prefix = "libfuzzer-v8-arm64",
        clusterfuzz_archive_subdir = "asan-arm64-sim",
        clusterfuzz_v8_targets_only = True,
        contact_team_email = "v8-infra@google.com",
        gn_extra_configs = [
            "v8_simulate_arm64",
            "disable_seed_corpus",
        ],
        **kwargs
    )

libfuzzer_linux_v8_arm64_builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    build_config = builder_config.build_config.RELEASE,
    console_short_name = "arm64",
)

libfuzzer_linux_v8_arm64_builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    build_config = builder_config.build_config.DEBUG,
    console_short_name = "arm64-dbg",
)

libfuzzer_linux_asan_builder(
    name = "Libfuzzer Upload Linux32 ASan",
    build_config = builder_config.build_config.RELEASE,
    target_bits = 32,
    console_short_name = "linux32",
    gn_extra_configs = [
        "disable_seed_corpus",
    ],
    max_concurrent_invocations = 3,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    test_builder_name = "linux-x86-libfuzzer-asan-rel-tests",
)

def libfuzzer_linux32_v8_arm_builder(**kwargs):
    return libfuzzer_linux_asan_builder(
        target_bits = 32,
        gn_extra_configs = [
            "v8_simulate_arm",
            "disable_seed_corpus",
        ],
        contact_team_email = "v8-infra@google.com",
        clusterfuzz_archive_name_prefix = "libfuzzer-v8-arm",
        clusterfuzz_archive_subdir = "asan-arm-sim",
        clusterfuzz_v8_targets_only = True,
        **kwargs
    )

libfuzzer_linux32_v8_arm_builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    build_config = builder_config.build_config.RELEASE,
    console_short_name = "arm",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

libfuzzer_linux32_v8_arm_builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    build_config = builder_config.build_config.DEBUG,
    console_short_name = "arm-dbg",
)

libfuzzer_linux_asan_builder(
    name = "android-desktop-x64-libfuzzer-asan",
    description_html = "This builder uploads android desktop libfuzzer fuzzers, for x64 using ASan.",
    # TODO(crbug.com/328559555): add this to the gardener_rotations
    gardener_rotations = args.ignore_default(None),
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.ANDROID,
    clusterfuzz_archive_subdir = "android-desktop-x64-asan",
    console_short_name = "android-desktop-x64",
    execution_timeout = 6 * time.hour,
    gclient_apply_configs = ["android"],
    gn_extra_configs = [
        "android",
        "asan",
        "android_fastbuild",
        "android_desktop",
    ],
    max_concurrent_invocations = 2,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

libfuzzer_linux_builder(
    name = "android-arm64-libfuzzer-hwasan",
    description_html = "This builder uploads android libfuzzer fuzzers, for arm64 using HWASan.",
    # TODO(crbug.com/328559555): add this to the gardener_rotations
    gardener_rotations = args.ignore_default(None),
    build_config = builder_config.build_config.RELEASE,
    target_arch = builder_config.target_arch.ARM,
    target_bits = 64,
    target_platform = builder_config.target_platform.ANDROID,
    clusterfuzz_archive_subdir = "android-arm64-hwasan",
    console_short_name = "android-arm64",
    contact_team_email = "chrome-fuzzing-core@google.com",
    gclient_apply_configs = ["android"],
    gn_extra_configs = [
        "android",
        "android_fastbuild",
        "hwasan",
    ],
    max_concurrent_invocations = 2,
    sanitizer = "hwasan",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

def libfuzzer_mac_asan_builder(**kwargs):
    return libfuzzer_builder(
        os = os.MAC_DEFAULT,
        build_config = builder_config.build_config.RELEASE,
        target_bits = 64,
        target_platform = builder_config.target_platform.MAC,
        gn_extra_configs = [
            "asan",
            "chrome_with_codecs",
            "optimize_for_fuzzing",
            "mojo_fuzzer",
            "pdf_xfa",
        ],
        sanitizer = "asan",
        **kwargs
    )

libfuzzer_mac_asan_builder(
    name = "Libfuzzer Upload Mac ASan",
    builderless = False,
    cores = 12,
    console_short_name = "mac-asan",
    execution_timeout = 4 * time.hour,
)

libfuzzer_mac_asan_builder(
    # TODO(https://crbug.com/431089340): Stand up a builder that uploads fuzz
    # targets to GCS for ClusterFuzz to fuzz with.
    name = None,
    builderless = True,
    cores = None,  # Use any bot in the builderless pool.
    cpu = cpu.ARM64,
    target_arch = builder_config.target_arch.ARM,
    console_short_name = "mac-arm64-asan",
    swarming_mixins = ["mac_15_arm64"],
    # Even if we don't actively fuzz this build configuration yet, it is useful
    # to test that things nominally work and do not regress.
    test_builder_name = "mac-arm64-libfuzzer-asan-rel-tests",
)

libfuzzer_builder(
    name = "Libfuzzer Upload Windows ASan",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    build_config = builder_config.build_config.RELEASE,
    target_bits = 64,
    target_platform = builder_config.target_platform.WIN,
    console_short_name = "win-asan",
    # crbug.com/1175182: Temporarily increase timeout
    # crbug.com/1372531: Increase timeout again
    execution_timeout = 8 * time.hour,
    # NOTE: optimize_for_fuzzing is used by the other libFuzzer build configs
    # but it does not work on Windows.
    gn_extra_configs = [
        "asan",
        "chrome_with_codecs",
        "minimal_symbols",
        "mojo_fuzzer",
        "pdf_xfa",
    ],
    # Schedule more concurrent builds only on trunk to reduce blamelist sizes.
    max_concurrent_invocations = 3 if settings.is_main else None,
    sanitizer = "asan",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
    swarming_mixins = ["win10-any"],
    test_builder_name = "win-x64-libfuzzer-asan-rel-tests",
    use_component_build = False,
)
