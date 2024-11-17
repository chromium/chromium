# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in chromium.build.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/html.star", "linkify_builder")
load("//lib/targets.star", "targets")

ci.defaults.set(
    bucket = "build",
    executable = ci.DEFAULT_EXECUTABLE,
    triggered_by = ["chrome-build-gitiles-trigger"],
    builder_group = "chromium.build.fyi",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    build_numbers = True,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 10 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    shadow_siso_project = siso.project.DEFAULT_UNTRUSTED,
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
    name = "chromium.build.fyi",
    repo = "https://chromium.googlesource.com/chromium/src",
)

ci.builder(
    name = "Mac Builder Siso FYI",
    description_html = "This builder is intended to test the latest Siso version on Mac.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to a child builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
                "siso_latest",
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = "ci/Mac Builder",
    targets = targets.bundle(
        targets = [
            "chromium_mac_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        per_test_modifications = {
            "check_static_initializers": targets.mixin(
                args = [
                    "--allow-coverage-initializer",
                ],
            ),
        },
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "build",
    ),
)

ci.thin_tester(
    name = "Mac Tests Siso FYI",
    description_html = """\
This builder is intended to shadow {}.<br/>\
But, the tests are built by {}.\
""".format(
        linkify_builder("ci", "mac14-tests"),
        linkify_builder("build", "Mac Builder Siso FYI"),
    ),
    triggered_by = ["build/Mac Builder Siso FYI"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    targets = targets.bundle(
        targets = [
            "chromium_mac_gtests_no_nacl",
            "chromium_mac_rel_isolated_scripts_once",
        ],
        mixins = [
            "mac_default_x64",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "gpu": None,
                    },
                    shards = 12,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "gpu": None,
                    },
                    shards = 18,
                ),
            ),
            "browser_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 20,  # crbug.com/1361887
                ),
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1475852
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 6,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 4,
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
        category = "mac",
        short_name = "test",
    ),
)
