# Robosushi - A (Semi)automatic Tool for Rolling FFmpeg

`Greetings, Adventurer!`

The Chrome Videostack team uses the open source FFmpeg project for various
decoding / demuxing tasks. We maintain a local copy of FFmpeg within the
chromium tree (//third_party/ffmpeg) that we keep reasonably up-to-date with the
FFmpeg project. This document describes how to do the "FFmpeg roll", which is
the process by which we import new versions of FFmpeg into Chromium.

"Robosushi" refers to the tool that we use to automatically merge new changes
from the FFmpeg maintainers into our local copy, build generated files, run
tests, and manage the review process.

This document assumes that you are the current FFmpeg roller. We'll describe how
you do that with robosushi, what the scripts are doing, and what to do if things
don't work right.

**Please contribute to this doc, the FFmpeg roll process, and to the scripts!**

This tool is only kept alive by the loving efforts of the Video Stack team. When
you find something that bothers you, fix it, or ask about it.

## Terminology

- **Origin** - the Chromium repo for FFmpeg. So, in this document,
  "**origin/master**" refers to Chromium's ffmpeg master branch.
- **Upstream** - the ffmpeg repo (i.e., the thing outside of Google)
- **ToT** - tip of the Chromium, rather than FFmpeg, repo.
- **Merge branch** - the branch locally, and in origin, that holds the
  intermediate results of the ffmpeg roll. If the roll is successful, then it
  will be merged back to origin/master.

## Overview

There are generally three sources of problems when updating Chromium's copy of
FFmpeg: local changes, unwanted upstream code, and test failures.

1. **Local Changes**: We maintain patches for security, correctness, or
    performance. These can conflict with upstream changes during a merge. When
    appropriate, we try to push bug fixes to upstream, however some changes
    in Chromium may either be currently be pending upstream, or may be Chromium
    specific and unlikely to ever be upstreamed. As a side effect, upstream
    changes can sometimes conflict with ours, either in a "git merge" sense or
    in a "semantically incorrect" sense.
2. **Unwanted Code**: Upstream may add code we don't want. Sometimes it's unused
   code that we'd like to compile out to
   save space. Sometimes, the new code isn't licensed appropriately / has patent
   restrictions / is otherwise unsuitable for inclusion in the Chromium project.
3. **Test Failures**: New FFmpeg code might break Chromium's tests, either
   because of an edge case in the tests, or a new bug in FFmpeg.

The FFmpeg roller has to sort these out.

## Roll early and often

Historically, FFmpeg rolls were labor-intensive, and happened only once per
Chromium milestone. We'd like to reduce that because:

- Many tests (layout, fuzzer) happen only @ToT
  - Fuzzers, in particular, benefit from having more time to run
- It's easier to figure out why things break when we have fewer upstream changes
  to look through

Therefore, it's in our interest to roll ffmpeg early and often.

While the roll is still a manual process, the robosushi scripts are written with
the intention of moving us as close to regular, automatic rolls as we can.
Please consider improving them!

## Understanding the Build Process

Before running the scripts, it is helpful to understand how Chromium (which uses
GN) builds FFmpeg (which uses shell scripts and Makefiles).

### GN Configs vs FFmpeg Configure

Chromium uses `.gn` files to build everything. FFmpeg uses a `configure` script.
To get these two to work together, `robosushi` performs the following dance:

1. **Configure**: It runs FFmpeg's `configure` script for every supported
    platform/architecture combination (Linux, ARM, etc.).
2. **Build**: It builds FFmpeg using the generated makefiles.
3. **Generate**: It analyzes the resulting `.o` files to determine exactly which
    source files were used.
4. **Translation**: It uses this information to generate the `BUILD.gn` files
    that Chromium uses.

This logic is handled by `build_ffmpeg.py` (builds locally) and `generate_gn.py`
(creates GN files). **Crucially**, if you only build for one platform (your
host), the GN files for other platforms will be incorrect. Robosushi handles
building all configurations automatically.

### DEPS and Repositories

The `//third_party/ffmpeg` repo is separate from Chromium. The `DEPS` file in
Chromium points to a specific commit hash in the FFmpeg repo.

1. You do all your work in `third_party/ffmpeg`.
2. You land your changes in `third_party/ffmpeg`.
3. **Final Step**: You update Chromium's `DEPS` file to point to your new
    commit. Until this step, your changes do not affect the rest of Chromium.

## The Safety Mechanism (Preventing Spam)

We do it that way because if you `git cl upload` a merge commit that brings in
thousands of upstream commits, Gerrit may interpret all those authors as active
participants in your CL and add them to the CC list. This spams the entire
FFmpeg community.

> **Respecting this pattern is probably the most important step you can take while doing a roll.**
>
> Our developer relationship with FFmpeg is very important and
> this specific issue has occurred in the past.

When merging from upstream, we have two goals. First, we want to `git push`
(**not** `git cl upload`) the merge commit itself to a merge branch in origin.
Note that patch fixes you add manually (e.g. as part of fixing a Fuzzer bug) can
be safely uploaded using `git cl upload`.

**Robosushi handles this automatically.** It pushes the merge to origin
immediately using `git push` (bypassing Gerrit) and sets your local branch to
track it. This ensures that when `git cl upload` runs later for your config
changes, it sees the upstream history as "already known" and only uploads your
specific changes.

## How to do the FFmpeg roll

### First, set up your host

> **The Robosushi tool assumes that you are running an x64 Linux machine!**
>
> The tool currently supports installing needed packages on Debian and Arch Linux
> distributions. Some limited support for other Linux-ish platforms, like Cygwin
> on Windows, is present but has not been tested recently and may not work. You
> may need to update [robo_lib/packages.py](robo_lib/packages.py) if using a
> different package manager.

Run this command once to install packages and toolchains:

```bash
cd /path/to/chromium/src
./media/ffmpeg/scripts/robosushi.py --setup
```

### Then, run an `auto-merge`

```bash
./media/ffmpeg/scripts/robosushi.py --auto-merge [--prompt]
```

This command runs the full pipeline. The `--prompt` option is recommended if you
want to approve each step (merging, building, uploading).

If the full pipeline succeeds, you may be able to just move to the next step.
Otherwise, check the logs outputted to `media/ffmpeg/scripts/robosushi.log` and
then jump to the
[Troubleshooting Common Failures](#troubleshooting-common-failures)
section.

### Finally, review and land your roll changelist

1. **Review**: Send the CL to the previous roller or an owner for review.
    - Include the output of `git diff origin/master configure | grep
      '^.*\[autodetect\]'` in the CL description or comments to help reviewers
        check for accidental feature enablement.
2. **Land**: Land the CL in the sushi branch.
3. **Finish**: Run `robosushi.py --auto-merge` one last time.
    - It will merge `origin/sushi-BRANCH` to `origin/master`.
    - It will trigger a DEPS roll in Chromium.
    - **Manual Step**: You must add the `*san` (ASAN, MSAN, etc.) bots to the
        DEPS roll CQ to ensure memory safety.

### Troubleshooting Common Failures

> **Robosushi outputs all logs to //media/ffmpeg/scripts/robosushi.log.**
>
> This is usually a good place to start when diagnosing issues.

#### Merge Conflicts

If `robosushi` stops and tells you there are conflicts:

- Fix the conflicts in `//third_party/ffmpeg`.
- **Tip**: If you need to edit an FFmpeg file to delete/modify something for
    Chromium, comment out the original code and add a `// Chrome:` comment
    explaining why. This helps future rollers.
- Commit the resolution locally.
- Run `robosushi.py --auto-merge` again.

#### Upload Failures

If the script fails to upload to Gerrit, verify you are a
member of the [Chrome Committers
group](https://chromium-review.googlesource.com/admin/groups/5595,members).

#### Test/Compilation Errors

If `media_unittests` or `ffmpeg_regression_tests` fail:

- Fix the code (in FFmpeg or Chromium).
- Commit the fixes locally.
- Run `robosushi.py --auto-merge` again.
- **Note**: If you changed build flags or added files, force a rebuild of the GN
    configs: `./media/ffmpeg/scripts/robosushi.py --auto-merge
    --force-gn-rebuild`

## Generated Files

**README.chromium** - This file tracks the upstream SHA1 and merge dates.
Robosushi updates this automatically.

## Git Tips and Tricks

### How to see what a patch originally looked like

During a merge conflict, it can be helpful to see the original patch that
modified a file to understand the intent of the local change.

To see the history of a file in the current branch (Chromium's copy):

```bash
git log HEAD -- path/to/file
```

You can then inspect specific commits to see the original patch:

```bash
git show <commit_hash>
```

To see the version of the file from the upstream branch you are merging:

```bash
git show MERGE_HEAD:path/to/file
```

To see the common ancestor (what the file looked like before both sides
diverged):

```bash
git show $(git merge-base HEAD MERGE_HEAD):path/to/file
```

## Manual Fallback / Under the Hood

If `robosushi` is acting up, or if you simply want to understand the magic, here
is what is happening under the hood.

1. **Branching**: A `sushi-DATE` branch is created from `origin/master`.
2. **Merging**: `upstream/master` is merged into the sushi branch.
3. **Preventing Spam**: The sushi branch is pushed to `origin/sushi-DATE`
   *without review*.
    - `git push origin sushi-DATE`
    - `git branch --set-upstream-to=origin/sushi-DATE`
    - This prevents `git cl upload` from seeing the upstream history as "new".
4. **Building**:
    - `build_ffmpeg.py` configures and builds FFmpeg for all architectures
      (Linux, ARM, etc.).
    - `generate_gn.py` scans the build artifacts to create `BUILD.gn` files.
    - `check_merge.py` verifies licenses and bad files.
5. **Testing**: `media_unittests` and `ffmpeg_regression_tests` are compiled and
   run.
6. **Upload**: `git cl upload` sends the GN config changes and patches for
   review.
7. **Merge Back**: After review, the sushi branch is merged into
   `origin/master`.
8. **DEPS Roll**: A standard Chromium DEPS roll is started pointing to the new
   `origin/master` tip.

## Want to Clean Things Up?

The parent tracking issue for FFmpeg rolls in Chromium is publicly viewable at
[crbug.com/450394703: FFmpeg Rolls In Chromium](https://crbug.com/450394703),
and should be the parent for all FFmpeg roll-related issues in the
Chromium project.

Some issues are tracked more directly in
[crbug.com/466458817: Rolling third_party/ffmpeg is unpleasant](https://crbug.com/466458817).

If you experience a new issue while performing the roll, please file an issue
(and fix it if you can!).
