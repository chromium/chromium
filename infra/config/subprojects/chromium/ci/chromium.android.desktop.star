# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.desktop builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    # TODO(crbug.com/439887309): Enable on ANDROID_BRANCHES
    builder_group = "chromium.android.desktop",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
        # Android emulator tasks often flake during emulator start-up, which
        # leads to the whole shard being marked as invalid.
        retry_invalid_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    builderless = False,
    os = os.LINUX_DEFAULT,
    tree_closing_notifiers = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS,
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

consoles.console_view(
    name = "chromium.android.desktop",
    branch_selector = branches.selector.MAIN,
    ordering = {
        None: ["builder", "tester"],
        "*cpu*": ["arm64", "x64"],
        "builder": "*cpu*",
    },
)

ci.builder(
    name = "android-desktop-arm64-compile-dbg",
    branch_selector = branches.selector.MAIN,
    description_html = "Android desktop ARM64 debug compile builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "remoteexec",
            "arm64",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "android-desktop-arm64-compile-rel",
    description_html = "Android desktop ARM64 release compile builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
            "webview_trichrome",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm64",
        short_name = "rel",
    ),
)

ci.builder(
    name = "android-desktop-x64-compile-dbg",
    description_html = "Android desktop x64 debug compile builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "remoteexec",
            "x64",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "android-desktop-x64-compile-rel",
    description_html = "Android desktop x64 release compile builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    # crbug.com/390061059: Explicitly compile android_lint to have lint coverage
    targets = targets.bundle(
        additional_compile_targets = "android_lint",
    ),
    gardener_rotations = gardener_rotations.ANDROID,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "android-desktop-x64-rel-15-tests",
    description_html = "Android desktop x64 release tests on Android 15.",
    parent = "ci/android-desktop-x64-compile-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "android_desktop_junit_tests",
            targets.bundle(
                targets = "android_desktop_gtests",
                mixins = [
                    "15-desktop-x64-emulator",
                    "emulator-8-cores",
                ],
            ),
            targets.bundle(
                targets = "android_desktop_isolated_script_tests",
                mixins = [
                    "15-desktop-x64-emulator",
                    "emulator-8-cores",
                ],
            ),
        ],
        mixins = [
            "isolate_profile_data",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.desktop.emulator_15.android_browsertests.filter",
                    "--emulator-debug-tags=all",
                ],
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "chrome_public_test_apk": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.desktop.chrome_public_test_apk_desktop.filter",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.desktop.emulator_15.chrome_public_test_apk.filter",
                ],
            ),
            "chrome_public_test_apk_desktop": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.desktop.chrome_public_test_apk_desktop.filter",
                ],
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "chrome_public_unit_test_apk": targets.mixin(
                args = [
                    # https://crbug.com/392649074
                    "--gtest_filter=-org.chromium.chrome.browser.ui.appmenu.AppMenuTest.testShowAppMenu_AnchorTop",
                ],
                ci_only = True,
            ),
            "android_chrome_wpt_tests": targets.mixin(
                ci_only = True,
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.desktop.emulator_15.unit_tests.filter",
                ],
            ),
            "media_unittests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    cores = 8,
    gardener_rotations = gardener_rotations.ANDROID,
    console_view_entry = consoles.console_view_entry(
        category = "tester|x64",
        short_name = "15-rel",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "android-desktop-arm64-deterministic-rel",
    description_html = "Deterministic arm64 release build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm64",
        ],
    ),
    builderless = False,
    cores = 32,
    # TODO(crbug.com/420639761): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "arm64-rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "android-desktop-arm64-deterministic-dbg",
    description_html = "Deterministic arm64 dbg build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "debug_builder",
            "remoteexec",
            "arm64",
        ],
    ),
    builderless = False,
    cores = 32,
    # TODO(crbug.com/420639761): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "arm64-dbg",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "android-desktop-x64-deterministic-rel",
    description_html = "Deterministic x64 release build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "x64",
        ],
    ),
    builderless = False,
    cores = 32,
    # TODO(crbug.com/420639761): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "x64-rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "android-desktop-x64-deterministic-dbg",
    description_html = "Deterministic x64 dbg build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "android_with_static_analysis",
            "debug_builder",
            "remoteexec",
            "x64",
        ],
    ),
    builderless = False,
    cores = 32,
    # TODO(crbug.com/420639761): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "x64-dbg",
    ),
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)
