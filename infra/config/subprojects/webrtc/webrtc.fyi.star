# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "builder", "cpu", "defaults", "os")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")

luci.bucket(
    name = "webrtc.fyi",
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.webrtc.fyi"],
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

# Define the shadow bucket of `webrtc.fyi`.
luci.bucket(
    name = "webrtc.fyi.shadow",
    shadows = "webrtc.fyi",
    # Only the builds with allowed pool and service account can be created
    # in this bucket.
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.webrtc.fyi"],
        service_accounts = [
            "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
            "webrtc-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        # for led permissions.
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "project-webrtc-led-users",
        ),
    ],
    dynamic = True,
)

luci.gitiles_poller(
    name = "webrtc-gitiles-trigger",
    bucket = "webrtc",
    repo = "https://webrtc.googlesource.com/src/",
    refs = ["refs/heads/main"],
)

defaults.set(
    bucket = "webrtc.fyi",
    executable = "recipe:chromium",
    triggered_by = ["webrtc-gitiles-trigger"],
    builder_group = "chromium.webrtc.fyi",
    pool = "luci.chromium.webrtc.fyi",
    builderless = None,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    contact_team_email = "webrtc-infra@google.com",
    execution_timeout = 3 * time.hour,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

# Builders are defined in lexicographic order by name

# For builders, specify targets if the builder has no associated
# tester (if it does, it will build what the tester needs).

builder(
    name = "WebRTC Chromium FYI Android Builder",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm64",
        ],
    ),
)

builder(
    name = "WebRTC Chromium FYI Android Builder (dbg)",
    description_html = "Building Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "base_config",
            apply_configs = [
                "dcheck",
                "mb",
                "android",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
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
    targets = targets.bundle(
        additional_compile_targets = [
            "capture_unittests",
            "content_browsertests",
            "content_unittests",
            "remoting_unittests",
        ],
    ),
)

builder(
    name = "WebRTC Chromium FYI Android Tester",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    parent = "WebRTC Chromium FYI Android Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
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
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_simple_gtests",
        ],
        mixins = [
            "panther_on_14",
        ],
        per_test_modifications = {
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/chromium.webrtc.fyi.android.tests.dbg.content_browsertests.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
)

builder(
    name = "WebRTC Chromium FYI Linux Builder",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
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
)

builder(
    name = "WebRTC Chromium FYI Linux Builder (dbg)",
    description_html = "Building Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "browser_tests",
            "capture_unittests",
            "content_browsertests",
            "content_unittests",
            "remoting_unittests",
        ],
    ),
)

builder(
    name = "WebRTC Chromium FYI Linux Tester",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    parent = "WebRTC Chromium FYI Linux Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
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
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            "x86-64",
            "linux-jammy",
        ],
    ),
)

builder(
    name = "WebRTC Chromium FYI Mac Builder",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
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
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium FYI Mac Builder (dbg)",
    description_html = "Building Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "browser_tests",
            "capture_unittests",
            "content_browsertests",
            "content_unittests",
            "remoting_unittests",
        ],
    ),
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium FYI Mac Tester",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    parent = "WebRTC Chromium FYI Mac Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
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
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium FYI Win Builder",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_webrtc_tot",
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
            target_platform = builder_config.target_platform.WIN,
        ),
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
    os = os.WINDOWS_ANY,
)

builder(
    name = "WebRTC Chromium FYI Win Builder (dbg)",
    description_html = "Building Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "no_com_init_hooks",
            "chrome_with_codecs",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "browser_tests",
            "capture_unittests",
            "content_browsertests",
            "content_unittests",
            "remoting_unittests",
        ],
    ),
    os = os.WINDOWS_ANY,
)

builder(
    name = "WebRTC Chromium FYI Win Tester",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    parent = "WebRTC Chromium FYI Win Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium_webrtc_tot"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "dcheck",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "webrtc_chromium_gtests",
        ],
        mixins = [
            "x86-64",
            "win11-any",
        ],
    ),
    os = os.WINDOWS_ANY,
)

# Builders run on the default Mac OS version offered
# in the Chrome infra however the tasks will be sharded
# to swarming bots with appropriate OS using swarming
# dimensions.
builder(
    name = "WebRTC Chromium FYI ios-device",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "ios"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "compile_only",
            "ios_device",
            "arm64",
            "ios_google_cert",
            "ios_disable_code_signing",
            "release_builder",
            "remoteexec",
            "ios_build_chrome_false",
        ],
    ),
    targets = targets.bundle(),
    os = os.MAC_ANY,
    xcode = xcode.xcode_default,
)

builder(
    name = "WebRTC Chromium FYI ios-simulator",
    description_html = "Testing WebRTC inside Chromium at each WebRTC commit",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "ios"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "remoteexec",
            "ios_simulator",
            "x64",
            "xctest",
            "ios_build_chrome_false",
        ],
    ),
    targets = targets.bundle(
        mixins = [
            "has_native_resultdb_integration",
            "mac_default_x64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    os = os.MAC_ANY,
    xcode = xcode.xcode_default,
)
