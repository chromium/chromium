# Chromium for Arm Macs

This document describes the state of Chromium on Apple Silicon Macs.
The short summary is that almost everything works, without needing Rosetta.

There's a [main waterfall
bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel)
that builds for Arm. It cross-builds on an x86-64 machine.

There's a [main waterfall
bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-on-arm64-rel)
that builds for Arm on an Arm bot as well. This bot does not have Rosetta
installed.

There's also a [tester
bot](https://ci.chromium.org/p/chromium/builders/ci/mac12-arm64-rel-tests)
that continuously runs tests. Most tests pass. The tester bots don't
have Rosetta installed.

ASan builds do not yet work ([tracking bug](https://crbug.com/1271140))

## Building _for_ Arm Macs

If you are on an Intel Mac, all that's required to build Chromium for arm64
is to add a `target_cpu = "arm64"` line to your `args.gn`. Then build normally.
If you are on an Arm Mac, your build will by default be an Arm build, though
please see the section below about building _on_ Arm Macs for specific things
to keep in mind.

A note about copying a Chromium build to your Arm Mac. If you don't do a
component build (e.g. a regular `is_debug=false` build), you can just copy
over Chromium.app from your build directory. If you copy it using
macOS's "Shared Folder" feature and Finder, Chromium.app should be directly
runnable. If you zip, upload Chromium.app to some web service and download
it to an Arm Mac, browsers will set the `com.apple.quarantine` bit, which will
cause the Finder to say `"Chromium" is damanged and can't be opened. You should
move it to the Trash."`. In Console.app, the kernel will log
`kernel: Security policy would not allow process: 2204,
/Users/you/Downloads/Chromium.app/Contents/MacOS/Chromium` and amfid will log
`amfid: /Users/you/Downloads/Chromium.app/Contents/MacOS/Chromium signature not
valid: -67050`. To fix this, open a terminal and run

    % cd ~/Downloads && xattr -rc Chromium.app

After that, it should start fine.

As an alternative to building locally, changes can be submitted to the opt-in
[mac12-arm64-rel
trybot](https://ci.chromium.org/p/chromium/builders/try/mac12-arm64-rel). A small
number of [swarming bots](https://goto.corp.google.com/run-on-dtk) are also
available for Googlers to run tests on.

Arm Mac-specific bugs are tagged with the
[Mac-ARM64 label](https://crbug.com/?q=label%3Amac-arm64).

### Universal Builds

A “universal” (or “fat”) `.app` can be created from distinct x86\_64 and arm64
builds produced from the same source version. Chromium has a `universalizer.py`
tool that can then be used to merge the two builds into a single universal
`.app`.

    % ninja -C out/release_x86_64 chrome
    % ninja -C out/release_arm64 chrome
    % mkdir out/release_universal
    % chrome/installer/mac/universalizer.py \
          out/release_x86_64/Chromium.app \
          out/release_arm64/Chromium.app \
          out/release_universal/Chromium.app

The universal build is produced in this way rather than having a single
all-encompassing `gn` configuration because:

 - Chromium builds tend to take a long time, even maximizing the parallelism
   capabilities of a single machine. This split allows an additional dimension
   of parallelism by delegating the x86\_64 and arm64 build tasks to different
   machines.
 - During the mac-arm64 bring-up, the x86\_64 and arm64 versions were built
   using different SDK and toolchain versions. When using the hermetic SDK and
   toolchain, a single version of this package must be shared by an entire
   source tree, because it’s managed by `gclient`, not `gn`. However, as of
   November 2020, Chromium builds for the two architectures converged and are
   expected to remain on the same version indefinitely, so this is now more of a
   historical artifact.

## Building _on_ arm Macs

It's possible to build _on_ an arm Mac, without Rosetta. This
configuration is covered by a [main waterfall
bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-on-arm64-rel).

Checking out and building (with reclient too) should just work.
You should be able to run `fetch chromium` normally, and then build, using
`gn`, `ninja` etc like normal.

Building Chrome/Mac/Intel on an arm Mac currently needs a small local tweak
to work, see [tracking bug](https://crbug.com/1280968).

All tests should build, run, and mostly pass.
