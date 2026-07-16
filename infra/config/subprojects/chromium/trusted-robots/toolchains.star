# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.trusted-robots builder group for toolchains."""

load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")

ci.builder(
    name = "linux_clang",
    description_html = "Builder for Clang toolchain",
    executable = "recipe:chromium_toolchain/trusted_packaging",
    schedule = "triggered",
    cores = 2,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.trusted-robots",
        short_name = "lnx",
    ),
    contact_team_email = "dlf@google.com",
    execution_timeout = 6 * time.hour,
    properties = {
        "toolchain": "CLANG",
    },
    service_account = "lexan-swarming-prod@lexan-release-infra-prod.iam.gserviceaccount.com",
)
