# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.rust builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.rust",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    notifies = ["chrome-rust-experiments"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.rust",
)

def rust_fyi_configs(*args):
    # Enables off-by-default GN configs to build extra experimental Rust
    # components.
    return list(args) + [
        "enable_rust_mojo",
        "enable_rust_mojom_bindings",
        "enable_rust_png",
    ]

ci.builder(
    name = "android-rust-arm32-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["android"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "android_builder",
            "arm",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_common_gtests",
            # Currently `can_build_rust_unit_tests` is false on Android
            # (because we need to construct an APK instead of compile an exe).
            # TODO(crbug.com/40201737): Cover `rust_native_tests` here.
        ],
        additional_compile_targets = [
            "mojo_rust",
            "rust_build_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android 32bit",
        short_name = "rel",
    ),
)

ci.builder(
    name = "android-rust-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["android"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "debug_builder",
            "remoteexec",
            "android_builder",
            "arm64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_common_gtests",
            # Currently `can_build_rust_unit_tests` is false on Android
            # (because we need to construct an APK instead of compile an exe).
            # TODO(crbug.com/40201737): Cover `rust_native_tests` here.
        ],
        additional_compile_targets = [
            "mojo_rust",
            "rust_build_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android 64bit",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "android-rust-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["android"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "android_builder",
            "arm64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_common_gtests",
            # Currently `can_build_rust_unit_tests` is false on Android
            # (because we need to construct an APK instead of compile an exe).
            # TODO(crbug.com/40201737): Cover `rust_native_tests` here.
        ],
        additional_compile_targets = [
            "mojo_rust",
            "rust_build_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android 64bit",
        short_name = "rel",
    ),
)

ci.builder(
    name = "linux-rust-x64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_host_gtests",
            "rust_native_tests",
        ],
        additional_compile_targets = [
            "mojo_rust",
            "mojo_rust_integration_unittests",
            "mojo_rust_unittests",
            "rust_build_tests",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "linux-rust-x64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_host_gtests",
            "rust_native_tests",
        ],
        additional_compile_targets = [
            "mojo_rust",
            "mojo_rust_integration_unittests",
            "mojo_rust_unittests",
            "rust_build_tests",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "rel",
    ),
)

ci.builder(
    name = "mac-rust-x64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "debug_builder",
            "remoteexec",
            "mac",
            "x64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_host_gtests",
            "rust_native_tests",
        ],
        additional_compile_targets = [
            "mojo_rust",
            "mojo_rust_integration_unittests",
            "mojo_rust_unittests",
            "rust_build_tests",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "Mac x64",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "win-rust-x64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "debug_builder",
            "remoteexec",
            "win",
            "x64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_host_gtests",
            "rust_native_tests",
        ],
        additional_compile_targets = [
            "mojo_rust",
            "mojo_rust_integration_unittests",
            "mojo_rust_unittests",
            "rust_build_tests",
        ],
        mixins = [
            "win10-any",
            "x86-64",
        ],
    ),
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "Windows x64",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "win-rust-x64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = rust_fyi_configs(
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "win",
            "x64",
        ),
    ),
    targets = targets.bundle(
        targets = [
            "rust_host_gtests",
            "rust_native_tests",
        ],
        additional_compile_targets = [
            "mojo_rust",
            "mojo_rust_integration_unittests",
            "mojo_rust_unittests",
            "rust_build_tests",
        ],
        mixins = [
            "win10-any",
            "x86-64",
        ],
    ),
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "Windows x64",
        short_name = "rel",
    ),
)
