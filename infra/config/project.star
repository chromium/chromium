# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Project-wide settings and common structs (e.g. platform names)."""

platform = struct(
    ANDROID = "android",
    CROS = "cros",
    CROS_LTS = "cros-lts",
    FUCHSIA = "fuchsia",
    IOS = "ios",
    LINUX = "linux",
    MAC = "mac",
    WINDOWS = "windows",
)

PLATFORMS = tuple([getattr(platform, a) for a in dir(platform)])

def _platform_settings(
        *,
        description,  # buildifier: disable=unused-variable
        gardener_rotation = None):
    """Declare settings for a platform on the project.

    This provides the project-wide settings for a platform, which should be
    modified based on what channels the milestone is being shipped to for each
    platform.

    Args:
      * description - A string describing why the platform is enabled in the
        project. This is not used by the starlark, but enables providing some
        form of documentaion in the json file.
      * gardener_rotation - The name of a gardener rotation in which to include
        builders that are selected for the corresponding platform.
    """
    return struct(
        gardener_rotation = gardener_rotation,
    )

def _project_settings(
        *,
        project,
        project_title,
        ref,
        chrome_project,
        is_main,
        platforms = {}):
    """Declare settings for the project.

    This provides the central location for what must be modified when
    setting up the project for a new branch or when a branch changes category
    (e.g. moves from a standard release channel to the long-term support
    channel).

    Args:
      * project - The name of the LUCI project. No logic should depend on this
        value, it should only be used where the name of a project is required.
      * project_title - A string identifying the project in title contexts (e.g.
        titles of consoles). No conditional logic should depend on this value,
        it should only be used where the title of a project is required.
      * ref - The git ref containing the code for this branch.
      * chrome_project - The name of the corresponding chrome project. No logic
        should depend on this value, it should only be used where the name of a
        project is required.
      * ref - The git ref containing the code for this branch. No logic should
        depend on this value, it should only be used where a ref is required.
      * is_main - Whether or not this is the project for the main ref.
      * platforms - A mapping from a platform ID value to the settings for the
        platform. The valid platform ID values are the members of the platform
        struct. The settings for the platforms are dicts giving arguments for
        the _platform_settings function.

    Returns:
      A struct with attributes set to the input parameters. Additionally, the
      is_main attribute is set to True if branch_types is empty or False if
      branch_types is not empty.
    """
    if is_main:
        if platforms:
            fail("main project should not have any platforms set")
    else:
        if not platforms:
            fail("Non-main projects must have at least one platform set")
        invalid_platforms = [p for p in platforms if not p in PLATFORMS]
        if invalid_platforms:
            fail("The following platforms are invalid: {}".format(invalid_platforms))

    platforms = {k: _platform_settings(**v) for k, v in platforms.items()}

    return struct(
        project = project,
        project_title = project_title,
        ref = ref,
        chrome_project = chrome_project,
        is_main = is_main,
        platforms = platforms,
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
} if settings.is_main else {}
