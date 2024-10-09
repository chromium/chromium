# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.linux builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify_builder")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.linux",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 8,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    orchestrator_siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_linking = True,
)

consoles.list_view(
    name = "tryserver.chromium.linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
)

try_.builder(
    name = "compile-size",
    branch_selector = branches.selector.MAIN,
    # TODO: crbug.com/370594503 - Add documents for compile-size.
    description_html = "Measures and prevents unexpected compile input size " +
                       "growth. See docs for details.",
    executable = "recipe:build_size_trybot",
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "system_headers_in_deps",
            "dcheck_off",
            "linux",
            "x64",
        ],
    ),
    # TODO: crbug.com/40190002 - Make builderful before productionizing.
    builderless = True,
    cores = 8,
    contact_team_email = "build@chromium.org",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "chrome",
            ],
            "compile_targets": [
                "chrome",
            ],
        },
    },
)

try_.builder(
    name = "layout_test_leak_detection",
    mirrors = [
        "ci/WebKit Linux Leak",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "leak_detection_linux",
    mirrors = ["ci/Leak Detection Linux"],
    gn_args = gn_args.config(
        configs = ["ci/Leak Detection Linux", "release_try_builder"],
    ),
)

try_.builder(
    name = "linux-afl-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    gn_args = gn_args.config(
        configs = [
            "afl",
            "asan",
            "shared",
            "release",
            "remoteexec",
            "no_symbols",
            "dcheck_always_on",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "mojo_fuzzer",
            "skip_generate_fuzzer_owners",
            "linux",
            "x64",
        ],
    ),
)

try_.builder(
    name = "linux-annotator-rel",
    mirrors = ["ci/linux-annotator-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-annotator-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-cast-arm-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-cast-arm-rel",
    ],
    gn_args = "ci/linux-cast-arm-rel",
    contact_team_email = "cast-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-cast-arm64-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-cast-arm64-rel",
    ],
    gn_args = "ci/linux-cast-arm64-rel",
    contact_team_email = "cast-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-bfcache-rel",
    mirrors = [
        "ci/linux-bfcache-rel",
    ],
    gn_args = "ci/linux-bfcache-rel",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-blink-heap-verification-try",
    mirrors = ["ci/linux-blink-heap-verification"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-blink-heap-verification",
            "try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-centipede-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    mirrors = ["ci/Centipede Upload Linux ASan"],
    gn_args = gn_args.config(
        configs = [
            "ci/Centipede Upload Linux ASan",
            "dcheck_always_on",
            "no_symbols",
            "skip_generate_fuzzer_owners",
        ],
    ),
)

try_.builder(
    name = "linux-clobber-rel",
    mirrors = [
        "ci/linux-archive-rel",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "no_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-dcheck-off-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "dcheck_off",
            "linux",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-extended-tracing-rel",
    mirrors = [
        "ci/linux-extended-tracing-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-extended-tracing-rel",
            "release_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-gcc-rel",
    mirrors = [
        "ci/linux-gcc-rel",
    ],
    gn_args = "ci/linux-gcc-rel",
    # Focal is needed for better C++20 support. See crbug.com/1284275.
    os = os.LINUX_FOCAL,
)

try_.builder(
    name = "linux-headless-shell-rel",
    mirrors = ["ci/linux-headless-shell-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-headless-shell-rel",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/headless/.+",
            "dbus/.+",
            "headless/.+",
        ],
    ),
)

try_.builder(
    name = "linux-fieldtrial-rel",
    mirrors = ["ci/linux-fieldtrial-rel"],
    gn_args = "ci/linux-fieldtrial-rel",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-multiscreen-fyi-rel",
    mirrors = [
        "ci/linux-multiscreen-fyi-rel",
    ],
    gn_args = "ci/linux-multiscreen-fyi-rel",
    contact_team_email = "web-windowing-team@google.com",
)

try_.builder(
    name = "linux-layout-tests-edit-ng",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "no_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    mirrors = [
        "ci/Libfuzzer Upload Linux ASan",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Libfuzzer Upload Linux ASan",
            "dcheck_always_on",
            "no_symbols",
            "skip_generate_fuzzer_owners",
        ],
    ),
    builderless = not settings.is_main,
    experiments = {
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-perfetto-rel",
    mirrors = ["ci/linux-perfetto-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-perfetto-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "base/trace_event/.+",
            "base/tracing/.+",
            "components/tracing/.+",
            "content/browser/tracing/.+",
            "services/tracing/.+",
        ],
    ),
)

try_.orchestrator_builder(
    name = "linux-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder",
        "ci/Linux Tests",
        "ci/GPU Linux Builder",
        "ci/Linux Release (NVIDIA)",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Linux Builder",
            "release_try_builder",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "linux-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # b/346598710
        "chromium.luci_analysis_v2": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "linux-rel-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "linux-full-remote-rel",
    description_html = "Experimental " + linkify_builder("try", "linux-rel", "chromium") + " builder with more kinds of remote actions. e.g. remote linking",
    mirrors = builder_config.copy_from("linux-rel"),
    builder_config_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    gn_args = "try/linux-rel",
    compilator = "linux-full-remote-rel-compilator",
    contact_team_email = "chrome-build-team@google.com",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "linux-full-remote-rel-compilator",
    contact_team_email = "chrome-build-team@google.com",
)

# TODO(crbug.com/40248746): Remove this builder after burning down failures
# and measuring performance to see if we can roll UBSan into ASan.
try_.builder(
    name = "linux-ubsan-fyi-rel",
    mirrors = [
        "ci/linux-ubsan-fyi-rel",
    ],
    gn_args = "ci/linux-ubsan-fyi-rel",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wayland-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Linux Builder (Wayland)",
            "release_try_builder",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    ssd = True,
    # TODO(crbug.com/329118490): Re-enable flake endorser.
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(
        location_filters = [
            "chrome/browser/.+(ui|browser)test.+",
            "chrome/browser/ui/views/.+test.+",
            "chrome/browser/ui/views/tabs/.+",
            "third_party/wayland/.+",
            "third_party/wayland-protocols/.+",
            "ui/ozone/platform/wayland/.+",
            "ui/views/widget/.+test.+",
        ],
    ),
    use_clang_coverage = True,
)

try_.builder(
    name = "linux-viz-rel",
    mirrors = ["ci/Linux Viz"],
    gn_args = gn_args.config(
        configs = [
            "ci/Linux Viz",
            "no_symbols",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-webkit-asan-rel",
    mirrors = [
        "ci/WebKit Linux ASAN",
    ],
    gn_args = "ci/WebKit Linux ASAN",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-webkit-msan-rel",
    mirrors = [
        "ci/WebKit Linux MSAN",
    ],
    gn_args = "ci/WebKit Linux MSAN",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-cast-x64-dbg",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-cast-x64-dbg",
    ],
    gn_args = "ci/linux-cast-x64-dbg",
    contact_team_email = "cast-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-cast-x64-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-cast-x64-rel",
    ],
    gn_args = "ci/linux-cast-x64-rel",
    contact_team_email = "cast-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_archive_rel_ng",
    mirrors = [
        "ci/linux-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux ASan LSan Builder",
        "ci/Linux ASan LSan Tests (1)",
    ],
    gn_args = "ci/Linux ASan LSan Builder",
    compilator = "linux_chromium_asan_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        "chromium.compilator_can_outlive_parent": 100,
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # b/346598710
        "chromium.luci_analysis_v2": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    siso_remote_linking = True,
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_cfi_rel_ng",
    mirrors = [
        "ci/Linux CFI",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Linux CFI",
        ],
    ),
    cores = 32,
    ssd = True,
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            # Diectories that have caused breakages in the past due to the
            # TensorFlowLite roll.
            "third_party/eigen3/.+",
            "third_party/farmhash/.+",
            "third_party/fft2d/.+",
            "third_party/flatbuffers/.+",
            "third_party/fp16/.+",
            "third_party/fxdiv/.+",
            "third_party/gemmlowp/.+",
            "third_party/pthreadpool/.+",
            "third_party/ruy/.+",
            "third_party/tflite/.+",
            "third_party/xnnpack/.+",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_chromeos_asan_rel_ng",
    mirrors = [
        "ci/Linux Chromium OS ASan LSan Builder",
        "ci/Linux Chromium OS ASan LSan Tests (1)",
    ],
    gn_args = "ci/Linux Chromium OS ASan LSan Builder",
    # TODO(crbug.com/41482936): Remove this when memory consumption during links
    # is reduced.
    cores = 16,
    ssd = True,
    # TODO(crbug.com/40728894): Remove this timeout once we figure out the
    # regression in compiler or toolchain.
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    mirrors = [
        "ci/Linux ChromiumOS MSan Builder",
        "ci/Linux ChromiumOS MSan Tests",
    ],
    gn_args = "ci/Linux ChromiumOS MSan Builder",
    cores = 16,
    ssd = True,
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_clobber_deterministic",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "no_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_compile_dbg_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = ["ci/Linux Builder (dbg)"],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = not settings.is_main,
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    experiments = {
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "linux_chromium_compile_rel_ng",
    mirrors = [
        "ci/Linux Builder",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "no_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_dbg_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder (dbg)",
        "Linux Tests (dbg)(1)",
    ],
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "build/.*check_gn_headers.*",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_msan_rel_ng",
    mirrors = [
        "ci/Linux MSan Builder",
        "ci/Linux MSan Tests",
    ],
    # This is intentionally a release_bot and not a release_trybot;
    # enabling DCHECKs seems to cause flaky failures that don't show up
    # on the continuous builder.
    gn_args = "ci/Linux MSan Builder",
    cores = 16,
    ssd = True,
    execution_timeout = 8 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux TSan Builder",
        "ci/Linux TSan Tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Linux TSan Builder",
            "release_try_builder",
            "minimal_symbols",
        ],
    ),
    compilator = "linux_chromium_tsan_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # b/346598710
        "chromium.luci_analysis_v2": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "linux-ubsan-vptr",
    mirrors = [
        "ci/linux-ubsan-vptr",
    ],
    # This is intentionally a release_bot and not a release_trybot to match
    # the CI configuration, where no debug builder exists.
    gn_args = "ci/linux-ubsan-vptr",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_upload_clang",
    executable = "recipe:chromium_toolchain/package_clang",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    ssd = True,
    execution_timeout = 5 * time.hour,
    notifies = ["chrome-rust-toolchain"],
)

try_.builder(
    name = "linux_upload_rust",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = True,
    cores = 32,
    ssd = True,
    execution_timeout = 5 * time.hour,
    notifies = ["chrome-rust-toolchain"],
)

try_.builder(
    name = "linux-v4l2-codec-rel",
    mirrors = [
        "ci/linux-v4l2-codec-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-v4l2-codec-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "media/gpu/chromeos/.+"),
            cq.location_filter(path_regexp = "media/gpu/v4l2/.+"),
        ],
    ),
)

try_.builder(
    name = "network_service_linux",
    mirrors = [
        "ci/Network Service Linux",
    ],
    gn_args = gn_args.config(
        configs = ["ci/Network Service Linux", "release_try_builder"],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "tricium-metrics-analysis",
    executable = "recipe:tricium_metrics",
)

try_.builder(
    name = "tricium-oilpan-analysis",
    executable = "recipe:tricium_oilpan",
)

try_.builder(
    name = "tricium-simple",
    executable = "recipe:tricium_simple",
)

try_.gpu.optional_tests_builder(
    name = "linux_optional_gpu_tests_rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "content/test/data/gpu/.+"),
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "media/audio/.+"),
            cq.location_filter(path_regexp = "media/base/.+"),
            cq.location_filter(path_regexp = "media/capture/.+"),
            cq.location_filter(path_regexp = "media/filters/.+"),
            cq.location_filter(path_regexp = "media/gpu/.+"),
            cq.location_filter(path_regexp = "media/mojo/.+"),
            cq.location_filter(path_regexp = "media/renderers/.+"),
            cq.location_filter(path_regexp = "media/video/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.linux.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

# This builder is different from try/linux-js-code-coverage builder below as
# this is a try builder meant to provide javascript coverage for webui related
# CLs, where as try/linux-js-code-coverage builder is there to test changes in
# ci/linux-js-code-coverage builder and would mostly be used by coverage devs
# only.
try_.builder(
    name = "linux-js-coverage-rel",
    mirrors = ["ci/linux-js-code-coverage"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-js-code-coverage",
        ],
    ),
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = r".*\.(js|ts)"),
        ],
    ),
    use_javascript_coverage = True,
)

# This builder is different from try/chromeos-js-code-coverage builder below as
# this is a try builder meant to provide javascript coverage for webui related
# CLs, where as try/chromeos-js-code-coverage builder is there to test changes
# in ci/chromeos-js-code-coverage builder and would mostly be used by coverage
# devs only.
try_.builder(
    name = "chromeos-js-coverage-rel",
    mirrors = ["ci/chromeos-js-code-coverage"],
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-js-code-coverage",
        ],
    ),
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 50,
        location_filters = [
            cq.location_filter(path_regexp = r".*\.(js|ts)"),
        ],
    ),
    use_javascript_coverage = True,
)

# Coverage builders set up mainly to test changes in CI builders
try_.builder(
    name = "linux-code-coverage",
    mirrors = ["ci/linux-code-coverage"],
    gn_args = "ci/linux-code-coverage",
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "linux-chromeos-code-coverage",
    mirrors = ["ci/linux-chromeos-code-coverage"],
    gn_args = "ci/linux-chromeos-code-coverage",
    execution_timeout = 20 * time.hour,
)

# This builder serves a different purpose than try/linux-js-coverage-rel
# See the note on linux-js-coverage-rel builder above to understand more.
try_.builder(
    name = "linux-js-code-coverage",
    mirrors = ["ci/linux-js-code-coverage"],
    gn_args = "ci/linux-js-code-coverage",
    execution_timeout = 20 * time.hour,
    use_javascript_coverage = True,
)

try_.builder(
    name = "chromeos-js-code-coverage",
    mirrors = ["ci/chromeos-js-code-coverage"],
    gn_args = "ci/chromeos-js-code-coverage",
    execution_timeout = 20 * time.hour,
    use_javascript_coverage = True,
)
############### Coverage Builders End ##################
