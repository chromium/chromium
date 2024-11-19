# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.memory builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.memory",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
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
        build_gs_bucket = "chromium-memory-archive",
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
    console_view_entry = consoles.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_enabled = True,
)

linux_memory_builder(
    name = "Linux ASan LSan Tests (1)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux ASan LSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
        ],
        per_test_modifications = {
            "accessibility_unittests": targets.mixin(
                args = [
                    # TODO(crbug.com/40266898): Re-enable.
                    "--gtest_filter=-AXPlatformNodeAuraLinuxTest.AtkComponentScrollTo:AtkUtilAuraLinuxTest.*",
                ],
            ),
            "browser_tests": targets.mixin(
                ci_only = True,
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 40,
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
            "webkit_unit_tests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 5,
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
        build_gs_bucket = "chromium-memory-archive",
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
        build_gs_bucket = "chromium-memory-archive",
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
                    shards = 20,
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
                    shards = 6,
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
        build_gs_bucket = "chromium-memory-archive",
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
    triggered_by = ["Linux Chromium OS ASan LSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
                    # https://crbug.com/1471857
                    shards = 14,
                ),
            ),
            "gin_unittests": targets.remove(
                reason = "https://crbug.com/831667",
            ),
            "interactive_ui_tests": targets.mixin(
                # These are slow on the ASan trybot for some reason, crbug.com/1257927
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "net_unittests": targets.mixin(
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 8,
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
        build_gs_bucket = "chromium-memory-archive",
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
    cores = 16,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros|msan",
        short_name = "bld",
    ),
    execution_timeout = 4 * time.hour,
)

linux_memory_builder(
    name = "Linux ChromiumOS MSan Tests",
    triggered_by = ["Linux ChromiumOS MSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
                    shards = 5,
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
        build_gs_bucket = "chromium-memory-archive",
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
    triggered_by = ["Linux MSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
                    shards = 23,
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
                    shards = 8,
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
        build_gs_bucket = "chromium-memory-archive",
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
    triggered_by = ["ci/Linux TSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
        ],
        per_test_modifications = {
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
                targets.mixin(
                    args = [
                        "--use-gpu-in-tests",
                        "--no-xvfb",
                    ],
                ),
                "linux_nvidia_gtx_1660_stable",
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
            "webkit_unit_tests": targets.mixin(
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
        build_gs_bucket = "chromium-memory-archive",
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
    triggered_by = ["ci/Linux UBSan Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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
            "webkit_unit_tests": targets.mixin(
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
    triggered_by = ["Mac ASan 64 Builder"],
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
        build_gs_bucket = "chromium-memory-archive",
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

ci.builder(
    name = "WebKit Linux ASAN",
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
        build_gs_bucket = "chromium-memory-archive",
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
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "asn",
    ),
)

ci.builder(
    name = "WebKit Linux Leak",
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
        build_gs_bucket = "chromium-memory-archive",
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
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "lk",
    ),
)

ci.builder(
    name = "WebKit Linux MSAN",
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
        build_gs_bucket = "chromium-memory-archive",
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
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux|webkit",
        short_name = "msn",
    ),
)

ci.builder(
    name = "android-asan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-memory-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "clang",
            "asan",
            "release_builder",
            "remoteexec",
            "strip_debug_info",
            "minimal_symbols",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_android_gtests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "bullhead",
            "nougat",
        ],
        per_test_modifications = {
            "android_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "angle_unittests": targets.remove(
                reason = "Times out listing tests crbug.com/1167314",
            ),
            "chrome_public_test_apk": targets.remove(
                reason = "https://crbug.com/964562",
            ),
            "chrome_public_test_vr_apk": targets.remove(
                reason = "https://crbug.com/964562",
            ),
            "chrome_public_unit_test_apk": targets.remove(
                reason = "https://crbug.com/964562",
            ),
            "components_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.asan.content_browsertests.filter",
                ],
                swarming = targets.swarming(
                    shards = 25,
                ),
            ),
            "content_shell_test_apk": targets.remove(
                reason = "https://crbug.com/964562",
            ),
            "ipc_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "mojo_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "perfetto_unittests": targets.remove(
                reason = "TODO(crbug.com/41440830): Fix permission issue when creating tmp files",
            ),
            "sandbox_linux_unittests": targets.remove(
                reason = "https://crbug.com/962650",
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.asan.unit_tests.filter",
                ],
            ),
            "webview_instrumentation_test_apk_multiple_process_mode": targets.remove(
                reason = "https://crbug.com/964562",
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    os = os.LINUX_DEFAULT,
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "asn",
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
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-memory-archive",
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
            "browser_tests": targets.mixin(
                # Tests shows tests run faster with fewer retries by using fewer jobs crbug.com/1411912
                args = [
                    "--test-launcher-jobs=3",
                ],
                # These are very slow on the ASAN trybot for some reason.
                # crbug.com/1257927
                swarming = targets.swarming(
                    shards = 60,
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
                    shards = 9,
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
            "webkit_unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
        },
    ),
    builderless = True,
    cores = "16|32",
    os = os.WINDOWS_DEFAULT,
    # TODO: crrev.com/i/7808548 - Drop cores=32 and add ssd=True after bot migration.
    ssd = None,
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
            "x64",
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
            "mac_default_x64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_16_main",
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
