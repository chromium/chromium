# Checking out and building Chromium for Android

There are instructions for other platforms linked from the
[get the code](get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-android-chrome](https://goto.google.com/building-android-chrome)
instead.

[TOC]

## System requirements

* An x86-64 machine running Linux with at least 8GB of RAM. More than 16GB is
  highly recommended.
* At least 100GB of free disk space.
* You must have Git and Python installed already.

Most development is done on Ubuntu. Other distros may or may not work;
see the [Linux instructions](linux/build_instructions.md) for some suggestions.

Building the Android client on Windows or Mac is not supported and doesn't work.

## Install depot\_tools

Clone the `depot_tools` repository:

```shell
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools`
to `/path/to/depot_tools`:

```shell
export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):

```shell
mkdir ~/chromium && cd ~/chromium
fetch --nohooks android
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
cd src
```

### Converting an existing Linux checkout

If you have an existing Linux checkout, you can add Android support by
appending `target_os = ['linux', 'android']` to your `.gclient` file (in the
directory above `src`):

```shell
echo "target_os = [ 'linux', 'android' ]" >> ../.gclient
```

Then run `gclient sync` to pull the new Android dependencies:

```shell
gclient sync
```

(This is the only difference between `fetch android` and `fetch chromium`.)

### Install additional build dependencies

Once you have checked out the code, run

```shell
build/install-build-deps.sh
```

to get all of the dependencies you need to build on Linux, *plus* all of the
Android-specific dependencies (you need some of the regular Linux dependencies
because an Android build includes a bunch of the Linux tools and utilities).

NOTE: For 32-bit builds, the `--lib32` command line switch could be used.
You may run into issues where `gperf` or `pkgconf` don't get installed,
without it. To remedy this, and potentially other missing packages, you will
have to install them manually using:

```shell
sudo apt-get install {missing_pkg}
```

### Run the hooks

Once you've run `install-build-deps` at least once, you can now run the
Chromium-specific hooks, which will download additional binaries and other
things you might need:

```shell
gclient runhooks
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
for Android, run `gn args out/Default` and edit the file to contain the
following arguments:

```gn
target_os = "android"
target_cpu = "arm64"  # See "Figuring out target_cpu" below
use_remoteexec = true  # Enables distributed builds. See "Faster Builds".
```

* You only have to run this once for each new build directory, Ninja will
  update the build files as needed.
* You can replace `Default` with another name, but
  it should be a subdirectory of `out`.
* For other build arguments, including release settings, see [GN build
  configuration](https://www.chromium.org/developers/gn-build-configuration).
  The default will be a debug component build.
* For more info on GN, run `gn help` on the command line or read the
  [quick start guide](https://gn.googlesource.com/gn/+/main/docs/quick_start.md).

Also be aware that some scripts (e.g. `tombstones.py`, `adb_gdb.py`)
require you to set `CHROMIUM_OUTPUT_DIR=out/Default`.

### Figuring out target\_cpu

The value of
[`target_cpu`](https://gn.googlesource.com/gn/+/main/docs/reference.md#var_target_cpu)
determines what instruction set to use for native code. Given a device (or
emulator), you can determine the correct instruction set with `adb shell getprop
ro.product.cpu.abi`:

| `getprop ro.product.cpu.abi` output | `target_cpu` value |
|-------------------------------------|--------------------|
| `arm64-v8a`                         | `arm64`            |
| `armeabi-v7a`                       | `arm`              |
| `x86`                               | `x86`              |
| `x86_64`                            | `x64`              |

*** promo
`arm` and `x86` may optionally be used instead of `arm64` and `x64` for
non-WebView targets. This is also allowed for Monochrome, but only when not set
as the WebView provider.
***

## Build Chromium

Build Chromium with Ninja using the command:

```shell
autoninja -C out/Default chrome_public_apk
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

You can get a list of all of the other build targets from GN by running `gn ls
out/Default` from the command line. To compile one, pass the GN label to Ninja
with no preceding "//" (so, for `//chrome/test:unit_tests` use `autoninja -C
out/Default chrome/test:unit_tests`).

### Multiple Chrome Targets

The Google Play Store allows apps to send customized bundles (`.aab` files)
depending on the version of Android running on a device. Chrome uses this
feature to package optimized versions for different OS versions.

1. `monochrome_public_bundle` (`MonochromePublic.aab`)
   * `minSdkVersion=24` (Nougat).
   * Contains both Chrome and WebView (to save disk space).
2. `trichrome_chrome_bundle` (`TrichromeChrome.aab`)
   * `minSdkVersion=29` (Android 10).
   * Native code shared with WebView through a "Static Shared Library APK": `trichrome_library_apk`
   * Corresponding WebView target: `trichrome_webview_bundle`
3. `chrome_public_bundle` & `chrome_public_apk` (`ChromePublic.aab`, `ChromePublic.apk`)
   * `minSdkVersion=24` (Nougat).
   * Used for local development (to avoid building WebView).
   * WebView packaged independently (`system_webview_bundle` / `system_webview_apk`).

*** note
**Notes:**
* These instructions use `chrome_public_apk`, but any of the other targets can
  be substituted.
* For more about bundles, see [android_dynamic feature modules.md](android_dynamic_feature_modules.md).
* For more about native library packaging & loading, see [android_native_libraries.md](android_native_libraries.md).
* There are closed-source equivalents to these targets (for Googlers), which
  are identical but link in some extra code.
***

## Updating your checkout

To update an existing checkout, you can run

```shell
$ git rebase-update
$ gclient sync
```

The first command updates the primary Chromium source repository and rebases
any of your local branches on top of tip-of-tree (aka the Git branch
`origin/main`). If you don't want to use this script, you can also just use
`git pull` or other common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed.

## Installing and Running Chromium on a device

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

### Enable apps from unknown sources

Allow Android to run APKs that haven't been signed through the Play Store:

*   Enable 'Unknown sources' under Settings \> Security

In case that setting isn't present, it may be possible to configure it via
`adb shell` instead:

```shell
third_party/android_sdk/public/platform-tools/adb shell settings put global verifier_verify_adb_installs 0
```

### Build the full browser

```shell
autoninja -C out/Default chrome_public_apk
```

And deploy it to your Android device:

```shell
out/Default/bin/chrome_public_apk install
```

The app will appear on the device as "Chromium".

### Build Content shell

Wraps the content module (but not the /chrome embedder). See
[https://www.chromium.org/developers/content-module](https://www.chromium.org/developers/content-module)
for details on the content module and content shell.

```shell
autoninja -C out/Default content_shell_apk
out/Default/bin/content_shell_apk install
```

this will build and install an Android apk under
`out/Default/apks/ContentShell.apk`.

### Build WebView

[Android WebView](https://developer.android.com/reference/android/webkit/WebView.html)
is a system framework component. Since Android KitKat, it is implemented using
Chromium code (based off the [content module](https://dev.chromium.org/developers/content-module)).

If you want to build the complete Android WebView framework component and test
the effect of your chromium changes in Android apps using WebView, you should
follow the [Android AOSP + chromium WebView
instructions](https://www.chromium.org/developers/how-tos/build-instructions-android-webview)

### Running

For Content shell:

```shell
out/Default/bin/content_shell_apk launch [--args='--foo --bar'] http://example.com
```

For Chrome public:

```shell
out/Default/bin/chrome_public_apk launch [--args='--foo --bar'] http://example.com
```

### Logging and debugging

Logging is often the easiest way to understand code flow. In C++ you can print
log statements using the LOG macro. In Java, refer to
[android_logging.md](android_logging.md).

You can see these log via `adb logcat`, or:

```shell
out/Default/bin/chrome_public_apk logcat
```

Logcat supports an additional feature of filtering and highlighting user-defined patterns. To use
this mechanism, define a shell variable: `CHROMIUM_LOGCAT_HIGHLIGHT` and assign your desired
pattern. The pattern will be used to search for any substring (ie. no need to prefix or suffix it
with `.*`), eg:

```shell
export CHROMIUM_LOGCAT_HIGHLIGHT='(WARNING|cr_Child)'
out/Default/bin/chrome_public_apk logcat
# Highlights messages/tags containing WARNING and cr_Child strings.
```

Note: both _Message_ and _Tag_ portion of logcat are matched against the pattern.

To debug C++ code, use one of the following commands:

```shell
out/Default/bin/content_shell_apk gdb
out/Default/bin/chrome_public_apk gdb
```

See [Android Debugging Instructions](android_debugging_instructions.md)
for more on debugging, including how to debug Java code.

### Testing

For information on running tests, see
[Android Test Instructions](/docs/testing/android_test_instructions.md)

## Faster Builds

### Use Reclient

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

### GN Args

Args that affect build speed:
 * `use_remoteexec = true` *(default=false)*
   * What it does: Enables distributed builds via Reclient
 * `symbol_level = 0` *(default=1)*
   * What it does: Disables debug information in native code.
   * Use this when doing primarily Java development.
   * To disable symbols only in Blink / V8: `blink_symbol_level = 0`, `v8_symbol_level = 0`
 * `is_component_build = true` *(default=`is_debug`)*
   * What it does: Uses multiple `.so` files instead of just one (faster links)
 * `is_java_debug = true` *(default=`is_debug`)*
   * What it does: Disables R8 (whole-program Java optimizer)
 * `treat_warnings_as_errors = false` *(default=`true`)*
   * Causes any compiler warnings or lint checks to not fail the build.
   * Allows you to iterate without needing to satisfy static analysis checks.
 * `android_static_analysis = "build_server"` *(default=`"on"`)*
   * Offloads static analysis steps to the build server. Explained below.
   * Set this to `"off"` if you want to turn off static analysis altogether.
 * `incremental_install = true` *(default=`false`)*
   * Makes build and install quite a bit faster. Explained in a later section.
 * `enable_chrome_android_internal = false` *(Googlers only)*
   * Disables non-public code, which exists even when building public targets.
   * Use this is you do not need to test internal-only things.

### Running Static Analysis Asynchronously

Normally analysis build steps like Lint and Error Prone will run as normal build
steps. The build will then wait for all analysis steps to complete successfully.
By offloading analysis build steps to a separate build server to be run lazily at
a low priority when the machine is idle, the actual build can complete much faster.

**Note**: Since the build completes before the analysis checks finish, the build
will not fail if an analysis check fails. Make sure to check the server's output
at regular intervals to fix outstanding issues caught by these analysis checks.

#### First way (by running it manually)

There are **two** steps to using the build server.
1. Add the gn arg `android_static_analysis = "build_server"`
2. Run the script at
[//build/android/fast_local_dev_server.py][fast_local_dev]

All your local builds will now forward analysis steps to this server, including
android lint, errorprone, bytecode processor.

If you run (2) in a terminal, the output of the checks will be displayed there.

#### Second way (using systemd)

Alternatively, you can set up the server as a Linux service, so it runs on the
background and starts on boot. If you're using systemd:

Save the following as /etc/systemd/user/fast-local-dev-server.service.
```
[Unit]
Description=Chrome server for android build static analysis

[Service]
Type=simple
ExecStart=<path to fast_local_dev_server.py>
Restart=always

[Install]
WantedBy=default.target
```

Then
```bash
systemctl --user daemon-reload
systemctl --user enable fast-local-dev-server
systemctl --user start fast-local-dev-server
```

The output can be inspected with
```
journalctl --user -e -u fast-local-dev-server
```

[fast_local_dev]: https://source.chromium.org/chromium/chromium/src/+/main:build/android/fast_local_dev_server.py

#### Incremental Install
[Incremental Install](/build/android/incremental_install/README.md) uses
reflection and sideloading to speed up the edit & deploy cycle (normally < 10
seconds). The initial launch of the apk will be a lot slower on older Android
versions (pre-N) where the OS needs to pre-optimize the side-loaded files, but
then be only marginally slower after the first launch.

To enable Incremental Install, add the gn args:

```gn
incremental_install = true
```

Some APKs (e.g. WebView) do not work with `incremental install = true` and are
always built as normal APKs. This behavior is controlled via
`never_incremental = true`.

## Installing and Running Chromium on an Emulator

Running on an emulator is the same as on a device. Refer to
[android_emulator.md](android_emulator.md) for setting up emulators.

## Tips, tricks, and troubleshooting

### Rebuilding libchrome.so for a particular release

These instructions are only necessary for Chrome 51 and earlier.

In the case where you want to modify the native code for an existing
release of Chrome for Android (v25+) you can do the following steps.
Note that in order to get your changes into the official release, you'll
need to send your change for a codereview using the regular process for
committing code to chromium.

1.  Open Chrome on your Android device and visit chrome://version
2.  Copy down the id listed next to "Build ID:"
3.  Go to
    [http://storage.googleapis.com/chrome-browser-components/BUILD\_ID\_FROM\_STEP\_2/index.html](http://storage.googleapis.com/chrome-browser-components/BUILD_ID_FROM_STEP_2/index.html)
4.  Download the listed files and follow the steps in the README.
