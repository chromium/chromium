# Chromium Branch Sheriffing

This document describes how to be a Chromium *branch* sheriff and how sheriffing
on a branch differs from sheriffing on trunk. For trunk sheriffing guidance, see
[//docs/sheriff.md][sheriff-md].

[TOC]

## Philosophy

The goals of a branch sheriff are quite similar to those of a trunk sheriff.
Branch sheriffs need to ensure that:

1. **Compile failures get fixed**, because compile failures on branches block
all tests (both automated and manual) and consequently reduce our confidence
in the quality of what we're shipping, possibly to the point of blocking
releases.
2. **Consistent test failures get repaired**, because they similarly reduce
our confidence in the quality of our code.

**Communication** is important for sheriffs in general, but it's particularly
important for branch sheriffs. Over the course of your shift, you may need to
coordinate with trunk sheriffs, troopers, release TPMs, and others -- don't
hesitate to do so, particularly if you have questions.

Points of contact (i.e. platform-specific sheriffs) can be found
[here](http://goto.google.com/chrome-branch-sheriffing#points-of-contact).

## Processes

In general, you'll want to follow the same processes outlined in
[//docs/sheriff.md][sheriff-md]. There are some differences, though.

### Checkout

You'll need to ensure that your checkout is configured to check out the branch
heads. You can do so by running

```
  src $ gclient sync --with_branch_heads
```

> This only needs to be done once, though running it more than once won't hurt.

You may also need to run:

```
  src $ git fetch
```

Once you've done that, you'll be able to check out branches:

```
  src $ git checkout branch-heads/$BRANCH_NUMBER  # e.g. branch-heads/4044 for M81
  src $ gclient sync
```

To determine the appropriate branch number, you can either use
[chromiumdash](#chromiumdash) or check [milestone.json][milestone-json]
directly.

### Findit

As FindIt is not available on branches, one way to try to find culprits is using
`git bisect` locally and upload changes to a gerrit CL and run the needed
trybots to check. This is especially useful when the errors are not reproducible
on your local builds or you don't have the required hardware to build the failed
tests.

### Flaky tests

Flaky tests that are disabled on trunk should also be disabled on any branches
with frequent failures of that test. If a trunk CL lands with no change other
than to disable one or more tests ([example](https://crrev.com/c/2507299)) and
it has an associated bug and the release manager is cc'd on the bug, you can and
should cherrypick it to the affected branch without requesting merge approval.

On the other hand, if you believe that a flake was introduced by a cherry-pick
to the branch in question and is not flaky on trunk, you will need to create a
new CL to disable it only on the branch and go through the usual merge request
process.

Note: there is little value in merging changes to the stable release
branch when the next milestone's stable release is less than a week away
(since there are usually no planned stable respins at that point).
You can find release dates on [chromiumdash][chromiumdash-schedule].

### Landing changes

When you need to land a change to a branch, you'll need to go through [the same
merge approval process](./process/merge_request.md) as other cherry-picks (see
exception for flaky tests above). You should feel free to ping the relevant
release TPM as listed on [chromiumdash][chromiumdash-schedule].

## Tools

### Sheriff-o-Matic

Use the [branch SoM console][sheriff-o-matic] rather than the main chromium
console.

### Consoles

Use the [beta][main-beta] and [stable][main-stable] branch consoles rather than
the main console. A new console is created for each milestone. They are named
"Chromium M## Console" and can be found under the
[Chromium Project](https://ci.chromium.org/p/chromium).

### Monorail issues (crbug)

Refer and use the
[Sheriff-Chrome-Release label](https://bugs.chromium.org/p/chromium/issues/list?q=label%3ASheriff-Chrome-Release)
to find and tag issues that are of importance to Branch sheriffs.

### Chromiumdash

[chromiumdash][chromiumdash] can help you determine the branch number for a
particular milestone or channel, along with a host of other useful information:

  * [Branches][chromiumdash-branches] lists the branches for each milestone.
  * [Releases][chromiumdash-releases] lists the builds currently shipping to
    each channel, which can help map from channel to milestone or to branch.
  * [Schedule][chromiumdash-schedule] lists the relevant dates for each
    milestone and includes the release TPMs responsible for each milestone by
    platform.

### Rotation

The current branch sheriff is listed [here][rotation-home]. The configuration
and source of truth for the schedule lives [here][rotation-config]. To swap,
simply send a CL changing schedule at the bottom of the file.
You can also use [Oncall Swapper](https://oncallswapper.corp.google.com/)
to find the swap and submit the CL for you.

[chromiumdash]: https://chromiumdash.appspot.com
[chromiumdash-branches]: https://chromiumdash.appspot.com/branches
[chromiumdash-releases]: https://chromiumdash.appspot.com/releases
[chromiumdash-schedule]: https://chromiumdash.appspot.com/schedule
[main-beta]: https://ci.chromium.org/p/chromium/g/main-m81/console
[main-stable]: https://ci.chromium.org/p/chromium/g/main-m80/console
[milestone-json]: https://goto.google.com/chrome-milestone-json
[rotation-home]: https://goto.google.com/chrome-branch-sheriff-amer-west
[rotation-config]: https://goto.google.com/chrome-branch-sheriff-amer-west-config
[sheriff-md]: /docs/sheriff.md
[sheriff-o-matic]: https://sheriff-o-matic.appspot.com/chrome_browser_release
