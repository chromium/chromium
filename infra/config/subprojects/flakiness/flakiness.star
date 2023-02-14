# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builders", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    bucket = "flakiness",
    pool = "luci.chromium.ci",
    os = os.LINUX_DEFAULT,
    free_space = builders.free_space.standard,
    build_numbers = True,
    execution_timeout = 3 * time.hour,
    # TODO(jeffyoon): replace with smaller scoped service account, and update
    # below for bucket ACL
    service_account = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
)

luci.bucket(
    name = "flakiness",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "mdb/chrome-flakiness",
        ),
    ],
)

consoles.defaults.set(
    repo = "https://chromium.googlesource.com/chromium/src",
)

consoles.console_view(
    name = "chromium.flakiness",
)

ci.builder(
    name = "flakiness-data-packager",
    executable = "recipe:flakiness/generate_builder_test_data",
    schedule = "0 */1 * * *",
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.flakiness",
        category = "flakiness",
        short_name = "model",
    ),
)
