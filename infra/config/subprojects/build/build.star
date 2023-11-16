# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in `build` bucket."""
# TODO: crbug.com/1502025 - Reduce duplicated configs from the shadow builders.
# Note that CI builders can't use `mirrors`.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "reclient", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//project.star", "settings")

luci.bucket(
    name = "build",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "project-chromium-ci-schedulers",
                "mdb/foundry-x-team",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
        acl.entry(
            roles = acl.SCHEDULER_TRIGGERER,
            groups = "project-chromium-scheduler-triggerers",
        ),
    ],
)

luci.gitiles_poller(
    name = "chrome-build-gitiles-trigger",
    bucket = "build",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
)

ci.defaults.set(
    bucket = "build",
    triggered_by = ["chrome-build-gitiles-trigger"],
    builder_group = "chromium.build",
    pool = ci.DEFAULT_POOL,
    builderless = False,
    # rely on the builder dimension for the bot selection.
    cores = None,
    build_numbers = True,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 10 * time.hour,
    notifies = ["chrome-build-perf"],
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_configs = [],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_enabled = True,
)

luci.console_view(
    name = "chromium.build",
    repo = "https://chromium.googlesource.com/chromium/src",
)

def cq_build_perf_builder(description_html, **kwargs):
    # Use CQ reclient instance and high reclient jobs/cores to simulate CQ builds.
    return ci.builder(
        description_html = description_html + "<br>Build stats is show in http://shortn/_gaAdI3x6o6.",
        reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
        reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
        siso_project = siso.project.DEFAULT_UNTRUSTED,
        use_clang_coverage = True,
        siso_configs = ["builder"],
        **kwargs
    )

cq_build_perf_builder(
    name = "build-perf-android",
    description_html = """\
This builder measures Android build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-arm64-rel-compilator">android-arm64-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf",
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
            config = "main_builder",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "ninja",
    ),
    gn_args = "try/android-arm64-rel",
)

cq_build_perf_builder(
    name = "build-perf-android-siso",
    description_html = """\
This builder measures Android build performance with Siso<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-arm64-rel-compilator">android-arm64-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "siso_latest",
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
            config = "main_builder",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "siso",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["try/android-arm64-rel", "no_reclient", "siso"]),
        "reproxy": "try/android-arm64-rel",
    },
)

cq_build_perf_builder(
    name = "build-perf-linux",
    description_html = """\
This builder measures Linux build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "ninja",
    ),
    gn_args = "try/linux-rel",
)

cq_build_perf_builder(
    name = "build-perf-linux-siso",
    description_html = """\
This builder measures Linux build performance with Siso.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "siso",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["try/linux-rel", "no_reclient", "siso"]),
        "reproxy": "try/linux-rel",
    },
)

cq_build_perf_builder(
    name = "build-perf-windows",
    description_html = """\
This builder measures Windows build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win-rel-compilator">win-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "windows",
        short_name = "ninja",
    ),
    gn_args = "try/win-rel",
)

cq_build_perf_builder(
    name = "build-perf-windows-siso",
    description_html = """\
This builder measures Windows build performance with Siso.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win-rel-compilator">win-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "windows",
        short_name = "siso",
    ),
    gn_args = {
        "builtin": gn_args.config(
            args = {
                "use_goma": False,
            },
            configs = ["try/win-rel", "no_reclient", "siso"],
        ),
        "reproxy": "try/win-rel",
    },
)

cq_build_perf_builder(
    name = "linux-chromeos-build-perf",
    description_html = """\
This builder measures ChromeOS build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros",
        short_name = "ninja",
    ),
    gn_args = "try/linux-chromeos-rel",
)

cq_build_perf_builder(
    name = "linux-chromeos-build-perf-siso",
    description_html = """\
This builder measures ChromeOS build performance with Siso.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros",
        short_name = "siso",
    ),
    gn_args = {
        "builtin": gn_args.config(
            args = {
                "use_goma": False,
            },
            configs = ["try/linux-chromeos-rel", "no_reclient", "siso"],
        ),
        "reproxy": "try/linux-chromeos-rel",
    },
)

cq_build_perf_builder(
    name = "mac-build-perf",
    description_html = """\
This builder measures Mac build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "ninja",
    ),
    gn_args = "try/mac-rel",
)

cq_build_perf_builder(
    name = "mac-build-perf-siso",
    description_html = """\
This builder measures Mac build performance with Siso.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "siso",
    ),
    gn_args = {
        "builtin": gn_args.config(
            configs = ["try/mac-rel", "no_reclient", "siso"],
        ),
        "reproxy": "try/mac-rel",
    },
)

def developer_build_perf_builder(description_html, **kwargs):
    # Use CQ reclient instance and high reclient jobs/cores to simulate CQ builds.
    return ci.builder(
        description_html = description_html + "<br>Build stats is show in http://shortn/_gaAdI3x6o6.",
        executable = "recipe:chrome_build/build_perf_developer",
        reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
        siso_project = siso.project.DEFAULT_UNTRUSTED,
        shadow_reclient_instance = None,
        **kwargs
    )

developer_build_perf_builder(
    name = "android-build-perf-developer",
    description_html = """\
This builder measures build performance for Android developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "siso_latest",
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
            config = "main_builder",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "dev",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["android_developer", "siso"]),
        "reproxy": gn_args.config(configs = ["android_developer", "reclient"]),
    },
    reclient_jobs = 5120,
)

developer_build_perf_builder(
    name = "linux-build-perf-developer",
    description_html = """\
This builder measures build performance for Linux developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "dev",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["developer", "siso"]),
        "reproxy": gn_args.config(configs = ["developer", "reclient"]),
    },
    reclient_jobs = 5120,
)

developer_build_perf_builder(
    name = "win-build-perf-developer",
    description_html = """\
This builder measures build performance for Windows developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "windows",
        short_name = "dev",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["developer", "siso"]),
        "reproxy": gn_args.config(configs = ["developer", "reclient"]),
    },
    reclient_jobs = 1000,
)

developer_build_perf_builder(
    name = "mac-build-perf-developer",
    description_html = """\
This builder measures build performance for Mac developer builds, by simulating developer build scenarios on a bot.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "dev",
    ),
    gn_args = {
        "builtin": gn_args.config(configs = ["developer", "siso"]),
        "reproxy": gn_args.config(configs = ["developer", "reclient"]),
    },
    reclient_jobs = 800,
)
