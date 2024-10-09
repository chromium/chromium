# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify_builder")
load("//lib/xcode.star", "xcode")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.mac",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    os = os.MAC_DEFAULT,
    ssd = True,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    orchestrator_siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("xcode", xcode.xcode_default)
    return try_.builder(name = name, **kwargs)

consoles.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = [
        branches.selector.MAC_BRANCHES,
        branches.selector.IOS_BRANCHES,
    ],
)

try_.builder(
    name = "mac-arm64-clobber-rel",
    mirrors = [
        "ci/mac-arm64-archive-rel",
    ],
    gn_args = "ci/mac-arm64-archive-rel",
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-arm64-on-arm64-rel",
    mirrors = [
        "ci/mac-arm64-on-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-arm64-on-arm64-rel",
            "release_try_builder",
        ],
    ),
    builderless = False,
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-clobber-rel",
    mirrors = [
        "ci/mac-archive-rel",
    ],
    gn_args = "ci/mac-archive-rel",
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-osxbeta-rel",
    mirrors = [
        "ci/mac-osxbeta-rel",
    ],
    gn_args = "ci/mac-osxbeta-rel",
    builderless = False,
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-intel-on-arm64-rel",
    mirrors = [
        "ci/mac-intel-on-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-intel-on-arm64-rel",
            "release_try_builder",
        ],
    ),
    builderless = False,
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-fieldtrial-tester",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac-fieldtrial-tester",
    ],
    gn_args = "ci/mac-arm64-rel",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-builder-next",
    mirrors = ["ci/Mac Builder Next"],
    gn_args = "ci/Mac Builder Next",
    builderless = False,
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-perfetto-rel",
    mirrors = [
        "ci/mac-perfetto-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-perfetto-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "mac-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/mac14-tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        "ci/Mac Retina Release (AMD)",
    ],
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "enable_dangling_raw_ptr_feature_flag",
            "enable_backup_ref_ptr_feature_flag",
            "mac",
            "x64",
        ],
    ),
    compilator = "mac-rel-compilator",
    coverage_test_types = ["overall", "unit"],
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
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    #use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "mac-rel-compilator",
    branch_selector = branches.selector.MAC_BRANCHES,
    cpu = cpu.ARM64,
    main_list_view = "try",
)

try_.builder(
    name = "mac11-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac11-arm64-rel-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "mac",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac-lsan-fyi-rel",
    mirrors = [
        "ci/mac-lsan-fyi-rel",
    ],
    gn_args = "ci/mac-lsan-fyi-rel",
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac-ubsan-fyi-rel",
    mirrors = [
        "ci/mac-ubsan-fyi-rel",
    ],
    gn_args = "ci/mac-ubsan-fyi-rel",
)

try_.builder(
    name = "mac12-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac12-arm64-rel-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "mac",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    main_list_view = "try",
)

try_.builder(
    name = "mac13-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac13-arm64-rel-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "mac",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "mac14-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Compiles and runs MacOS 14 tests on ARM machines",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac14-arm64-rel-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "mac",
        ],
    ),
    compilator = "mac14-arm64-rel-compilator",
    contact_team_email = "bling-engprod@google.com",
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(
        # TODO (crbug.com/338209817): move out of
        # experimental CQ after confirming it's consistently
        # green and fast.
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "mac14-arm64-rel-compilator",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "compilator for mac14-arm64-rel",
    cpu = cpu.ARM64,
    contact_team_email = "bling-engprod@google.com",
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
    main_list_view = "try",
)

# NOTE: the following trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.builder(
    name = "mac_chromium_11.0_rel_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac11 Tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = False,
)

try_.builder(
    name = "mac12-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Mac Builder",
            "release_try_builder",
            "remoteexec",
            "x64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac13-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac13 Tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Mac Builder",
            "release_try_builder",
            "remoteexec",
            "x64",
        ],
    ),
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac14-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Runs default MacOS 14 tests on try.",
    mirrors = [
        "ci/Mac Builder",
        "ci/mac14-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Mac Builder",
            "release_try_builder",
            "remoteexec",
        ],
    ),
    cpu = cpu.ARM64,
    contact_team_email = "bling-engprod@google.com",
)

try_.builder(
    name = "mac_chromium_asan_rel_ng",
    mirrors = [
        "ci/Mac ASan 64 Builder",
        "ci/Mac ASan 64 Tests (1)",
    ],
    gn_args = gn_args.config(
        configs = [
            "asan",
            "dcheck_always_on",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder (dbg)",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/Mac Builder (dbg)",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    experiments = {
        # crbug/940930
        "chromium.enable_cleandead": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "mac_chromium_compile_rel_ng",
    mirrors = [
        "ci/Mac Builder",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_dbg_ng",
    mirrors = [
        "ci/Mac Builder (dbg)",
        "ci/mac14-tests-dbg",
    ],
    gn_args = "ci/Mac Builder (dbg)",
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_upload_clang",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = False,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "mac_upload_clang_arm",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = False,
    cpu = cpu.ARM64,
    execution_timeout = 8 * time.hour,
)

try_.builder(
    name = "mac_upload_rust",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = False,
    execution_timeout = 8 * time.hour,
)

try_.builder(
    name = "mac_upload_rust_arm",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = False,
    cpu = cpu.ARM64,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "mac-code-coverage",
    mirrors = ["ci/mac-code-coverage"],
    gn_args = "ci/mac-code-coverage",
    cpu = cpu.ARM64,
    execution_timeout = 20 * time.hour,
)

ios_builder(
    name = "ios-asan",
    mirrors = [
        "ci/ios-asan",
    ],
    gn_args = "ci/ios-asan",
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-blink-dbg-fyi",
    mirrors = [
        "ci/ios-blink-dbg-fyi",
    ],
    gn_args = "ci/ios-blink-dbg-fyi",
    builderless = True,
    cpu = cpu.ARM64,
    execution_timeout = 4 * time.hour,
    xcode = xcode.x15betabots,
)

ios_builder(
    name = "ios-catalyst",
    mirrors = [
        "ci/ios-catalyst",
    ],
    gn_args = "ci/ios-catalyst",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-device",
    mirrors = [
        "ci/ios-device",
    ],
    gn_args = "ci/ios-device",
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-fieldtrial-rel",
    mirrors = ["ci/ios-fieldtrial-rel"],
    gn_args = "ci/ios-fieldtrial-rel",
    builderless = True,
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-m1-simulator",
    mirrors = ["ci/ios-m1-simulator"],
    gn_args = "ci/ios-m1-simulator",
    cpu = cpu.ARM64,
)

try_.orchestrator_builder(
    name = "ios-simulator",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/ios-simulator",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    # TODO (crbug.com/1372179): Move back to orchestrator bots once they can be
    # properly rate limited
    # use_orchestrator_pool = True,
    cores = 2,
    os = os.LINUX_DEFAULT,
    compilator = "ios-simulator-compilator",
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # b/346598710
        "chromium.luci_analysis_v2": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    xcode = xcode.xcode_default,
)

try_.compilator_builder(
    name = "ios-simulator-compilator",
    branch_selector = branches.selector.IOS_BRANCHES,
    # Set builderless to False so that branch builders use builderful bots
    builderless = False,
    cpu = cpu.ARM64,
    ssd = None,
    main_list_view = "try",
)

ios_builder(
    name = "ios-simulator-exp",
    description_html = "Experimental " + linkify_builder("try", "ios-simulator", "chromium") + " builder to test new features and changes.",
    mirrors = builder_config.copy_from("try/ios-simulator"),
    builder_config_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    gn_args = "try/ios-simulator",
    cpu = cpu.ARM64,
    contact_team_email = "chrome-build-team@google.com",
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    # TODO: crbug.com/336382863 - Comment out 'tryjob' to not keep this bot running.
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
    use_clang_coverage = True,
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator-full-configs",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/ios-simulator-full-configs",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    cpu = cpu.ARM64,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "ios/.+",
        ],
    ),
    use_clang_coverage = True,
)

ios_builder(
    name = "ios-simulator-noncq",
    mirrors = [
        "ci/ios-simulator-noncq",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/ios-simulator-noncq",
        ],
    ),
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "third_party/crashpad/crashpad/.+",
        ],
    ),
)

ios_builder(
    name = "ios-wpt-fyi-rel",
    mirrors = [
        "ci/ios-wpt-fyi-rel",
    ],
    gn_args = "ci/ios-wpt-fyi-rel",
)

ios_builder(
    name = "ios17-beta-simulator",
    mirrors = ["ci/ios17-beta-simulator"],
    gn_args = "ci/ios17-beta-simulator",
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios17-sdk-simulator",
    mirrors = ["ci/ios17-sdk-simulator"],
    gn_args = "ci/ios17-sdk-simulator",
    cpu = cpu.ARM64,
    xcode = xcode.x16_1betabots,
)

ios_builder(
    name = "ios18-beta-simulator",
    mirrors = [
        "ci/ios18-beta-simulator",
    ],
    gn_args = "ci/ios18-beta-simulator",
    cpu = cpu.ARM64,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios18-sdk-simulator",
    mirrors = [
        "ci/ios18-sdk-simulator",
    ],
    gn_args = "ci/ios18-sdk-simulator",
    cpu = cpu.ARM64,
    xcode = xcode.x16betabots,
)

ios_builder(
    name = "ios-simulator-code-coverage",
    mirrors = ["ci/ios-simulator-code-coverage"],
    gn_args = gn_args.config(
        configs = [
            "ci/ios-simulator-code-coverage",
            "ios_simulator",
        ],
    ),
    builderless = True,
    cpu = cpu.ARM64,
    execution_timeout = 20 * time.hour,
)

try_.gpu.optional_tests_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.selector.IOS_BRANCHES,
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_fyi_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "mac",
            "x64",
        ],
    ),
    cpu = cpu.ARM64,
    ssd = None,
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
            cq.location_filter(path_regexp = "services/shape_detection/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.mac.json"),
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
