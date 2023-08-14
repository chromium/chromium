# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builders", "os")
load("//lib/consoles.star", "consoles")

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
    executable = "recipe:flakiness/reproducer",
    pool = "luci.chromium.try",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.flakiness",
        category = "flakiness",
        short_name = "reproducer",
    ),
    build_numbers = False,
    execution_timeout = 2 * time.hour,
    service_account = "flaky-reproducer-builder@chops-service-accounts.iam.gserviceaccount.com",
)
