# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
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
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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

def fyi_reclient_comparison_builder(*, name, **kwargs):
    kwargs.setdefault("executable", "recipe:reclient_reclient_comparison")
    kwargs["reclient_bootstrap_env"] = kwargs.get("reclient_bootstrap_env", {})
    kwargs["reclient_bootstrap_env"].update({
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_experimental_goma_deps_cache": "true",
        "RBE_deps_cache_mode": "reproxy",
        "RBE_fast_log_collection": "true",
    })
    return ci.builder(name = name, **kwargs)

def fyi_ios_builder(*, name, **kwargs):
    kwargs.setdefault("cores", None)
    if kwargs.get("builderless", False):
        kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("reclient_scandeps_server", True)
    kwargs.setdefault("xcode", xcode.x15main)
    return ci.builder(name = name, **kwargs)

def mac_builder_defaults(**kwargs):
    kwargs.setdefault("cores", 4)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("reclient_scandeps_server", True)
    return kwargs

def fyi_mac_builder(*, name, **kwargs):
    return ci.builder(name = name, **mac_builder_defaults(**kwargs))

def fyi_mac_reclient_comparison_builder(*, name, **kwargs):
    return fyi_reclient_comparison_builder(name = name, **mac_builder_defaults(**kwargs))

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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "try_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "arm64",
            "strip_debug_info",
        ],
    ),
    notifies = ["Site Isolation Android"],
)

ci.builder(
    name = "chromeos-jacuzzi-rel-skylab-fyi",
    description_html = """\
This builder builds public image and runs tests on DUTs in the lab.<br/>\
This is experimental.
""",
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
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "jacuzzi",
                "arm64-generic",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chrome-test-builds",
            gs_extra = "ash",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ash",
        short_name = "jcz",
    ),
    contact_team_email = "chromeos-velocity@google.com",
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "ozone_headless",
            "dcheck_off",
            "reclient",
            "jacuzzi",
            "include_unwind_tables",
            "also_build_lacros_chrome_for_architecture_arm64",
            "is_skylab",
        ],
    ),
)

ci.builder(
    name = "chromeos-octopus-rel-skylab-fyi",
    description_html = """\
This builder builds public image and runs tests on octopus DUTs in the lab.<br/>\
This is experimental.
""",
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
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "octopus",
                "amd64-generic",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chrome-test-builds",
            gs_extra = "ash",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ash",
        short_name = "oct",
    ),
    contact_team_email = "chromeos-velocity@google.com",
    gn_args = gn_args.config(
        configs = [
            "also_build_lacros_chrome_for_architecture_amd64",
            "chromeos_device",
            "include_unwind_tables",
            "is_skylab",
            "octopus",
            "ozone_headless",
            "reclient",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
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
            target_cros_boards = "kevin:arm-generic",
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "arm-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "arm64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "reclient",
            "use_cups",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "reclient",
            "enable_blink_animation_use_time_delta",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "enable_blink_heap_verification",
            "dcheck_always_on",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
            "webview_trichrome",
            "no_secondary_abi",
            "webview_shell",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "debug_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
            "android_builder",
            "x64",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
        ],
    ),
)

ci.builder(
    name = "linux-wpt-chromium-rel",
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
            "dcheck_always_on",
        ],
    ),
)

ci.builder(
    name = "linux-wpt-content-shell-fyi-rel",
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "release_builder",
            "try_builder",
            "reclient",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "minimal_symbols",
            "ios_simulator",
            "x64",
            "release_builder",
            "reclient",
            "xctest",
            "dcheck_always_on",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug",
            "no_symbols",
            "dcheck_always_on",
            "static",
            "reclient",
            "use_dummy_lastchange",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "headless_shell",
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "devtools_do_typecheck",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    notifies = ["chrometto-sheriff"],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    notifies = ["chrometto-sheriff"],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    notifies = ["chrometto-sheriff"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Deterministic%20Android%20(dbg)">Deterministic Android (dbg)</a>.\
""",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "android_builder",
                "debug_static_builder",
                "reclient",
                "arm64",
                "webview_google",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "android_builder",
                "debug_static_builder",
                "reclient",
                "arm64",
                "webview_google",
            ],
        ),
    },
    reclient_cache_silo = "Comparison Android - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient) (reproxy cache)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test using reproxy's deps cache.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Comparison%20Android%20(reclient)">Comparison Android (reclient)</a>.\
""",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|expcache",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_cache_silo = "Comparison Android (reproxy cache) - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac (reclient)",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
    },
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 (reclient)",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
    },
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 on arm64 (reclient)",
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
                "disable_nacl",
            ],
        ),
    },
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (8 cores) (reclient)",
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
    },
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 80,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (reclient)",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
    },
    reclient_cache_silo = "Comparison Windows - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Simple Chrome (reclient)",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "chromeos_device",
                "dcheck_off",
                "reclient",
                "amd64-generic-vm",
                "ozone_headless",
                "use_fake_dbus_clients",
                "also_build_lacros_chrome_for_architecture_amd64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "chromeos_device",
                "dcheck_off",
                "reclient",
                "amd64-generic-vm",
                "ozone_headless",
                "use_fake_dbus_clients",
                "also_build_lacros_chrome_for_architecture_amd64",
            ],
        ),
    },
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)",
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "debug_static_builder",
                "reclient",
                "ios_simulator",
                "x64",
                "xctest",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "debug_static_builder",
                "reclient",
                "ios_simulator",
                "x64",
                "xctest",
            ],
        ),
    },
    reclient_cache_silo = "Comparison ios - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    xcode = xcode.x15main,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient)(CQ)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-pie-arm64-rel-compilator">android-pie-arm64-rel-compilator</a>.\
""",
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|cq",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_cache_silo = "Comparison Android CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac (reclient)(CQ)",
    description_html = """\
This builder measures Mac build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
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
    },
    reclient_cache_silo = "Comparison Mac CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (reclient)(CQ)",
    description_html = """\
This builder measures Windows build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|cq",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Windows CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Simple Chrome (reclient)(CQ)",
    description_html = """\
This builder measures Simple Chrome build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)(CQ)",
    description_html = """\
This builder measures iOS build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/ios-simulator">ios-simulator</a>.\
""",
    builderless = True,
    cores = None,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "ios|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison ios CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    xcode = xcode.x15main,
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
        ],
    ),
    reclient_bootstrap_env = {
        "RBE_clang_depscan_archive": "true",
    },
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_reclient_instance = None,
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    shadow_reclient_instance = None,
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_reclient_instance = None,
)

# TODO(crbug.com/1260232): remove this after the migration.
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_reclient_instance = None,
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
)

fyi_mac_builder(
    name = "mac11-arm64-wpt-content-shell-fyi-rel",
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-vm",
            "ozone_headless",
            "use_fake_dbus_clients",
            "also_build_lacros_chrome_for_architecture_amd64",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-vm",
            "ozone_headless",
            "use_fake_dbus_clients",
            "also_build_lacros_chrome_for_architecture_amd64",
        ],
    ),
    reclient_ensure_verified = True,
    reclient_jobs = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-vm",
            "ozone_headless",
            "lacros",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
    ),
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
)

ci.builder(
    name = "win-celab-builder-rel",
    executable = "recipe:celab",
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
    builderless = False,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    contact_team_email = "bling-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
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
            # Release for now due to binary size being too large (crbug.com/1464415)
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "ios-blk",
    ),
    execution_timeout = 3 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "try_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "use_blink",
            "xctest",
        ],
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
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "mwd",
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "x64",
            "xctest",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "x64",
            "xctest",
            "no_lld",
        ],
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
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "ios17",
        ),
    ],
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
)

fyi_ios_builder(
    name = "ios17-sdk-device",
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
            category = "iOS|iOS17",
            short_name = "dev",
        ),
    ],
    gn_args = gn_args.config(
        configs = [
            "ios_device",
            "arm64",
            "ios_disable_code_signing",
            "release_builder",
            "reclient",
            "xctest",
        ],
    ),
    xcode = xcode.x15betabots,
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
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
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "ios16",
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
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
    os = os.MAC_14,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "sdk16",
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    xcode = xcode.x15betabots,
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
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "debug_static_builder",
            "reclient",
            "disable_nacl",
            "dcheck_off",
            "shared",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "mac_strip",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "reclient",
        ],
    ),
)

ci.builder(
    name = "Win 10 Fast Ring",
    description_html = (
        "This builder is intended to run builds & tests on pre-release " +
        "versions of Windows. However, flashing such images on the bots " +
        "is not supported at this time.<br/>So this builder remains paused " +
        "until a solution can be determined. For more info, see " +
        "<a href=\"http://shortn/_B7cJcHq55P\">http://shortn/_B7cJcHq55P</a>."
    ),
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
    builderless = False,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "try_builder",
            "reclient",
        ],
    ),
    notifies = ["Win 10 Fast Ring"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win10-multiscreen-fyi-rel",
    description_html = (
        "This builder is intended to run tests related to multiscreen " +
        "functionality on Windows. For more info, see " +
        "<a href=\"http://shortn/_4L6uYvA1xU\">http://shortn/_4L6uYvA1xU</a>."
    ),
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
    contact_team_email = "web-windowing-team@google.com",
    experimental = True,
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    shadow_reclient_instance = None,
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
    builderless = False,
    cores = "8|16",
    os = os.WINDOWS_DEFAULT,
    cpu = cpu.X86,
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "disable_nacl",
            "minimal_symbols",
            "release_builder",
            "reclient",
        ],
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
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
        ],
    ),
    notifies = ["annotator-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)
