# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.accessibility builder group."""

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
    builder_group = "chromium.accessibility",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    notifies = ["cr-accessibility"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.accessibility",
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

ci.builder(
    name = "fuchsia-x64-accessibility-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
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
            "blink_symbol",
            "minimal_symbols",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = "fuchsia_accessibility_browsertests",
        additional_compile_targets = "content_browsertests",
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
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "rel",
            short_name = "fsia-x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "a11y",
        ),
    ],
)

ci.builder(
    name = "linux-blink-web-tests-force-accessibility-rel",
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
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "linux_force_accessibility_gtests",
            "chromium_webkit_isolated_scripts",
        ],
        additional_compile_targets = [
            "blink_tests",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--flag-specific=force-renderer-accessibility",
                ],
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--flag-specific=force-renderer-accessibility",
                ],
            ),
            "chrome_wpt_tests": targets.mixin(
                args = [
                    "--flag-specific=force-renderer-accessibility",
                ],
            ),
            "headless_shell_wpt_tests": targets.mixin(
                args = [
                    "--flag-specific=force-renderer-accessibility",
                ],
            ),
        },
    ),
    gardener_rotations = gardener_rotations.CHROMIUM,
    console_view_entry = consoles.console_view_entry(
        category = "rel",
        short_name = "x64",
    ),
)
