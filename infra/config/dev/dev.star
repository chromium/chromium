# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/chrome_settings.star", "chrome_settings")

luci.project(
    name = "chromium",
    config_dir = "luci",
    dev = True,
    buildbucket = "cr-buildbucket-dev.appspot.com",
    logdog = "luci-logdog-dev.appspot.com",
    milo = "luci-milo-dev.appspot.com",
    scheduler = "luci-scheduler-dev.appspot.com",
    swarming = "chromium-swarm-dev.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-dev-writers",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
    bindings = [
        # Roles for LUCI Analysis.
        luci.binding(
            roles = "role/analysis.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/analysis.queryUser",
            groups = "authenticated-users",
        ),
        luci.binding(
            roles = "role/analysis.editor",
            groups = ["project-chromium-committers", "googlers"],
        ),
    ],
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg",
)

chrome_settings.per_builder_outputs(
    root_dir = "builders-dev",
)

# An all-purpose public realm.
luci.realm(
    name = "public",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "luci-resultdb-access",
        ),
        # Other roles are inherited from @root which grants them to group:all.
    ],
)

# @project realm.
luci.realm(
    name = "@project",
    bindings = [
        # Allow everyone (including non-logged-in users) to see chromium tree status.
        luci.binding(
            roles = "role/treestatus.limitedReader",
            groups = [
                "all",
            ],
        ),
        # Only allow Googlers to see PII.
        luci.binding(
            roles = "role/treestatus.reader",
            groups = [
                "googlers",
            ],
            users = [
                "luci-notify-dev@appspot.gserviceaccount.com",
            ],
        ),
        # Only allow Googlers and service accounts.
        luci.binding(
            roles = "role/treestatus.writer",
            groups = [
                "googlers",
            ],
            users = [
                "luci-notify-dev@appspot.gserviceaccount.com",
            ],
        ),
    ],
)

luci.builder.defaults.test_presentation.set(resultdb.test_presentation(grouping_keys = ["status", "v.test_suite"]))

exec("//dev/swarming.star")

exec("//recipes.star")
exec("//gn_args/gn_args.star")

exec("//dev/subprojects/chromium/subproject.star")
