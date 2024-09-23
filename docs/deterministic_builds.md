Deterministic builds
====================

Chromium's build is deterministic. This means that building Chromium at the
same revision will produce exactly the same binary in two builds, even if
these builds are on different machines, in build directories with different
names, or if one build is a clobber build and the other build is an incremental
build with the full build done at a different revision. This is a project goal,
and we have bots that verify that it's true.

Furthermore, even if a binary is built at two different revisions but none of
the revisions in between logically affect a binary, then builds at those two
revisions should produce exactly the same binary too (imagine a revision that
modifies code `chrome/` while we're looking at `base_unittests`). This isn't
enforced by bots, and it's currently not always true in Chromium's build -- but
it's true for some binaries at least, and it's supposed to become more true
over time.

Having deterministic builds is important, among other things, so that swarming
can cache test results based on the hash of test inputs.

This document currently describes how to handle failures on the deterministic
bots.

There's also
https://www.chromium.org/developers/testing/isolated-testing/deterministic-builds;
over time all documentation over there will move to here.

Handling failures on the deterministic bots
-------------------------------------------

This section describes what to do when `compare_build_artifacts` is failing on
a bot.

The deterministic bots make sure that building the same revision of chromium
always produces the same output.

To analyze the failing step, it's useful to understand what the step is doing.

There are two types of checks.

1. The full determinism check makes sure that build artifacts are independent
   of the name of the build directory, and that full and incremental builds
   produce the same output. This is done by having bots that have two build
   directories: `out/Release` does incremental builds, and `out/Release.2`
   does full clobber builds. After doing the two builds, the bot checks
   that all built files needed to run tests on swarming are identical in the
   two build directories. The full determinism check is currently used on
   Linux and Windows bots. (`Deterministic Linux (dbg)` has one more check:
   it doesn't use reclient for the incremental build, to check that using
   reclient doesn't affect built files either.)

2. The simple determinism check does a clobber build in `out/Release`, moves
   this to a different location (`out/Release.1`), then does another clobber
   build in `out/Release`, moves that to another location (`out/Release.2`),
   and then does the same comparison as done in the full build. Since both
   builds are done at the same path, and since both are clobber builds,
   this doesn't check that the build is independent of the name of the build
   directory, and it doesn't check that incremental and full builds produce
   the same results. This check is used on Android and macOS, but over time
   all platforms should move to the full determinism check.

### Understanding `compare_build_artifacts` error output

`compare_build_artifacts` prints a list of all files it compares, followed by
`": None`" for files that have no difference. Files that are different between
the two build directories are followed by `": DIFFERENT(expected)"` or
`": DIFFERENT(unexpected)"`, followed by e.g. `"different size: 195312640 !=
195311616"` if the two files have different size, or by e.g. `"70 out of
5091840 bytes are different (0.00%)"` if they're the same size.

You can ignore lines that say `": None"` or `": DIFFERENT(expected)"`, these
don't turn the step red. `": DIFFERENT(expected)"` is for files that are known
to not yet be deterministic; these are listed in
[`src/tools/determinism/deterministic_build_ignorelist.pyl`][1].  If the
deterministic bots turn red, you usually do *not* want to add an entry to this
list, but figure out what introduced the nondeterminism and revert that.

[1]: https://chromium.googlesource.com/chromium/src/+/HEAD/tools/determinism/deterministic_build_ignorelist.pyl

If only a few bytes are different, the script prints a diff of the hexdump
of the two files. Most of the time, you can ignore this.

After this list of filenames, the script prints a summary that looks like

```
Equals:           5454
Expected diffs:   3
Unexpected diffs: 60
Unexpected files with diffs:
```

followed by a list of all files that contained `": DIFFERENT(unexpected)"`.
This is the most interesting part of the output.

After that, the script tries to compute all build inputs of each file with
a difference, and compares the inputs. For example, if a .exe is different,
this will try to find all .obj files the .exe consists of, and try to compare
these too. Nowadays, the compile step is usually deterministic, so this can
usually be ignored too. Here's an example output:

```
fixed_build_dir C:\b\s\w\ir\cache\builder\src\out\Release exists. will try to use orig dir.
Checking verifier_test_dll_2.dll.pdb difference: (1 deps)
```

### Diagnosing bot redness

Things to do, in order of involvedness and effectiveness:

- Look at the list of files following `"Unexpected files with diffs:"` and check
  if they have something in common. If the blame list on the first red build
  has a change to that common thing, try reverting it and see if it helps.
  If many, seemingly unrelated files have differences, look for changes to
  the build config (Ctrl-F ".gn") or for toolchain changes (Ctrl-F "clang").

- The deterministic bots try to upload a tar archive to Google Storage.
  Use `gsutil.py ls gs://chrome-determinism` to see available archives,
  and use e.g. `gsutil.py cp gs://chrome-determinism/Windows\
  deterministic/9998/deterministic_build_diffs.tgz .` to copy one archive to
  your workstation. You can then look at the diffs in more detail. See
  https://bugs.chromium.org/p/chromium/issues/detail?id=985285#c6 for an
  example.

- Try to reproduce the problem locally. First, set up two build directories
  with identical args.gn. Then do a full build at the last known green revision
  in the first build directory:

    ```
    $ gn clean out/gn
    $ autoninja -C out/gn base_unittests
    ```

  Then, sync to the first bad revision (make sure to also run `gclient sync`
  to update dependencies), do an incremental build in the
  first build directory and a full build in the second build directory, and
  run `compare_build_artifacts.py` to compare the outputs:

    ```
    $ autoninja -C out/gn base_unittests
    $ gn clean out/gn2
    $ autoninja -C out/gn2 base_unittests
    $ tools/determinism/compare_build_artifacts.py \
         --first-build-dir out/gn \
         --second-build-dir out/gn2 \
         --target-platform linux
    ```

  This will hopefully reproduce the error, and then you can binary search
  between good and bad revisions to identify the bad commit.


Things *not* to do:

- Don't clobber the deterministic bots. Clobbering a deterministic bot will
  turn it green if build nondeterminism is caused by incremental and full
  clobber builds producing different outputs. However, this is one of the
  things we want these bots to catch, and clobbering them only removes the
  symptom on this one bot -- all CQ bots will still have nondeterministic
  incremental builds, which is (among other things) bad for caching. So while
  clobbering a deterministic bot might make it green, it's papering over issues
  that the deterministic bots are supposed to catch.

- Don't add entries to `src/tools/determinism/deterministic_build_ignorelist.py`.
  Instead, try to revert commits introducing nondeterminism.
