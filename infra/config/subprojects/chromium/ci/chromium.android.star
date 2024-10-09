# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")
load("//project.star", "settings")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.ANDROID,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

targets.settings_defaults.set(
    os_type = targets.os_type.ANDROID,
)

consoles.console_view(
    name = "chromium.android",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    ordering = {
        None: ["cronet", "builder", "tester"],
        "*cpu*": ["arm", "arm64", "x86", "x64"],
        "cronet": "*cpu*",
        "builder": "*cpu*",
        "builder|det": consoles.ordering(short_names = ["rel", "dbg"]),
        "tester": ["phone", "tablet"],
        "builder_tester|arm64": consoles.ordering(short_names = ["M proguard"]),
    },
)

ci.builder(
    name = "Android ASAN (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android_clang",
            apply_configs = [
                "errorprone",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "clang_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "clang",
            "asan",
            "debug_builder",
            "remoteexec",
            # TODO(crbug.com/40282985): Remove no_symbols when unit_tests binary size
            # issue is resolved.
            "no_symbols",
            "strip_debug_info",
            "arm",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    builderless = False,
    cores = None,
    # TODO(crbug.com/40282985): Restore tree-closing and gardener rotation if/when
    # bot is fixed.
    # tree_closing = True,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    contact_team_email = "clank-engprod@google.com",
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    # TODO(crbug.com/40882299): Check why the compile takes longer time.
    execution_timeout = 8 * time.hour,
)

ci.thin_tester(
    name = "Android WebView O (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
            apply_configs = [
                "remove_all_system_webviews",
            ],
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = "webview_bot_all_gtests",
        mixins = [
            "has_native_resultdb_integration",
            "oreo_mr1_fleet",
            "walleye",
            targets.mixin(
                swarming = targets.swarming(
                    expiration_sec = 10800,
                ),
            ),
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
    # https://crbug.com/341875331: Expend to 4 hours due to test device shortage
    execution_timeout = 4 * time.hour,
)

ci.thin_tester(
    name = "Android WebView P (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
            apply_configs = [
                "remove_all_system_webviews",
            ],
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = "webview_bot_all_gtests",
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
)

ci.builder(
    name = "Android arm Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "debug_builder",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_junit_tests_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    free_space = builders.free_space.high,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Android arm64 Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "arm64",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "android_lint",
    ),
    # The 'All' version of this builder below provides the same build coverage
    # but cycles much faster due to beefier machine resources. So any regression
    # that this bot would close the tree on would always be caught by the 'All'
    # bot much faster.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# We want to confirm that we can compile everything.
# Android however has some non standard buildchains
# which cause gn analyze to not filter out our compile targets
# when running a try bot.
# This means that our trybots would result in compile times of
# 5+ hours. So instead we have this bot which will compile all on CI.
# It should match "Android arm64 Builder (dbg)"
# History: crbug.com/1246468
ci.builder(
    name = "Android arm64 Builder All Targets (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "arm64",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    builderless = False,
    cores = None,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# This builder should be used for trybot mirroring when no need to compile all.
# See the builder "Android x64 Builder All Targets (dbg)" for more details.
# Don't enable this for branches unless the triggered testers are enabled for
# branches, "Android x64 Builder All Targets (dbg)" is already building all.
ci.builder(
    name = "Android x64 Builder (dbg)",
    builder_spec = builder_config.copy_from("ci/Android x64 Builder All Targets (dbg)"),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(),
    builderless = False,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

# Similar to crbug.com/1246468#c34, as android has some non standard
# buildchains, `mb analyze` will not be able to filter out compile targets when
# running a trybot and thus tries to compile everythings. If the builder
# specifies `all` target, the compile time can take 5+ hours.
# So this builder matches "Android x64 Builder (dbg)", but compiles everything.
ci.builder(
    name = "Android x64 Builder All Targets (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "64-all",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "Android x86 Builder (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_static_builder",
            "remoteexec",
            "x86",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    ssd = True,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
)

ci.builder(
    name = "android-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.thin_tester(
    name = "android-webview-10-x86-rel-tests",
    triggered_by = ["ci/android-x86-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "webview_trichrome_10_cts_tests_gtest",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "10-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|x86",
        short_name = "10",
    ),
    contact_team_email = "woa-engprod@google.com",
)

ci.builder(
    name = "Cast Android (dbg)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "cast_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "cast_android",
            "cast_receiver",
            "clang",
            "debug_static_builder",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_junit_tests",
        ],
        additional_compile_targets = [
            "cast_junit_test_lists",
            "cast_browser_apk",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "cst",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
)

ci.builder(
    name = "android-cast-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "cast_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_debug",
            "cast_java_debug",
            "android_builder",
            "clang",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True as configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        short_name = "and32dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "cast_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_release",
            "cast_java_release",
            "android_builder",
            "clang",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True as configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        short_name = "and32rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "cast_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_debug",
            "cast_java_debug",
            "android_builder",
            "clang",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm64_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True as configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        short_name = "and64dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "android-cast-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Android and Cast Receiver build and tests on ARM64 Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "cast_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_android",
            "cast_release",
            "cast_java_release",
            "android_builder",
            "clang",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cast_receiver_junit_tests",
            "chromium_android_cast_receiver",
            "chromium_android_cast_receiver_arm64_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True as configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        short_name = "and64rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "Deterministic Android",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm",
        ],
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
    notifies = ["Deterministic Android"],
)

ci.builder(
    name = "Deterministic Android (dbg)",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_builder",
            "remoteexec",
            "arm",
        ],
    ),
    cores = 16,
    ssd = True,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "dbg",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Android"],
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.thin_tester(
    name = "Oreo Phone Tester",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = "android_oreo_gtests",
        mixins = [
            "has_native_resultdb_integration",
            "oreo_mr1_fleet",
            "walleye",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                # TODO(crbug.com/41414027): Re-enable this once the test
                # are either passing or there is more capacity.
                experiment_percentage = 0,
            ),
            "webview_instrumentation_test_apk": targets.mixin(
                # TODO(crbug.com/40641956): Enable this once it's passing.
                # TODO(crbug.com/41414027): Re-enable this once the tests
                # are either passing or there is more capacity.
                experiment_percentage = 0,
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "android-10-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_wpr_tests",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_10_rel_gtests",
        ],
        additional_compile_targets = [
            "check_chrome_static_initializers",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "device_os": "QQ1A.191205.008",
                        "device_os_flavor": "google",
                    },
                ),
            ),
            "has_native_resultdb_integration",
            "walleye",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "10",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-x64-dbg-tests",
    description_html = "Run Chromium tests on Android 12l tablet-flavor emulator.",
    triggered_by = ["ci/Android x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "android_12l_emulator_gtests",
        ],
        mixins = [
            "12l-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    ("--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*:" +
                     # https://crbug.com/1468262
                     "All/PaymentHandlerEnforceFullDelegationTest.WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject/1"),
                ],
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.remove(
                reason = "TODO(crbug.com/40263601): Temporarily remove it from android-12L builder due to infra flakiness.",
            ),
            # "content_browsertests": targets.mixin(
            #     # TODO(crbug.com/40265619): Remove experiment and ci_only
            #     # once the test suite is stable.
            #     ci_only = True,
            #     experiment_percentage 100,
            #     args = [
            #         '--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.content_browsertests.filter',
            #     ],
            #     swarming = targets.swarming(
            #         shards = 40,
            #     ),
            # ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l.content_shell_test_apk.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
        short_name = "12L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-landscape-x64-dbg-tests",
    description_html = "Run Chromium tests on Android 12l tablet-flavor " +
                       "emulator in Landscape Mode.",
    triggered_by = ["ci/Android x64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "android_12l_landscape_emulator_gtests",
        ],
        mixins = [
            "12l-landscape-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12l_landscape.chrome_public_test_apk.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
        short_name = "12L-L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-x64-rel-cq",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run CQ-specific Chromium test suites on Android 12l " +
                       "tablet-flavor emulator.",
    builder_spec = builder_config.copy_from("ci/android-13-x64-rel"),
    gn_args = "ci/android-13-x64-rel",
    targets = targets.bundle(
        targets = "android_12l_rel_cq_gtests",
        mixins = [
            "12l-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "12L",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-arm64-proguard-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_google",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_marshmallow_gtests",
            "chromium_junit_tests_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "chrome_public_test_apk": targets.mixin(
                swarming = targets.swarming(
                    shards = 25,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.arm64_proguard_rel.crashpad_tests.filter",
                ],
            ),
            "gin_unittests": targets.mixin(
                args = [
                    # https://crbug.com/1404782
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.pie_arm64.gin_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.arm64_proguard_rel.gl_tests.filter",  # https://crbug.com/1034007
                ],
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/931138): Fix permission issue when creating tmp files",
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "M proguard",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 8 * time.hour,
)

ci.builder(
    name = "android-bfcache-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_monochrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "bfcache_android_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "oreo-x86-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "bf_cache_content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-binary-size-generator",
    executable = "recipe:binary_size_generator_tot",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "chrome_with_codecs",
            "remoteexec",
            "minimal_symbols",
            "official_optimize",
            "stable_channel",
            "v8_release_branch",
            # Allows the bot to measure low-end arm32 and high-end arm64 using
            # a single build.
            "android_low_end_secondary_toolchain",
            # Disable PGO due to too much volatility: https://crbug.com/344608183
            "pgo_phase_0",
        ],
    ),
    builderless = False,
    cores = 32,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|other",
        short_name = "size",
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "arm_no_neon",
            "release_java",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm_no_neon",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-asan-arm-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm_no_neon",
            "clang",
            "android_asan",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        additional_compile_targets = [
            "cronet_package",
            "cronet_perf_test_apk",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "bullhead",
            "marshmallow",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
ci.builder(
    name = "android-cronet-mainline-clang-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "arm64",
            "cronet_android_mainline_clang",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|arm64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang (coverage-enabled)
ci.builder(
    name = "android-cronet-mainline-clang-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "cronet_android_mainline_clang",
            "use_clang_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang_coverage|arm64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
ci.builder(
    name = "android-cronet-mainline-clang-riscv64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "riscv64",
            "cronet_android_mainline_clang",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
ci.builder(
    name = "android-cronet-mainline-clang-riscv64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "riscv64",
            "strip_debug_info",
            "cronet_android_mainline_clang",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
ci.builder(
    name = "android-cronet-mainline-clang-x86-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x86",
            "cronet_android_mainline_clang",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|x86",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang (coverage-enabled)
ci.builder(
    name = "android-cronet-mainline-clang-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "cronet_android_mainline_clang",
            "use_clang_coverage",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang_coverage|x86",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

# Runs on a specific machine with an attached phone
ci.builder(
    name = "android-cronet-marshmallow-arm64-perf-rel",
    executable = "recipe:cronet",
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
        ],
    ),
    cores = None,
    os = os.ANDROID,
    cpu = None,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test|perf",
        short_name = "m",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-riscv64-dbg",
    description_html = "Verifies building Cronet against RISC-V64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "riscv64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-riscv64-rel",
    description_html = "Verifies building Cronet against RISC-V64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "riscv64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x86-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x86",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_perf_test_apk",
            "cronet_smoketests_apk",
        ],
        per_test_modifications = {
            # Do not run cronet_sizes on CQ builders (binaries are instrumented
            # for code coverage)
            "cronet_sizes": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_dbg_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_perf_test_apk",
            "cronet_smoketests_apk",
        ],
        per_test_modifications = {
            # Do not run cronet_sizes on CQ builders (binaries are instrumented
            # for code coverage)
            "cronet_sizes": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-12-tests",
    triggered_by = ["ci/android-cronet-x64-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "12",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-13-tests",
    triggered_by = ["ci/android-cronet-x64-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "13",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-14-tests",
    description_html = "Tests Cronet against Android 14",
    triggered_by = ["ci/android-cronet-x64-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "14-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "14",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x64-dbg-15-tests",
    description_html = "Tests Cronet against Android 15",
    triggered_by = ["ci/android-cronet-x64-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "15",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-marshmallow-tests",
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "marshmallow-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "m",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-nougat-tests",
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "nougat-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "n",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-oreo-tests",
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "oreo-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "o",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-pie-tests",
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "pie-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                # crbug.com/1046060
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "p",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "10-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "net_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_10.net_unittests.filter",
                ],
            ),
        },
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "10",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-11-tests",
    triggered_by = ["ci/android-cronet-x86-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = [
            "cronet_gtests",
        ],
        mixins = [
            "11-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "11",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
            "cronet_smoketests_apk",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-x64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "cronet_builder",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x64_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "cronet_android",
            "official_optimize",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "cronet_common_compile_targets",
            "cronet_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "cronet_package_ci",
            "cronet_smoketests_apk",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    notifies = ["cronet"],
)

ci.builder(
    name = "android-oreo-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "enable_wpr_tests",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_monochrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            targets.bundle(
                targets = [
                    "android_oreo_emulator_gtests",
                    "oreo_isolated_scripts",
                ],
                mixins = targets.mixin(
                    args = [
                        "--use-persistent-shell",
                    ],
                ),
            ),
            "chromium_android_scripts",
        ],
        additional_compile_targets = [
            "chrome_nocompile_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "oreo-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            # If you change this, make similar changes in android-x86-code-coverage
            "android_browsertests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "android_sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            # TODO: crbug.com/40258588 - This flag should have no effect on this
            # script test, but it needs to be present to satisfy the
            # targets-config-verifier, so remove it after landing
            "check_network_annotations": targets.mixin(
                args = [
                    "--avd-config=../../tools/android/avd/proto/generic_android26.textpb",
                ],
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o.chrome_public_test_apk.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o.chrome_public_unit_test_apk.filter",
                ],
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_browsertests": targets.mixin(
                args = [
                    "--gtest-also-run-pre-tests",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 75,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_shell_crash_test": targets.remove(
                reason = "crbug.com/1084353",
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "content_shell_test_apk": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 6,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o_p.gl_tests.filter",
                ],
            ),
            "leveldb_unittests": targets.remove(
                reason = "TODO(crbug.com/40264100): Runs too slowly in this configuration",
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "services_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o.services_unittests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "telemetry_chromium_minidump_unittests": targets.mixin(
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "telemetry_monochrome_minidump_unittests": targets.mixin(
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "telemetry_perf_unittests_android_chrome": targets.mixin(
                # For whatever reason, automatic browser selection on this bot chooses
                # webview instead of the full browser, so explicitly specify it here.
                args = [
                    "--browser=android-chromium",
                ],
                ci_only = True,
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
            # If you change this, make similar changes in android-x86-code-coverage
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                # This builder is mirrored by CQ builder. Only multiple process
                # tests run in CQ.
                ci_only = True,
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    # TODO(crbug.com/355704916): Revert back to builderless after compile OOM
    # issue is resolved.
    builderless = not settings.is_main,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x86",
        short_name = "O",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
)

ci.thin_tester(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    targets = targets.bundle(
        targets = "android_pie_gtests",
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-pie-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_monochrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            targets.bundle(
                targets = [
                    "android_pie_rel_gtests",
                    # TODO(crbug.com/40142574): Re-enable this if/when
                    # additional capacity has been deployed.
                    # "marshmallow_nougat_pie_isolated_scripts_with_proguard",
                ],
                mixins = targets.mixin(
                    args = [
                        "--use-persistent-shell",
                    ],
                ),
            ),
            "chromium_android_scripts",
        ],
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1034001
                    "--gtest_filter=-ImportantSitesUtilBrowserTest.DSENotConsideredImportantInRegularMode",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    dimensions = {
                        "device_type": "crosshatch",
                    },
                    # TODO(crbug.com/40917979): Temporarily increase the shard
                    # by 1 for the surging number of tests. Recover to original
                    # number of shards(4) after the bug is fixed.
                    shards = 5,
                ),
            ),
            "blink_platform_unittests": targets.mixin(
                ci_only = True,
                # TODO(crbug.com/40142574): Move this back to walleye if/when
                # capacity has been deployed.
                swarming = targets.swarming(
                    dimensions = {
                        "device_type": "crosshatch",
                    },
                ),
            ),
            "cc_unittests": targets.mixin(
                ci_only = True,
            ),
            "chrome_public_smoke_test": targets.mixin(
                ci_only = True,
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    # https://crbug.com/1010211
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.pie_arm64_rel.chrome_public_test_apk.filter",
                    "--use-persistent-shell",
                ],
                # https://crbug.com/1233700
                ci_only = True,
            ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "chrome_public_test_vr_apk": targets.remove(
            #     reason = "https://crbug.com/1010211",
            # ),
            "chrome_public_unit_test_apk": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "device_type": "crosshatch",
                    },
                    # TODO(crbug.com/40922014): Temporarily increase the shard
                    # to 5 for the network issue.
                    shards = 5,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--gtest-also-run-pre-tests",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.pie_arm64.content_browsertests_coverage.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    # TODO(crbug.com/40922014): Temporarily increase the shard
                    # by 5 for the surging number of tests. Recover to default
                    # number of shards(20) after the bug is fixed.
                    shards = 25,
                ),
            ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "gl_tests_validating": targets.mixin(
            #     args = [
            #         # https://crbug.com/1034007
            #         "--test-launcher-filter-file=../../testing/buildbot/filters/android.pie_arm64_rel.gl_tests.filter",
            #     ],
            # ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "media_unittests": targets.mixin(
            #     args = [
            #         # https://crbug.com/1034009
            #         "--gtest_filter=-AAudio/AudioOutputTest.Play200HzTone/0",
            #     ],
            # ),
            "monochrome_public_bundle_smoke_test": targets.mixin(
                ci_only = True,
            ),
            "monochrome_public_smoke_test": targets.mixin(
                ci_only = True,
            ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "perfetto_unittests": targets.remove(
            #     reason = "TODO(crbug.com/41440830) Fix permission issue when creating tmp files",
            # ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "telemetry_perf_unittests": targets.remove(
            #     reason = "existed at migration, originally removed in https://crrev.com/c/1968238",
            # ),
            # TODO(crbug.com/40142574) Remove or restore as appropriate if
            # marshmallow_nougat_pie_isolated_scripts_with_proguard is reenabled
            # "unit_tests": targets.remove(
            #     reason = "https://crbug.com/1010211",
            # ),
            "viz_unittests": targets.mixin(
                ci_only = True,
            ),
            "webview_64_cts_tests": targets.mixin(
                # TODO(crbug.com/40142574): Move this back to walleye if/when
                # additional capacity has been deployed.
                swarming = targets.swarming(
                    dimensions = {
                        "device_type": "crosshatch",
                    },
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                # crbug/1368281
                ci_only = True,
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
                # This builder is mirrored by CQ builder. Only multiple process
                # tests run in CQ.
                ci_only = True,
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
)

ci.builder(
    name = "android-pie-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_monochrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_pie_rel_emulator_gtests",
            "chromium_android_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "pie-x86-emulator",
            "emulator-4-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1034001
                    "--gtest_filter=-ImportantSitesUtilBrowserTest.DSENotConsideredImportantInRegularMode",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        # crbug.com/1292221
                        "cores": "8",
                    },
                ),
            ),
            "cc_unittests": targets.mixin(
                args = [
                    # https://crbug.com/1039860
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.cc_unittests.filter",
                ],
            ),
            # TODO: crbug.com/40258588 - This flag should have no effect on this
            # script test, but it needs to be present to satisfy the
            # targets-config-verifier, so remove it after landing
            "check_network_annotations": targets.mixin(
                args = [
                    "--avd-config=../../tools/android/avd/proto/android_28_google_apis_x86.textpb",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    # https://crbug.com/1046059
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_p.chrome_public_test_apk.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        "cores": "8",
                    },
                    # See https://crbug.com/1230192, runs of 40-60 minutes at 20 shards.
                    shards = 30,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_p.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    # Flaking with only 20 shards with 40-60 minute runs.
                    # https://crbug.com/1230118
                    shards = 30,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--gtest_filter=-org.chromium.content.browser.input.ImeInputModeTest.testShowAndHideInputMode*",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_o_p.gl_tests.filter",
                ],
            ),
            "net_unittests": targets.mixin(
                # crbug.com/1046060
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.net_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/41440830): Fix permission issue when creating tmp files",
            ),
            "services_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40203477): Fix the failed tests
                    "--gtest_filter=-PacLibraryTest.ActualPacMyIpAddress*",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    # crbug.com/1294924
                    shards = 8,
                ),
            ),
            "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    # crbug.com/1294924
                    shards = 5,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "P",
    ),
    contact_team_email = "clank-engprod@google.com",
)

# TODO(crbug.com/40152686): Update the console view config once on CQ
ci.builder(
    name = "android-11-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x86",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_11_emulator_gtests",
        ],
        mixins = [
            "11-x86-emulator",
            "emulator-4-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
            ),
            "cc_unittests": targets.mixin(
                # https://crbug.com/1039860
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11_12.cc_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.chrome_public_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
                # TODO(crbug.com/40210655) Remove experiment and enable on
                # trybot once the device offline issue is resolved.
                ci_only = True,
                experiment_percentage = 100,
                swarming = targets.swarming(
                    dimensions = {
                        "cores": "8",
                    },
                ),
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "components_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40174843): Fix the test failure
                    "--gtest_filter=-FieldFormatterTest.DifferentLocales",
                ],
            ),
            "content_browsertests": targets.mixin(
                # TODO(crbug.com/40152686): Remove ci_only and experiment_percentage
                # after the test suite is green.
                ci_only = True,
                experiment_percentage = 100,
                # TODO(crbug.com/40152686): Revisit the shard number once failed tests
                # are fixed.
                swarming = targets.swarming(
                    dimensions = {
                        # use 8-core to shorten runtime
                        "cores": "8",
                    },
                    shards = 30,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.content_shell_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/41440830): Fix permission issue when creating tmp files",
            ),
            "services_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40203477): Fix the failed tests
                    "--gtest_filter=-PacLibraryTest.ActualPacMyIpAddress*",
                ],
            ),
            "viz_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11.viz_unittests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    # TODO(crbug.com/40173842) Enable this test once the issue is fixed.
                    "--gtest_filter=-org.chromium.net.NetworkChangeNotifierTest.testNetworkChangeNotifierJavaObservers",
                ],
            ),
            "webview_ui_test_app_test_apk": targets.remove(
                reason = "crbug.com/1165280",
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    # TODO(crbug.com/40152686): Add it back to gardening once the builder is
    # stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "11",
    ),
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-12-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 12 emulator.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_12_emulator_gtests",
            "private_code_failure_test",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--use-persistent-shell",
                ],
            ),
            "12-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
            ),
            "android_browsertests_no_fieldtrial": targets.mixin(
                args = [
                    # https://crbug.com/1289764
                    "--gtest_filter=-All/ChromeBrowsingDataLifetimeManagerScheduledRemovalTest.History/*",
                ],
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "cc_unittests": targets.mixin(
                # https://crbug.com/1039860
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_11_12.cc_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.chrome_public_test_apk.filter",
                    "--timeout-scale=2.0",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.chrome_public_unit_test_apk.filter",
                ],
                ci_only = True,
            ),
            "components_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40174843): Fix the test failure
                    "--gtest_filter=-FieldFormatterTest.DifferentLocales",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.content_browsertests.filter",
                ],
                # TODO(crbug.com/40188616): Remove experiment and ci_only
                # once the test suite is stable.
                ci_only = True,
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gl_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12.gl_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40203477): Fix the failed tests
                    "--gtest_filter=-PacLibraryTest.ActualPacMyIpAddress*",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "12",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-13-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 13 emulators.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_13_emulator_gtests",
            "android_rel_isolated_scripts",
        ],
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/1414886
                    "--gtest_filter=-OfferNotificationControllerAndroidBrowserTestForMessagesUi.MessageShown",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "android_sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.chrome_public_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.content_browsertests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_13.content_shell_test_apk.filter",
                ],
                ci_only = True,
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
                # TODO(crbug.com/337935399): Remove experiment after the bug is fixed.
                experiment_percentage = 100,
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "services_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "13",
    ),
    contact_team_email = "clank-engprod@google.com",
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
)

ci.builder(
    name = "android-14-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run chromium tests on Android 14 devices.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_device_gtests",
            "chromium_android_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "panther_on_14",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "cc_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device.cc_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14.chrome_public_test_apk.filter",
                ],
                ci_only = True,
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--gtest-also-run-pre-tests",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_14.content_browsertests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.14.webview_instrumentation_test_apk.filter",
                ],
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "14",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-14-x64-rel",
    description_html = "Run chromium tests on Android 14 emulators.",
    # TODO(crbug.com/40286106): Enable on branches once stable
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_14_emulator_gtests",
        ],
        mixins = [
            "14-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/361042311
                    "--gtest_filter=-All/SharedStorageChromeBrowserTest.CrossOriginWorklet_SelectURL_Success/*",
                ],
            ),
            "android_sync_integration_tests": targets.mixin(
                args = [
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
                # https://crbug.com/345579530
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.chrome_public_test_apk.filter",
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.chrome_public_unit_test_apk.filter",
                ],
            ),
            "components_browsertests": targets.mixin(
                args = [
                    # TODO(crbug.com/40746860): Fix the test failure
                    "--gtest_filter=-V8ContextTrackerTest.AboutBlank",
                ],
            ),
            "components_unittests": targets.mixin(
                args = [
                    # crbug.com/361638641
                    "--gtest_filter=-BrowsingTopicsStateTest.EpochsForSite_FourEpochs_SwitchTimeArrived",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.content_shell_test_apk.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.unit_tests.filter",
                ],
            ),
            "webkit_unit_tests": targets.mixin(
                args = [
                    # https://crbug.com/352586409
                    "--gtest_filter=-All/HTMLPreloadScannerLCPPLazyLoadImageTest.TokenStreamMatcherWithLoadingLazy/*",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.14.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    # TODO(crbug.com/40286106): Enable gardening once tests are stable
    gardener_rotations = args.ignore_default(None),
    # TODO(crbug.com/40286106): Enable tree_closing once compile are stable
    #tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-15-x64-rel",
    description_html = "Run chromium tests on Android 15 emulators.",
    # TODO(crbug.com/40286106): Enable on branches once stable
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_15_emulator_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--emulator-debug-tags=all,-qemud,-sensors",
                ],
            ),
            "15-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    # https://crbug.com/361042311
                    "--gtest_filter=-All/SharedStorageChromeBrowserTest.CrossOriginWorklet_SelectURL_Success/*",
                ],
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14.base_unittests.filter",
                ],
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.chrome_public_unit_test_apk.filter",
                ],
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_shell_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.content_shell_test_apk.filter",
                ],
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.content_unittests.filter",
                ],
            ),
            "crashpad_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.crashpad_tests.filter",
                ],
            ),
            "device_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.device_unittests.filter",
                ],
            ),
            "gl_tests_validating": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_12_12l_13.gl_tests.filter",
                ],
            ),
            "gwp_asan_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.gwp_asan_unittests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator.media_unittests.filter",
                ],
            ),
            "net_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/362292404): Fix the failed test
                    "--gtest_filter=-TrafficStatsAndroidTest.*",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40201873): Fix the failed test
                    "--gtest_filter=-ScopedDirTest.CloseOutOfScope",
                ],
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_14_15.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.emulator_15.webview_instrumentation_test_apk.filter",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "15",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-webview-13-x64-hostside-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = (
        "Runs WebView host-driven CTS on Android 13 emulator."
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = "webview_trichrome_64_cts_hostside_gtests",
        mixins = [
            "13-x64-emulator",
            "emulator-8-cores",
            "has_native_resultdb_integration",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "webview_trichrome_64_cts_hostside_tests full_mode": targets.mixin(
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
        },
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "13-hs",
    ),
    contact_team_email = "woa-engprod@google.com",
)
