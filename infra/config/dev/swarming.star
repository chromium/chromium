# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Permissions for Chromium dev/staging swarming pools."""

load("//lib/swarming.star", "swarming")

swarming.root_permissions()

swarming.task_accounts(
    realm = "@root",
    groups = ["project-chromium-test-dev-task-accounts"],
)

swarming.pool_realm(
    name = "pools/ci",
    projects = [
        "infra",
        "infra-experimental",
    ],
)

swarming.pool_realm(name = "pools/try")

swarming.pool_realm(
    name = "pools/tests",
    groups = [
        "project-chromium-ci-dev-task-accounts",
        "project-chromium-try-dev-task-accounts",
    ],
)

swarming.task_triggerers(
    builder_realm = "@root",
    pool_realm = "@root",
    groups = ["mdb/chrome-troopers"],
)
