Chromium for arm Macs
=====================

Apple is planning on selling Macs with arm chips by the end of 2020.
This document describes the state of native binaries for these Macs.

There's a [bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel)
that builds for arm. It cross-builds on an Intel machine.

There's also a [tester
bot](https://ci.chromium.org/p/chromium/builders/ci/mac-arm64-rel-tests)
that continuously runs tests. Most tests pass.

Building _for_ arm Macs
-----------------------

You can build Chrome for arm macs on an Intel Mac. To build for arm64, you have
to do 2 things:

1. use the `MacOSX11.0.sdk` that comes with
   Xcode 12 beta. If you're on Google's corporate network, edit your `.gclient`
   file and add this `custom_vars`:

       "custom_vars": { "mac_xcode_version": "xcode_12_beta" },

   Then just run `gclient sync` and you'll automatically get that SDK and will
   build with it.

   Otherwise, manually download and install the current Xcode 12 beta and make
   it the active Xcode with `xcode-select`.

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

Building _on_ arm Macs
-----------------------

Building _on_ arm Macs means that all the tools we use to build chrome need
to either work under Rosetta or have a native arm binary.

We think it makes sense to use arch-specific binaries for stuff that's
downloaded in a hook (things pulled from cipd and elsewhere in gclient hooks --
I think this includes clang, gn, clang-format, python3 in depot\_tools, ...),
and fat binaries for things where we'd end up downloading both binaries anyways
(mostly ninja-mac in depot\_tools). There's a
[tracking bug](https://crbug.com/1103236) for eventually making native arm
binaries available for everything.

Go does [not yet](https://github.com/golang/go/issues/38485) support building
binaries for arm macs, so all our go tooling needs to run under Rosetta for
now.

`cipd` defaults to downloading Intel binaries on arm macs for now, so that
they can run under Rosetta.

If a binary runs under Rosetta, the subprocesses it spawns by default also
run under rosetta, even if they have a native slice. The `arch` tool
can be used to prevent this ([example cl](https://chromium-review.googlesource.com/c/chromium/tools/depot_tools/+/2287751)),
which can be useful to work around Rosetta bugs.

As of today, it's possible to install depot\_tools and then run
`fetch chromium`, and it will download Chromium and its dependencies,
but it will die in `runhooks`.

`ninja`, `gn`, and `gomacc` all work fine under Rosetta.
