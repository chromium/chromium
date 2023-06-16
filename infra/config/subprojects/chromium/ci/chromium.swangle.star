# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.swangle builder group."""

load("//lib/builders.star", "reclient", "sheriff_rotations")
load("//lib/builder_config.star", "builder_config")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = "recipe:angle_chromium",
    builder_group = "chromium.swangle",
    pool = ci.gpu.POOL,
    sheriff_rotations = sheriff_rotations.CHROMIUM_GPU,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.gpu.SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.swangle",
    ordering = {
        None: ["DEPS", "ToT SwiftShader", "Chromium"],
        "*os*": ["Windows", "Mac"],
        "*cpu*": consoles.ordering(short_names = ["x86", "x64"]),
        "DEPS": "*os*",
        "DEPS|Windows": "*cpu*",
        "DEPS|Linux": "*cpu*",
        "ToT SwiftShader": "*os*",
        "ToT SwiftShader|Windows": "*cpu*",
        "ToT SwiftShader|Linux": "*cpu*",
        "Chromium": "*os*",
    },
)

ci.gpu.linux_builder(
    name = "linux-swangle-chromium-x64",
    executable = ci.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Linux",
        short_name = "x64",
    ),
    list_view = "chromium.gpu.experimental",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "linux-swangle-chromium-x64-exp",
    executable = ci.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Linux",
        short_name = "exp",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "linux-swangle-tot-swiftshader-x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Linux",
        short_name = "x64",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "linux-swangle-x64",
    executable = ci.DEFAULT_EXECUTABLE,
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
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux",
        short_name = "x64",
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "linux-swangle-x64-exp",
    executable = ci.DEFAULT_EXECUTABLE,
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
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.mac_builder(
    name = "mac-swangle-chromium-x64",
    executable = ci.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Mac",
        short_name = "x64",
    ),
)

ci.gpu.windows_builder(
    name = "win-swangle-chromium-x86",
    executable = ci.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Windows",
        short_name = "x86",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "win-swangle-tot-swiftshader-x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x64",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "win-swangle-tot-swiftshader-x86",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x86",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "win-swangle-x64",
    executable = ci.DEFAULT_EXECUTABLE,
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
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x64",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "win-swangle-x86",
    executable = ci.DEFAULT_EXECUTABLE,
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
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-swangle-archive",
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x86",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)
