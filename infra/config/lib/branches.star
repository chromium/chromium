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
* MAIN - The resource is defined only for main/trunk
    [`settings.is_main`]
* STANDARD_BRANCHES - The resource is defined only for the beta and stable
    branches.
    [`branch_type.STANDARD in settings.branch_types`]
* DESKTOP_EXTENDED_STABLE_BRANCHES - The resource is defined only for the
    desktop extended stable branch.
    [`branch_type.DESKTOP_EXTENDED_STABLE in settings.branch_types`]
* CROS_LTS_BRANCHES - The resource is defined only for the long-term support branches
    (LTC and LTR).
    [`branch_type.CROS_LTS in settings.branch_types`]
* FUCHSIA_LTS_BRANCHES - The resource is defined only for the fuchsia support
    branches.
    [`branch_type.FUCHSIA_LTS in settings.branch_types`]

The `branch_selector` argument can also be one of the following constants
composing multiple categories:
* STANDARD_MILESTONE - The resource is defined for a branch as it moves through
    the standard release channels: trunk -> beta -> stable.
* DESKTOP_EXTENDED_STABLE_MILESTONE - The resource is defined for a branch as it
    moves through the desktop extended stable release channels:
    trunk -> beta -> stable -> desktop extended stable
* CROS_LTS_MILESTONE - The resource is defined for a branch as it move through the
    long-term suport release channels: trunk -> beta -> stable -> LTC -> LTR.
* FUCHSIA_LTS_MILESTONE - The resource is define only for a branch as it moves
    through the fuchsia support channels: trunk -> beta -> stable -> Fuchsia LTS.
* ALL_BRANCHES - The resource is defined for all branches and main/trunk.
* NOT_MAIN - The resource is defined for all branches, but not for main/trunk.

The `branch_selector` constants are also accessible via the `branches` struct.

For other uses cases where execution needs to vary by branch, the following are
also accessible via the `branches` struct:
* matches - Allows library code to be written that takes branch-specific
    behavior.
* value - Allows for providing different values between main/trunk and branches.
* exec - Allows for conditionally executing starlark modules.
"""

load("//project.star", "branch_type", "settings")

def _branch_selector(tag):
    return struct(__branch_selector__ = tag)

MAIN = _branch_selector("MAIN")
STANDARD_BRANCHES = _branch_selector("STANDARD_BRANCHES")
DESKTOP_EXTENDED_STABLE_BRANCHES = _branch_selector("DESKTOP_EXTENDED_STABLE_BRANCHES")
CROS_LTS_BRANCHES = _branch_selector("CROS_LTS_BRANCHES")
FUCHSIA_LTS_BRANCHES = _branch_selector("FUCHSIA_LTS_BRANCHES")

_BRANCH_SELECTORS = (MAIN, STANDARD_BRANCHES, DESKTOP_EXTENDED_STABLE_BRANCHES, CROS_LTS_BRANCHES, FUCHSIA_LTS_BRANCHES)

def _normalize_branch_selector(branch_selector):
    """Convert provided branch selector to a set of basic selectors.
    """

    # A single basic selector was provided, return a set containing just it
    if type(branch_selector) == type(struct()):
        return set([branch_selector])

    # The provided selector is either:
    # * a compound selector, which are tuples of basic selectors
    # * an iterable of arbitrary selectors
    # Iterate over the selector, extracting the basic selectors from each
    branch_selectors = set()
    for s in branch_selector:
        if type(s) == type(struct()):
            s = [s]
        branch_selectors = branch_selectors.union(s)
    return branch_selectors

def _matches(branch_selector, *, target = None):
    """Returns whether `branch_selector` matches the project settings.

    Args:
      branch_selector: A single branch selector value or a list of branch
        selector values.
      target: A single branch selector value or a list of branch selector values
        to match branch_selector against. The return value will indicate whether
        there is an intersection between branch_selector and target instead of
        matching against the project settings.

    Returns:
      True if any of the specified branch selectors matches, False otherwise.
    """
    branch_selectors = _normalize_branch_selector(branch_selector)

    if target != None:
        targets = _normalize_branch_selector(target)
        for b in branch_selectors:
            if b in targets:
                return True
        return False

    for b in branch_selectors:
        if b == MAIN:
            if settings.is_main:
                return True
        elif b == STANDARD_BRANCHES:
            if branch_type.STANDARD in settings.branch_types:
                return True
        elif b == DESKTOP_EXTENDED_STABLE_BRANCHES:
            if branch_type.DESKTOP_EXTENDED_STABLE in settings.branch_types:
                return True
        elif b == CROS_LTS_BRANCHES:
            if branch_type.CROS_LTS in settings.branch_types:
                return True
        elif b == FUCHSIA_LTS_BRANCHES:
            if branch_type.FUCHSIA_LTS in settings.branch_types:
                return True
        else:
            fail("elements of branch_selectors must be one of {}, got {!r}"
                .format(_BRANCH_SELECTORS, b))
    return False

def _value(values, *, default = None):
    """Provide a value that varies depending on the project settings.

    Args:
      values - A mapping from branch selectors to the value to be used for the
        matching branches. The keys can be either a single selector or a tuple
        of selectors. The selectors will be matched in the order declared in the
        mapping.
      default - The value to be returned if the project settings don't match any
        of the branch selectors in the keys of `values`.
    """
    for selector, value in values.items():
        if _matches(selector):
            return value
    return default

def _exec(module, *, branch_selector = MAIN):
    """Execute `module` if `branch_selector` matches the project settings."""
    if not _matches(branch_selector):
        return
    exec(module)

def _make_branch_conditional(fn):
    def conditional_fn(*args, branch_selector = MAIN, **kwargs):
        if not _matches(branch_selector):
            return None
        return fn(*args, **kwargs)

    return conditional_fn

branches = struct(
    # Basic branch selectors
    MAIN = MAIN,
    STANDARD_BRANCHES = STANDARD_BRANCHES,
    DESKTOP_EXTENDED_STABLE_BRANCHES = DESKTOP_EXTENDED_STABLE_BRANCHES,
    CROS_LTS_BRANCHES = CROS_LTS_BRANCHES,
    FUCHSIA_LTS_BRANCHES = FUCHSIA_LTS_BRANCHES,

    # Branch selectors for tracking milestones through release channels
    STANDARD_MILESTONE = (MAIN, STANDARD_BRANCHES),
    DESKTOP_EXTENDED_STABLE_MILESTONE = (MAIN, STANDARD_BRANCHES, DESKTOP_EXTENDED_STABLE_BRANCHES),
    CROS_LTS_MILESTONE = (MAIN, STANDARD_BRANCHES, CROS_LTS_BRANCHES),
    FUCHSIA_LTS_MILESTONE = (MAIN, STANDARD_BRANCHES, FUCHSIA_LTS_BRANCHES),

    # Branch selectors to apply widely to branches
    ALL_BRANCHES = _BRANCH_SELECTORS,
    NOT_MAIN = tuple([b for b in _BRANCH_SELECTORS if b != MAIN]),

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
