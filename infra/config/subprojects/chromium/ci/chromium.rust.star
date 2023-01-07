# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.rust builder group."""

load("//lib/builders.star", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.rust",
    builderless = False,
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    pool = ci.DEFAULT_POOL,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    os = os.LINUX_DEFAULT,
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
)

ci.builder(
    name = "android-rust-arm-rel",
    console_view_entry = consoles.console_view_entry(
        category = "Android",
        short_name = "rel",
    ),
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
