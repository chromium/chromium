# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "reclient", "siso", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/structs.star", "structs")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.fyi",
    branch_selector = [
        branches.selector.CROS_LTS_BRANCHES,
        branches.selector.IOS_BRANCHES,
        branches.selector.LINUX_BRANCHES,
    ],
    ordering = {
        None: [
            "code_coverage",
            "cronet",
            "mac",
            "deterministic",
            "fuchsia",
            "chromeos",
            "iOS",
            "infra",
            "linux",
            "recipe",
            "site_isolation",
            "network",
            "viz",
            "win10",
            "win11",
            "win32",
            "buildperf",
            # TODO(crbug.com/1441164): remove after CR2023 launch.
            "cr23",
        ],
        "code_coverage": consoles.ordering(
            short_names = ["and", "ann", "lnx", "lcr", "jcr", "mac"],
        ),
        "mac": consoles.ordering(short_names = ["bld", "15", "herm"]),
        "deterministic|mac": consoles.ordering(short_names = ["rel", "dbg"]),
        "iOS|iOS13": consoles.ordering(short_names = ["dev", "sim"]),
        "linux|blink": consoles.ordering(short_names = ["TD"]),
    },
)

def fyi_ios_builder(*, name, **kwargs):
    kwargs.setdefault("cores", None)
    if kwargs.get("builderless", False):
        kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("reclient_scandeps_server", True)
    kwargs.setdefault("xcode", xcode.x14main)
    return ci.builder(name = name, **kwargs)

def fyi_mac_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 4)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("reclient_scandeps_server", True)
    return ci.builder(name = name, **kwargs)

ci.builder(
    name = "Linux Viz",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "viz",
    ),
)

ci.builder(
    name = "Site Isolation Android",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "arm64_builder_mb"),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
)

ci.builder(
    name = "lacros-amd64-generic-rel-fyi",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mb_no_luci_auth",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["eve"],
            cros_boards_with_qemu_images = ["amd64-generic"],
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lcr",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    # Some tests on this bot depend on being unauthenticated with GS, so
    # don't run the tests inside a luci-auth context to avoid having the
    # BOTO config setup for the task's service account.
    # TODO(crbug.com/1217155): Fix this.
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_platform = "chromeos",
            target_cros_boards = "eve",
            cros_boards_with_qemu_images = "amd64-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lsf",
    ),
)

ci.builder(
    name = "lacros-arm-generic-rel-skylab-fyi",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            target_bits = 32,
            target_platform = "chromeos",
            target_cros_boards = "kevin:jacuzzi:arm-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "arm",
    ),
)

ci.builder(
    name = "lacros-arm64-generic-rel-skylab-fyi",
    # Some tests on this bot depend on being unauthenticated with GS, so
    # don't run the tests inside a luci-auth context to avoid having the
    # BOTO config setup for the task's service account.
    # TODO(crbug.com/1217155): Fix this.
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_platform = "chromeos",
            target_cros_boards = "kevin:jacuzzi",
            cros_boards_with_qemu_images = "arm64-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "larsf",
    ),
)

ci.builder(
    name = "linux-annotator-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    notifies = ["annotator-rel"],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-annotator-rel",
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "rel",
    ),
    execution_timeout = 3 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-lacros-version-skew-fyi",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-blink-wpt-reset-rel",
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
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "BIr",
    ),
)

ci.builder(
    name = "linux-blink-animation-use-time-delta",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "TD",
    ),
)

ci.builder(
    name = "linux-blink-heap-verification",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    notifies = ["linux-blink-fyi-bots"],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-fieldtrial-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.thin_tester(
    name = "mac-fieldtrial-tester",
    triggered_by = ["ci/mac-arm64-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.thin_tester(
    name = "win-network-sandbox-tester",
    triggered_by = ["ci/Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "network|sandbox",
        short_name = "win",
    ),
)

ci.builder(
    name = "linux-network-sandbox-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|sandbox",
        short_name = "lnx",
    ),
)

ci.builder(
    name = "android-fieldtrial-rel",
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
    builderless = False,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
    ),
)

fyi_ios_builder(
    name = "ios-fieldtrial-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = "ios",
        ),
    ),
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "linux-lacros-builder-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "linux-lacros-tester-fyi-rel",
    triggered_by = ["linux-lacros-builder-fyi-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "linux-lacros-dbg-fyi",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "linux-lacros-dbg-tests-fyi",
    triggered_by = ["linux-lacros-dbg-fyi"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "android-perfetto-rel",
    schedule = "triggered",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
    ),
)

ci.builder(
    name = "linux-perfetto-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

fyi_mac_builder(
    name = "mac-perfetto-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    cores = 8,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "linux-wpt-content-shell-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

ci.builder(
    name = "linux-wpt-content-shell-leak-detection",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

ci.builder(
    name = "linux-wpt-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

ci.builder(
    name = "linux-wpt-content-shell-asan-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

ci.builder(
    name = "linux-wpt-identity-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

ci.builder(
    name = "linux-wpt-input-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
)

fyi_ios_builder(
    name = "ios-wpt-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = "ios",
        ),
    ),
    builderless = False,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "mac-osxbeta-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = False,
    cores = 12,
    os = os.MAC_13,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "beta",
    ),
    main_console_view = None,
)

ci.builder(
    name = "linux-headless-shell-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "hdls",
    ),
    notifies = ["headless-owners"],
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "linux-rel-no-external-ip",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
    ),
    builderless = False,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
)

ci.builder(
    name = "win-perfetto-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "win10-rel-no-external-ip",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
    ),
    builderless = False,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
)

ci.builder(
    name = "linux-upload-perfetto",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
    ),
)

fyi_mac_builder(
    name = "mac-upload-perfetto",
    schedule = "with 3h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "mac",
    ),
)

ci.builder(
    name = "win-upload-perfetto",
    schedule = "with 3h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "win",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Comparison Android (reclient)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Deterministic%20Android%20(dbg)">Deterministic Android (dbg)</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Android - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

ci.builder(
    name = "Comparison Android (reclient) (reproxy cache)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test using reproxy's deps cache.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Comparison%20Android%20(reclient)">Comparison Android (reclient)</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|expcache",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Android (reproxy cache) - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

ci.builder(
    name = "Comparison Linux (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Linux - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

fyi_mac_builder(
    name = "Comparison Mac (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "GLOG_vmodule": "bridge*=2",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

fyi_mac_builder(
    name = "Comparison Mac arm64 (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "GLOG_vmodule": "bridge*=2",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

fyi_mac_builder(
    name = "Comparison Mac arm64 on arm64 (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "GLOG_vmodule": "bridge*=2",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

ci.builder(
    name = "Comparison Windows (8 cores) (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 80,
)

ci.builder(
    name = "Comparison Windows (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Windows - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
)

fyi_mac_builder(
    name = "Comparison ios (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison ios - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    xcode = xcode.x14main,
)

ci.builder(
    name = "Comparison Android (reclient)(CQ)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-pie-arm64-rel-compilator">android-pie-arm64-rel-compilator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|cq",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Android CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

ci.builder(
    name = "Comparison Linux (reclient)(CQ)",
    description_html = """\
This builder measures Linux build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|cq",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Linux CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
)

fyi_mac_builder(
    name = "Comparison Mac (reclient)(CQ)",
    description_html = """\
This builder measures Mac build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Mac CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
)

ci.builder(
    name = "Comparison Windows (reclient)(CQ)",
    description_html = """\
This builder measures Windows build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|cq",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    goma_enable_ats = False,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Windows CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)(CQ)",
    description_html = """\
This builder measures Simple Chrome build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison Simple Chrome CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

fyi_mac_builder(
    name = "Comparison ios (reclient)(CQ)",
    description_html = """\
This builder measures iOS build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/ios-simulator">ios-simulator</a>.\
""",
    executable = "recipe:reclient_reclient_comparison",
    builderless = True,
    cores = None,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "ios|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
    },
    reclient_cache_silo = "Comparison ios CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    xcode = xcode.x14main,
)

# Build Perf builders use CQ reclient instance and high reclient jobs/cores and
# SSD to represent CQ build performance.

def build_perf_builder(**kwargs):
    kwargs.setdefault("executable", "recipe:chrome_build/build_perf")
    kwargs.setdefault("reclient_instance", reclient.instance.DEFAULT_UNTRUSTED)
    kwargs.setdefault("reclient_jobs", reclient.jobs.HIGH_JOBS_FOR_CQ)
    kwargs.setdefault("use_clang_coverage", True)
    return ci.builder(
        service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        # rely on the builder dimension for the bot selection.
        builderless = False,
        cores = None,
        siso_config = "remote_all",
        siso_enable_cloud_profiler = True,
        siso_enable_cloud_trace = True,
        siso_project = siso.project.DEFAULT_UNTRUSTED,
        notifies = ["chrome-build-perf"],
        **kwargs
    )

build_perf_builder(
    name = "build-perf-android",
    description_html = """\
This builder measures Android build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-arm64-rel-compilator">android-arm64-rel-compilator</a>.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "and",
    ),
)

build_perf_builder(
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
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "andss",
    ),
)

build_perf_builder(
    name = "android-build-perf-developer",
    description_html = """\
This builder measures build performance for Android developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    executable = "recipe:chrome_build/build_perf_developer",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "anddev",
    ),
    reclient_instance = reclient.instance.DEVELOPER,
    reclient_jobs = 5120,
    use_clang_coverage = None,
)

build_perf_builder(
    name = "build-perf-linux",
    description_html = """\
This builder measures Linux build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "lnx",
    ),
)

build_perf_builder(
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
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "lnxss",
    ),
)

build_perf_builder(
    name = "linux-build-perf-developer",
    description_html = """\
This builder measures build performance for Linux developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    executable = "recipe:chrome_build/build_perf_developer",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "lnxdev",
    ),
    reclient_instance = reclient.instance.DEVELOPER,
    reclient_jobs = 5120,
    use_clang_coverage = None,
)

build_perf_builder(
    name = "build-perf-windows",
    description_html = """\
This builder measures Windows build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win-rel-compilator">win-rel-compilator</a>.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "win",
    ),
)

build_perf_builder(
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
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "winss",
    ),
)

build_perf_builder(
    name = "win-build-perf-developer",
    description_html = """\
This builder measures build performance for Windows developer builds, by simulating developer build scenarios on a high spec bot.\
""",
    executable = "recipe:chrome_build/build_perf_developer",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "windev",
    ),
    reclient_instance = reclient.instance.DEVELOPER,
    reclient_jobs = 1000,
    use_clang_coverage = None,
)

build_perf_builder(
    name = "linux-chromeos-build-perf",
    description_html = """\
This builder measures ChromeOS build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "cros",
    ),
)

build_perf_builder(
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
                "checkout_siso",
                "siso_latest",
                # TODO(crbug.com/1441379): remove after the permission issue gets fixed.
                "chromium_no_telemetry_dependencies",
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
        category = "buildperf",
        short_name = "crosss",
    ),
)

ci.builder(
    name = "Linux Builder (j-500) (reclient)",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    reclient_jobs = 500,
    reclient_rewrapper_env = {
        "RBE_platform": "container-image=docker://gcr.io/cloud-marketplace/google/rbe-ubuntu16-04@sha256:b4dad0bfc4951d619229ab15343a311f2415a16ef83bcaa55b44f4e2bf1cf635,pool=linux-e2-custom_0",
    },
)

ci.builder(
    name = "Linux Builder (reclient compare)",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = ["reclient_test"],
            ),
            build_gs_bucket = None,
        ),
    ),
    cores = 32,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    execution_timeout = 14 * time.hour,
    reclient_bootstrap_env = {
        "RBE_clang_depscan_archive": "true",
    },
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
    },
)

ci.builder(
    name = "Win x64 Builder (reclient)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                "reclient_test",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
)

ci.builder(
    name = "Win x64 Builder (reclient compare)",
    description_html = "Verifies whether local and remote build artifacts are identical.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage", "reclient_test"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
    },
)

fyi_mac_builder(
    name = "Mac Builder (reclient compare)",
    description_html = "Verifies whether local and remote build artifacts are identical.",
    schedule = "@daily",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                "reclient_test",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    cores = None,  # crbug.com/1245114
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
    },
)

fyi_mac_builder(
    name = "mac10.15-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_mac_builder(
    name = "mac11-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_mac_builder(
    name = "mac12-arm64-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_mac_builder(
    name = "mac12-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_mac_builder(
    name = "mac13-arm64-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_mac_builder(
    name = "mac13-wpt-content-shell-fyi-rel",
    # TODO(crbug.com/1385202): Enable scheduler when machine has been allocated.
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "chromeos-amd64-generic-rel (reclient)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = [
                "amd64-generic",
                "amd64-generic-vm",
            ],
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
    ),
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_cache_silo": "chromeos-amd64-generic-rel (reclient)"},
)

ci.builder(
    name = "chromeos-amd64-generic-rel (reclient compare)",
    description_html = "Verifies whether local and remote build artifacts are identical.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = [
                "amd64-generic",
                "amd64-generic-vm",
            ],
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_ensure_verified = True,
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_compare": "true"},
)

ci.builder(
    name = "lacros-amd64-generic-rel (reclient)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["amd64-generic"],
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
    ),
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_cache_silo": "lacros-amd64-generic-rel (reclient)"},
)

ci.builder(
    name = "linux-lacros-builder-rel (reclient)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
    ),
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
)

ci.builder(
    name = "win-celab-builder-rel",
    executable = "recipe:celab",
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    properties = {
        "exclude": "chrome_only",
        "pool_name": "celab-chromium-ci",
        "pool_size": 20,
        "tests": "*",
    },
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

fyi_ios_builder(
    name = "ios-m1-simulator",
    schedule = "0 1,5,9,13,17,21 * * *",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mac_toolchain"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOSM1",
        short_name = "iosM1",
    ),
)

fyi_ios_builder(
    name = "ios-blink-dbg-fyi",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "ios-blk",
    ),
    execution_timeout = 3 * time.hour,
)

fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.selector.IOS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "intel",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
)

fyi_ios_builder(
    name = "ios-m1-simulator-cronet",
    schedule = "0 1,5,9,13,17,21 * * *",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mac_toolchain"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "m1",
    ),
)

fyi_ios_builder(
    name = "ios-simulator-multi-window",
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "mwd",
    ),
)

fyi_ios_builder(
    name = "ios-webkit-tot",
    schedule = "0 1-23/6 * * *",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = ["ios_webkit_tot"],
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "wk",
    ),
    xcode = xcode.x14wk,
)

fyi_ios_builder(
    name = "ios17-beta-simulator",
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_13,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "ios17",
        ),
    ],
)

fyi_ios_builder(
    name = "ios17-sdk-simulator",
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_13,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "sdk17",
        ),
    ],
    xcode = xcode.x15betabots,
)

fyi_ios_builder(
    name = "ios16-beta-simulator",
    schedule = "0 0,4,8,12,16,20 * * *",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_13,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "ios16",
    ),
)

fyi_ios_builder(
    name = "ios16-sdk-device",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_13,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS16",
            short_name = "dev",
        ),
    ],
    xcode = xcode.x14betabots,
)

fyi_ios_builder(
    name = "ios16-sdk-simulator",
    schedule = "0 2,6,10,14,18,22 * * *",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_13,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "sdk16",
    ),
    xcode = xcode.x14betabots,
)

fyi_mac_builder(
    name = "Mac Builder Next",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    cores = None,
    os = os.MAC_13,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
)

fyi_mac_builder(
    name = "Mac deterministic",
    executable = "recipe:swarming/deterministic_build",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "rel",
    ),
    execution_timeout = 6 * time.hour,
)

fyi_mac_builder(
    name = "Mac deterministic (dbg)",
    executable = "recipe:swarming/deterministic_build",
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "dbg",
    ),
    execution_timeout = 6 * time.hour,
)

ci.builder(
    name = "Win 10 Fast Ring",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    notifies = ["Win 10 Fast Ring"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win10-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    experimental = True,
)

ci.builder(
    name = "win11-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "win11",
    ),
    experimental = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win32-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    cores = "8|16",
    os = os.WINDOWS_DEFAULT,
    cpu = cpu.X86,
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    reclient_jobs = 150,
)

ci.builder(
    name = "win-fieldtrial-rel",
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-annotator-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "win",
    ),
    execution_timeout = 16 * time.hour,
    notifies = ["annotator-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

# TODO(crbug.com/1441164): ChromeRefresh2023 builders. Remove after launch.
ci.builder(
    name = "linux-cr23-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_BIONIC,
    console_view_entry = consoles.console_view_entry(
        category = "cr23",
        short_name = "lnx",
    ),
)

ci.builder(
    name = "win-cr23-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "cr23",
        short_name = "win",
    ),
)

fyi_mac_builder(
    name = "mac-cr23-rel",
    # TODO(crbug.com/1422735): use thin_tester once FYI bots have
    # dedicated compilators.
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cr23",
        short_name = "mac",
    ),
)
