# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.chromiumos builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.chromiumos",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.modified_default({
        "Unhealthy": struct(
            build_time = struct(
                p50_mins = 60,
            ),
        ),
    }),
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
    name = "chromium.chromiumos",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    ordering = {
        None: ["default"],
        "default": consoles.ordering(short_names = ["ful", "rel"]),
        "simple": ["release", "debug"],
    },
)

ci.builder(
    name = "chromeos-amd64-generic-asan-rel",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "asan",
            "chromeos",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "cfi_full",
            "thin_lto",
            "chromeos",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "cfi",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-chrome-build@google.com",
    health_spec = health_spec.modified_default({
        "Unhealthy": struct(
            build_time = struct(
                p50_mins = 100,
            ),
        ),
    }),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
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
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "debug",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a compile only builder for Ash chrome.",
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
        build_gs_bucket = "chromium-chromiumos-archive",
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
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "compile",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "chromeos-amd64-generic-rel-gtest",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a tester builder for Ash chrome." +
                       " This builder only run gtest.",
    triggered_by = ["ci/chromeos-amd64-generic-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    targets = targets.bundle(
        targets = [
            "gpu_chromeos_telemetry_tests",
            "chromeos_vm_gtests",
            "chromeos_isolated_scripts",
        ],
        mixins = [
            "chromeos-generic-vm",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.CROS_CHROME,
        os_type = targets.os_type.CROS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "gtest",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    notifies = ["chrome-fake-vaapi-test"],
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "chromeos-amd64-generic-rel-tast",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a tester builder for Ash chrome." +
                       " This builder only run tast tests. If you see" +
                       " test failures, please contact ChromeOS gardeners" +
                       " for help.",
    triggered_by = ["ci/chromeos-amd64-generic-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    targets = targets.bundle(
        targets = [
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
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "tast",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm-generic-dbg",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "arm-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic",
            "debug",
            "ozone_headless",
            "arm",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["arm-generic"],
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic",
            "ozone_headless",
            "arm",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = ["arm64-generic-vm"],
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "remoteexec",
            "arm64-generic-vm",
            "dcheck_always_on",
            "ozone_headless",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromeos_arm_gtests",
        ],
        additional_compile_targets = [
            "chromiumos_preflight",
        ],
        mixins = [
            "chromeos-generic-vm",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.CROS_CHROME,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "a64",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-chrome-build@google.com",
    notifies = ["chrome-v4l2-visl-test"],
)

ci.builder(
    name = "chromeos-jacuzzi-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = """\
This builder builds chromium and tests it on the public CrOS image on skylab DUTs.
""",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "jacuzzi",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            # Both CI and try use the same `chromium-skylab-try` bucket.
            gs_bucket = "chromium-skylab-try",
            gs_extra = "ash",
        ),
    ),
    builder_config_settings = builder_config.ci_settings(
        # Disabling shard-level-retry-on-chromium-recipe for skylab builders,
        # since a failed shard is retried even on CTP, which is more efficient.
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "include_unwind_tables",
            "is_skylab",
            "jacuzzi",
            "ozone_headless",
            "remoteexec",
            "arm64",
        ],
    ),
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "jcz",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = """\
This builder builds chromium and tests it on the public CrOS image on skylab DUTs.
""",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "octopus",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            # Both CI and try use the same `chromium-skylab-try` bucket.
            gs_bucket = "chromium-skylab-try",
            gs_extra = "ash",
        ),
    ),
    builder_config_settings = builder_config.ci_settings(
        # Disabling shard-level-retry-on-chromium-recipe for skylab builders,
        # since a failed shard is retried even on CTP, which is more efficient.
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "include_unwind_tables",
            "is_skylab",
            "octopus",
            "ozone_headless",
            "remoteexec",
            "x64",
        ],
    ),
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "oct",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
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
        build_gs_bucket = "chromium-chromiumos-archive",
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
    targets = targets.bundle(
        targets = [
            "linux_chromeos_gtests",
            "linux_chromeos_isolated_scripts",
        ],
        mixins = [
            "x86-64",
            "linux-jammy",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 140,
                ),
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "net_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "pthreadpool_unittests": targets.remove(
                reason = "pthreadpool is not built for ChromeOS currently.",
            ),
            "unit_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "wayland_client_perftests": targets.remove(
                reason = "https://crbug.com/859307",
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    # Inconsistent compile times can cause this builder to flakily hit the
    # default 3 hour timeout.
    execution_timeout = 6 * time.hour,
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            build_time = struct(
                p50_mins = 150,
            ),
        ),
    }),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-chromiumos-archive",
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
            "linux_chromeos_rel_cq",
            "linux_chromeos_isolated_scripts",
            "chromeos_annotation_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "x86-64",
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "angle_unittests": targets.mixin(
                # crbug.com/41493162: angle_unittests has a high failure rate.
                # Re-enable cq when the issue is fixed.
                ci_only = True,
            ),
            "browser_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    dimensions = {
                        "kvm": "1",
                    },
                    shards = 60,
                ),
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                # crbug.com/1475852
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 5,
                ),
            ),
            "pthreadpool_unittests": targets.remove(
                reason = "pthreadpool is not built for ChromeOS currently.",
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "unit_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    # See crbug.com/1345687. This builder need higher memory.
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-chrome-build@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

# For Chromebox for meetings(CfM)
ci.builder(
    name = "linux-cfm-rel",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cfm",
            "release_builder",
            "remoteexec",
            "chromeos",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "linux_cfm_gtests",
        ],
        mixins = [
            "linux-jammy",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default|cfm",
        short_name = "cfm",
    ),
    main_console_view = "main",
    contact_team_email = "core-devices-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)
