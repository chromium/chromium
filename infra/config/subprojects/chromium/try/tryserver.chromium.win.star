# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.win builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.win",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    compilator_cores = 16,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

consoles.list_view(
    name = "tryserver.chromium.win",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
)

try_.builder(
    name = "win-annotator-rel",
    mirrors = ["ci/win-annotator-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/win-annotator-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "win-asan",
    mirrors = [
        "ci/win-asan",
    ],
    gn_args = "ci/win-asan",
    cores = 16,
    ssd = True,
    execution_timeout = 9 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "win-celab-try-rel",
    executable = "recipe:celab",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    properties = {
        "exclude": "chrome_only",
        "pool_name": "celab-chromium-try",
        "pool_size": 20,
        "tests": "*",
    },
)

try_.builder(
    name = "win-clobber-rel",
    mirrors = [
        "ci/win-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-archive-rel",
            "no_symbols",
            "dcheck_always_on",
        ],
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
)

try_.builder(
    name = "win-libfuzzer-asan-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    executable = "recipe:chromium/fuzz",
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "release_try_builder",
            "reclient",
            "chrome_with_codecs",
            "pdf_xfa",
            "disable_nacl",
            "mojo_fuzzer",
            "skip_generate_fuzzer_owners",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_ANY,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.orchestrator_builder(
    name = "win-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win10 Tests x64",
        "ci/GPU Win x64 Builder",
        "ci/Win10 x64 Release (NVIDIA)",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Win x64 Builder",
            "release_try_builder",
            "no_resource_allowlisting",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "enable_dangling_raw_ptr_feature_flag",
        ],
    ),
    compilator = "win-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 5,
        "chromium.compilator_can_outlive_parent": 100,
        # crbug/940930
        "chromium.enable_cleandead": 50,
    },
    main_list_view = "try",
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    #use_orchestrator_pool = True,
    siso_enabled = True,
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "win-rel-compilator",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    cores = 32 if settings.is_main else 16,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "win32-clobber-rel",
    mirrors = [
        "ci/win32-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win32-archive-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
)

try_.builder(
    name = "win_chromium_compile_dbg_ng",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/Win Builder (dbg)",
        ],
    ),
    builderless = False,
    cores = 16,
    ssd = True,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(
        # TODO(crbug.com/1335555) Remove once cancelling doesn't wipe
        # out builder cache
        cancel_stale = False,
    ),
)

try_.builder(
    name = "win_chromium_compile_siso_dbg_ng",
    description_html = """\
This builder shadows win_chromium_compile_dbg_ng builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating win_chromium_compile_dbg_ng from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/win_chromium_compile_dbg_ng"),
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = "try/win_chromium_compile_dbg_ng",
    builderless = False,
    cores = 16,
    ssd = True,
    contact_team_email = "chrome-build-team@google.com",
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    siso_enabled = True,
    tryjob = try_.job(
        cancel_stale = False,
        experiment_percentage = 10,
    ),
)

try_.builder(
    name = "win_chromium_compile_rel_ng",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/Win Builder",
            "release_try_builder",
            "resource_allowlisting",
        ],
    ),
)

try_.builder(
    name = "win_chromium_x64_rel_ng",
    mirrors = [
        "ci/Win x64 Builder",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Win x64 Builder",
            "release_try_builder",
        ],
    ),
)

try_.builder(
    name = "win_upload_clang",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = False,
    cores = 32,
    os = os.WINDOWS_ANY,
    execution_timeout = 6 * time.hour,
    reclient_instance = None,
)

try_.builder(
    name = "win_upload_rust",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = False,
    cores = 32,
    os = os.WINDOWS_ANY,
    execution_timeout = 6 * time.hour,
    reclient_instance = None,
)

try_.builder(
    name = "win10-dbg",
    mirrors = [
        "ci/Win x64 Builder (dbg)",
        "ci/Win10 Tests x64 (dbg)",
    ],
    gn_args = "ci/Win x64 Builder (dbg)",
    cores = 16,
    os = os.WINDOWS_10,
    ssd = True,
)

try_.builder(
    name = "win10-multiscreen-fyi-rel",
    description_html = (
        "This builder is intended to run tests related to multiscreen " +
        "functionality on Windows. For more info, see " +
        "<a href=\"http://shortn/_4L6uYvA1xU\">http://shortn/_4L6uYvA1xU</a>."
    ),
    mirrors = [
        "ci/win10-multiscreen-fyi-rel",
    ],
    gn_args = "ci/win10-multiscreen-fyi-rel",
    os = os.WINDOWS_10,
    contact_team_email = "web-windowing-team@google.com",
)

try_.builder(
    name = "win10-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/win10-wpt-content-shell-fyi-rel",
    ],
    gn_args = "ci/win10-wpt-content-shell-fyi-rel",
    os = os.WINDOWS_10,
)

try_.builder(
    name = "win11-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/win11-wpt-content-shell-fyi-rel",
    ],
    gn_args = "ci/win11-wpt-content-shell-fyi-rel",
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "win11-x64-fyi-rel",
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win11 Tests x64",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Win x64 Builder",
            "release_try_builder",
            "no_resource_allowlisting",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "enable_dangling_raw_ptr_feature_flag",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_10,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(
        # TODO(https://crbug.com/1441206): Enable after resources verified.
        experiment_percentage = 10,
        location_filters = [
            "sandbox/win/.+",
            "sandbox/policy/win/.+",
        ],
    ),
    use_clang_coverage = True,
)

try_.builder(
    name = "win-fieldtrial-rel",
    mirrors = ["ci/win-fieldtrial-rel"],
    gn_args = "ci/win-fieldtrial-rel",
    os = os.WINDOWS_DEFAULT,
)

try_.builder(
    name = "win-perfetto-rel",
    mirrors = [
        "ci/win-perfetto-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-perfetto-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "win10-code-coverage",
    mirrors = ["ci/win10-code-coverage"],
    gn_args = "ci/win10-code-coverage",
    execution_timeout = 20 * time.hour,
)

try_.gpu.optional_tests_builder(
    name = "win_optional_gpu_tests_rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_internal",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "dcheck_always_on",
            "disable_nacl",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "chrome/browser/media/.+"),
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "components/cdm/renderer/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "device/vr/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "media/audio/.+"),
            cq.location_filter(path_regexp = "media/base/.+"),
            cq.location_filter(path_regexp = "media/capture/.+"),
            cq.location_filter(path_regexp = "media/cdm/.+"),
            cq.location_filter(path_regexp = "media/filters/.+"),
            cq.location_filter(path_regexp = "media/gpu/.+"),
            cq.location_filter(path_regexp = "media/mojo/.+"),
            cq.location_filter(path_regexp = "media/renderers/.+"),
            cq.location_filter(path_regexp = "media/video/.+"),
            cq.location_filter(path_regexp = "services/webnn/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.win.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/vr/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/xr/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.win.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)
