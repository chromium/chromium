# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the presubmit.linux builder group."""

load("//lib/builders.star", "os", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    pool = ci.DEFAULT_POOL,
    console_view = "checks",
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "checks",
)

ci.builder(
    name = "linux-presubmit",
    executable = "recipe:presubmit",
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "presubmit",
        short_name = "linux",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480000,
        },
        "repo_name": "chromium",
    },
)

ci.builder(
    name = "win-presubmit",
    executable = "recipe:presubmit",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "presubmit",
        short_name = "win",
    ),
    execution_timeout = 6 * time.hour,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480000,
        },
        "repo_name": "chromium",
    },
)
