# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builders.star", "builders", "os")
load("@chromium-luci//consoles.star", "consoles")

consoles.defaults.set(
    repo = "https://chromium.googlesource.com/chromium/src",
)

consoles.console_view(
    name = "chromium.flakiness",
)

luci.bucket(
    name = "flaky-reproducer",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-tryjob-access",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "mdb/chrome-flakiness",
        ),
    ],
)

builders.builder(
    name = "runner",
    bucket = "flaky-reproducer",
    description_html = "Runs Flaky Reproducer strategies to find reliable reproducing steps for flaky tests.",
    executable = "recipe:flakiness/reproducer",
    pool = "luci.chromium.try",
    cores = 8,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.flakiness",
        category = "flakiness",
        short_name = "reproducer",
    ),
    build_numbers = False,
    contact_team_email = "chrome-flakiness@google.com",
    execution_timeout = 2 * time.hour,
    service_account = "flaky-reproducer-builder@chops-service-accounts.iam.gserviceaccount.com",
)
