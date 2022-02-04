# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The value for each attribute should be the corresponding value that can be
# passed to the --type flag of scripts/branch.py
branch_type = struct(
    STANDARD = "standard",
    DESKTOP_EXTENDED_STABLE = "desktop-extended-stable",
    CROS_LTS = "cros-lts",
    FUCHSIA_LTS = "fuchsia-lts",
)

BRANCH_TYPES = tuple([getattr(branch_type, a) for a in dir(branch_type)])

def _project_settings(
        *,
        project,
        project_title,
        ref,
        chrome_project,
        branch_types = []):
    """Declare settings for the project.

    This provides the central location for what must be modified when
    setting up the project for a new branch or when a branch changes category
    (e.g. moves from a standard release channel to the long-term support
    channel).

    Args:
      * project - The name of the LUCI project.
      * project_title - A string identifying the project in title contexts (e.g.
        titles of consoles).
      * ref - The git ref containing the code for this branch.
      * chrome_project - The name of the corresponding chrome project.
      * branch_types - Values indicating what type(s) apply to the branch. If no
        branch types are specified, that indicates main. It is an error for
        branch_type.STANDARD to appear along with any other values.

    Returns:
      A struct with attributes set to the input parameters. Additionally, the
      is_main attribute is set to True if branch_types is empty or False if
      branch_types is not empty.
    """
    invalid_branch_types = [t for t in branch_types if t not in BRANCH_TYPES]
    if invalid_branch_types:
        fail("The following branch types are invalid: {}".format(invalid_branch_types))
    if branch_type.STANDARD in branch_types and len(branch_types) != 1:
        fail("STANDARD branch type cannot be specified along with other branch types")
    return struct(
        project = project,
        project_title = project_title,
        ref = ref,
        chrome_project = chrome_project,
        is_main = not branch_types,
        branch_types = branch_types,
    )

settings = _project_settings(**json.decode(io.read_file("./settings.json")))

def _generate_project_pyl(ctx):
    ctx.output["project.pyl"] = "\n".join([
        "# This is a non-LUCI generated file",
        "# This is consumed by presubmit checks that need to validate the config",
        repr(dict(
            # On main, we want to ensure that we don't have source side specs
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
