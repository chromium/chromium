# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _project_settings(
        *,
        project,
        project_title,
        is_main,
        is_lts_branch,
        chrome_project,
        ref):
    """Declare settings for the project.

    This provides the central location for what must be modified when
    setting up the project for a new branch or when a branch changes category
    (e.g. moves from a standard release channel to the long-term support
    channel).

    Args:
      * project - The name of the LUCI project.
      * project_title - A string identifying the project in title contexts (e.g.
        titles of consoles).
      * is_main - Whether this branch is main/master/trunk.
      * is_lts_branch - Whether this branch is in the LTS channel.
      * ref - The git ref containing the code for this branch.
    """
    if is_main and is_lts_branch:
        fail("is_main and is_lts_branch can't both be True")
    return struct(
        project = project,
        project_title = project_title,
        is_main = is_main,
        is_lts_branch = is_lts_branch,
        ref = ref,
        chrome_project = chrome_project,
    )

settings = _project_settings(**json.decode(io.read_file("./settings.json")))

def _generate_project_pyl(ctx):
    ctx.output["project.pyl"] = "\n".join([
        "# This is a non-LUCI generated file",
        "# This is consumed by presubmit checks that need to validate the config",
        repr(dict(
            # On master, we want to ensure that we don't have source side specs
            # defined for non-existent builders
            # On branches, we don't want to re-generate the source side specs as
            # that would increase branch day toil and complicate cherry-picks
            validate_source_side_specs_have_builder = settings.is_main,
        )),
        "",
    ])

lucicfg.generator(_generate_project_pyl)

def _milestone_details(*, project, ref):
    """Define the details for an active milestone.

    Args:
      * project - The name of the LUCI project that is configured for the
        milestone.
      * ref - The ref in the git repository that contains the code for the
        milestone.
    """
    return struct(
        project = project,
        ref = ref,
    )

# The milestone names and branch numbers of branches that we have builders
# running for (including milestone-specific projects)
# Branch numbers and milestones can be viewed in the chromium column at
# https://chromiumdash.appspot.com/releases
# The 3rd portion of the version number is the branch number for the associated
# milestone
ACTIVE_MILESTONES = {
    m["name"]: _milestone_details(project = m["project"], ref = m["ref"])
    for m in json.decode(io.read_file("./milestones.json")).values()
}
