# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builder", "cpu", "defaults", "os", "reclient")
load("//lib/gn_args.star", "gn_args")

luci.bucket(
    name = "webrtc",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-ci-schedulers",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-webrtc-admins",
        ),
    ],
)

defaults.bucket.set("webrtc")
defaults.builder_group.set("chromium.webrtc")
defaults.builderless.set(None)
defaults.build_numbers.set(True)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set("recipe:chromium")
defaults.execution_timeout.set(2 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set("chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com")
defaults.triggered_by.set(["chromium-gitiles-trigger"])
defaults.reclient_instance.set(reclient.instance.DEFAULT_TRUSTED)
defaults.reclient_jobs.set(reclient.jobs.DEFAULT)

defaults.properties.set({
    "perf_dashboard_machine_group": "ChromiumWebRTC",
})

# Builders are defined in lexicographic order by name

builder(
    name = "WebRTC Chromium Android Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
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
            "debug_static_builder",
            "reclient",
            "arm64",
        ],
    ),
)

builder(
    name = "WebRTC Chromium Android Tester",
    triggered_by = ["WebRTC Chromium Android Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
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
            "reclient",
        ],
    ),
)

builder(
    name = "WebRTC Chromium Linux Tester",
    triggered_by = ["WebRTC Chromium Linux Builder"],
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
            "reclient",
        ],
    ),
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium Mac Tester",
    triggered_by = ["WebRTC Chromium Mac Builder"],
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
            "reclient",
            "minimal_symbols",
            "no_com_init_hooks",
            "chrome_with_codecs",
        ],
    ),
    os = os.WINDOWS_ANY,
)

builder(
    name = "WebRTC Chromium Win10 Tester",
    triggered_by = ["WebRTC Chromium Win Builder"],
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
)
