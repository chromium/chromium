# Using Skia Autoroller to manage third-party dependencies

[TOC]

If a dependency is a Git repository and can be used **without modification**,
you should follow this guide to setup mirroring and Skia Autoroller to keep
your dependency fresh.

In technical terms, "**without modification**" means:

* The dependency MUST be checked out as a Git submodule, inside
  `third_party/<name>/src/` directory
* The checked out commit MUST exist in the upstream repository, unless you're
  making [Urgent Chromium patches](#applying-urgent-chromium-patches)

In practice, this means:

* You MUST NOT modify the dependency's source code to satisfy Chromium's
  requirements, or to make it build in Chromium (e.g. change `std::span` usage
  to Chromium's `base::span`).
* You MUST pull in the entire repository into Chromium. Pulling in only certain
  directories or files (i.e. a subset of the repository) **aren't supported**.
* You CAN add files alongside `third_party/<name>/src/` to integrate the
  dependency with Chromium. Typically: BUILD.gn, README.chromium metadata,
  C++ header guards
  ([example](https://source.chromium.org/chromium/chromium/src/+/main:third_party/microsoft_dxheaders/include/directml.h;drc=37a8609bbbe5200fa034718304b3c3a04296240b)),
  and integration tests.


**Recommended for:** Well-maintained or mature projects. The project maintainers
should be willing to accept outside contributions (e.g. we can upstream a bug fix),
and should be reasonably active in responding to pull requests and security bugs.

**Examples:**

* Chromium projects: Dawn, v8
* Google projects: [re2](https://github.com/google/re2)
* Well maintained projects: [zstd](https://github.com/facebook/zstd),
  [jsoncpp](https://github.com/open-source-parsers/jsoncpp)


## Directory structure

```
third_party/
  README.chromium
  OWNERS
  BUILD.gn    <-- Optional, build rules and files for Chromium integration
  src/        <-- Git submodule. MUST point to a commit in the upstream
    xxx       <-- Upstream's content, managed by gclient.
```

## Initial setup

### Step 1: Obtain approval to include the dependency in Chromium

You first MUST obtain approval from Chrome ALTs to include the dependencies
in Chromium. Please refer to
["Adding to third-party > Before you start"](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md#before-you-start).

### Step 2: Ask a Chromium Git Admin to mirror the repository to Gerrit
Create a Git Admin request
[here](https://g-issues.chromium.org/issues/new?component=1456263&template=1923295).
Use the following template (feel free to copy the following Markdown snippet
into the bug description):

```
=== Create a third-party repo mirror ===

Googlesource host: **chromium**

Upstream repo: **Link to upstream repo, like `https://github.com/abc/xyz`**

Where to mirror: **external/<git-host>/<repo-name>**, like `external/github.com/abc/xyz`

What to mirror: **all branches and tags**, imported with `upstream/` prefix

Permissions: **Inherit from** `chromium/deps` project
```

### Step 3. Add the mirrored dependency to Chromium

You need to make a CL to add this dependency to Chromium. This CL typically contains:
  * DEPS file change
  * Git submodules change
  * README.chromium
  * Top-level OWNERS entry to grant Git submodule's ownership
  * Files to integrate with Chromium, e.g. BUILD.gn, DEPS
  * Tests to verify the dependency works as intended and to detect breakages

Please follow the instructions here:
[https://chromium.googlesource.com/chromium/src/+/main/docs/dependencies.md#adding-dependencies](docs/dependencies.md#adding-dependencies)

### Step 4. Setup Skia Autoroller to auto-update the dependency

Create a Skia Infrastructure request to setup the autoroller
[here](https://issues.skia.org/issues/new?pli=1&component=1389291&template=1850622).

Please provide all of the requested information in the issue template, and
mention **"I want the autoroller to update both the DEPS entry and the
README.chromium file"**.

**Additionally**, mention how the autoroller should check for new releases.

In general, we recommend to roll to latest Git tag (usually corresponds to a
versioned release). You need to provide a regular expression to extract a
integer tuple from the tag name. The autoroller will roll to the Git tag
with the largest tuple (e.g. `(1,2,3)` is greater than `(1,1,2)`).

For example, the following regular expression matches semantic version style
`vX.Y.Z` Git version tags in the mirrored repo:
`^upstream/v(\\d+)\\.(\\d+)\\.(\\d+)$`.

Alternatively, you can request to track the latest commit in a branch (e.g.
`main`). This is commonly used for Google maintained projects.

After Skia Autoroller is setup, it will propose future CLs to update the
revision in DEPS, update Git submodules and set metadata fields in
README.chromium.


## Keeping the dependency fresh

Skia autoroller periodically checks the upstream for newer releases and
creates an autoroll CL for you to review.

### When an autoroll CL is proposed
The autoroller will send you a CL for review when a roll happens. You should
review the upstream changes introduced by the roll is reasonable, then LGTM
and submit the CL.

The autoroller tries to generate a CL description to summarize the upstream
change (e.g. a list of commits). In the rare cases where the CL description
generation fails, you should check upstream release notes and review the diff.


### When an autoroll CL fails
This could happen when the upstream introduces a breaking change (caught by
your tests on Chromium CQ), or if BUILD.gn files need updating.

You should **take-over** the autoroller's CL, fix breakages, have another
committer review the change, then submit the CL.

We recommend you download the CL from Gerrit Web UI, using the "Download"
button, update the CL locally, then upload it for review (i.e. you
become the autoroll CL's uploader).

### Applying urgent Chromium patches
In general, you should work with the upstream to fix bugs. Skia Autoroller will
automatically update the dependency in Chromium when the upstream publishes a
new tag or a new commit (depending on your choice in Step 2).

In the rare cases where you need to apply a patch before the upstream makes the
release, please follow the following steps.

First, edit the Git submodule locally, test and commit the change.

Then, **inside the dependency's `src` directory**, run
`git cl upload --target_branch=<branch_name>` to upload your change for review.

We recommend naming the branches like so to keep track of changes:
`<upstream_tag_or_revision>-chromium_patch-<number-or-description>`

Then, get another committer to review the change and submit it to the mirrored
repository.

Finally, update chromium/src's DEPS entry and .gitmodules to point to the
new commit, and land this CL. You can make use of
[depot_tools `roll-dep`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/depot_tools/roll_dep.py)
script.

*** note
The above procedure is intended as a temporary "break glass" fix. You're
expected to upstream and the fix, and roll in the updated dependency as soon
as possible.

You MAY need to pause Skia Autoroller depending on the upstream's release
cadence (e.g. will the next release include my patch).

* If you don't pause the autoroller, it will propose a change to use an
  upstream tag or a commit that's newer than your patch commit.
* This WILL discard your patch if your pull request isn't merged into the
  upstream.

**Warning:** If you've paused the autoroller for too long, and the dependency
becomes stale, you MAY be asked to adopt a different well-lit path that better
handles patching.
***
