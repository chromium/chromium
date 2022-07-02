# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuchsia.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.fuchsia.fyi",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = 10 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.FUCHSIA,
)

consoles.console_view(
    name = "chromium.fuchsia.fyi",
)

# The chromium.fuchsia.fyi console includes some entries for builders from the chrome project
[branches.console_view_entry(
    builder = "chrome:ci/{}".format(name),
    console_view = "chromium.fuchsia.fyi",
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("fuchsia-fyi-arm64-size", "release", "a64-size"),
    ("fuchsia-builder-perf-fyi", "perf", "arm64 builder"),
    ("fuchsia-builder-perf-x64", "perf", "x64 builder"),
    ("fuchsia-x64", "release", "chrome-x64"),
)]

ci.builder(
    name = "fuchsia-fyi-arm64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci",
            short_name = "a64-dbg",
        ),
    ],
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
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        run_tests_serially = True,
    ),
)

ci.builder(
    name = "fuchsia-fyi-x64-asan",
    console_view_entry = [
        consoles.console_view_entry(
            category = "asan",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci",
            short_name = "asan",
        ),
    ],
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
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        run_tests_serially = True,
    ),
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fuchsia ci",
            short_name = "x64-dbg",
        ),
    ],
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
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        run_tests_serially = True,
    ),
)
