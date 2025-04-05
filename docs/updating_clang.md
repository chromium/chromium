# Updating clang

We distribute prebuilt packages of LLVM binaries, including clang and lld, that
all developers and bots pull at `gclient runhooks` time. These binaries are
just regular LLVM binaries built at a fixed upstream revision. This document
describes how to build a package at a newer revision and update Chromium to it.
An archive of all packages built so far is at https://is.gd/chromeclang

1.  Check that https://ci.chromium.org/p/chromium/g/chromium.clang/console
    looks reasonably green. Red bots with seemingly normal test failures are
    usually ok, that likely means the test is broken with the stable Clang as
    well.
1.  Sync your Chromium tree to the latest revision to pick up any plugin
    changes.
1.  Run
    [go/chrome-promote-clang](https://goto.google.com/chrome-promote-clang),
    passing in the revision of the Clang and Rust package version you want to
    push. For example:

    ```
    $ copy_staging_to_prod_and_goma.sh --clang-rev llvmorg-21-init-5118-g52cd27e6-1 --rust-rev f7b43542838f0a4a6cfdb17fbeadf45002042a77-1
    ```

    Alternatively, use
    [go/chrome-push-clang-to-goma](https://goto.google.com/chrome-push-clang-to-goma).
    This takes a recent dry run CL to update clang, and if the trybots were
    successful it will copy the binaries from the staging bucket to the
    production one.

    Writing to the production bucket requires special permissions.
    Then it will push the packages to RBE. If you
    do not have the necessary credentials to do the upload, ask
    clang@chromium.org to find someone who does.
    *   Alternatively, to create your own roll CL, you can manually run
	`tools/clang/scripts/upload_revision.py` with a recent upstream LLVM
    commit hash as the argument. After the `*_upload_clang` and `*_upload_rust`
    trybots are successfully finished, run
	[go/chrome-promote-clang](https://goto.google.com/chrome-promote-clang)
	with the new Clang and Rust package names.
1.  Run `tools/clang/scripts/sync_deps.py` to update the deps entries in DEPS.
1.  `gclient sync` to download those packages.
1.  Run `tools/rust/gnrt_stdlib.py` to update the GN files for the Rust standard library.
1.  Run an exhaustive set of try jobs to test the new compiler. The CL
    description created previously by upload_revision.py includes
    `Cq-Include-Trybots:` lines for all needed bots, so it's sufficient to just
    run `git cl try` (or hit "CQ DRY RUN" on gerrit).
1.  Commit the roll CL from the previous step.
1.  The bots will now pull the prebuilt binary, and RBE will have a matching
    binary, too.

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

# Adding a New Package

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
