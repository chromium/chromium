#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

"""Entrypoint for `lucicfg generate infra/config/main.star`."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//chromium_luci.star", "chromium_luci")
load(
    "//lib/builder_exemptions.star",
    "exempted_from_contact_builders",
    "exempted_from_description_builders",
    "exempted_gardened_mirrors_in_cq_builders",
    "mega_cq_excluded_builders",
    "mega_cq_excluded_gardener_rotations",
    "standalone_trybot_excluded_builder_groups",
    "standalone_trybot_excluded_builders",
)
load("//project.star", "settings")

lucicfg.check_version(
    min = "1.44.1",
    message = "Update depot_tools",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "builders/*/*/*",
        "builders/*/*/*/*",
        "builders/alerting-builders.txt",
        "builders/gn_args_locations.json",
        "builder-owners/*.txt",
        "cq-builders.md",
        "cq-usage/default.cfg",
        "cq-usage/full.cfg",
        "cq-usage/mega_cq_bots.txt",
        "health-specs/health-specs.json",
        "luci/commit-queue.cfg",
        "luci/cr-buildbucket.cfg",
        "luci/luci-analysis.cfg",
        "luci/luci-bisection.cfg",
        "luci/luci-logdog.cfg",
        "luci/luci-milo.cfg",
        "luci/luci-notify.cfg",
        "luci/luci-notify/email-templates/*.template",
        "luci/luci-scheduler.cfg",
        "luci/project.cfg",
        "luci/realms.cfg",
        "luci/testhaus.cfg",
        "outages.pyl",
        "sheriff-rotations/*.txt",
        "project.pyl",
        "testing/*.pyl",
    ],
    fail_on_warnings = True,
)

# Just copy Testhaus config to generated outputs.
lucicfg.emit(
    dest = "luci/testhaus.cfg",
    data = io.read_file("testhaus.cfg"),
)

# Just copy LUCI Analysis config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-analysis.cfg",
    data = io.read_file("luci-analysis.cfg"),
)

# Just copy LUCI Bisection config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-bisection.cfg",
    data = io.read_file("luci-bisection.cfg"),
)

luci.project(
    name = settings.project,
    config_dir = "luci",
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
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            groups = [
                "project-chromium-try-task-accounts",
                "project-chromium-ci-task-accounts",
            ],
        ),
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
        # Role for builder health indicators
        luci.binding(
            roles = "role/buildbucket.healthUpdater",
            users = ["generate-builder@cr-builder-health-indicators.iam.gserviceaccount.com"],
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = "chromium-cq-status.appspot.com",
    honor_gerrit_linked_accounts = True,
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

chromium_luci.configure_project(
    name = settings.project,
    ref = settings.ref,
    is_main = settings.is_main,
    platforms = settings.platforms,
    experiments = [
        "builder_config.targets_spec_directory_relative_to_source_dir",
    ],
)

chromium_luci.configure_per_builder_outputs(
    root_dir = "builders",
)

chromium_luci.configure_builder_config(
    exempted_gardened_mirrors_in_cq_builders = exempted_gardened_mirrors_in_cq_builders,
    mega_cq_excluded_builders = mega_cq_excluded_builders,
    mega_cq_excluded_gardener_rotations = mega_cq_excluded_gardener_rotations,
    standalone_trybot_excluded_builder_groups = standalone_trybot_excluded_builder_groups,
    standalone_trybot_excluded_builders = standalone_trybot_excluded_builders,
)

chromium_luci.configure_builder_health_indicators(
    unhealthy_period_days = 7,
    pending_time_p50_min = 20,
    exempted_from_contact_builders = exempted_from_contact_builders,
)

chromium_luci.configure_builders(
    enable_alerts_configuration = True,
    os_dimension_overrides = {
        os.LINUX_DEFAULT: chromium_luci.os_dimension_overrides(
            default = os.LINUX_JAMMY,
            overrides = json.decode(io.read_file("//lib/linux-default.json")),
        ),
        os.MAC_DEFAULT: os.MAC_15,
        os.MAC_BETA: "Mac-15|Mac-26",
        os.WINDOWS_DEFAULT: os.WINDOWS_10,
    },
    exempted_from_description_builders = exempted_from_description_builders,
)

chromium_luci.configure_ci(
    main_console_view = "main" if not settings.is_main else None,
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
)

chromium_luci.configure_gardener_rotations(
    rotation_files_path = "sheriff-rotations",
)

chromium_luci.configure_targets(
    generate_pyl_files = True,
    autoshard_exceptions_file = "//autoshard_exceptions.json",
)

chromium_luci.configure_try(
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
    additional_default_exclude_path_regexps = ["docs/.+"],
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

luci.realm(
    name = "ci",
    bindings = [
        # Allow CI builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.realm(
    name = "try",
    bindings = [
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = [
                "project-chromium-try-task-accounts",
                # In order to be able to reproduce test tasks that have
                # ResultDB enabled (at this point that should be all
                # tests), a realm must be provided. The ability to
                # trigger machines in the test pool is associated with
                # the try realm, so allow those who can trigger swarming
                # tasks in that pool tasks to create invocations.
                "chromium-led-users",
                "project-chromium-tryjob-access",
            ],
        ),
    ],
)

# Allows builders to write baselines and query ResultDB for new tests.
# TODO(crbug.com/40276195) @project is not available, and @root should inherit into
# project so we'll do this for now until @project is supported.
luci.realm(
    name = "@root",
    bindings = [
        luci.binding(
            roles = "role/resultdb.baselineWriter",
            groups = [
                "project-chromium-ci-task-accounts",
                "project-chromium-try-task-accounts",
            ],
            users = [
                "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/resultdb.baselineReader",
            groups = [
                "project-chromium-try-task-accounts",
            ],
            users = [
                "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ],
)

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
                "chromium-status-hr@appspot.gserviceaccount.com",
                "luci-notify@appspot.gserviceaccount.com",
            ],
        ),
        # Only allow Googlers and service accounts.
        luci.binding(
            roles = "role/treestatus.writer",
            groups = [
                "googlers",
            ],
            users = [
                "luci-notify@appspot.gserviceaccount.com",
            ],
        ),
    ],
)

luci.realm(
    name = "webrtc",
    bindings = [
        # Allow WebRTC builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.builder.defaults.test_presentation.set(resultdb.test_presentation(grouping_keys = ["status", "v.test_suite"]))

exec("//swarming.star")

exec("//recipes.star")
exec("//gn_args/gn_args.star")

exec("@chromium-targets//declarations.star")

exec("//notifiers.star")

exec("//subprojects/build/subproject.star")
exec("//subprojects/chrome/subproject.star")
exec("//subprojects/chromium/subproject.star")
exec("//subprojects/infra.star")
branches.exec("//subprojects/codesearch/subproject.star")
branches.exec("//subprojects/findit/subproject.star")
branches.exec("//subprojects/flakiness/subproject.star")
branches.exec("//subprojects/reviver/subproject.star")
branches.exec("//subprojects/webrtc/subproject.star")

exec("//generators/cq-usage.star")
branches.exec("//generators/cq-builders-md.star")

exec("//generators/builder-owners.star")
exec("//generators/sort-consoles.star")

# Execute validators after eveything except the outage file so that we're
# validating the final non-outages configuration
exec("//validators/builder-group-triggers.star")
exec("//validators/builders-in-consoles.star")

# Notify findit about completed builds for code coverage purposes
luci.buildbucket_notification_topic(
    name = "projects/findit-for-me/topics/buildbucket_notification",
)

# Execute this file last so that any configuration changes needed for handling
# outages gets final say
exec("//outages/outages.star")
