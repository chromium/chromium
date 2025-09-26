# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the presubmit.linux builder group."""

load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")

ci.defaults.set(
    pool = ci_constants.DEFAULT_POOL,
    console_view = "checks",
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
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
    gardener_rotations = gardener_rotations.CHROMIUM,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "presubmit",
        short_name = "linux",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
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
    cores = "16|32",
    os = os.WINDOWS_DEFAULT,
    # TODO: crrev.com/i/7808548 - Drop cores=32 and add ssd=True after bot migration.
    ssd = None,
    gardener_rotations = gardener_rotations.CHROMIUM,
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
    description_html = "Weekly scan for third party license errors.",
    executable = "recipe:chromium_licenses/scan",
    schedule = "15 22 * * 1",  # Once a week 10:15pm UTC / 8:15am AEST / 1:15am PST
    triggered_by = None,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "3p-licenses",
        short_name = "linux",
    ),
    contact_team_email = "chops-security-core@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    notifies = ["peeps-security-core-ssci"],
)
