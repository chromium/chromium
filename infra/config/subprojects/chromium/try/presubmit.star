# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/builders.star", "os")
load("//lib/branches.star", "branches")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "branch_type")

try_.defaults.set(
    cores = 8,
    execution_timeout = 15 * time.minute,
    list_view = "presubmit",
    main_list_view = "try",
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = try_.DEFAULT_POOL,
    # Default priority for buildbucket is 30, see
    # https://chromium.googlesource.com/infra/infra/+/bb68e62b4380ede486f65cd32d9ff3f1bbe288e4/appengine/cr-buildbucket/creation.py#42
    # This will improve our turnaround time for landing infra/config changes
    # when addressing outages
    priority = 25,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "presubmit",
    branch_selector = branches.ALL_BRANCHES,
    title = "presubmit builders",
)

def presubmit_builder(*, name, tryjob, **kwargs):
    """Define a presubmit builder.

    Presubmit builders are builders that run fast checks that don't require
    building. Their results aren't re-used because they tend to provide guards
    against generated files being out of date, so they MUST run quickly so that
    the submit after a CQ dry run doesn't take long.
    """
    tryjob_args = {a: getattr(tryjob, a) for a in dir(tryjob)}
    tryjob_args["disable_reuse"] = True
    tryjob_args["add_default_excludes"] = False
    tryjob = try_.job(**tryjob_args)
    return try_.builder(name = name, tryjob = tryjob, **kwargs)

# Errors that this builder would catch would go unnoticed until a project is set
# up on a branch day or even worse when a branch was turned into an LTS branch,
# long after the change has been made, so make it a presubmit builder to ensure
# it's checked with current code. The builder runs in a few minutes and only for
# infra/config changes, so it won't impose a heavy burden on our capacity.
presubmit_builder(
    name = "branch-config-verifier",
    executable = "recipe:branch_configuration/tester",
    properties = {
        "branch_script": "infra/config/scripts/branch.py",
        "branch_configs": [
            {
                "name": branch_type.STANDARD,
                "branch_types": [branch_type.STANDARD],
            },
            {
                "name": branch_type.DESKTOP_EXTENDED_STABLE,
                "branch_types": [branch_type.DESKTOP_EXTENDED_STABLE],
            },
            {
                "name": branch_type.CROS_LTS,
                "branch_types": [branch_type.CROS_LTS],
            },
            {
                "name": "{} + {}".format(branch_type.DESKTOP_EXTENDED_STABLE, branch_type.CROS_LTS),
                "branch_types": [branch_type.DESKTOP_EXTENDED_STABLE, branch_type.CROS_LTS],
            },
        ],
        "starlark_entry_points": ["infra/config/main.star", "infra/config/dev.star"],
    },
    tryjob = try_.job(
        location_regexp = [r".+/[+]/infra/config/.+"],
    ),
)

presubmit_builder(
    name = "reclient-config-deployment-verifier",
    executable = "recipe:reclient_config_deploy_check/tester",
    properties = {
        "fetch_script": "buildtools/reclient_cfgs/fetch_reclient_cfgs.py",
        "rbe_project": [
            {
                "name": "rbe-chromium-trusted",
                "cfg_file": [
                    "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg",
                    "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_windows.cfg",
                    "buildtools/reclient_cfgs/nacl/rewrapper_linux.cfg",
                    "buildtools/reclient_cfgs/nacl/rewrapper_windows.cfg",
                ],
            },
        ],
    },
    tryjob = try_.job(
        location_regexp = [r".+/[+]/tools/clang/scripts/update.py"],
    ),
)

presubmit_builder(
    name = "chromium_presubmit",
    branch_selector = branches.ALL_BRANCHES,
    executable = "recipe:presubmit",
    execution_timeout = 40 * time.minute,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480,
        },
        "repo_name": "chromium",
    },
    tryjob = try_.job(),
)
