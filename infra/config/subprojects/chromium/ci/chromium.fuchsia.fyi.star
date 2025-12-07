# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuchsia.fyi builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "free_space", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuchsia.fyi",
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.FUCHSIA,
    execution_timeout = 10 * time.hour,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    notifies = ["cr-fuchsia"],
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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
            "linux-jammy",
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
            category = "fuchsia ci|arm64",
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
            "dcheck_always_on",
        ],
    ),
    targets = targets.bundle(
        # Passthrough is used since these emulators use SwiftShader, which
        # forces use of the passthrough decoder even if validating is specified.
        targets = "fuchsia_standard_passthrough_tests",
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
            category = "fuchsia ci|x64",
            short_name = "asan",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 24 * time.hour,
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
            "linux-jammy",
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

# TODO(fxbug.dev/370067428): Remove once Netstack2 no longer exists.
ci.builder(
    name = "fuchsia-netstack2-x64-cast-receiver-rel",
    description_html = "x64 release build of fuchsia components using Netstack2 with cast receiver",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_netstack2_x64",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "fuchsia",
            "cast_receiver_size_optimized",
            "x64",
            "dcheck_always_on",
        ],
    ),
    # Do not forget to update
    # infra/config/subprojects/chromium/ci/chromium.clang.star when adding or
    # removing targets.
    targets = targets.bundle(
        targets = [
            # Passthrough is used since these emulators use SwiftShader, which
            # forces use of the passthrough decoder even if validating is
            # specified.
            "fuchsia_standard_passthrough_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "fuchsia-large-device-spec",
            "isolate_profile_data",
            "linux-jammy",
            "fuchsia-netstack2-x64",
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "kvm": "1",
                    },
                ),
            ),
        ],
        per_test_modifications = {
            "blink_web_tests": [
                targets.mixin(
                    swarming = targets.swarming(
                        shards = 1,
                    ),
                ),
            ],
            "blink_wpt_tests": [
                targets.mixin(
                    swarming = targets.swarming(
                        shards = 1,
                    ),
                ),
            ],
            "chrome_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
        },
    ),
    console_view_entry = [
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci|x64",
            short_name = "ns2",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
)

ci.builder(
    name = "fuchsia-x64-perf-cast-receiver-rel",
    description_html = "x64 perf preferred release build of fuchsia components with cast receiver",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "fuchsia",
            "cast_receiver_perf_optimized",
            "x64",
            "dcheck_always_on",
        ],
    ),
    # Do not forget to update
    # infra/config/subprojects/chromium/ci/chromium.clang.star when adding or
    # removing targets.
    targets = targets.bundle(
        targets = [
            # Passthrough is used since these emulators use SwiftShader, which
            # forces use of the passthrough decoder even if validating is
            # specified.
            "fuchsia_standard_passthrough_tests",
        ],
        additional_compile_targets = [
            "all",
            "cast_test_lists",
        ],
        mixins = [
            "fuchsia-large-device-spec",
            "isolate_profile_data",
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
            "blink_web_tests": [
                targets.mixin(
                    swarming = targets.swarming(
                        shards = 1,
                    ),
                ),
            ],
            "blink_wpt_tests": [
                targets.mixin(
                    swarming = targets.swarming(
                        shards = 1,
                    ),
                ),
            ],
            "chrome_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
        },
    ),
    console_view_entry = [
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci|x64",
            short_name = "perf",
        ),
    ],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
)
