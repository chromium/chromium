# Profiling Content Shell on Android

Below are the instructions for setting up profiling for Content Shell on
Android. This will let you generate profiles for ContentShell. This will require
linux, building an userdebug Android build, and wiping the device.

[TOC]

## Prepare your device.

You need an Android 4.2+ device (Galaxy Nexus, Nexus 4, 7, 10, etc.) which you
don’t mind erasing all data, rooting, and installing a userdebug build on.

## Get and build `content_shell_apk` for Android

More detailed insturctions in [android_build_instructions.md](android_build_instructions.md).

```shell
ninja -C out/Release content_shell_apk
```

## Setup the physical device

Plug in your device. Make sure you can talk to your device, try:

```shell
third_party/android_sdk/public/platform-tools/adb shell ls
```

## Root your device and install a userdebug build

1.  This may require building your own version of Android:
    http://source.android.com/source/building-devices.html
1.  A build that works is: `manta / android-4.2.2_r1` or
    `master / full_manta-userdebug`.

## Root your device

1.  Run `adb root`. Every time you connect your device you’ll want to run this.
1.  If adb is not available, make sure to run `. build/android/envsetup.sh`

If you get the error `error: device offline`, you may need to become a developer
on your device before Linux will see it. On Jellybean 4.2.1 and above this
requires going to “about phone” or “about tablet” and clicking the build number
7 times:
http://androidmuscle.com/how-to-enable-usb-debugging-developer-options-on-nexus-4-and-android-4-2-devices/

## Enable profiling

Rebuild `content_shell_apk` with profiling enabled.

With GN:

    gn args out/Profiling
    # add "enable_profiling = true"
    ninja -C out/Profiling content_shell_apk
    export CHROMIUM_OUTPUT_DIR="$PWD/out/Profiling"

## Run a Telemetry perf profiler

You can run any Telemetry benchmark with `--profiler=perf`, and it will:

1.  Download `perf` and `perfhost`
2.  Install on your device
3.  Run the test
4.  Setup symlinks to work with the `--symfs` parameter

You can also run "manual" tests with Telemetry, more information here:
https://www.chromium.org/developers/telemetry/profiling#TOC-Manual-Profiling---Android

The following steps describe building `perf`, which is no longer necessary if
you use Telemetry.

## Use `adb_profile_chrome`

Even if you're not running a Telemetry test, you can use Catapult to
automatically push binaries and pull the profile data for you.

    build/android/adb_profile_chrome --browser=content_shell --perf

While you still have to build, install and launch the APK yourself, Catapult
will take care of creating the symfs etc. (i.e. you can skip the "not needed for
Telemetry" steps below).

## Install `/system/bin/perf` on your device (not needed for Telemetry)

    # From inside the Android source tree (not inside Chromium)
    mmm external/linux-tools-perf/
    adb remount # (allows you to write to the system image)
    adb sync
    adb shell perf top # check that perf can get samples (don’t expect symbols)

## Install and Run ContentShell

Install with the following:

    out/Release/bin/content_shell_apk run

If `content_shell` “stopped unexpectedly” use `adb logcat` to debug.

## Setup a `symbols` directory with symbols from your build (not needed for Telemetry)

1.  Figure out exactly what path `content_shell_apk` (or chrome, etc) installs
    to.
    *   On the device, navigate ContentShell to about:crash


    adb logcat | grep libcontent_shell_content_view.so

You should find a path that’s something like
`/data/app-lib/org.chromium.content_shell-1/libcontent_shell_content_view.so`

1.  Make a symbols directory
    ```
    mkdir symbols (this guide assumes you put this next to src/)
    ```
1.  Make a symlink from your symbols directory to your un-stripped
    `content_shell`.

    ```
    # Use whatever path in app-lib you got above
    mkdir -p symbols/data/app-lib/org.chromium.content_shell-1
    ln -s `pwd`/src/out/Release/lib/libcontent_shell_content_view.so \
        `pwd`/symbols/data/app-lib/org.chromium.content_shell-1
    ```

## Install `perfhost_linux` locally (not needed for Telemetry)

Note: modern versions of perf may also be able to process the perf.data files
from the device.

1.  `perfhost_linux` can be built from:
    https://android.googlesource.com/platform/external/linux-tools-perf/.
1.  Place `perfhost_linux` next to symbols, src, etc.

    chmod a+x perfhost_linux

## Actually record a profile on the device!

Run the following:

    out/Release/content_shell_apk ps (look for the pid of the sandboxed_process)
    adb shell perf record -g -p 12345 sleep 5
    adb pull /data/perf.data

## Create the report

1.  Run the following:

    ```
    ./perfhost_linux report -g -i perf.data --symfs symbols/
    ```

1.  If you don’t see chromium/webkit symbols, make sure that you built/pushed
    Release, and that the symlink you created to the .so is valid!

## Add symbols for the kernel

1.  By default, /proc/kallsyms returns 0 for all symbols, to fix this, set
    `/proc/sys/kernel/kptr_restrict` to `0`:

    ```
    adb shell echo “0” > /proc/sys/kernel/kptr_restrict
    ```

1.  See http://lwn.net/Articles/420403/ for explanation of what this does.

    ```
    adb pull /proc/kallsyms symbols/kallsyms
    ```

1.  Now add --kallsyms to your perfhost\_linux command:
    ```
    ./perfhost_linux report -g -i perf.data --symfs symbols/ \
        --kallsyms=symbols/kallsyms
    ```
