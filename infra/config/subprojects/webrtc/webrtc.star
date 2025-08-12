# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "builder", "cpu", "defaults", "os")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/siso.star", "siso")

luci.bucket(
    name = "webrtc",
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.webrtc"],
        service_accounts = [
            "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
            "webrtc-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "project-webrtc-led-users",
        ),
        luci.binding(
            roles = "role/buildbucket.triggerer",
            groups = [
                "project-chromium-ci-schedulers",
                "project-webrtc-admins",
            ],
        ),
        luci.binding(
            roles = "role/buildbucket.owner",
            groups = "project-chromium-admins",
        ),
        luci.binding(
            roles = "role/scheduler.owner",
            groups = "project-webrtc-admins",
        ),
        luci.binding(
            roles = "role/swarming.poolUser",
            groups = "project-webrtc-admins",
        ),
        luci.binding(
            roles = "role/swarming.taskTriggerer",
            groups = "project-webrtc-admins",
        ),
    ],
)

defaults.set(
    bucket = "webrtc",
    executable = "recipe:chromium",
    triggered_by = ["chromium-gitiles-trigger"],
    builder_group = "chromium.webrtc",
    builderless = None,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    execution_timeout = 2 * time.hour,
    properties = {
        "perf_dashboard_machine_group": "ChromiumWebRTC",
    },
    service_account = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

# Builders are defined in lexicographic order by name

builder(
    name = "WebRTC Chromium Android Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "base_config",
            apply_configs = [
                "dcheck",
                "mb",
                "android",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
        build_gs_bucket = "chromium-webrtc",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_static_builder",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(),
)

builder(
    name = "WebRTC Chromium Android Tester",
    parent = "WebRTC Chromium Android Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "base_config",
            apply_configs = [
                "dcheck",
                "mb",
                "android",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
        build_gs_bucket = "chromium-webrtc",
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_simple_gtests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
)

builder(
    name = "WebRTC Chromium Linux Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["webrtc_test_resources"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "remoting_unittests",
        ],
    ),
)

builder(
    name = "WebRTC Chromium Linux Tester",
    parent = "WebRTC Chromium Linux Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
)

builder(
    name = "WebRTC Chromium Mac Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["webrtc_test_resources"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "remoting_unittests",
        ],
    ),
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium Mac Tester",
    parent = "WebRTC Chromium Mac Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
)

builder(
    name = "WebRTC Chromium Win Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["webrtc_test_resources"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "no_com_init_hooks",
            "chrome_with_codecs",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "remoting_unittests",
        ],
    ),
    os = os.WINDOWS_ANY,
)

builder(
    name = "WebRTC Chromium Win10 Tester",
    parent = "WebRTC Chromium Win Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-webrtc",
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "os": "Windows-10",
                    },
                ),
            ),
        ],
    ),
)
