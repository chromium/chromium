# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.mojo",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = 10 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.mojo",
)

ci.builder(
    name = "Mojo Android",
    console_view_entry = consoles.console_view_entry(
        short_name = "and",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Mojo ChromiumOS",
    console_view_entry = consoles.console_view_entry(
        short_name = "cr",
    ),
)

ci.builder(
    name = "Mojo Linux",
    console_view_entry = consoles.console_view_entry(
        short_name = "lnx",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Mojo Windows",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "mac-mojo-rel",
    console_view_entry = consoles.console_view_entry(
        short_name = "mac",
    ),
    cores = 4,
    os = os.MAC_ANY,
)
