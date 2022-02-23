# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.rust builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.rust",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.rust",
    ordering = {
        None: [
            "linux",
            "android",
        ],
    },
)

ci.builder(
    name = "android-rust-arm-rel",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "android|rel",
        short_name = "32",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-rust-x64-rel",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux|rel",
        short_name = "64",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)
