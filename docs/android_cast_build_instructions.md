# Checking out and building Cast for Android

**Note**: it is **not possible** to build a binary functionally
equivalent to a Chromecast. This is to build a single-page content
embedder with similar functionality to Cast products.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-android-cast](https://goto.google.com/building-android-cast) instead.

[TOC]

## System requirements

* An x86-64 machine running Linux with at least 8GB of RAM. More than 16GB is
  highly recommended.
* At least 100GB of free disk space.
* You must have Git and Python installed already.

Most development is done on Ubuntu. Other distros may or may not work;
see the [Linux instructions](linux/build_instructions.md) for some suggestions.

Building the Android client on Windows or Mac is not supported and doesn't work.

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools`
to `/path/to/depot_tools`:

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):

```shell
$ mkdir ~/chromium && cd ~/chromium
$ fetch --nohooks android
```

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to `fetch`.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and `fetch`
will automatically execute `gclient runhooks` at the end.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory. The remaining instructions
assume you have switched to the `src` directory:

```shell
$ cd src
```

### Converting an existing Linux checkout

If you have an existing Linux checkout, you can add Android support by
appending `target_os = ['android']` to your `.gclient` file (in the
directory above `src`):

```shell
$ echo "target_os = [ 'android' ]" >> ../.gclient
```

Then run `gclient sync` to pull the new Android dependencies:

```shell
$ gclient sync
```

(This is the only difference between `fetch android` and `fetch chromium`.)

### Install additional build dependencies

Once you have checked out the code, run

```shell
$ build/install-build-deps.sh --android
```

to get all of the dependencies you need to build on Linux, *plus* all of the
Android-specific dependencies (you need some of the regular Linux dependencies
because an Android build includes a bunch of the Linux tools and utilities).

### Run the hooks

Once you've run `install-build-deps` at least once, you can now run the
Chromium-specific hooks, which will download additional binaries and other
things you might need:

```shell
$ gclient runhooks
```

*Optional*: You can also [install API
keys](https://www.chromium.org/developers/how-tos/api-keys) if you want your
build to talk to some Google services, but this is not necessary for most
development and testing purposes.

## Setting up the build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along with
a tool called [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory which builds Chrome
for Android, run:

```shell
$ gn gen --args='target_os="android" is_cast_android=true' out/Default
```

* You only have to run this once for each new build directory, Ninja will
  update the build files as needed.
* You can replace `Default` with another name, but
  it should be a subdirectory of `out`.
* For other build arguments, including release settings, see [GN build
  configuration](https://www.chromium.org/developers/gn-build-configuration).
  The default will be a debug component build matching the current host
  operating system and CPU.
* For more info on GN, run `gn help` on the command line or read the
  [quick start guide](https://gn.googlesource.com/gn/+/main/docs/quick_start.md).

Also be aware that some scripts (e.g. `tombstones.py`, `adb_gdb.py`)
require you to set `CHROMIUM_OUTPUT_DIR=out/Default`.

### Faster builds

This section contains some things you can change to speed up your builds,
sorted so that the things that make the biggest difference are first.

#### Use Reclient

*** note
**Warning:** If you are a Google employee, do not follow the instructions below.
See
[go/building-android-chrome#initialize-remote-execution-distributed-builds](https://goto.google.com/building-android-chrome#initialize-remote-execution-distributed-builds)
instead.
***

Chromium's build can be sped up significantly by using a remote execution system
compatible with [REAPI](https://github.com/bazelbuild/remote-apis). This allows
you to benefit from remote caching and executing many build actions in parallel
on a shared cluster of workers.

To use Reclient, follow the corresponding
[Linux build instructions](linux/build_instructions.md#use-reclient).

## Build cast\_shell\_apk

Build `cast_browser_apk` with Ninja using the command:

```shell
$ autoninja -C out/Default cast_browser_apk
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

## Installing and Running `cast_browser_apk` on a device

### Plug in your Android device

Make sure your Android device is plugged in via USB, and USB Debugging
is enabled.

To enable USB Debugging:

*   Navigate to Settings \> About Phone \> Build number
*   Click 'Build number' 7 times
*   Now navigate back to Settings \> Developer Options
*   Enable 'USB Debugging' and follow the prompts

You may also be prompted to allow access to your PC once your device is
plugged in.

You can check if the device is connected by running:

```shell
third_party/android_sdk/public/platform-tools/adb devices
```

Which prints a list of connected devices. If not connected, try
unplugging and reattaching your device.

### Build the APK

```shell
autoninja -C out/Release cast_browser_apk
```

And deploy it to your Android device:

```shell
out/Default/bin/cast_browser_apk install
# Or to install and run:
out/Default/bin/cast_browser_apk run "http://google.com"
```

The app will appear on the device as "Chromium".

### Testing

For information on running tests, see
[Android Test Instructions](testing/android_test_instructions.md).
