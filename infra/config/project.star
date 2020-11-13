# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _project_settings(
        *,
        project,
        branch_title,
        is_master,
        is_lts_branch,
        ref,
        cq_ref_regexp,
        try_triggering_projects,
        tree_status_host):
    """Declare settings for the project.

    This provides the central location for what must be modified when
    setting up the project for a new branch or when a branch changes category
    (e.g. moves from a standard release channel to the long-term support
    channel).

    Args:
      * project - The name of the LUCI project.
      * branch_title - A string identifying the branch in console titles.
      * is_master - Whether this branch is main/master/trunk.
      * is_lts_branch - Whether this branch is in the LTS channel.
      * ref - The git ref containing the code for this branch.
      * cq_ref_regexp - A regular expression determining the git refs that the
        CQ group for this project should watch.
      * try_trigger_projects - A list of names of other LUCI projects whose CQ
        is allowed to trigger this project's try builders. None can also be
        passed to prohibit other projects' CQ from triggering this project's try
        builders.
      * tree_status_host - The host of the tree status app associated with this
        project. Builders with tree closers configured will notify this host and
        CQ attempts will block if the host indicates the tree is closed. It also
        appears at the top of the console header. None indicates there is no
        associated tree status app for the project.
    """
    if is_master and is_lts_branch:
        fail("is_master and is_lts_branch can't both be True")
    return struct(
        project = project,
        is_master = is_master,
        is_lts_branch = is_lts_branch,
        ref = ref,
        cq_ref_regexp = cq_ref_regexp,
        try_triggering_projects = try_triggering_projects,
        tree_status_host = tree_status_host,
        main_console_title = "{} Main Console".format(branch_title),
        cq_mirrors_console_title = "{} CQ Mirrors Console".format(branch_title),
        main_list_view_title = "{} CQ Console".format(branch_title),
    )

settings = _project_settings(
    # Set this to the name of the milestone's project
    project = "chromium-m88",
    # Set this to how the branch should be referred to in console titles
    branch_title = "Chromium M88",
    # Set this to False for branches
    is_master = False,
    # Set this to True for LTC/LTS branches
    is_lts_branch = False,
    # Set this to the branch ref for branches
    ref = "refs/branch-heads/4324",
    # Set this to the branch ref for branches
    cq_ref_regexp = "refs/branch-heads/4324",
    # Set this to None for branches
    try_triggering_projects = None,
    # Set this to None for branches
    tree_status_host = None,
)

def _generate_project_pyl(ctx):
    ctx.output["project.pyl"] = "\n".join([
        "# This is a non-LUCI generated file",
        "# This is consumed by presubmit checks that need to validate the config",
        repr(dict(
            # On master, we want to ensure that we don't have source side specs
            # defined for non-existent builders
            # On branches, we don't want to re-generate the source side specs as
            # that would increase branch day toil and complicate cherry-picks
            validate_source_side_specs_have_builder = settings.is_master,
        )),
        "",
    ])

lucicfg.generator(_generate_project_pyl)

def _milestone_details(*, project, ref, channel):
    """Define the details for an active milestone.

    Args:
      * project - The name of the LUCI project that is configured for the
        milestone.
      * ref - The ref in the git repository that contains the code for the
        milestone.
      * channel - The name of the release channel the milestone is in (used for
        identifying the milestone in the console header).
    """
    return struct(
        project = project,
        ref = ref,
        channel = channel,
    )

# The milestone names and branch numbers of branches that we have builders
# running for (including milestone-specific projects)
# Branch numbers and milestones can be viewed in the chromium column at
# https://chromiumdash.appspot.com/branches
# The 2 highest milestones will be the ones with active branches
ACTIVE_MILESTONES = {
    "m86": _milestone_details(
        project = "chromium-m86",
        ref = "refs/branch-heads/4240",
        channel = "Stable",
    ),
    "m87": _milestone_details(
        project = "chromium-m87",
        ref = "refs/branch-heads/4280",
        channel = "Beta",
    ),
}
