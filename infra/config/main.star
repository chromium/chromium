#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/branches.star", "branches")
load("//project.star", "settings")

lucicfg.check_version(
    min = "1.19.0",
    message = "Update depot_tools",
)

# Enable LUCI Realms support.
lucicfg.enable_experiment("crbug.com/1085650")

# Enable tree closing.
lucicfg.enable_experiment("crbug.com/1054172")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "commit-queue.cfg",
        "cq-builders.md",
        "cr-buildbucket.cfg",
        "luci-logdog.cfg",
        "luci-milo.cfg",
        "luci-notify.cfg",
        "luci-notify/email-templates/*.template",
        "luci-scheduler.cfg",
        "project.cfg",
        "project.pyl",
        "realms.cfg",
        "tricium-prod.cfg",
    ],
    fail_on_warnings = True,
    lint_checks = [
        "default",
        "-confusing-name",
        "-function-docstring",
        "-function-docstring-args",
        "-function-docstring-return",
        "-function-docstring-header",
        "-module-docstring",
    ],
)

# Just copy tricium-prod.cfg to the generated outputs
lucicfg.emit(
    dest = "tricium-prod.cfg",
    data = io.read_file("tricium-prod.cfg"),
)

luci.project(
    name = settings.project,
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
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
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = "chromium-cq-status.appspot.com",
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg",
)

luci.notify(
    tree_closing_enabled = True,
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
            groups = "project-chromium-tryjob-access",
        ),
        # Other roles are inherited from @root which grants them to group:all.
    ],
)

exec("//swarming.star")

exec("//recipes.star")

exec("//notifiers.star")

exec("//subprojects/chromium/subproject.star")
branches.exec("//subprojects/codesearch/subproject.star")
branches.exec("//subprojects/findit/subproject.star")
branches.exec("//subprojects/goma/subproject.star")
branches.exec("//subprojects/webrtc/subproject.star")

branches.exec("//generators/cq-builders-md.star")

# This should be exec'ed before exec'ing scheduler-noop-jobs.star because
# attempting to read the buildbucket field that is not set for the noop jobs
# actually causes an empty buildbucket message to be set
# TODO(https://crbug.com/1062385) The automatic generation of job IDs causes
# problems when the number of builders with the same name goes from 1 to >1 or
# vice-versa. This generator makes sure both the bucketed and non-bucketed IDs
# work so that there aren't transient failures when the configuration changes
branches.exec("//generators/scheduler-bucketed-jobs.star")

# TODO(https://crbug.com/819899) There are a number of noop jobs for dummy
# builders defined due to legacy requirements that trybots mirror CI bots
# no-op scheduler jobs are not supported by the lucicfg libraries, so this
# generator adds in the necessary no-op jobs
# The trybots should be update to not require no-op jobs to be triggered so that
# the no-op jobs can be removed
exec("//generators/scheduler-noop-jobs.star")
exec("//generators/sort-consoles.star")

exec("//validators/builders-in-consoles.star")

# Execute this file last so that any configuration changes needed for handling
# outages gets final say
exec("//outages/outages.star")
