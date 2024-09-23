# Cross-compiling Chrome/win

As many Chromium developers are on Linux/Mac, cross-compiling Chromium for
Windows targets facilitates development for Windows targets on non-Windows
machines.

It's possible to build most parts of the codebase on a Linux or Mac host while
targeting Windows.  It's also possible to run the locally-built binaries on
swarming.  This document describes how to set that up, and current restrictions.

## Limitations

What does *not* work:

* `js2gtest` tests are omitted from the build ([bug](https://crbug.com/1010561))
  Note that newer WebUI tests are not based on js2gtest
  (see migration progress at [crbug.com/1457360](https://crbug.com/1457360)) and
  are included in the build.
* on Mac hosts, 32-bit builds don't work ([bug](https://crbug.com/794838) has
  more information, and this is unlikely to ever change)

All other targets build fine (including `chrome`, `browser_tests`, ...).

Uses of `.asm` files have been stubbed out.  As a result, Crashpad cannot
report crashes, and NaCl defaults to disabled and cannot be enabled in cross
builds ([.asm bug](https://crbug.com/762167)).

## .gclient setup

1. Tell gclient that you need Windows build dependencies by adding
   `target_os = ['win']` to the end of your `.gclient`.  (If you already
   have a `target_os` line in there, just add `'win'` to the list.) e.g.

       solutions = [
         {
           ...
         }
       ]
       target_os = ['android', 'win']

1. `gclient sync`, follow instructions on screen.

### If you're at Google

`gclient sync` should automatically download the Windows SDK for you. If this
fails with an error:

    Please follow the instructions at
    https://chromium.googlesource.com/chromium/src/+/HEAD/docs/win_cross.md

then you may need to re-authenticate via (with your google.com account):

    cd path/to/chrome/src
    # Follow instructions, enter 0 as project id.
    download_from_google_storage --config

`gclient sync` should now succeed. Skip ahead to the [GN setup](#gn-setup)
section.

### If you're not at Google

After installing [Microsoft's development tools](windows_build_instructions.md#visual-studio),
you can package your Windows SDK installation into a zip file by running the following on a Windows machine:

    cd path/to/depot_tools/win_toolchain
    python package_from_installed.py <vs version> -w <win version>

where `<vs version>` and `<win version>` correspond respectively to the
versions of Visual Studio (e.g. 2019) and of the Windows SDK (e.g.
10.0.19041.0) installed on the Windows machine. Note that if you didn't
install the ARM64 components of the SDK as noted in the link above, you
should add `--noarm` to the parameter list.

These commands create a zip file named `<hash value>.zip`. Then, to use the
generated file in a Linux or Mac host, the following environment variables
need to be set:

    export DEPOT_TOOLS_WIN_TOOLCHAIN_BASE_URL=<base url>
    export GYP_MSVS_HASH_<toolchain hash>=<hash value>

`<base url>` is the path of the directory containing the zip file (note that
specifying scheme `file://` is not required).

`<toolchain hash>` is hardcoded in `src/build/vs_toolchain.py` and can be found by
setting `DEPOT_TOOLS_WIN_TOOLCHAIN_BASE_URL` and running `gclient sync`:

    gclient sync
    ...
    Running hooks:  17% (11/64) win_toolchain
    ________ running '/usr/bin/python src/build/vs_toolchain.py update --force' in <chromium dir>
    Windows toolchain out of date or doesn't exist, updating (Pro)...
    current_hashes:
    desired_hash: <toolchain hash>

## GN setup

Add

    target_os = "win"

to your args.gn.

If you're building on an arm host (e.g. a Mac with an Apple Silicon chip),
you very likely also want to add

    target_cpu = "x64"

lest you build an arm64 chrome/win binary.

Then just build, e.g.

    ninja -C out/gnwin base_unittests.exe

## RBE

This should be supported by the default RBE (remote execution).

## Copying and running chrome

A convenient way to copy chrome over to a Windows box is to build the
`mini_installer` target.  Then, copy just `mini_installer.exe` over
to the Windows box and run it to install the chrome you just built.

Note that the `mini_installer` doesn't include PDB files. PDB files are needed
to correctly symbolize stack traces (or if you want to attach a debugger).

## Running tests on swarming

You can run the Windows binaries you built on swarming, like so:

    tools/run-swarmed.py out/gnwin base_unittests -- [ --gtest_filter=... ]

See the contents of run-swarmed.py for how to do this manually.

The
[linux-win-cross-rel](https://ci.chromium.org/p/chromium/builders/ci/linux-win-cross-rel)
buildbot does 64-bit release cross builds, and also runs tests. You can look at
it to get an idea of which tests pass in the cross build.
