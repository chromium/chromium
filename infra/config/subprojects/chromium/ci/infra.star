# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the infra builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "infra",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "infra",
)

ci.builder(
    name = "linux-bootstrap",
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "bootstrap|linux",
        short_name = "bld",
    ),
    schedule = "triggered",
    triggered_by = [],
)

ci.builder(
    name = "linux-bootstrap-tests",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        parent = "ci/linux-bootstrap",
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "bootstrap|linux",
        short_name = "tst",
    ),
)

ci.builder(
    name = "win-bootstrap",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "bootstrap|win",
        short_name = "bld",
    ),
    os = os.WINDOWS_10,
    schedule = "triggered",
    triggered_by = [],
)

ci.builder(
    name = "win-bootstrap-tests",
    console_view_entry = consoles.console_view_entry(
        category = "bootstrap|win",
        short_name = "tst",
    ),
    triggered_by = ["ci/win-bootstrap"],
)
