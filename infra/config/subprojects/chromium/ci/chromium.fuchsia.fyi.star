# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuchsia.fyi builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fuchsia.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.FUCHSIA,
    execution_timeout = 10 * time.hour,
    notifies = ["cr-fuchsia"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.fuchsia.fyi",
)

# The chromium.fuchsia.fyi console includes some entries for builders from the chrome project.
[branches.console_view_entry(
    console_view = "chromium.fuchsia.fyi",
    builder = "chrome:ci/{}".format(name),
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("fuchsia-builder-perf-x64", "p/chrome|x64", "perf-bld"),
    ("fuchsia-fyi-arm64-size", "p/chrome|arm64", "size"),
    ("fuchsia-x64", "p/chrome|x64", "rel"),
)]

ci.builder(
    name = "fuchsia-arm64-chrome-rel",
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
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-fyi-archive",
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "a64-chrome",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            # TODO(crbug.com/1372224): Move to "fuchsia ci|arm64" once green.
            category = "fyi|arm64",
            short_name = "chrome",
        ),
    ],
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
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|fuchsia ci|arm64",
            short_name = "dbg",
        ),
    ],
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
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "asan",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|fuchsia ci|x64",
            short_name = "asan",
        ),
    ],
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg",
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
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|fuchsia ci|x64",
            short_name = "dbg",
        ),
    ],
)

ci.builder(
    name = "fuchsia-x64-chrome-rel",
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
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "x64-chrome",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            # TODO(crbug.com/1372224): Move to "fuchsia ci|x64" once green.
            category = "fyi|x64",
            short_name = "chrome",
        ),
    ],
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg-persistent-emulator",
    triggered_by = ["ci/fuchsia-fyi-x64-dbg"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "x64-llemu",
        ),
    ],
)
