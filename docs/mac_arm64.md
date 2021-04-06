# Chromium for arm Macs

Apple is planning on selling Macs with arm chips by the end of 2020.
This document describes the state of native binaries for these Macs.

There's a [bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel)
that builds for arm. It cross-builds on an Intel machine.

There's also a [tester
bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel-tests)
that continuously runs tests. Most tests pass.

## Building _for_ arm Macs

You can build Chrome for arm macs on an Intel Mac. To build for arm64, you have
to do 2 things:

1. use the `MacOSX11.0.sdk` that comes with Xcode 12.2. If you're on Google's
   corporate network, this SDK is part of the hermetic toolchain and will be
   used automatically. Otherwise, manually download and install this version of
   Xcode and, if necessary, make it the active Xcode with `xcode-select`.

2. Add `target_cpu = "arm64"` to your `args.gn`.

Then build normally.

To run a built Chromium, you need to copy it to your arm mac. If you don't
do a component build (e.g. a regular `is_debug=false` build), you can just
copy over Chromium.app from your build directory. If you copy it using
macOS's "Shared Folder" feature and Finder, Chromium.app should be directly
runnable. If you zip, upload Chromium.app to some web service and download
it on the DTK, browsers will set the `com.apple.quarantine` bit, which will
cause Finder to say `"Chromium" is damanged and can't be opened. You should
move it to the Trash."`. In Console.app, the kernel will log
`kernel: Security policy would not allow process: 2204,
/Users/you/Downloads/Chromium.app/Contents/MacOS/Chromium` and amfid will log
`amfid: /Users/you/Downloads/Chromium.app/Contents/MacOS/Chromium signature not
valid: -67050`. To fix this, open a terminal and run

    % cd ~/Downloads && xattr -rc Chromium.app

After that, it should start fine.

As an alternative to building locally, changes can be submitted to the opt-in
[mac-arm64-rel
trybot](https://ci.chromium.org/p/chromium/builders/try/mac-arm64-rel). A small
number of [swarming bots](https://goto.corp.google.com/run-on-dtk) are also
available for Googlers to run tests on.

You can follow the [Mac-ARM64 label](https://crbug.com/?q=label%3Amac-arm64) to
get updates on progress.

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

It's possible to build _on_ an arm Mac, without Rosetta. However, this
configuration is not yet covered by bots, so it might be broken from time to
time. If you run into issues, complain on https://crbug.com/1103236

Also, several of the hermetic binaries in depot\_tools aren't available for
arm yet. Most notably, `vpython` is not yet working ([tracking
bug](https://crbug.com/1103275)). `vpython` is needed by `git cl`, so things
like `git cl upload` don't yet work on an arm Mac. The build will also use
system `python`, `python3`, and `git`, instead of depot\_tools's hermetic
versions for now.

However, enough works to be able to check out and build, with some setup.

1. opt in to arm64 binaries from cipd by running

       echo arm64 > $(dirname $(which gclient))/.cipd_client_platform

   (If you want to build `tools/metrics:histograms_xml`, you also need to
   `echo arm64 > third_party/depot_tools/.cipd_client_platform` in your
   chromium checkout. This isn't needed for building chromium or any test
   targets.)

2. opt out of vpython by running

       export VPYTHON_BYPASS="manually managed python not supported by chrome operations"

With this, you should be able to run `fetch chromium` normally, and then
build, using `gn`, `ninja` etc like normal.

gtest-based binaries should build, run, and mostly pass. Web tests probably
don't work yet due to lack of an arm apache binary
([tracking bug](https://crbug.com/1190885)).

(goma does not yet work, [internal tracking bug](https://b/183118231).)
