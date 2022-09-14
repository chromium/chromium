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
            category = "ci|x64",
            short_name = "det",
        ),
    ],
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
)

ci.builder(
    name = "Fuchsia ARM64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci|arm64",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "Fuchsia x64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "release",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci|x64",
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

# TODO(crbug.com/1294938): Rename the category to "cast-receiver" when replacing
# with the -cast-receiver bots. Same for the x64 bot.
ci.builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "cast",
            short_name = "arm64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci|arm64",
            short_name = "cast",
        ),
    ],
    # Set tree_closing to false to disable the default tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    notifies = ["cr-fuchsia", "close-on-any-step-failure"],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
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
    name = "fuchsia-x64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "cast",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci|x64",
            short_name = "cast",
        ),
    ],
    # Set tree_closing to false to disable the default tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    notifies = ["cr-fuchsia", "close-on-any-step-failure"],
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
            category = "ci|x64",
            short_name = "dbg",
        ),
    ],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
                "enable_reclient",
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
