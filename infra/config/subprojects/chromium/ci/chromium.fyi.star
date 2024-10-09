# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify", "linkify_builder")
load("//lib/structs.star", "structs")
load("//lib/targets.star", "targets")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    health_spec = health_spec.DEFAULT,
    priority = ci.DEFAULT_FYI_PRIORITY,
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
    name = "chromium.fyi",
    # FYI builders should not be branched; since they are not gardened there's
    # no guarantee that the branch builders would be in a good state when they
    # are created and become the responsibility of the branch gardeners
    branch_selector = branches.selector.MAIN,
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
            "remoteexec",
            "linux",
            "x64",
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
            "remoteexec",
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
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    notifies = ["annotator-rel"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            "remoteexec",
            "use_cups",
            "x64",
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
            "remoteexec",
            "use_cups",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "rel",
    ),
    execution_timeout = 3 * time.hour,
    notifies = ["annotator-rel"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            "remoteexec",
            "minimal_symbols",
            "linux",
            "x64",
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
            "remoteexec",
            "enable_blink_heap_verification",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    notifies = ["linux-blink-fyi-bots"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            "remoteexec",
            "minimal_symbols",
            "linux",
            "x64",
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
    targets = targets.bundle(
        targets = [
            "fieldtrial_browser_tests",
        ],
        mixins = [
            "finch-chromium-swarming-pool",
            "mac_default_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "linux-multiscreen-fyi-rel",
    description_html = (
        "This builder is intended to run tests related to multiscreen " +
        "functionality on Linux. For more info, see crbug.com/346565331."
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "remoteexec",
            "minimal_symbols",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mulitscreen",
    ),
    contact_team_email = "web-windowing-team@google.com",
    notifies = ["multiscreen-owners"],
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
            "remoteexec",
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
    description_html = (
        "Builds the open-source version of Chrome for iOS and runs tests, " +
        "passing the --disable-field-trial-config flag. This causes " +
        "testing/variations/fieldtrial_testing_config.json not to be used for" +
        " determining which finch experiments are enabled. Instead, the " +
        "default configuration of every feature flag is used."
    ),
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
            "remoteexec",
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
            "release_builder",
            "remoteexec",
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
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "linux-rr-orchestrator-fyi",
    description_html = (
        "The orchestrator to schedules child builds of rr test launcher, and" +
        " these child builds run top flaky tests using the rr tool and" +
        " upload recorded traces."
    ),
    executable = "recipe:chromium_rr/orchestrator",
    schedule = "with 3h interval",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rr",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
)

ci.builder(
    name = "linux-rr-test-launcher-fyi",
    description_html = (
        "The rr test launcher compiles input test suites, run" +
        " input tests using the rr tool and upload recorded traces."
    ),
    executable = "recipe:chromium_rr/test_launcher",
    schedule = "triggered",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rr",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
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
            "release_builder",
            "remoteexec",
            "mac",
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

fyi_mac_builder(
    name = "mac13-wpt-chromium-rel",
    description_html = "Runs {} against Chrome.".format(
        linkify("https://web-platform-tests.org", "web platform tests"),
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "remoteexec",
            "minimal_symbols",
            "mac",
            "x64",
            "dcheck_always_on",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
    experimental = True,
)

ci.builder(
    name = "linux-wpt-chromium-rel",
    description_html = "Runs {} against Chrome.".format(
        linkify("https://web-platform-tests.org", "web platform tests"),
    ),
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
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
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
            "remoteexec",
            "xctest",
            "dcheck_always_on",
        ],
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "mac-osxbeta-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
            ],
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
            "remoteexec",
            "mac",
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
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "hdls",
    ),
    notifies = ["headless-owners"],
)

# TODO(crbug.com/40223366): Remove this builder after experimentation.
ci.builder(
    name = "linux-rel-no-external-ip",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "devtools_do_typecheck",
            "linux",
            "x64",
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
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
)

# TODO(crbug.com/40223366): Remove this builder after experimentation.
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
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
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
            "remoteexec",
            "perfetto_zlib",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
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
            "remoteexec",
            "perfetto_zlib",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "win",
    ),
    notifies = ["chrometto-sheriff"],
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("ci", "Deterministic Android (dbg)")),
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "android_builder",
                "debug_static_builder",
                "remoteexec",
                "arm64",
                "webview_google",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "android_builder",
                "debug_static_builder",
                "remoteexec",
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
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient) (reproxy cache)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test using reproxy's deps cache.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("ci", "Comparison Android (reclient)")),
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
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac (reclient)",
    schedule = "0 */4 * * *",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
                "x64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
                "x64",
            ],
        ),
    },
    builderless = True,
    cores = 12,
    cpu = cpu.X86_64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 (reclient)",
    schedule = "0 */4 * * *",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
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
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac arm64 on arm64 (reclient)",
    schedule = "0 */4 * * *",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "arm64",
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "mac",
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
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (8 cores) (reclient)",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "win",
                "x64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "win",
                "x64",
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
    execution_timeout = 14 * time.hour,
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = 80,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (reclient)",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "win",
                "x64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "minimal_symbols",
                "win",
                "x64",
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
    execution_timeout = 14 * time.hour,
    reclient_cache_silo = "Comparison Windows - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_reclient_comparison_builder(
    name = "Comparison Simple Chrome (reclient)",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "chromeos_device",
                "dcheck_off",
                "remoteexec",
                "amd64-generic-vm",
                "ozone_headless",
                "use_fake_dbus_clients",
                "x64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "chromeos_device",
                "dcheck_off",
                "remoteexec",
                "amd64-generic-vm",
                "ozone_headless",
                "use_fake_dbus_clients",
                "x64",
            ],
        ),
    },
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)",
    schedule = "0 */4 * * *",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "debug_static_builder",
                "remoteexec",
                "ios_simulator",
                "x64",
                "xctest",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "debug_static_builder",
                "remoteexec",
                "ios_simulator",
                "x64",
                "xctest",
            ],
        ),
    },
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison ios - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
    xcode = xcode.xcode_default,
)

fyi_reclient_comparison_builder(
    name = "Comparison Android (reclient)(CQ)",
    description_html = """\
This builder measures Android build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "android-arm64-rel-compilator")),
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|cq",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    reclient_cache_silo = "Comparison Android CQ - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison Mac (reclient)(CQ)",
    description_html = """\
This builder measures Mac build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "mac-rel-compilator")),
    schedule = "0 */4 * * *",
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|cq",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac CQ - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = 150,
)

fyi_reclient_comparison_builder(
    name = "Comparison Windows (reclient)(CQ)",
    description_html = """\
This builder measures Windows build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "win-rel-compilator")),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|cq",
        short_name = "re",
    ),
    execution_timeout = 14 * time.hour,
    reclient_cache_silo = "Comparison Windows CQ - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

fyi_reclient_comparison_builder(
    name = "Comparison Simple Chrome (reclient)(CQ)",
    description_html = """\
This builder measures Simple Chrome build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "linux-chromeos-rel-compilator")),
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64|cq",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome CQ - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_enabled = True,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

fyi_mac_reclient_comparison_builder(
    name = "Comparison ios (reclient)(CQ)",
    description_html = """\
This builder measures iOS build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "ios-simulator")),
    schedule = "0 */4 * * *",
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
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = 150,
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
            "remoteexec",
            "linux",
            "x64",
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
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_siso_project = None,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = None,
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
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    shadow_siso_project = None,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = None,
)

ci.builder(
    name = "Win x64 Builder (reclient compare)",
    description_html = "Verifies whether local and remote build artifacts are identical.",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
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
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_siso_project = None,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = None,
)

# TODO(crbug.com/40201781): remove this after the migration.
fyi_mac_builder(
    name = "Mac Builder (reclient compare)",
    description_html = "Verifies whether local and remote build artifacts are identical.",
    schedule = "0 */4 * * *",
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
            "remoteexec",
            "minimal_symbols",
            "x64",
            "mac",
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
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    shadow_siso_project = None,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = None,
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
            "arm64",
            "xctest",
        ],
    ),
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
            "remoteexec",
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
            "remoteexec",
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
            "remoteexec",
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
            "remoteexec",
            "xctest",
        ],
    ),
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "dev",
        ),
    ],
    xcode = xcode.x16_1betabots,
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
            "remoteexec",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS17",
            short_name = "sdk17",
        ),
    ],
    xcode = xcode.x16_1betabots,
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
            "remoteexec",
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
            "remoteexec",
            "ios_simulator",
            "arm64",
            "xctest",
        ],
    ),
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS18",
        short_name = "sdk18",
    ),
    xcode = xcode.x16betabots,
)

fyi_mac_builder(
    name = "Mac Builder Next",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
            ],
        ),
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
            "remoteexec",
            "dcheck_off",
            "shared",
            "mac",
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
            "remoteexec",
            "mac_strip",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
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
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
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
        "{}.".format(linkify("http://shortn/_B7cJcHq55P", "http://shortn/_B7cJcHq55P"))
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
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    notifies = ["Win 10 Fast Ring"],
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win10-wpt-chromium-rel",
    description_html = "Runs {} against Chrome.".format(
        linkify("https://web-platform-tests.org", "web platform tests"),
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
    experimental = True,
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
            "remoteexec",
            "win",
        ],
    ),
    builderless = False,
    cores = "8|16",
    os = os.WINDOWS_DEFAULT,
    cpu = cpu.X86,
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    siso_remote_jobs = 150,
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
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
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
            "remoteexec",
            "win",
            "x64",
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
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)
