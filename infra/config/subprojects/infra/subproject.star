# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the infra bucket."""

load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//html.star", "linkify_builder")
load("@chromium-luci//try.star", "try_")
load("//lib/ci_constants.star", "ci_constants")

consoles.console_view(
    name = "infra",
    repo = "https://chromium.googlesource.com/chromium/src",
)

# Bucket-wide defaults
builders.defaults.set(
    bucket = "infra",
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    shadow_pool = ci_constants.DEFAULT_SHADOW_POOL,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

luci.bucket(
    name = "infra",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "project-chromium-infra-schedulers",
            ],
            users = [
                "chromium-autosharder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
        acl.entry(
            roles = acl.SCHEDULER_TRIGGERER,
            groups = [
                "project-chromium-infra-schedulers",
            ],
        ),
    ],
)

luci.bucket(
    name = "infra.shadow",
    shadows = "infra",
    constraints = luci.bucket_constraints(
        pools = [ci_constants.DEFAULT_SHADOW_POOL],
        service_accounts = [ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "mdb/chrome-troopers",
        ),
    ],
    dynamic = True,
)

builders.builder(
    name = "autosharder",
    bucket = "infra",
    executable = "recipe:chromium/autosharder",
    # Run once daily at 2 AM Pacific/9 AM UTC (with DST)
    schedule = "0 9 * * *",
    triggered_by = [],
    pool = "luci.chromium.ci",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        console_view = "infra",
        category = "autosharder",
        short_name = "auto",
    ),
    notifies = [
        luci.notifier(
            name = "chromium-autosharder-notifier",
            notify_emails = ["chrome-dev-infra-auto+alerts@google.com"],
            on_occurrence = ["FAILURE", "INFRA_FAILURE"],
        ),
    ],
    properties = {
        "exclude_builders": [
            "mac-rel",
            "mac14-arm64-rel",
            "ios-simulator",
            "ios-simulator-full-configs",
            "android-arm64-rel",
            # TODO(crbug.com/402190537): Revisit the emulator-based Android CQ
            # builders after the slowness in CIPD package deployment is fixed
            "android-desktop-x64-rel",
            "android-x64-rel",
            "android-x86-rel",
        ],
        "exclude_suites": [
            "chrome_all_tast_tests",
        ],
        "target_runtime": 15.0,
    },
    service_account = "chromium-autosharder@chops-service-accounts.iam.gserviceaccount.com",
)

try_.builder(
    name = "autosharder_test",
    bucket = "infra",
    description_html = "Tests shard exceptions produced by " + linkify_builder("infra", "autosharder"),
    executable = "recipe:chromium/autosharder_test",
    pool = "luci.chromium.try",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        console_view = "infra",
        category = "autosharder",
        short_name = "auto-tst",
    ),
    contact_team_email = "chrome-dev-infra-team@google.com",
)
