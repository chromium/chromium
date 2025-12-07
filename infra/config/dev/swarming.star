# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Permissions for Chromium dev/staging swarming pools."""

load("@chromium-luci//swarming.star", "swarming")

swarming.root_permissions(
    owner_group = "project-chromium-admins",
    viewer_group = "all",
)

swarming.task_accounts(
    realm = "@root",
    groups = ["project-chromium-test-dev-task-accounts"],
)

swarming.pool_realm(
    name = "pools/ci",
    user_projects = [
        "infra",
        "infra-experimental",
        "v8",
    ],
)

luci.binding(
    realm = "pools/ci",
    roles = "role/swarming.poolViewer",
    projects = [
        "infra",
        "infra-experimental",
        "v8",
    ],
)

swarming.pool_realm(name = "pools/try")

swarming.pool_realm(
    name = "pools/tests",
    user_groups = [
        "project-chromium-ci-dev-task-accounts",
        "project-chromium-try-dev-task-accounts",
        #TODO(b/258041976): mac os vm experiments
        "chromium-swarming-dev-led-access",
    ],
)

swarming.task_triggerers(
    builder_realm = "try",
    pool_realm = "pools/tests",
    groups = ["chromium-swarming-dev-led-access"],
)

swarming.task_triggerers(
    builder_realm = "@root",
    pool_realm = "@root",
    groups = ["mdb/chrome-troopers"],
)
