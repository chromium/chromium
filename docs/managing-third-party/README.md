# Managing third-party dependencies

[TOC]

Before reading documentation in this directory, please check the general
policies in
[Adding third_party Libraries](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md).
You MUST obtain the required approvals before adding a third-party dependency
and accept the responsibility as a dependency owner.

This directory contains how-to guides on managing your dependency with autoroll
tooling (aka. well-lit paths). Please refer to individual files to understand
their use cases and constraints. You should adopt **one well-lit path** that
suits your need (e.g. code organization, need for patching).

You can request to be exempted from the well-lit paths if your dependency does
not release new versions or updating would introduce performance regressions.
Please refer to [Autoroll Exceptions](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md#autoroll-exceptions) for more detail.

If you are not sure which path to take, please file a bug in
["Chromium > Third Party > Freshness" component](https://issues.chromium.org/issues/new?noWizard=true&component=1900398&template=2268619)
for consultation.

If you're a Googler, you can alternatively email
[chrome-ssci-team@google.com](mailto:chrome-ssci-team@google.com).

## Well-lit paths {#wlps}

### [Skia Autoroller](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/managing-third-party/skia-autoroller.md)

Recommended for dependencies that are imported as Git submodules, and can be
used without modifying the dependency's source code. You can add BUILD.gn rules
and tests alongside your dependency.

### [Crowbar Workflow](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/managing-third-party/crowbar-workflow.md)

Recommended for checked-in code (e.g. a subset of files), or if you need to
patch upstream source code.

### [3PP + CIPD](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/cipd_and_3pp.md)

Recommended for dependencies which cannot be built from source.


## Handling onboarding bugs

Owners of existing dependencies will progressively receive bugs to onboard them
to autoroll mechanisms as they become available. The bugs propose a well-lit
path suitable for the dependency based on static analysis, but you may choose
any of the three above.

There are several steps involved with onboarding a dependency to automated
updates. We recommend doing them in the below order: landing a change between
each step, to reduce the chance of build breakage and make changes more
reviewable.

### Prepare

1. Determine what state the dependency is in. Understanding the current state
will help you figure out what automated paths support your needs and what
pre-work needs to be done.
    * Is the dependency a pristine copy of the upstream?
    * Is it a partial checkout of a git repo, which removes large files or
    directories?
    * Is the Chromium copy carrying patches? Are the patches clean, or do they
    also contain formatting changes?
    * Does the upstream tag release versions, or will a time based roll be
    more effective?
    * Are there post update code generation steps?
    * Does the metadata in the README reflect the version that's actually used
    in Chromium? Have there been historic cherry picks which put us between
    versions?
    * Is there a backward dependency on components like `//base` which make
    git submodule use undesirable?
1. Ensure the dependency uses the [recommended directory structure](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md#standard-dep-structure). If it does not, this
is the optimal time to fix it.
    * We recommend you do this, resolve any BUILD breakages and send a CL to
    land this restructure before making any further changes.
1. Clean up any existing patches. Some dependencies may already have patch
    files; but there's no guarantee they're in good condition, or still required.
    * Patches MUST NOT contain formatting changes.
    * If your dependency requires patches you must create a `patches/`
    directory, following the [recommended directory structure](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md#standard-dep-structure) and store patches
    there, as numbered files. We recommend one patch file per logic change
    to make future maintenance easier.
    * Patches should apply cleanly to a fresh checkout of the upstream, at the
    same version. *A fresh copy of the upstream at the same revision, with
    patches applied, should produce the same logical file content as what is
    currently used in Chromium.* If you're backing out formatting changes,
    there may still be a git diff.

### Update

Now that your dependency is cleaned up, it's time to update.

*   It's recommended to do a one-time update to the newest version before
    onboarding to automation. This will allow a clean onboarding and allow the
    testing of patches to identify any edge cases.
*   Dependencies which are extremely old may benefit from several incremental
    updates rather than one large update.
*   Ensure the README.chromium metadata is correct and complete.

### Onboard

Determine which well-lit path your dependency should be using and create the
required configuration, following the [guides above](#wlps).

*   Mark the bug as `Fixed` and re-assign to chrome-ssci-team@google.com
for verification.
