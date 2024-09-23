# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.desktop builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "os", "siso")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android.desktop",
    pool = ci.DEFAULT_POOL,
    builderless = False,
    os = os.LINUX_DEFAULT,
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
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
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-desktop-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
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
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_mb",
        ),
        build_gs_bucket = "chromium-android-desktop-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
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
            config = "android",
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-desktop-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
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
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-desktop-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "builder|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "android-desktop-x64-rel-14-tests",
    branch_selector = branches.selector.MAIN,
    description_html = "Android desktop x64 release tests on Android 14.",
    triggered_by = ["ci/android-desktop-x64-compile-rel"],
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-desktop-archive",
    ),
    targets = targets.bundle(
        targets = [
            "android_desktop_junit_tests",
            targets.bundle(
                targets = "android_desktop_tests",
                mixins = [
                    "14-desktop-x64-emulator",
                    "emulator-8-cores",
                ],
            ),
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    builderless = True,
    cores = 8,
    console_view_entry = consoles.console_view_entry(
        category = "tester|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
)
