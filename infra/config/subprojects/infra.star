# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the infra bucket."""

load("//lib/builders.star", "builders", "cpu", "os")
load("//lib/consoles.star", "consoles")

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
            notify_emails = ["chrome-browser-infra-team@google.com"],
            on_occurrence = ["FAILURE", "INFRA_FAILURE"],
        ),
    ],
    service_account = "chromium-autosharder@chops-service-accounts.iam.gserviceaccount.com",
)
