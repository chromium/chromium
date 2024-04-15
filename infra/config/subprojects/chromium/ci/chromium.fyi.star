# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/structs.star", "structs")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    health_spec = health_spec.DEFAULT,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
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
        "RBE_fast_log_collection": "true",
    })
    return ci.builder(name = name, **kwargs)

def fyi_ios_builder(*, name, **kwargs):
    kwargs.setdefault("cores", None)
    if kwargs.get("builderless", False):
        kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("reclient_scandeps_server", True)
    kwargs.setdefault("xcode", xcode.xcode_default)
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "try_builder",
            "reclient",
        ],
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
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
        ],
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
    name = "linux-chromeos-dbg-oslogin",
    description_html = "This builder is used to debug spefically oslogin issues related " +
                       "to linux-chromeos-dbg-oslogin",
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
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "debug_builder",
            "reclient",
            "use_cups",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    contact_team_email = "chrome-dev-infra-team@google.com",
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "reclient",
            "use_cups",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "BIr",
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "enable_blink_heap_verification",
            "dcheck_always_on",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.MAC,
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
            target_platform = builder_config.target_platform.WIN,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    builderless = True,
    cpu = cpu.ARM64,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
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
            target_platform = builder_config.target_platform.CHROMEOS,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "debug_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
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
            target_platform = builder_config.target_platform.CHROMEOS,
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
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
            "android_builder",
            "x64",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
        ],
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
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "linux-rr-fyi",
    description_html = "Runs top flaky tests using the rr tool and upload recorded traces.",
    executable = "recipe:chromium/rr_test_launcher",
    schedule = "with 3h interval",
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "reclient",
            "release_builder",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "linux-rr",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
            "dcheck_always_on",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "release_builder",
            "try_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug",
            "no_symbols",
            "dcheck_always_on",
            "static",
            "reclient",
            "x64",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "headless_shell",
            "release_builder",
            "reclient",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "devtools_do_typecheck",
        ],
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "perfetto",
            "release_builder",
            "reclient",
        ],
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
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "mac",
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "perfetto_zlib",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "win",
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
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
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
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 (reclient)",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
    },
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 on arm64 (reclient)",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "reclient",
                "minimal_symbols",
            ],
        ),
    },
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (8 cores) (reclient)",
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
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 80,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (reclient)",
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
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Windows - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Simple Chrome (reclient)",
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
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)",
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
    builderless = True,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison ios - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    xcode = xcode.xcode_default,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient)(CQ)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-arm64-rel-compilator">android-arm64-rel-compilator</a>.\
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac (reclient)(CQ)",
    description_html = """\
This builder measures Mac build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
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
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win-rel-compilator">win-rel-compilator</a>.\
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = reclient.instance.TEST_UNTRUSTED,
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    shadow_reclient_instance = reclient.instance.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = reclient.instance.TEST_UNTRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)(CQ)",
    description_html = """\
This builder measures iOS build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/ios-simulator">ios-simulator</a>.\
""",
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
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
    xcode = xcode.xcode_default,
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
        ],
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,  # crbug.com/1245114
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 16 * time.hour,
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
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
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
    ),
    reclient_jobs = None,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOSM1",
        short_name = "iosM1",
    ),
    contact_team_email = "bling-engprod@google.com",
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
    builderless = True,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "ios-blk",
    ),
    execution_timeout = 3 * time.hour,
    xcode = xcode.x15betabots,
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "x64",
            "xctest",
        ],
    ),
    cpu = cpu.ARM64,
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "ios17",
        ),
    ],
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
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "dev",
        ),
    ],
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    os = os.MAC_BETA,
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
    name = "ios18-beta-simulator",
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS18",
        short_name = "ios18",
    ),
)

fyi_ios_builder(
    name = "ios18-sdk-simulator",
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
    gn_args = gn_args.config(
        configs = [
            "debug_static_builder",
            "reclient",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS18",
        short_name = "sdk18",
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "debug_static_builder",
            "reclient",
            "dcheck_off",
            "shared",
        ],
    ),
    cores = None,
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
)

fyi_mac_builder(
    name = "Mac deterministic",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "mac_strip",
            "minimal_symbols",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "reclient",
        ],
    ),
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "try_builder",
            "reclient",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    contact_team_email = "web-windowing-team@google.com",
    experimental = True,
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "reclient",
            "minimal_symbols",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "win11",
    ),
    experimental = True,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "minimal_symbols",
            "release_builder",
            "reclient",
        ],
    ),
    builderless = False,
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
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    builderless = False,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
        ],
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

# TODO(crbug.com/324461153) remove this builder once dangling check is on CQ.
ci.builder(
    name = "linux-lacros-rel-dangling-ptr-fyi",
    description_html = "Dangling ptr check for lacros.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
            "use_cups",
            "enable_dangling_raw_ptr_checks",
            "enable_dangling_raw_ptr_feature_flag",
            "enable_backup_ref_ptr_feature_flag",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)
