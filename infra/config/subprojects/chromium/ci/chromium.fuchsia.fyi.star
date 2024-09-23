# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuchsia.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "free_space", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuchsia.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.FUCHSIA,
    execution_timeout = 10 * time.hour,
    health_spec = health_spec.DEFAULT,
    notifies = ["cr-fuchsia"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

targets.settings_defaults.set(
    browser_config = targets.browser_config.WEB_ENGINE_SHELL,
    os_type = targets.os_type.FUCHSIA,
)

ci.builder(
    name = "fuchsia-fyi-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
                "fuchsia_arm64_host",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "fuchsia_smart_display",
            "arm64_host",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "fuchsia_standard_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "arm64",
            "docker",
            "linux-jammy-or-focal",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.remove(
                reason = "No arm64 apache suppport for fuchsia arm64 bots yet",
            ),
            "blink_wpt_tests": targets.remove(
                reason = "No arm64 apache suppport for fuchsia arm64 bots yet",
            ),
            "cc_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.cc_unittests.filter",
                ],
            ),
            "chrome_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "compositor_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.compositor_unittests.filter",
                ],
            ),
            "context_lost_validating_tests": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "expected_color_pixel_validating_test": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "gpu_process_launch_tests": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "hardware_accelerated_feature_tests": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "media_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.media_unittests.filter",
                ],
            ),
            "pixel_skia_gold_validating_test": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "screenshot_sync_validating_tests": targets.remove(
                reason = "crbug.com/42050042, crbug.com/42050537 this test does not work on swiftshader on arm64",
            ),
            "snapshot_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.snapshot_unittests.filter",
                ],
            ),
            "views_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.views_unittests.filter",
                ],
            ),
            "viz_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.debug.viz_unittests.filter",
                ],
            ),
        },
    ),
    free_space = free_space.high,
    console_view_entry = [
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|fuchsia ci|arm64",
            short_name = "dbg",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
)

ci.builder(
    name = "fuchsia-fyi-x64-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-fyi-archive",
        # This builder is slow naturally, running everything in serial to avoid
        # using too much resource.
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "fuchsia",
            "asan",
            "lsan",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = "fuchsia_standard_tests",
        mixins = [
            "linux-jammy",
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "kvm": "1",
                    },
                ),
            ),
        ],
        per_test_modifications = {
            # anglebug.com/6894
            "angle_unittests": targets.mixin(
                args = [
                    "--gtest_filter=-ConstructCompilerTest.DefaultParameters",
                ],
            ),
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.lsan.base_unittests.filter",
                ],
            ),
            "chrome_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "content_browsertests": targets.remove(
                reason = "TODO(crbug.com/40241445): Enable on Fuchsia asan/clang builders",
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.lsan.content_unittests.filter",
                ],
            ),
            "gin_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.lsan.gin_unittests.filter",
                ],
            ),
            "perfetto_unittests": targets.mixin(
                # TODO(crbug.com/40761189): Error messages only show up in klog.
                args = [
                    "--gtest_filter=-PagedMemoryTest.AccessUncommittedMemoryTriggersASAN",
                ],
            ),
            "services_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.lsan.services_unittests.filter",
                ],
            ),
        },
    ),
    console_view_entry = [
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|fuchsia ci|x64",
            short_name = "asan",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg-persistent-emulator",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "fuchsia_smart_display",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = "fuchsia_facility_gtests",
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "fuchsia-persistent-emulator",
            "linux-focal",
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "kvm": "1",
                        "pool": "chromium.tests.fuchsia",
                    },
                ),
            ),
        ],
    ),
    free_space = free_space.high,
    console_view_entry = [
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "x64-llemu",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
)
