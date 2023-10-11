# Copyright 2022 The Chromium Authors
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
    contact_team_email = "chrome-browser-infra-team@google.com",
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
    contact_team_email = "chrome-browser-infra-team@google.com",
    execution_timeout = 6 * time.hour,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480000,
        },
        "repo_name": "chromium",
    },
)

ci.builder(
    name = "linux-3p-licenses",
    description_html = "Daily scan for third party license errors.",
    executable = "recipe:chromium_licenses/scan",
    schedule = "15 22 * * 1",  # 10:15pm UTC / 8:15am AEST / 1:15am PST
    triggered_by = None,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "3p-licenses",
        short_name = "linux",
    ),
    contact_team_email = "chops-security-core@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    notifies = ["peeps-security-core-ssci"],
)

ci.builder(
    name = "win-3p-licenses",
    description_html = "Daily scan for third party license errors.",
    executable = "recipe:chromium_licenses/scan",
    schedule = "15 22 * * 1",  # Once a week 10:15pm UTC / 8:15am AEST / 1:15am PST
    triggered_by = None,
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "3p-licenses",
        short_name = "win",
    ),
    contact_team_email = "chops-security-core@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    notifies = ["peeps-security-core-ssci"],
)
