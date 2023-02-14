# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.rust builder group."""

load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.rust",
    pool = ci.DEFAULT_POOL,
    builderless = False,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    notifies = ["chrome-rust-experiments"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "android-rust-arm-rel",
    console_view_entry = consoles.console_view_entry(
        category = "Android",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
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
