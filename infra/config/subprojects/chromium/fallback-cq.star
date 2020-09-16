# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//project.star", "ACTIVE_BRANCHES")

def _trailing_digit_regex(n):
    if n == 0:
        return ""
    if n == 1:
        return r"\d"
    return r"\d{%d}" % n

def _numbers_of_lengths(l, u):
    if l > u:
        return []

    if l == u:
        if l == 1:
            return []
        if l == 2:
            return [r"[1-9]\d"]
        return [r"[1-9]\d{%s}" % (l - 1)]

    return [r"[1-9]\d{%s,%s}" % (l - 1, u - 1)]

def _char_to_digit(c):
    d = ord(c) - ord("0")
    if d < 0 or d > 9:
        fail("Unxpected character: {}".format(c))
    return d

def _digit_range(l, u):
    if l == u:
        return str(l)
    if l < u:
        return "[{}-{}]".format(l, u)
    return None

def _to_next_position(x, prefix = ""):
    """Create regexes that match the range (x, 10^len(x)).

    Args:
      x - The non-inclusive lower bound.
      prefix - A prefix to add to each regex.

    Returns:
      A list of strings that that match the specified range. The returned regexes
      are in order of the ranges of numbers that would be matched.
    """
    regexes = []
    for i, c in enumerate(x.elems(), 1):
        position_str = _digit_range(_char_to_digit(c) + 1, 9)
        if position_str != None:
            regexes.append("{prefix}{position}{suffix}".format(
                prefix = prefix,
                position = position_str,
                suffix = _trailing_digit_regex(len(x) - i),
            ))
        prefix += c
    return reversed(regexes)

def _from_current_position(x, prefix = ""):
    """Create regexes that match the range [10^(len(x)-1), x).

    Args:
      x - The non-inclusive upper bound.
      prefix - A prefix to add to each regex.

    Returns:
      A list of strings that that match the specified range. The returned regexes
      are in order of the ranges of numbers that would be matched.
    """
    regexes = []
    for i, c in enumerate(x.elems(), 1):
        min_leading = 0 if prefix else 1
        position_str = _digit_range(min_leading, _char_to_digit(c) - 1)
        if position_str != None:
            regexes.append("{prefix}{position}{suffix}".format(
                prefix = prefix,
                position = position_str,
                suffix = _trailing_digit_regex(len(x) - i),
            ))
        prefix += c
    return regexes

def _get_fallback_branch_number_regexes():
    # Check that they're int first since we're relying on numeric qualities to
    # create the regexes
    branch_strs = []
    for b in sorted([b for _, b in ACTIVE_BRANCHES]):
        if type(b) != type(0):
            fail("Branch numbers in ACTIVE_BRANCHES are expected to be ints," +
                 " got {} ({})".format(type(b), b))
        branch_strs.append(str(b))

    regexes = []

    # Lower than the lowest active branch number
    regexes.extend(_numbers_of_lengths(1, len(branch_strs[0]) - 1))
    regexes.extend(_from_current_position(branch_strs[0]))

    # In between successive active branches
    for lower, upper in zip(branch_strs, branch_strs[1:]):
        if len(lower) < len(upper):
            regexes.extend(_to_next_position(lower))
            regexes.extend(_numbers_of_lengths(len(lower) + 1, len(upper) - 1))
            regexes.extend(_from_current_position(upper))
            break

        prefix = ""
        for i in range(len(lower)):
            if lower[i] != upper[i]:
                break
            prefix += lower[i]

        regexes.extend(_to_next_position(lower[i + 1:], prefix = prefix + lower[i]))

        # Get regexes for the digits between the first two different digits
        # e.g. regexes for 430 to 450 when comparing 425 and 461
        lower_bound = _char_to_digit(lower[i]) + 1
        upper_bound = _char_to_digit(upper[i]) - 1
        position_str = _digit_range(lower_bound, upper_bound)
        if position_str != None:
            regexes.append("{prefix}{position}{suffix}".format(
                prefix = prefix,
                position = position_str,
                suffix = _trailing_digit_regex(len(lower) - i - 1),
            ))

        regexes.extend(_from_current_position(upper[i + 1:], prefix = prefix + upper[i]))

    # Greater than the greatest active branch number
    regexes.extend(_to_next_position(branch_strs[-1]))

    # Catch all numbers with more digits than the highest active branch
    regexes.append(r"[1-9]\d{%d,}" % len(branch_strs[-1]))

    return regexes

# Declare a CQ group that watches all branch heads
# We won't add any builders, but SUBMIT TO CQ fails on Gerrit if there is no CQ
# group
luci.cq_group(
    name = "fallback-empty-cq",
    retry_config = cq.RETRY_ALL_FAILURES,
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/chromium/src",
        refs = (
            # \D - non-digit, match any branch that is not entirely numeric
            [r"^refs/branch-heads/.*\D.*$"] +
            [
                "^refs/branch-heads/{}$".format(regex)
                for regex in _get_fallback_branch_number_regexes()
            ]
        ),
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = "project-chromium-committers",
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = "project-chromium-tryjob-access",
        ),
    ],
)

# TODO(https://crbug.com/966115) Run a generator to set the fallback field for
# the empty CQ group until it's exposed in lucicfg or there is a better way to
# create a CQ group for all of the canary branches
def _generate_cq_group_fallback(ctx):
    cq_cfg = ctx.output["commit-queue.cfg"]

    for c in cq_cfg.config_groups:
        if c.name == "fallback-empty-cq":
            c.fallback = 1  # YES
            return c

    fail("Could not find empty CQ group")

lucicfg.generator(_generate_cq_group_fallback)
