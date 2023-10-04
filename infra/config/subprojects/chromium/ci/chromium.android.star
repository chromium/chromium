# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "os", "reclient", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.ANDROID,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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
    builderless = False,
    cores = None,
    # TODO(crbug.com/1486663): Restore tree-closing when bot is fixed.
    # tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    contact_team_email = "clank-engprod@google.com",
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    # TODO(crbug.com/1395760): Check why the compile takes longer time.
    execution_timeout = 8 * time.hour,
)

ci.thin_tester(
    name = "Android WebView N (dbg)",
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
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "N",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
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
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
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
    free_space = builders.free_space.high,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "woa-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
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
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.thin_tester(
    name = "Nougat Phone Tester",
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
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "N",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "clank-engprod@google.com",
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
    sheriff_rotations = args.ignore_default(None),
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
    sheriff_rotations = args.ignore_default(None),
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
    builderless = False,
    cores = 32,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|other",
        short_name = "size",
    ),
    contact_team_email = "clank-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|arm64",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|arm64",
        short_name = "rel",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|x86",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
)

# Compiles with Android Mainline Clang
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|mainline_clang|x86",
        short_name = "rel",
    ),
    notifies = ["cronet"],
)

# Runs on a specific machine with an attached phone
ci.builder(
    name = "android-cronet-marshmallow-arm64-perf-rel",
    executable = "recipe:cronet",
    cores = None,
    os = os.ANDROID,
    cpu = None,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test|perf",
        short_name = "m",
    ),
    contact_team_email = "clank-engprod@google.com",
    notifies = ["cronet"],
    reclient_jobs = reclient.jobs.DEFAULT,
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "dbg",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "s",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "t",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "l",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "m",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "n",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "o",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "p",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "10",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "11",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
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
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x64",
        short_name = "rel",
    ),
    notifies = ["cronet"],
)

ci.builder(
    name = "android-nougat-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android", "enable_wpr_tests"],
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x86",
        short_name = "N",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

# TODO(crbug.com/1459433): Enable the branch once the builder is ready.
ci.builder(
    name = "android-oreo-x86-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android", "enable_wpr_tests"],
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
    tree_closing = True,
    # TODO(crbug.com/1459433): Update the console view config once on CQ
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
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
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "P",
    ),
    contact_team_email = "clank-engprod@google.com",
)

# TODO(crbug.com/1137474): Update the console view config once on CQ
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
    # TODO(crbug.com/1137474): Add it back to sheriff once the builder is more
    # stable.
    sheriff_rotations = args.ignore_default(None),
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
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x64",
        short_name = "12",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-13-x64-rel",
    # TODO(crbug.com/1405331): Enable on branches once stable
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
    # TODO(crbug.com/1405331): Enable sheriff once tests are stable
    sheriff_rotations = args.ignore_default(None),
    # TODO(crbug.com/1405331): Enable tree_closing once compile are stable
    #tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "13",
    ),
    execution_timeout = 4 * time.hour,
)
