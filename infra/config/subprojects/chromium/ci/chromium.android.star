# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "gardener_rotations", "os", "siso")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    gardener_rotations = gardener_rotations.ANDROID,
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
        ],
    ),
    builderless = False,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    contact_team_email = "clank-engprod@google.com",
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    # TODO(crbug.com/40882299): Check why the compile takes longer time.
    execution_timeout = 8 * time.hour,
    # TODO(crbug.com/40282985): Restore tree-closing and gardener rotation if/when
    # bot is fixed.
    # tree_closing = True,
    gardener_rotations = args.ignore_default(None),
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
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
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
            "has_native_resultdb_integration",
            "pie_fleet",
            "walleye",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
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
        ],
    ),
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "cst",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
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
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "10",
    ),
    contact_team_email = "clank-engprod@google.com",
)

ci.builder(
    name = "android-12l-x64-dbg-tests",
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
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
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
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "M proguard",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 8 * time.hour,
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|arm64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang_coverage|arm64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
# TODO: crbug.com/41484811 - remove "triggered_by" and "schedule".
ci.builder(
    name = "android-cronet-mainline-clang-riscv64-dbg",
    schedule = "triggered",
    triggered_by = [],
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
# TODO: crbug.com/41484811 - remove "triggered_by" and "schedule".
ci.builder(
    name = "android-cronet-mainline-clang-riscv64-rel",
    schedule = "triggered",
    triggered_by = [],
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|x86",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang_coverage|x86",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test|perf",
        short_name = "m",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
    notifies = ["cronet"],
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-riscv64-dbg",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    gardener_rotations = args.ignore_default(None),
    notifies = ["cronet"],
)

ci.builder(
    name = "android-cronet-riscv64-rel",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-sheriff@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "12",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "13",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "14",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
    notifies = ["cronet"],
)

ci.thin_tester(
    name = "android-cronet-x86-dbg-lollipop-tests",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "l",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "m",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "n",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "o",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "p",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "10",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "11",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-team@google.com",
    gardener_rotations = args.ignore_default(None),
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x86",
        short_name = "O",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
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
            "has_native_resultdb_integration",
            "pie_fleet",
            "walleye",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "11",
    ),
    execution_timeout = 4 * time.hour,
    # TODO(crbug.com/40152686): Add it back to gardening once the builder is
    # stable.
    gardener_rotations = args.ignore_default(None),
)

ci.builder(
    name = "android-12-x64-rel",
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "13",
    ),
    contact_team_email = "clank-engprod@google.com",
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
    # TODO(crbug.com/40286106): Enable tree_closing once compile are stable
    #tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "14",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    # TODO(crbug.com/40286106): Enable gardening once tests are stable
    gardener_rotations = args.ignore_default(None),
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
    # TODO(crbug.com/40286106): Enable tree_closing once compile are stable
    #tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "15",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    # TODO(crbug.com/40286106): Enable gardening once tests are stable
    gardener_rotations = args.ignore_default(None),
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "13-hs",
    ),
    contact_team_email = "woa-engprod@google.com",
)
