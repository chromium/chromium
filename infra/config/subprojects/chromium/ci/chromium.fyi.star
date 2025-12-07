# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fyi builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    priority = ci_constants.DEFAULT_FYI_PRIORITY,
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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
            "compositor",
        ],
        "code_coverage": consoles.ordering(
            short_names = [
                "and",
                "ann",
                "lnx",
                "lcr",
                "jcr",
                "mac",
            ],
        ),
        "mac": consoles.ordering(short_names = [
            "bld",
            "15",
            "herm",
        ]),
        "deterministic|mac": consoles.ordering(short_names = [
            "rel",
            "dbg",
        ]),
        "iOS|iOS13": consoles.ordering(short_names = [
            "dev",
            "sim",
        ]),
        "linux|blink": consoles.ordering(short_names = ["TD"]),
    },
)

def fyi_ios_builder(*, name, **kwargs):
    kwargs.setdefault("cores", None)
    if kwargs.get("builderless", False):
        kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("xcode", xcode.xcode_default)
    return ci.builder(name = name, **kwargs)

def mac_builder_defaults(**kwargs):
    kwargs.setdefault("cores", 4)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    return kwargs

def fyi_mac_builder(*, name, **kwargs):
    return ci.builder(name = name, **mac_builder_defaults(**kwargs))

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
    targets = targets.bundle(
        targets = [
            "linux_viz_gtests",
            "vulkan_swiftshader_isolated_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
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
            config = "main_builder",
            apply_configs = ["mb"],
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
            "arm64",
            "strip_debug_info",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "site_isolation_android_fyi_gtests",
        ],
        additional_compile_targets = [
            "content_browsertests",
            "content_unittests",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "os": "Android",
                    },
                ),
            ),
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
)

ci.builder(
    name = "linux-trees-in-viz-rel",
    description_html = "Runs tests with TreesInViz feature enabled",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
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
            "release_builder",
            "remoteexec",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "trees_in_viz_fyi_gtests",
            "trees_in_viz_fyi_blink_web_tests",
        ],
        mixins = [
            "linux-jammy",
            "x86-64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "compositor",
    ),
    contact_team_email = "chrome-compositor@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "test_traffic_annotation_auditor_script",
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
    targets = targets.bundle(
        targets = [
            "test_traffic_annotation_auditor_script",
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
    name = "chromeos-structured-test-ids-amd64-generic-rel-fyi",
    description_html = ("This is a builder for Ash chrome that runs " +
                        "with an experiment for structured test ids."),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                # This is necessary due to a child builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "shared_build_dir",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    gn_args = gn_args.config(
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
    targets = targets.bundle(
        targets = [
            "gpu_chromeos_telemetry_tests",
            "chromeos_vm_gtests",
            "chromeos_isolated_scripts",
            "chromeos_vm_tast",
        ],
        mixins = [
            "chromeos-generic-vm",
        ],
        per_test_modifications = {
            "chrome_all_tast_tests": targets.mixin(
                args = [
                    "--tast-shard-method=hash",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.CROS_CHROME,
        os_type = targets.os_type.CROS,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "compile_RDB",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

fyi_ios_builder(
    name = "ios-structured-test-ids-simulator-fyi",
    description_html = "iOS builder for running tests with an experiment for" +
                       " structured test ids.",
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
    targets = targets.bundle(
        targets = [
            "ios_simulator_tests",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "mac_default_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_16_main",
            "xctest",
        ],
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "sim_RDB",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-browser-infra-team@google.com",
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
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
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-driver-flag=--force-browsing-instance-reset-between-tests",
                ],
                # The flag above will slow the tests down, and we don't want
                # the bot to timeout, so set a higher timeout here.
                # TODO(wjmaclean): It would be nice if we could somehow specify
                # a delta to the default/inherited timeout instead of an
                # absolute.
                swarming = targets.swarming(
                    hard_timeout_sec = 1500,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--additional-driver-flag=--force-browsing-instance-reset-between-tests",
                ],
            ),
        },
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
    targets = targets.bundle(
        targets = [
            "chromium_gtests",
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
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
    targets = targets.bundle(
        targets = [
            "fieldtrial_browser_tests",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.thin_tester(
    name = "mac-fieldtrial-tester",
    parent = "ci/mac-arm64-rel",
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
            # TODO(crbug.com/372265654): Revert to use fieldtrial_browser_tests
            # after project work is complete.
            "fieldtrial_browser_tests_mac",
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
    targets = targets.bundle(
        targets = [
            "chromium_multiscreen_gtests_fyi",
        ],
        mixins = [
            "linux-noble",
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
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
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
            "webview_shell",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_fieldtrial_rel_webview_tests",
        ],
        mixins = [
            "12-x64-emulator",
            "has_native_resultdb_integration",
            "finch-chromium-swarming-pool",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
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
    targets = targets.bundle(
        targets = [
            "fieldtrial_ios_simulator_tests",
        ],
        mixins = [
            "finch-chromium-swarming-pool",
            "has_native_resultdb_integration",
            "mac_default_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
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
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "android_builder",
            "android_with_static_analysis",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "perfetto_gtests_android",
        ],
        additional_compile_targets = [
            "chrome_public_apk",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "perfetto_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
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
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "perfetto_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_10.15",
        ],
    ),
    builderless = True,
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

fyi_ios_builder(
    name = "mac-vm",
    description_html = "Mac builder for running testing targets on Mac Virtual Machines",
    # every 4 hours beginning at 2am, deliberately offsetting from ios-vm
    schedule = "0 2-23/4 * * *",
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
            "release_builder",
            "remoteexec",
            "mac",
            "arm64",
            "minimal_symbols",
            "gpu_tests",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "mac_vm_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_vm",
        ],
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "macvm",
        short_name = "mac",
    ),
    contact_team_email = "bling-engprod@google.com",
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
    targets = targets.bundle(
        targets = [
            "headless_shell_wpt_tests_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "mac_13_x64",
        ],
        per_test_modifications = {
            "headless_shell_wpt_tests_inverted": targets.mixin(
                args = [
                    "--test-type",
                    "testharness",
                    "reftest",
                    "crashtest",
                    "print-reftest",
                    "--exit-after-n-crashes-or-timeouts=1000",
                    "--exit-after-n-failures=1000",
                ],
                experiment_percentage = 100,
            ),
        },
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
    targets = targets.bundle(
        targets = [
            "headless_shell_wpt_tests_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "linux-jammy",
        ],
        per_test_modifications = {
            "headless_shell_wpt_tests_inverted": targets.mixin(
                args = [
                    "--test-type",
                    "testharness",
                    "reftest",
                    "crashtest",
                    "print-reftest",
                    "--exit-after-n-crashes-or-timeouts=1000",
                    "--exit-after-n-failures=1000",
                ],
                experiment_percentage = 100,
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
    experimental = True,
)

ci.builder(
    name = "linux-blink-tracing-rel",
    description_html = "Runs {} with <code>blink*</code> traces added to test results.".format(
        linkify("https://web-platform-tests.org/", "web platform tests"),
    ),
    schedule = "with 24h interval",
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
            "linux",
            "x64",
            # Instrument binaries with `blink.bindings` trace events.
            "extended_tracing",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "headless_shell_wpt_tests_tracing",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "headless_shell_wpt_tests_tracing": targets.mixin(
                # This builder isn't latency-sensitive, so use fewer shards than
                # the base test suite.
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "trace",
    ),
    contact_team_email = "chrome-product-engprod@google.com",
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
    targets = targets.bundle(
        targets = [
            "wpt_tests_ios_suite",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "ioswpt-chromium-swarming-pool",
            "mac_15_x64",
            "mac_toolchain",
            "xcode_26_main",
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
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests_no_nacl",
            "chromium_mac_osxbeta_rel_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_beta_x64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 35,
                ),
            ),
            # TODO(crbug.com/40794640): dyld was rebuilt for macOS 12, which
            # breaks the tests. Run this experimentally on all the macOS bots
            # >= 12 and remove this exception once fixed.
            "crashpad_tests": targets.mixin(
                experiment_percentage = 100,
            ),
            # TODO (crbug.com/1278617) Re-enable once fixed
            "interactive_ui_tests": targets.mixin(
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 7,
                ),
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
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
    targets = targets.bundle(
        targets = [
            "headless_browser_gtests",
        ],
        mixins = [
            "linux-jammy",
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
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.no-external-ip",
                    },
                    expiration_sec = 43200,
                ),
            ),
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "telemetry_perf_unittests": targets.mixin(
                args = [
                    "--xvfb",
                    "--jobs=1",
                ],
            ),
        },
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
    targets = targets.bundle(
        targets = [
            "perfetto_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
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
    targets = targets.bundle(
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "pool": "chromium.tests.no-external-ip",
                    },
                    expiration_sec = 43200,
                ),
            ),
            "x86-64",
            "win10",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
            "browser_tests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "components_browsertests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "interactive_ui_tests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "sync_integration_tests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "crbug.com/40622135",
            ),
            "telemetry_unittests": targets.remove(
                reason = "crbug.com/40622135",
            ),
        },
    ),
    builderless = False,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
)

# Temporary builder for testing changes to resultdb with structured test
# id uploads.
ci.builder(
    name = "linux-structured-test-ids-rel-fyi",
    description_html = (
        "Run tests for checking changes to resultdb structured " +
        "test id uploads."
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to child builders running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
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
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "devtools_do_typecheck",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts_once",
            "chromium_linux_scripts",
            "gtests_once",
            "variations_smoke_tests",  # single module scheme
            "mojo_python_unittests",  # pyunit scheme
            "grit_python_unittests",  # pyunit scheme
            "webgpu_cts_structured_test_id_dedicated_worker_tests",  # webgpucts scheme
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
            ),
            "browser_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
            ),
            "not_site_per_process_blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
            ),
            "telemetry_perf_unittests": targets.mixin(
                args = [
                    "--xvfb",
                    "--jobs=1",
                ],
            ),
            "unit_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
            ),
            "webdriver_wpt_tests": targets.mixin(
                ci_only = True,
            ),
            "webgpu_cts_structured_test_id_dedicated_worker_tests": [
                "linux_nvidia_gtx_1660_stable",
            ],
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "lnx_RDB",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
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
    targets = targets.bundle(
        targets = [
            "upload_perfetto",
        ],
        additional_compile_targets = [
            "trace_processor_shell",
        ],
    ),
    targets_settings = targets.settings(
        use_swarming = False,
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
    targets = targets.bundle(
        targets = [
            "upload_perfetto",
        ],
        additional_compile_targets = [
            "trace_processor_shell",
        ],
    ),
    targets_settings = targets.settings(
        use_swarming = False,
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

fyi_ios_builder(
    name = "ios-blink-rel-fyi",
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
            host_platform = builder_config.host_platform.MAC,
        ),
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
    targets = targets.bundle(
        targets = [
            "ios_blink_rel_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "mac_beta_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
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
)

fyi_ios_builder(
    name = "tvos-rel-fyi",
    description_html = "tvOS builder for building and testing tvOS chromium.",
    schedule = "0 */1 * * *",  # every hour
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tvos",
            apply_configs = [
                "mb",
            ],
            # Release for now due to binary size being too large (crbug.com/1464415)
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
            host_platform = builder_config.host_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "try_builder",
            "remoteexec",
            "ios_simulator",
            "arm64",
            "use_blink",
            "tvos_platform",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "tvos_rel_tests",
        ],
        additional_compile_targets = [
            "base_unittests",
            "content_shell",
            "components_browsertests",
            "components_unittests",
            "content_unittests",
            "media_unittests",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "mac_default_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    builderless = True,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "tv",
    ),
    contact_team_email = "cobalt-appletv@google.com",
    execution_timeout = 3 * time.hour,
    xcode = xcode.xcode_default,
)

fyi_ios_builder(
    name = "ios-vm",
    description_html = "iOS builder for running testing targets on Mac Virtual Machines",
    schedule = "0 */4 * * *",  # every 4 hours
    triggered_by = [],
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
            "arm64",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios_vm_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "mac_vm",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "macvm",
        short_name = "ios",
    ),
    contact_team_email = "bling-engprod@google.com",
    xcode = xcode.xcode_default,
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
    targets = targets.bundle(
        targets = [
            "ios_webkit_tot_tests",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_custom_webkit",
            "mac_default_x64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "wk",
    ),
    xcode = xcode.x14wk,
)

fyi_ios_builder(
    name = "ios18-sdk-device",
    description_html = (
        "Validates that Chromium on iOS compiles for device using the latest iOS SDK." +
        "Particularly useful during WWDC season when new beta SDKs are being frequently" +
        "released."
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
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
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS18",
            short_name = "dev",
        ),
    ],
    contact_team_email = "bling-engprod@google.com",
    xcode = xcode.x16betabots,
)

fyi_ios_builder(
    name = "ios26-sdk-simulator",
    schedule = "0 1,5,9,13,17,21 * * *",
    triggered_by = [],
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
            "arm64",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios26_sdk_simulator_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_beta_test_pool",
            "mac_15_beta_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_beta",
            "xctest",
        ],
    ),
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS26",
            short_name = "sdk26",
        ),
    ],
    xcode = xcode.x26betabots,
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
    # ios18-beta-sim compiles with xcode version n-1, but
    # runs testers with xcode n during an xcode roll.
    targets = targets.bundle(
        targets = [
            "ios18_beta_simulator_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_beta_test_pool",
            "mac_15_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_beta",
            "xctest",
        ],
    ),
    # TODO(crbug.com/393136335): changing to MAC_BETA to validate Mac-15 prior
    # to upgrading the rest of the waterfall. Reset to MAC_DEFAULT once the
    # rest of the waterfall is Mac-15.
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS18",
        short_name = "ios18",
    ),
)

fyi_ios_builder(
    name = "ios26-beta-simulator",
    schedule = "0 3,7,11,15,19,23 * * *",
    triggered_by = [],
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
            "arm64",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios26_beta_simulator_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_beta_test_pool",
            "mac_15_beta_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_beta",
            "xctest",
        ],
    ),
    # TODO(crbug.com/393136335): changing to MAC_BETA to validate Mac-15 prior
    # to upgrading the rest of the waterfall. Reset to MAC_DEFAULT once the
    # rest of the waterfall is Mac-15.
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS26",
            short_name = "ios26",
        ),
    ],
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
    # ios18-sdk-sim compiles with xcode version n, and runs
    # testers with xcode n during an xcode roll.
    targets = targets.bundle(
        targets = [
            "ios18_sdk_simulator_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_beta_test_pool",
            "mac_15_beta_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_beta",
            "xctest",
        ],
    ),
    os = os.MAC_BETA,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS18",
        short_name = "sdk18",
    ),
    xcode = xcode.x26betabots,
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
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests_no_nacl",
            "chromium_mac_osxbeta_rel_isolated_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_26_arm64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 25,  # crbug.com/1419045
                ),
            ),
            # TODO(crbug.com/40794640): dyld was rebuilt for macOS 12, which
            # breaks the tests. Run this experimentally on all the macOS bots
            # >= 12 and remove this exception once fixed.
            "crashpad_tests": targets.mixin(
                experiment_percentage = 100,
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 7,
                ),
            ),
        },
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
    targets = targets.bundle(
        targets = [
            "chromium_win_gtests",
        ],
    ),
    targets_settings = targets.settings(
        use_swarming = False,
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
    targets = targets.bundle(
        targets = [
            "headless_shell_wpt_tests_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "x86-64",
            "win10",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "headless_shell_wpt_tests_inverted": targets.mixin(
                args = [
                    "--test-type",
                    "testharness",
                    "reftest",
                    "crashtest",
                    "print-reftest",
                    "--exit-after-n-crashes-or-timeouts=1000",
                    "--exit-after-n-failures=1000",
                ],
                experiment_percentage = 100,
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.WINDOWS,
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
    targets = targets.bundle(
        targets = [
            "fieldtrial_browser_tests",
        ],
        mixins = [
            "win10",
            "finch-chromium-swarming-pool",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "test_traffic_annotation_auditor_script",
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

ci.builder(
    name = "win-no-safe-browsing-rel",
    description_html = "Builds for Windows with `safe_browsing_mode = 0`.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "win",
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
            "no_safe_browsing",
            "release_builder",
            "remoteexec",
            "x86",
            "no_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "browser_tests",
            "components_unittests",
            "unit_tests",
        ],
        additional_compile_targets = [
            "chrome",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "nosb",
    ),
    contact_team_email = "chrome-counter-abuse-core@google.com",
)
