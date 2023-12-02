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
    cores = 16,
    ssd = True,
    execution_timeout = 9 * time.hour,
    gn_args = "ci/win-asan",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "win-celab-try-rel",
    executable = "recipe:celab",
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
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "ci/win-archive-rel",
            "no_symbols",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
)

try_.builder(
    name = "win-libfuzzer-asan-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    executable = "recipe:chromium/fuzz",
    builderless = False,
    os = os.WINDOWS_ANY,
    experiments = {
        "chromium.skip_successful_tests": 50,
    },
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
    compilator = "win-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 5,
        "chromium.compilator_can_outlive_parent": 100,
        "chromium.skip_successful_tests": 50,
    },
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
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    #use_orchestrator_pool = True,
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

# TODO: crbug.com/1502025 - Reduce duplicated configs from the shadow builder.
try_.orchestrator_builder(
    name = "win-siso-rel",
    description_html = """\
This builder shadows win-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating win-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/win-rel"),
    compilator = "win-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 5,
    },
    main_list_view = "try",
    tryjob = try_.job(
        # Decreasing the experiment percentage while enabling tests to reduce
        # extra workloads on the test pool.
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "win-siso-rel-compilator",
    # TODO(jwata): Change to 32 once bots have landed
    cores = "16|32",
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
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "ci/win32-archive-rel",
            "release_try_builder",
        ],
    ),
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
    builderless = False,
    cores = 16,
    ssd = True,
    gn_args = gn_args.config(
        configs = [
            "ci/Win Builder (dbg)",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(
        # TODO(crbug.com/1335555) Remove once cancelling doesn't wipe
        # out builder cache
        cancel_stale = False,
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
    cores = 16,
    os = os.WINDOWS_10,
    ssd = True,
    gn_args = "ci/Win x64 Builder (dbg)",
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
    os = os.WINDOWS_10,
    contact_team_email = "web-windowing-team@google.com",
    gn_args = "ci/win10-multiscreen-fyi-rel",
)

try_.builder(
    name = "win10-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/win10-wpt-content-shell-fyi-rel",
    ],
    os = os.WINDOWS_10,
    gn_args = "ci/win10-wpt-content-shell-fyi-rel",
)

try_.builder(
    name = "win11-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/win11-wpt-content-shell-fyi-rel",
    ],
    os = os.WINDOWS_ANY,
    gn_args = "ci/win11-wpt-content-shell-fyi-rel",
)

try_.builder(
    name = "win11-x64-fyi-rel",
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win11 Tests x64",
    ],
    builderless = True,
    os = os.WINDOWS_10,
    coverage_test_types = ["unit", "overall"],
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
    os = os.WINDOWS_DEFAULT,
    gn_args = "ci/win-fieldtrial-rel",
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
    execution_timeout = 20 * time.hour,
    gn_args = "ci/win10-code-coverage",
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
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
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
