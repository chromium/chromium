# Updating Clang and Rust

We distribute prebuilt packages of LLVM binaries, including clang, rustc, and
lld, that all developers and bots pull using `gclient runhooks` (which
also runs automatically as part of `gclient sync`). These binaries are
just regular LLVM binaries built at a fixed upstream revision. This document
describes how to build a new package, and update Chromium to use it.
An archive of all packages built so far is at https://is.gd/chromeclang.

It's recommended that you read our
[primer on clang gardening](clang_gardening.md) first, to get an overview of
our infrastructure.

1.  Check that https://ci.chromium.org/p/chromium/g/chromium.clang/console
    looks reasonably green. Red bots with seemingly normal test failures are
    usually ok, that likely means the test is broken with the stable Clang as
    well.
1.  Find a recent CL that ran the upload bots.
    1. This will probably be a [dry run CL](clang_gardening.md#dry-runs), but
       you can also [create your own](#making-your-own-roll-cl).
1. Run [go/chrome-promote-clang](https://goto.google.com/chrome-promote-clang),
    passing in the revision of the Clang and Rust package version you want to
    push. This will copy the specified packages from the untrusted staging
    bucket to the production bucket, allowing them to be pulled by `depot_tools`.
    For example:

    ```
    $ /path/to/copy_staging_to_prod_and_goma.sh --clang-rev llvmorg-21-init-5118-g52cd27e6-1 --rust-rev f7b43542838f0a4a6cfdb17fbeadf45002042a77-1
    ```
    1. The clang and rust revisions to pass here correspond to the names of the
       packages produced by the upload bots. You can see them by selecting a bot,
       opening the logs for the "package clang/rust" step, and scrolling to the
       bottom where it prints the upload logs.
    1. For dry run CLs, the old and new revisions also appear in the CL title.
    1. Writing to the production bucket requires special permissions.
       Then it will push the packages to RBE. If you do not have the necessary
       credentials to do the upload, ask the lexan team to find someone who
       does. You can also email clang@chromium.org.
1. Switch to a local branch with the contents of the roll CL. Make sure it's
   up-to-date before proceeding (pull and sync)!
    1. The most convenient way to do this is to run `git cl patch <gerrit url>`,
       which will download the contents of the patch and associate it with the
       existing dry run CL. We prefer to re-use the CL instead of creating a
       new one.
1.  Run `tools/clang/scripts/sync_deps.py` to update the deps entries in DEPS.
1.  Run `gclient sync` to download those packages.
1.  Run `tools/rust/gnrt_stdlib.py` to update the GN files for the Rust standard
    library.
1.  Commit and upload your changes.
1.  Run an exhaustive set of try jobs to test the new compiler. The CL
    description created previously by upload_revision.py includes
    `Cq-Include-Trybots:` lines for all needed bots, so it's sufficient to just
    run `git cl try` (or hit "CQ DRY RUN" on gerrit).
    1. Much like a mega CQ run, it's common for some of the bots to be red for
       unrelated reasons. You may need to do some digging to determine if a
       problem is due to the roll CL or not.
1.  Submit the CL! The bots will now pull the new package, and developers will
    get it the next time they sync.

## Making your own roll CL

The dry run CLs aren't magic; you can create one yourself! The main reason for
doing this is to update the toolchain package without actually rolling forward.
This is required, for example, after someone has changed one of our [compiler plugins](writing_clang_plugins.md), or if we want to try building clang with different settings (this is rare).

To create your own roll CL
1. Make sure your workspace is up to date (pull and sync).
1. Change the CLANG_REVISION variable in
   [tools/clang/scripts/update.py](https://source.chromium.org/chromium/chromium/src/+/main:tools/clang/scripts/update.py;l=42;drc=6f920cff25ae8852f16c3bb71007b7a9aaebc497)
to the new version you want to roll to.
1. If you're not rolling forward (i.e. you didn't change the CLANG_REVISION),
   then change the SUB_REVISION to a new number. This will ensure the uploaded
   package has a different name [1].
1. Repeat the previous steps for Rust in [tools/rust/update_rust.py](https://source.chromium.org/chromium/chromium/src/+/main:tools/rust/update_rust.py;drc=8758f776da008edac978399f0d0e703ffd5e33b7).
1. Upload the CL to gerrit.
1. Run the upload bots (Choose Tryjobs > filter by `upload_` > select all).

You can skip the rust steps if you only want to update the clang package, and
vice-versa.

Instead of doing the above steps manually, you can also manually run
`tools/clang/scripts/upload_revision.py` and pass it the revisions you want to
roll to.

[1] IMPORTANT CAVEAT ([bug](https://crbug.com/496183734)): If the commit hash +
subrevision pair have already been used, the upload bots will fail to upload
the new package, but they will still consider the run a success. You must always
make sure to increment the subrevision before a new run, and ideally check the
output of the upload bots to make sure they didn't skip an upload. If someone
else is also working on packaging, make sure to coordinate your subrevision
numbers. If in doubt, just add an extra digit or something.

## On local patches and cherry-picks

Except for the addition of Clang plugins, which do not affect the compiler
output, Chromium's LLVM binaries are vanilla builds of the upstream source code
at a specific revision, with no alterations.

We believe this helps when working with upstream: our compiler should behave
exactly the same as if someone else built LLVM at the same revision, making
collaboration easier.

It also creates an incentive for stabilizing the HEAD revision of LLVM: since
we ship a vanilla build of an upstream revision, we have to ensure that a
revision can be found which is stable enough to build Chromium and pass all its
tests. While allowing local cherry-picks, reverts, or other patches, would
probably allow more regular toolchain releases, we believe we can perform
toolchain testing and fix issues fast enough that finding a stable revision is
possible, and that this is the right trade-off for us and for the LLVM
community.

For Rust, since the interface between tip-of-tree rustc and LLVM is less
stable, and since landing fixes in Rust is much slower (even after approval, a
patch can take more than 24 hours to land), we allow cherry-picks of such fixes
in our Rust toolchain build.

## Performance regressions

After doing a clang roll, you may get a performance bug assigned to you
([example](https://crbug.com/1094671)). Some performance noise is expected
while doing a clang roll.

You can check all performance data for a clang roll via
`https://chromeperf.appspot.com/group_report?rev=XXXXXX`, where `XXXXXX` is the
Chromium revision number, e.g. `778090` for the example bug (look in the first
message of the performance bug to find this). Click the checkboxes to display
graphs. Hover over points in the graph to see the value and error.

Serious regressions require bisecting upstream commits (TODO: how to repro?).
If the regressions look insignificant and there is green as well as red, you
can close the bug as "WontFix" with an explanation.

## Adding files to the clang package

The clang package is downloaded unconditionally by all bots and devs. It's
called "clang" for historical reasons, but nowadays also contains other
mission-critical toolchain pieces besides clang.

We try to limit the contents of the clang package. They should meet these
criteria:

- things that are used by most developers use most of the time (e.g. a
  compiler, a linker, sanitizer runtimes)
- things needed for doing official builds

## Adding a New Package

If you want to make artifacts available that do not meet the criteria for
being included in the "clang" package, you can make package.py upload it to
a separate zip file and then download it on an opt-in basis by using
update.py's --package option.  Here is [an example of adding a new package].

To test changes to `package.py`, change `CLANG_SUB_REVISION` in `update.py` to
a random number above 2000 and run the `*_upload_clang` trybots.

Once the change to `package.py` is in, file a bug under `Tools > LLVM`
requesting that a new package be created ([example bug]).

Once it's been uploaded and rolled, you can download it via:

```
tools/clang/scripts/update.py --package your-package-name
```

[an example of adding a new package](https://chromium-review.googlesource.com/c/chromium/src/+/5463029)
[example bug]: https://crbug.com/335730441
