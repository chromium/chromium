# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
        # Shards of browser_tests and interactive_ui_tests are fundamentally
        # flaky on various sanitizer builds, and end up timing out without any
        # results. Such shards are considered "invalid". crbug.com/429435587 is
        # on file to address the fundamental flakiness, but a proper fix is
        # not likely. So just retry all such invalid shards on all memory
        # builders.
        retry_invalid_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    tree_closing_notifiers = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS,
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.memory",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ordering = {
        None: ["win", "mac", "linux", "cros"],
        "*build-or-test*": consoles.ordering(short_names = ["bld", "tst"]),
        "linux|TSan v2": "*build-or-test*",
        "linux|asan lsan": "*build-or-test*",
        "linux|webkit": consoles.ordering(short_names = ["asn", "msn"]),
    },
)

# TODO(gbeaty) Find a way to switch testers to use ci.thin_tester while ensuring
# that the builders and testers targeting linux set the necessary notifies

def linux_memory_builder(*, name, **kwargs):
    kwargs["notifies"] = kwargs.get("notifies", []) + ["linux-memory"]
    return ci.builder(name = name, **kwargs)

linux_memory_builder(
    name = "Linux ASan LSan Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "lsan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "fail_on_san_warnings",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    # crbug.com/372503456: This builder gets died on a 8 cores bot when it fails
    # to get builder cache.
    cores = 16,
    ssd = True,
    # TODO(crbug.com/388307198): Re-enable tree closing when the bot is stable.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
)

linux_memory_builder(
    name = "Linux ASan LSan Tests (1)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    parent = "ci/Linux ASan LSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "lsan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_and_gl_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "linux-jammy",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "accessibility_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40266898): Re-enable.
                    "--gtest_filter=-AXPlatformNodeAuraLinuxTest.AtkComponentScrollTo:AtkUtilAuraLinuxTest.*",
                ],
            ),
            "blink_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "browser_tests": targets.mixin(
                ci_only = True,
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 70,
                ),
            ),
            "components_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 24,
                ),
            ),
            "content_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "gin_unittests": targets.remove(
                reason = "https://crbug.com/831667",
            ),
            "gl_tests_passthrough": [
                targets.mixin(
                    # TODO(kbr): figure out a better way to specify blocks of
                    # arguments like this for tests on multiple machines.
                    args = [
                        "--use-gpu-in-tests",
                        "--no-xvfb",
                    ],
                ),
                "linux_nvidia_gtx_1660_stable",
            ],
            "interactive_ui_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
                # These are slow on the ASan trybot for some reason, crbug.com/1257927
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "net_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "unit_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
                # These are slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_project = None,
)

linux_memory_builder(
    name = "Linux TSan Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tsan2",
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
            "tsan",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
)

linux_memory_builder(
    name = "Linux CFI",
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
            "cfi_full",
            "cfi_icall",
            "cfi_diag",
            "thin_lto",
            "release",
            "static",
            "dcheck_always_on",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_and_gl_gtests",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    # https://crbug.com/1361973
                    shards = 30,
                ),
            ),
            "crashpad_tests": targets.remove(
                reason = "https://crbug.com/crashpad/306",
            ),
            "gl_tests_passthrough": [
                targets.mixin(
                    args = [
                        "--use-gpu-in-tests",
                        "--no-xvfb",
                    ],
                ),
                "linux_nvidia_gtx_1660_stable",
            ],
            "interactive_ui_tests": targets.mixin(
                # Slow on certain debug builders, see crbug.com/1513713.
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
        },
    ),
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 5 * time.hour,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "lsan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "chromeos",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x64",
        ],
    ),
    cores = 16,
    ssd = True,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "bld",
    ),
    # TODO(crbug.com/40661942): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
)

linux_memory_builder(
    name = "Linux Chromium OS ASan LSan Tests (1)",
    parent = "Linux Chromium OS ASan LSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "lsan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "linux_chromeos_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "x86-64",
            "linux-jammy",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                # And even more slow on linux-chromeos: crbug.com/1491533.
                swarming = targets.swarming(
                    hard_timeout_sec = 7200,
                    shards = 140,
                ),
            ),
            "components_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    # https://crbug.com/1471857, crbug.com/409823026
                    shards = 28,
                ),
            ),
            "gin_unittests": targets.remove(
                reason = "https://crbug.com/831667",
            ),
            "interactive_ui_tests": targets.mixin(
                # These are slow on the ASan trybot for some reason, crbug.com/1257927
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
            "net_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "unit_tests": targets.mixin(
                # These are slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros|asan",
        short_name = "tst",
    ),
    siso_project = None,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
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
            "chromeos",
            "msan",
            "release_builder",
            "remoteexec",
            "x64",
        ],
    ),
    cores = None,
    ssd = True,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "bld",
    ),
    execution_timeout = 4 * time.hour,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Tests",
    parent = "Linux ChromiumOS MSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    targets = targets.bundle(
        # TODO(crbug.com/40126889): Use the main 'linux_chromeos_gtests' suite
        # when OOBE tests no longer fail on MSAN.
        targets = [
            "linux_chromeos_gtests_oobe",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.msan.browser_tests.oobe_negative.filter",
                ],
                # These are very slow on the Chrome OS MSAN trybot, most likely because browser_tests on cros has ~40% more tests. Also, these tests
                # run on ash, which means every test starts and shuts down ash, which most likely explains why it takes longer than on other platforms.
                # crbug.com/40585695 and crbug.com/326621525
                swarming = targets.swarming(
                    shards = 100,
                ),
            ),
            "content_unittests": targets.mixin(
                # These are very slow on the Chrome OS MSAN trybot for some reason.
                # crbug.com/865455
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "gl_unittests_ozone": targets.remove(
                reason = "Can't run on MSAN because gl_unittests_ozone uses the hardware driver, which isn't instrumented.",
            ),
            "interactive_ui_tests": targets.mixin(
                # These are very slow on the Chrome OS MSAN trybot for some reason.
                # crbug.com/865455
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "net_unittests": targets.mixin(
                # These are very slow on the Chrome OS MSAN trybot for some reason.
                # crbug.com/865455
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "services_unittests": targets.remove(
                reason = "https://crbug.com/831676",
            ),
            "unit_tests": targets.mixin(
                # These are very slow on the Chrome OS MSAN trybot for some reason.
                # crbug.com/865455
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "tst",
    ),
    execution_timeout = 4 * time.hour,
    siso_project = None,
)

linux_memory_builder(
    name = "Linux MSan Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
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
            "msan",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    # Requires dedicated extra memory builder (crbug.com/352281723).
    builderless = False,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "bld",
    ),
)

linux_memory_builder(
    name = "Linux MSan Tests",
    parent = "Linux MSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_and_gl_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 40,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    # https://crbug.com/1497706
                    shards = 10,
                ),
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "Can't run on MSAN because gl_tests uses the hardware driver, which isn't instrumented.",
            ),
            "gl_unittests": targets.remove(
                reason = "Can't run on MSAN because gl_unittests uses the hardware driver, which isn't instrumented.",
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
            "services_unittests": targets.remove(
                reason = "https://crbug.com/831676",
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|msan",
        short_name = "tst",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Mac ASan 64 Builder",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "minimal_symbols",
            "release_builder",
            "remoteexec",
            "dcheck_always_on",
            "mac",
            "x64",
        ],
    ),
    builderless = False,
    cores = None,  # Swapping between 8 and 24
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
)

linux_memory_builder(
    name = "Linux TSan Tests",
    branch_selector = branches.selector.LINUX_BRANCHES,
    parent = "ci/Linux TSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_tsan2",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_and_gl_and_vulkan_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "linux-jammy",
            "retry_only_failed_tests",
        ],
        per_test_modifications = {
            "blink_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "browser_tests": targets.remove(
                reason = "https://crbug.com/368525",
            ),
            "cc_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "components_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "content_browsertests": targets.mixin(
                # https://crbug.com/1498240
                ci_only = True,
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "crashpad_tests": targets.remove(
                reason = "https://crbug.com/crashpad/304",
            ),
            "gl_tests_passthrough": [
                "gpu-swarming-pool",
                "no_gpu",
                "linux-jammy",
                "x86-64",
                targets.mixin(
                    args = [
                        "--test-launcher-filter-file=../../testing/buildbot/filters/linux.swiftshader.tsan.gl_tests_passthrough.filter",
                    ],
                ),
            ],
            "interactive_ui_tests": targets.mixin(
                # https://crbug.com/1498240
                ci_only = True,
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
                # These are slow on the TSan bots for some reason, crbug.com/1257927
                swarming = targets.swarming(
                    # Adjusted for testing, see https://crbug.com/1179567
                    shards = 32,
                ),
            ),
            "net_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                # https://crbug.com/1498240
                ci_only = True,
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

linux_memory_builder(
    name = "Linux UBSan Builder",
    description_html = "Compiles a linux build with ubsan.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
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
            "ubsan_no_recover",
            "fail_on_san_warnings",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "bld",
    ),
)

linux_memory_builder(
    name = "Linux UBSan Tests",
    description_html = "Runs tests against a linux ubsan build.",
    parent = "ci/Linux UBSan Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
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
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|ubsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Mac ASan 64 Tests (1)",
    parent = "Mac ASan 64 Builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_asan",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "mac_default_x64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                # crbug.com/1196416
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/mac.mac-rel.browser_tests.filter",
                ],
                # https://crbug.com/1251657
                experiment_percentage = 100,
                swarming = targets.swarming(
                    shards = 30,
                ),
            ),
            "content_browsertests": targets.mixin(
                # https://crbug.com/1200640
                experiment_percentage = 100,
            ),
            "interactive_ui_tests": targets.mixin(
                # https://crbug.com/1251656
                experiment_percentage = 100,
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
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
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "tst",
    ),
    siso_project = None,
)

_WEB_TESTS_LINK = "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md"

ci.builder(
    name = "linux-blink-asan-rel",
    description_html = "Runs {} with address-sanitized binaries.".format(
        linkify(
            _WEB_TESTS_LINK,
            "web (platform) tests",
        ),
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "asan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "lsan",
            "release_builder_blink",
            "remoteexec",
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
            "chrome_wpt_tests": targets.mixin(
                args = [
                    "-j6",
                ],
            ),
            "blink_web_tests": targets.mixin(
                args = [
                    "--timeout-ms",
                    "48000",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--timeout-ms",
                    "48000",
                ],
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "headless_shell_wpt_tests": targets.mixin(
                args = [
                    "-j6",
                ],
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "asn",
    ),
)

ci.builder(
    name = "linux-blink-leak-rel",
    description_html = "Runs {} with {} enabled.".format(
        linkify(
            _WEB_TESTS_LINK,
            "web (platform) tests",
        ),
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/controller/blink_leak_detector.h",
            "DOM leak detection",
        ),
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
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
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        additional_compile_targets = [
            "blink_tests",
        ],
        mixins = [
            "linux-jammy",
            "web-test-leak",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--timeout-ms",
                    "48000",
                ],
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--timeout-ms",
                    "48000",
                ],
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            # TODO(crbug.com/422245278): Reenable print-reftests once the bug is
            # fixed.
            "headless_shell_wpt_tests": targets.per_test_modification(
                replacements = targets.replacements(
                    args = {
                        "print-reftest": None,
                    },
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "lk",
    ),
)

ci.builder(
    name = "linux-blink-msan-rel",
    description_html = "Runs {} with memory-sanitized binaries.".format(
        linkify(
            _WEB_TESTS_LINK,
            "web (platform) tests",
        ),
    ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "asan",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "msan",
            "release_builder_blink",
            "remoteexec",
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
                    "--timeout-ms",
                    "66000",
                ],
                swarming = targets.swarming(
                    expiration_sec = 36000,
                    hard_timeout_sec = 10800,
                    io_timeout_sec = 3600,
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--timeout-ms",
                    "66000",
                ],
                swarming = targets.swarming(
                    expiration_sec = 36000,
                    hard_timeout_sec = 10800,
                    io_timeout_sec = 3600,
                    shards = 12,
                ),
            ),
            "headless_shell_wpt_tests": targets.mixin(
                args = [
                    "-j6",
                ],
                swarming = targets.swarming(
                    expiration_sec = 36000,
                    hard_timeout_sec = 10800,
                    io_timeout_sec = 3600,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "msn",
    ),
)

ci.builder(
    name = "win-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan",
            apply_configs = [
                # TODO(https://crbug.com/440203328): cache is causing build
                # failures.
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "fuzzer",
            "static",
            "v8_heap",
            "minimal_symbols",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_gtests",
        ],
        mixins = [
            "win10",
        ],
        per_test_modifications = {
            "blink_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "browser_tests": targets.mixin(
                # Tests shows tests run faster with fewer retries by using fewer jobs crbug.com/1411912
                args = [
                    "--test-launcher-jobs=3",
                ],
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 80,
                ),
            ),
            "components_unittests": targets.mixin(
                # TODO(crbug.com/41491387): With a single shard seems to hit time limit.
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "content_browsertests": targets.mixin(
                # Tests shows tests run faster with fewer retries by using fewer jobs crbug.com/1411912
                args = [
                    "--test-launcher-jobs=3",
                ],
                swarming = targets.swarming(
                    # ASAN bot is slow: https://crbug.com/1484550#c3
                    shards = 32,
                ),
            ),
            "gcp_unittests": targets.mixin(
                # Flakily times out with only a single shard due to slow runtime.
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                # Tests shows tests run faster with fewer retries by using fewer jobs crbug.com/1411912
                args = [
                    "--test-launcher-jobs=3",
                ],
                swarming = targets.swarming(
                    shards = 18,
                ),
            ),
            "net_unittests": targets.mixin(
                # TODO(crbug.com/40200867): net_unittests is slow under ASan.
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                # Tests shows tests run faster with fewer retries by using fewer jobs crbug.com/1411912
                args = [
                    "--test-launcher-jobs=3",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    # ASAN bot is slow: https://crbug.com/1484550#c4
                    shards = 4,
                ),
            ),
            "updater_tests_system": targets.mixin(
                # crbug.com/369478225: These are slow and could timeout on the ASAN
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    builderless = True,
    cores = 16,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "asn",
    ),
    # This builder is normally using 2.5 hours to run with a cached builder. And
    # 1.5 hours additional setup time without cache, https://crbug.com/1311134.
    execution_timeout = 5 * time.hour,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "ios-asan",
    description_html = (
        "Builds the open-source version of Chrome for iOS with " +
        "AddressSanitizer (ASan) and runs unit tests for detecting memory " +
        "errors."
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
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "ios-asan",
            archive_subdir = "ios-asan",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-asan",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "ios_simulator",
            "arm64",
            "release_builder",
            "remoteexec",
            "asan",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios_asan_tests",
        ],
        additional_compile_targets = [
            "ios_chrome_clusterfuzz_asan_build",
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
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    gardener_rotations = args.ignore_default(gardener_rotations.IOS),
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "asn",
    ),
    xcode = xcode.xcode_default,
)

ci.builder(
    name = "linux-codeql-generator",
    description_html = "Compiles a CodeQL database on a Linux host and uploads the result.",
    executable = "recipe:chrome_codeql_database_builder",
    # Run once daily at 5am Pacific/1 PM UTC
    schedule = "0 13 * * *",
    triggered_by = [],
    cores = 32,
    ssd = True,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = [
        consoles.console_view_entry(
            category = "codeql-linux",
            short_name = "cdql-lnx",
        ),
    ],
    contact_team_email = "chrome-memory-safety-team@google.com",
    execution_timeout = 18 * time.hour,
    notifies = ["codeql-infra"],
    properties = {
        "codeql_version": "version:3@2.18.1",
    },
)

ci.builder(
    name = "linux-codeql-query-runner",
    description_html = "Runs a set of CodeQL queries against a CodeQL database on a Linux host and uploads the result.",
    executable = "recipe:chrome_codeql_query_runner",
    # Run once daily at 5am Pacific/1 PM UTC
    schedule = "0 13 * * *",
    triggered_by = [],
    cores = 32,
    ssd = True,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = [
        consoles.console_view_entry(
            category = "codeql-linux-queries",
            short_name = "cdql-lnx-qrs",
        ),
    ],
    contact_team_email = "chrome-memory-safety-team@google.com",
    execution_timeout = 18 * time.hour,
    notifies = ["codeql-infra"],
    properties = {
        "codeql_version": "version:3@2.18.1",
    },
)
