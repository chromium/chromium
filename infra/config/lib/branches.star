# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing utilities for providing branch-specific definitions.

The module provide the `branches` struct which provides access to versions of
a subset of luci functions with an additional `branch_selector` keyword argument
that controls what branches the definition is actually executed for. If
`branch_selector` doesn't match the current branch as determined by values on
the `settings` struct in '//project.star', then the resource is not defined. The
`branch_selector` argument can be one of the following constants referring to
the category of the branch:
* MAIN - The resource is defined only for main/master/trunk
    [`settings.is_master`]
* STANDARD_BRANCHES - The resource is defined only for the beta and stable
    branches.
    [`not settings.is_master and not settings.is_lts_branch`]
* LTS_BRANCHES - The resource is defined only for the long-term support branches
    (LTC and LTR).
    [`not settings.is_master and settings.is_lts_branch`]

The `branch_selector` argument can also be one of the following constants
composing multiple categories:
* STANDARD_MILESTONES - The resource is defined for a branch as it moves through
    the standad release channels: trunk -> beta -> stable.
* LTS_MILESTONES - The resource is defined for a branch as it move through the
    long-term suport release channels: trunk -> beta -> stable -> LTC -> LTR.
* ALL_BRANCHES - The resource is defined for all branches and main/master/trunk.
* NOT_MAIN - The resource is defined for all branches, but not for
    main/master/trunk.

The `branch_selector` constants are also accessible via the `branches` struct.

For other uses cases where execution needs to vary by branch, the following are
also accessible via the `branches` struct:
* matches - Allows library code to be written that takes branch-specific
    behavior.
* value - Allows for providing different values between main/master/trunk and
    branches.
* exec - Allows for conditionally executing starlark modules.
"""

load("//project.star", "settings")

def _branch_selector(tag):
    return struct(__branch_selector__ = tag)

MAIN = _branch_selector("MAIN")
STANDARD_BRANCHES = _branch_selector("STANDARD_BRANCHES")
LTS_BRANCHES = _branch_selector("LTS_BRANCHES")

_BRANCH_SELECTORS = (MAIN, STANDARD_BRANCHES, LTS_BRANCHES)

def _matches(branch_selector):
    """Returns whether `branch_selector` matches the project settings."""
    if type(branch_selector) == type(struct()):
        branch_selectors = [branch_selector]
    else:
        branch_selectors = branch_selector
    for b in branch_selectors:
        if b == MAIN:
            if settings.is_master:
                return True
        elif b == STANDARD_BRANCHES:
            if not settings.is_master and not settings.is_lts_branch:
                return True
        elif b == LTS_BRANCHES:
            if settings.is_lts_branch:
                return True
        else:
            fail("elements of branch_selectors must be one of {}, got {!r}"
                .format(_BRANCH_SELECTORS, b))
    return False

def _value(*, for_main = None, for_branches = None):
    """Provide a value that varies between main/master/trunk and branches.

    If the current project settings indicate that this is main/master/trunk,
    then `for_main` will be returned. Otherwise, `for_branches` will be
    returned.
    """
    return for_main if settings.is_master else for_branches

def _exec(module, *, branch_selector = MAIN):
    """Execute `module` if `branch_selector` matches the project settings."""
    if not _matches(branch_selector):
        return
    exec(module)

def _make_branch_conditional(fn):
    def conditional_fn(*args, branch_selector = MAIN, **kwargs):
        if not _matches(branch_selector):
            return
        fn(*args, **kwargs)

    return conditional_fn

branches = struct(
    # Basic branch selectors
    MAIN = MAIN,
    STANDARD_BRANCHES = STANDARD_BRANCHES,
    LTS_BRANCHES = LTS_BRANCHES,

    # Branch selectors for tracking milestones through release channels
    STANDARD_MILESTONE = [MAIN, STANDARD_BRANCHES],
    LTS_MILESTONE = [MAIN, STANDARD_BRANCHES, LTS_BRANCHES],

    # Branch selectors to apply widely to branches
    ALL_BRANCHES = _BRANCH_SELECTORS,
    NOT_MAIN = [b for b in _BRANCH_SELECTORS if b != MAIN],

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
