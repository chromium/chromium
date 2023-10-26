# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.android.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.android.fyi",
    ordering = {
        None: ["android", "memory", "weblayer", "webview"],
    },
)

ci.builder(
    name = "android-chrome-pie-x86-wpt-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|chrome",
        short_name = "p-x86",
    ),
)

ci.builder(
    name = "android-chrome-pie-x86-wpt-android-specific",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|chrome",
        short_name = "p-x86",
    ),
    experimental = True,
)

ci.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "wpt|webview",
        short_name = "p-x86",
    ),
)

# TODO(crbug.com/1022533#c40): Remove this builder once there are no associated
# disabled tests.
ci.builder(
    name = "android-pie-x86-fyi-rel",
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "x86_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x86|rel",
        short_name = "P",
    ),
)

# TODO(crbug.com/1137474): This and android-12-x64-fyi-rel
# are being kept around so that build links in the related
# bugs are accessible
# Remove these once the bugs are closed
ci.builder(
    name = "android-11-x86-fyi-rel",
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
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
        android_config = builder_config.android_config(config = "x86_builder_mb"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x86|rel",
        short_name = "11",
    ),
)

ci.builder(
    name = "android-12-x64-fyi-rel",
    triggered_by = ["ci/android-12-x64-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "12",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

# TODO(https://crbug.com/1431581): Remove after experimental is done.
ci.builder(
    name = "android-12l-x64-fyi-dbg",
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
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|dbg",
        short_name = "12L",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

# TODO(https://crbug.com/1431581): Remove after experimental is done.
ci.builder(
    name = "android-13-x64-fyi-rel",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder_mb",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "emulator|x64|rel",
        short_name = "13",
    ),
    # Android x64 builds take longer than x86 builds to compile
    # So they need longer timeouts
    # Matching the execution time out of the android-12-x64-rel
    execution_timeout = 4 * time.hour,
)

ci.builder(
    name = "android-annotator-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-android-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "and",
    ),
    notifies = ["annotator-rel"],
)

# TODO(crbug.com/1299910): Move to non-FYI once the tester works fine.
ci.thin_tester(
    name = "android-webview-12-x64-dbg-tests",
    triggered_by = ["Android x64 Builder (dbg)"],
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
        category = "tester|webview",
        short_name = "12",
    ),
)

ci.thin_tester(
    name = "android-webview-13-x64-dbg-tests",
    triggered_by = ["Android x64 Builder (dbg)"],
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
        category = "tester|webview",
        short_name = "13",
    ),
    notifies = [],
)

# TODO(crbug.com/1299910): Move to non-FYI once the tester works fine.
ci.thin_tester(
    name = "android-12-x64-dbg-tests",
    triggered_by = ["Android x64 Builder (dbg)"],
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
        category = "tester|phone",
        short_name = "12",
    ),
)

ci.builder(
    name = "android-cronet-asan-x86-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.COMPILE_AND_TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = ["cronet_builder", "mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
    contact_team_email = "cronet-team@google.com",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "dbg",
    ),
    contact_team_email = "cronet-sheriff@google.com",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet|riscv64",
        short_name = "rel",
    ),
    contact_team_email = "cronet-sheriff@google.com",
)
