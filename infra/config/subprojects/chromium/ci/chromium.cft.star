# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions for the chromium.cft (chrome for testing) builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.cft",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    gardener_rotations = gardener_rotations.CFT,
    tree_closing = False,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

def builder_spec(*, target_platform, build_config, is_arm64 = False, additional_configs = None):
    additional_configs = additional_configs or []
    if is_arm64:
        additional_configs.append("arm64")
    return builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = additional_configs,
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = build_config,
            target_arch = builder_config.target_arch.ARM if is_arm64 else None,
            target_bits = 64,
            target_platform = target_platform,
        ),
    )

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.cft",
)

ci.builder(
    name = "mac-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.MAC,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests_no_nacl",
            "chromium_mac_rel_isolated_scripts",
            "chromium_mac_scripts",
        ],
        # TODO(crbug.com/40883191) - for some reason gcapi_example
        # is failing to build in this config. For now, just try
        # to build the test binaries and we can decide if we really
        # need everything to build later.
        #
        # additional_compile_targets = [
        #     "all",
        # ],
        mixins = [
            "mac_default_x64",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/cft.blink_web_tests.filter",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        "gpu": None,
                    },
                    shards = 12,
                ),
            ),
            "blink_wpt_tests": targets.remove(
                reason = "go/chrome-for-testing-test-strategy",
            ),
            "browser_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/mac.mac-rel-cft.browser_tests.filter",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 20,  # crbug.com/1361887
                ),
            ),
            "check_static_initializers": targets.mixin(
                args = [
                    "--allow-coverage-initializer",
                ],
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/mac.mac-rel-cft.interactive_ui_tests.filter",
                ],
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "mac_signing_tests": targets.remove(
                reason = "Does not exist in the CfT config.",
            ),
            "sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "telemetry_perf_unittests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        short_name = "mac-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
)

ci.builder(
    name = "linux-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.LINUX,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts",
            "chromium_linux_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/cft.blink_web_tests.filter",
                    "--flag-specific=chrome-for-testing",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.remove(
                reason = "go/chrome-for-testing-test-strategy",
            ),
            "browser_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/linux.linux-rel-cft.browser_tests.filter",
                ],
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/linux.linux-rel-cft.interactive_ui_tests.filter",
                ],
            ),
            "not_site_per_process_blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                    "--flag-specific=chrome-for-testing",
                ],
            ),
            "telemetry_perf_unittests": targets.mixin(
                args = [
                    "--xvfb",
                    "--jobs=1",
                ],
            ),
            "webdriver_wpt_tests": targets.remove(
                reason = "https://crbug.com/929689, https://crbug.com/936557",
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        short_name = "linux-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
)

ci.builder(
    name = "win-rel-cft",
    builder_spec = builder_spec(
        build_config = builder_config.build_config.RELEASE,
        target_platform = builder_config.target_platform.WIN,
        additional_configs = [
            # This is necessary due to this builder running the
            # telemetry_perf_unittests suite.
            "chromium_with_telemetry_dependencies",
        ],
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "chrome_for_testing",
            "chrome_with_codecs",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts",
            "chromium_win_scripts",
        ],
        additional_compile_targets = [
            "pdf_fuzzers",
        ],
        mixins = [
            "x86-64",
            "win10",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/cft.blink_web_tests.filter",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "blink_wpt_tests": targets.remove(
                reason = "go/chrome-for-testing-test-strategy",
            ),
            "browser_tests": targets.mixin(
                args = [
                    # crbug.com/868082
                    "--disable-features=WebRTC-H264WithOpenH264FFmpeg",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.win-rel-cft.browser_tests.filter",
                ],
                swarming = targets.swarming(
                    # This is for slow test execution that often becomes a critical path of
                    # swarming jobs. crbug.com/868114
                    shards = 15,
                ),
            ),
            "browser_tests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "chromedriver_py_tests": targets.mixin(
                # TODO(crbug.com/40868908): Fix & re-enable.
                isolate_profile_data = False,
            ),
            "components_browsertests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "content_browsertests": targets.mixin(
                # crbug.com/868082
                args = [
                    "--disable-features=WebRTC-H264WithOpenH264FFmpeg",
                ],
            ),
            "content_unittests": targets.mixin(
                # crbug.com/337984655
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.win-rel-cft.content_unittests.filter",
                ],
            ),
            "interactive_ui_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.win-rel-cft.interactive_ui_tests.filter",
                ],
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "interactive_ui_tests_no_field_trial": targets.remove(
                reason = "crbug.com/40630866",
            ),
            "pixel_browser_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "pixel_interactive_ui_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
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
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        short_name = "win-rel-cft",
    ),
    contact_team_email = "browser-automation-staff@google.com",
    execution_timeout = 6 * time.hour,
)
