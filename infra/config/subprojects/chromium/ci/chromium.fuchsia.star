# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuchsia builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.fuchsia",
    cores = 8,
    cq_mirrors_console_view = "mirrors",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    main_console_view = "main",
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    reclient_instance = None,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.console_view(
    name = "chromium.fuchsia",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    ordering = {
        None: ["release", "debug"],
    },
)

ci.builder(
    name = "Deterministic Fuchsia (dbg)",
    console_view_entry = [
        consoles.console_view_entry(
            category = "det",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "det",
        ),
    ],
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
)

ci.builder(
    name = "fuchsia-arm64-cast-receiver-rel",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "cast-receiver",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|arm64",
            short_name = "cast",
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
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "fuchsia-arm64-rel",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|arm64",
            short_name = "rel",
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
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "fuchsia-x64-cast-receiver-rel",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "cast-receiver",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "cast",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "fuchsia-x64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "debug",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "dbg",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    goma_jobs = None,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "fuchsia-x64-rel",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "rel",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
)
