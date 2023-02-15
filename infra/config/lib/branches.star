# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing utilities for providing branch-specific definitions.

The module provide the `branches` struct which provides access to versions of
a subset of luci functions with an additional `branch_selector` keyword argument
that controls what branches the definition is actually executed for. If
`branch_selector` doesn't match the current branch as determined by values on
the `settings` struct in '//project.star', then the resource is not defined.

The valid branch selectors are in the `branches.selector` struct. The
following selectors cause the resource to be defined on main or if a branch
project includes the corresponding platform value in its settings:
* ANDROID_BRANCHES: platform.ANDROID
* CROS_BRANCHES: platform.CROS
* CROS_LTS_BRANCHES: platform.CROS, platform.CROS_LTS
* DESKTOP_BRANCHES: platform.LINUX, platform.MAC, platform.WINDOWS
* FUCHSIA_BRANCHES: platform.FUCHSIA
* IOS_BRANCHES: platform.IOS
* LINUX_BRANCHES: platform.LINUX
* MAC_BRANCHES: platform.MAC
* WINDOWS_BRANCHES: platform.WINDOWS

The MAIN branch selector causes a resource to be defined only on the main
project. The ALL_BRANCHES branch selector causes the resource to be defined on
all branches.

For other uses cases where execution needs to vary by branch, the following are
also accessible via the `branches` struct:
* matches - Allows library code to be written that takes branch-specific
    behavior.
* value - Allows for providing values depending on the platforms that the branch
    is running on.
* exec - Allows for conditionally executing starlark modules.
"""

load("./args.star", "args")
load("//project.star", "PLATFORMS", "platform", "settings")

def _branch_selector(tag, *, platforms = None):
    return struct(
        __branch_selector__ = tag,
        platforms = tuple(platforms or []),
    )

selector = struct(
    # Branch selectors corresponding to the individual platform values (except
    # CROS_LTS_BRANCHES also implies CROS_BRANCHES)
    ANDROID_BRANCHES = _branch_selector("ANDROID_BRANCHES", platforms = [platform.ANDROID]),
    CROS_BRANCHES = _branch_selector("CROS_BRANCHES", platforms = [platform.CROS]),
    CROS_LTS_BRANCHES = _branch_selector("CROS_LTS_BRANCHES", platforms = [platform.CROS, platform.CROS_LTS]),
    FUCHSIA_BRANCHES = _branch_selector("FUCHSIA_BRANCHES", platforms = [platform.FUCHSIA]),
    IOS_BRANCHES = _branch_selector("IOS_BRANCHES", platforms = [platform.IOS]),
    LINUX_BRANCHES = _branch_selector("LINUX_BRANCHES", platforms = [platform.LINUX]),
    MAC_BRANCHES = _branch_selector("MAC_BRANCHES", platforms = [platform.MAC]),
    WINDOWS_BRANCHES = _branch_selector("WINDOWS_BRANCHES", platforms = [platform.WINDOWS]),

    # Linux, Mac & Windows
    DESKTOP_BRANCHES = _branch_selector("DESKTOP_BRANCHES", platforms = [platform.LINUX, platform.MAC, platform.WINDOWS]),

    # Branch selector for just the main project
    MAIN = _branch_selector("MAIN"),

    # Branch selector matching all branches
    ALL_BRANCHES = _branch_selector("ALL_BRANCHES", platforms = PLATFORMS),
)

def _matches(branch_selector, *, platform = None):
    """Returns whether `branch_selector` matches the project settings.

    Args:
      * branch_selector: A single branch selector or a list of branch selectors.
      * platform: A single platform name or a list of platform names to match
        against. If not provided, the branch selectors will be matched against
        the project's platforms.

    Returns:
      True if any of the specified branch selectors matches, False otherwise.
      The main project will match any branch selector iff platform is not
      specified.
    """
    if type(platform) == type(""):
        platforms = [platform]
    elif platform == None:
        platforms = settings.platforms
    else:
        platforms = platform

    for b in args.listify(branch_selector):
        if not hasattr(b, "__branch_selector__"):
            fail("got {!r} for a branch selector, must be one of the branch_selector enum values: {}"
                .format(b, ", ".join(dir(selector))))
        for p in b.platforms:
            if p in platforms:
                return True

    return platform == None and settings.is_main

def _value(*, branch_selector, value):
    """Provide a value that varies depending on the project settings.

    Args:
      * branch_selector: A single branch selector or a list of branch selectors.
      * value: The value if the project's settings match the branch selector(s).

    Returns:
      `value` if `branch_selector` matches the project settings, None otherwise.
    """
    if _matches(branch_selector):
        return value
    return None

def _exec(module, *, branch_selector = selector.MAIN):
    """Execute `module` if `branch_selector` matches the project settings."""
    if not _matches(branch_selector):
        return
    exec(module)

def _make_branch_conditional(fn):
    def conditional_fn(*args, branch_selector = selector.MAIN, **kwargs):
        if not _matches(branch_selector):
            return None
        return fn(*args, **kwargs)

    return conditional_fn

branches = struct(
    selector = selector,

    # Branch functions
    matches = _matches,
    exec = _exec,
    value = _value,

    # Make conditional versions of luci functions that define resources
    # This does not include any of the service configurations
    # This also does not include any functions such as recipe that don't
    # generate config unless they're referred to; it doesn't cause a problem
    # if they're not referred to
    **{a: _make_branch_conditional(getattr(luci, a)) for a in (
        "realm",
        "binding",
        "bucket",
        "builder",
        "gitiles_poller",
        "list_view",
        "list_view_entry",
        "console_view",
        "console_view_entry",
        "external_console_view",
        "cq_group",
        "cq_tryjob_verifier",
    )}
)
