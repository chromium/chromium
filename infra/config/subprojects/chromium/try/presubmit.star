# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/builders.star", "os")
load("//lib/branches.star", "branches")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "BRANCH_TYPES", "branch_type")
load("../fallback-cq.star", "fallback_cq")

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
    if tryjob:
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
def branch_configs():
    """Get the branch configs to be tested.

    Returns:
      A list of objects that can be used as the value of the "branch_configs"
      property for the branch_configuration/tester recipe. See
      https://chromium.googlesource.com/chromium/tools/build/+/refs/heads/main/recipes/recipes/branch_configuration/tester.proto
      The returned configs will cover standard branches and every combination of
      post-stable branches.
    """
    type_combos = []
    for t in BRANCH_TYPES:
        # The standard branch type can only appear alone, so add it afterwards
        if t == branch_type.STANDARD:
            continue
        type_combos = type_combos + [[t]] + [c + [t] for c in type_combos]

    type_combos = [[branch_type.STANDARD]] + sorted(type_combos, key = lambda x: (len(x), x))
    return [{
        "name": " + ".join(c),
        "branch_types": c,
    } for c in type_combos]

presubmit_builder(
    name = "branch-config-verifier",
    executable = "recipe:branch_configuration/tester",
    properties = {
        "branch_script": "infra/config/scripts/branch.py",
        "branch_configs": branch_configs(),
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
        location_regexp = [
            r".+/[+]/tools/clang/scripts/update.py",
            r".+/[+]/DEPS",
        ],
    ),
)

presubmit_builder(
    name = "builder-config-verifier",
    description_html = "checks that builder configs in properties files match the recipe-side configs",
    executable = "recipe:chromium/builder_config_verifier",
    properties = {
        "builder_config_directory": "infra/config/generated/builders",
    },
    tryjob = try_.job(
        location_regexp = [r".+/[+]/infra/config/generated/builders/.*"],
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

presubmit_builder(
    name = "requires-testing-checker",
    cq_group = fallback_cq.GROUP,
    description_html = "prevents CLs that requires testing from landing on branches with no CQ",
    executable = "recipe:requires_testing_checker",
    tryjob = try_.job(),
)
