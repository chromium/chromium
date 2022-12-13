# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions for the chromium.cft (chrome for testing) builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.cft",
    executable = ci.DEFAULT_EXECUTABLE,
    builderless = True,
    cores = 8,
    ssd = True,
    pool = ci.DEFAULT_POOL,
    sheriff_rotations = sheriff_rotations.CFT,
    tree_closing = False,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.cft",
)

ci.builder(
    name = "mac-rel-cft",
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
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(),
)
