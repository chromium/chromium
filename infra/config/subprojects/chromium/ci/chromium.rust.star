# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.rust builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.rust",
    builderless = False,
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    notifies = ["chrome-rust-experiments"],
)

consoles.console_view(
    name = "chromium.rust",
)

ci.builder(
    name = "android-rust-arm-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "Android",
        short_name = "dbg",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "android-rust-arm-rel",
    console_view_entry = consoles.console_view_entry(
        category = "Android",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-rust-x64-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "linux-rust-x64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
        short_name = "rel",
    ),
)
