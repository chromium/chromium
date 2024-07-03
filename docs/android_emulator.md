# Using an Android Emulator
Always use x86 emulators (or x86\_64 for testing 64-bit APKs). Although arm
emulators exist, they are so slow that they are not worth your time.

[TOC]

## Building for Emulation
You need to target the correct architecture via GN args:
```gn
target_cpu = "x86"  # or "x64" if you have an x86_64 emulator
```

## Running an Emulator

### Googler-only Emulator Instructions

See http://go/clank-emulator/

### Using Prebuilt CIPD packages

Chromium has a set of prebuilt images stored as CIPD packages. These are used
by various builders to run tests on the emulator. Their configurations are
currently stored in [`//tools/android/avd/proto`](../tools/android/avd/proto/).
You can run this command to list them:
```sh
tools/android/avd/avd.py list
```

| Configurations | Android Version | CPU Arch| AVD Target | Builder |
|:-------------- |:--------------- |:------- |:---------- |:------- |
| `generic_android26.textpb` | 8.0 (O) | x86 | google_apis | N/A |
| `generic_android27.textpb` | 8.1 (O_MR1) | x86 | google_apis | N/A |
| `android_28_google_apis_x86.textpb` | 9 (P) | x86 | google_apis | [android-pie-x86-rel][android-pie-x86-rel] |
| `android_29_google_apis_x86.textpb` | 10 (Q) | x86 | google_apis | N/A |
| `android_30_google_apis_x86.textpb` | 11 (R) | x86 | google_apis | [android-11-x86-rel][android-11-x86-rel] |
| `android_31_google_apis_x64.textpb` | 12 (S) | x86_64 | google_apis | [android-12-x64-rel][android-12-x64-rel] |
| `android_32_google_apis_x64_foldable.textpb` | 12L (S_V2) | x86_64 | google_apis | [android-12l-x64-dbg-tests][android-12l-x64-dbg-tests] |
| `android_33_google_apis_x64.textpb` | 13 (T) | x86_64 | google_apis | [android-13-x64-rel][android-13-x64-rel] |
| `android_34_google_apis_x64.textpb` | 14 (U) | x86_64 | google_apis | [android-14-x64-rel][android-14-x64-rel] |

You can use these configuration files to run the same emulator images locally.

[android-pie-x86-rel]: https://ci.chromium.org/p/chromium/builders/ci/android-pie-x86-rel
[android-11-x86-rel]: https://ci.chromium.org/p/chromium/builders/ci/android-11-x86-rel
[android-12-x64-rel]: https://ci.chromium.org/p/chromium/builders/ci/android-12-x64-rel
[android-12l-x64-dbg-tests]: https://ci.chromium.org/p/chromium/builders/ci/android-12l-x64-dbg-tests
[android-13-x64-rel]: https://ci.chromium.org/p/chromium/builders/ci/android-13-x64-rel
[android-14-x64-rel]: https://ci.chromium.org/p/chromium/builders/ci/android-14-x64-rel

#### Prerequisite

 * Make sure KVM (Kernel-based Virtual Machine) is enabled.
   See this
   [link](https://developer.android.com/studio/run/emulator-acceleration#vm-linux)
   from android studio for more details and instructions.

 * You need to have the permissions to use KVM.
   Use the following command to see if you are in group `kvm`:

   ```
     $ grep kvm /etc/group
   ```

   If your username is not shown in the group, add yourself to the group:

   ```
     $ sudo adduser $USER kvm
     $ newgrp kvm
   ```

   You need to log out and log back in so the new groups take effect.

#### Running via the test runner

The android test runner can run emulator instances on its own. In doing so, it
starts the emulator instances, runs tests against them, and then shuts them
down. This is how builders run the emulator.

##### Options

 * `--avd-config`

    To have the test runner run an emulator instance, use `--avd-config`:

    ```
      $ out/Debug/bin/run_base_unittests \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb
    ```

 * `--emulator-count`

    The test runner will launch one instance by default. To have it run multiple
    instances, use `--emulator-count`:

    ```
      $ out/Debug/bin/run_base_unittests \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --emulator-count 4
    ```

 * `--emulator-enable-network`

    The test runner runs the emulator without network access by default. To have
    it run with network access, use `--emulator-enable-network`:

    ```
    $ out/Debug/bin/run_base_unittests \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --emulator-enable-network
    ```

 * `--emulator-window`

    The test runner runs the emulator in headless mode by default. To have it run
    with a window, use `--emulator-window`:

    ```
      $ out/Debug/bin/run_base_unittests \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --emulator-window
    ```

#### Running standalone

The test runner will set up and tear down the emulator on each invocation.
To manage emulator lifetime independently, use `tools/android/avd/avd.py`.

##### Options

 * `--avd-config`

    This behaves the same as it does for the test runner.

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb
    ```

    > Note: `avd.py start` will start an emulator instance and then terminate.
    > To shut down the emulator, use `adb emu kill`.

 * `--enable-network`

    Like the test runner, `avd.py` runs the emulator without network access by
    default. To enable network access, use `--enable-network`:

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --enable-network
    ```

 * `--emulator-window`

    Like the test runner, `avd.py` runs the emulator in headless mode by default.
    To have it run with a window, use `--emulator-window`:

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --emulator-window
    ```

 * `--gpu-mode GPU_MODE`

    Override the mode of hardware OpenGL ES emulation indicated by the AVD.
    See "emulator -help-gpu" for a full list of modes.

 * `--no-read-only`

    `avd.py` runs the emulator in read-only mode by default. To run a modifiable
    emulator, use `--no-read-only`:

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --no-read-only
    ```

 * `--wipe-data`

    Reset the /data partition to the factory defaults. This removes all user
    settings from the AVD.

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --wipe-data
    ```

 * `--writable-system`

    Makes system & vendor image writable. It's necessary to run
    ```
    adb root
    adb remount
    ```
    after the emulator starts.

 * `--debug-tags`

    `avd.py` disables the emulator log by default. When this option is used,
    emulator log will be enabled. It is useful when the emulator cannot be
    launched correctly. See `emulator -help-debug-tags` for a full list of tags.
    Use `--debug-tags=all` if you want to output all logs (warning: it is quite
    verbose).

    ```
      $ tools/android/avd/avd.py start \
          --avd-config tools/android/avd/proto/android_33_google_apis_x64.textpb \
          --debug-tags init,snapshot
    ```

### Using Your Own Emulator Image

By far the easiest way to set up emulator images is to use Android Studio.
If you don't have an [Android Studio project](android_studio.md) already, you
can create a blank one to be able to reach the Virtual Device Manager screen.

Refer to: https://developer.android.com/studio/run/managing-avds.html

Where files live:
 * System partition images are stored within the sdk directory.
 * Emulator configs and data partition images are stored within
   `~/.android/avd/`.

#### Creating an Image

##### Choosing a Skin

Choose a skin with a small screen for better performance (unless you care about
testing large screens).

##### Choosing an Image

Android Studio's image labels roughly translate to the following:

| AVD "Target" | Virtual Device Configuration tab | GMS? | Build Properties |
| --- | --- | --- | --- |
| Google Play | "Recommended" (the default tab) | This has GMS | `user`/`release-keys` |
| Google APIs | "x86 Images" | This has GMS | `userdebug`/`dev-keys` |
| No label | "x86 Images" | AOSP image, does not have GMS | `eng`/`test-keys` |

*** promo
**Tip:** if you're not sure which to use, choose **Google APIs** under the **x86
Images** tab in the Virtual Device Configuration wizard.
***

##### Configuration

"Show Advanced Settings" > scroll down:
* Set internal storage to 4000MB (component builds are really big).
* Set SD card to 1000MB (our tests push a lot of files to /sdcard).

##### Known Issues

 * Our test & installer scripts do not work with pre-MR1 Jelly Bean.
 * Component builds do not work on pre-KitKat (due to the OS having a max
   number of shared libraries).
 * Jelly Bean and KitKat images sometimes forget to mount /sdcard :(.
   * This causes tests to fail.
   * To ensure it's there: `adb -s emulator-5554 shell mount` (look for /sdcard)
   * Can often be fixed by editing `~/.android/avd/YOUR_DEVICE/config.ini`.
     * Look for `hw.sdCard=no` and set it to `yes`
 * The "Google APIs" Android L and M emulator images are configured to expect
   the "AOSP" WebView package (`com.android.webview`). This does not resemble
   production devices with GMS, which expect the ["Google WebView"
   configuration](/android_webview/docs/webview-providers.md#webview-provider-options)
   (`com.google.android.webview` on L and M). See [Removing preinstalled
   WebView](/android_webview/docs/build-instructions.md#Removing-preinstalled-WebView)
   if you need to install a local build or official build.


#### Starting an Emulator from the Command Line

Refer to: https://developer.android.com/studio/run/emulator-commandline.html.

*** promo
Ctrl-C will gracefully close an emulator.
***

*** promo
**Tip:** zsh users can add https://github.com/zsh-users/zsh-completions to
provide tab completion for the `emulator` command line tool.
***

#### Basic Command Line Use

```shell
$ # List virtual devices that you've created:
$ ~/Android/Sdk/emulator/emulator -list-avds
$ # Start a named device:
$ ~/Android/Sdk/emulator/emulator @EMULATOR_ID
```

#### Running a Headless Emulator

You can run an emulator without creating a window on your desktop (useful for
`ssh`):
```shell
$ ~/Android/Sdk/emulator/emulator -no-window @EMULATOR_ID
$ # This also works for new enough emulator builds:
$ ~/Android/Sdk/emulator/emulator-headless @EMULATOR_ID
```

#### Running Multiple Emulators

Tests are automatically sharded amongst available devices. If you run multiple
emulators, then running test suites becomes much faster. Refer to the
"Multiple AVD instances" section of these [emulator release notes](
https://androidstudio.googleblog.com/2018/11/emulator-28016-stable.html)
for more about how this works.
```shell
$ # Start 8 emulators. Press Ctrl-C to stop them all.
$ ( for i in $(seq 8); do ~/Android/Sdk/emulator/emulator @EMULATOR_ID -read-only & done; wait )
$ # Start 12 emulators. More than 10 requires disabling audio on some OS's. Reducing cores increases parallelism.
$ ( for i in $(seq 12); do ~/Android/Sdk/emulator/emulator @EMULATOR_ID -read-only -no-audio -cores 2 & done; wait )
```

#### Writable system partition

Unlike physical devices, an emulator's `/system` partition cannot be modified by
default (even on rooted devices). If you need to do so (such as to remove a
system app), you can start your emulator like so:
```shell
$ ~/Android/Sdk/emulator/emulator -writable-system @EMULATOR_ID
```

## Using an Emulator
 * Emulators show up just like devices via `adb devices`
   * Device serials will look like "emulator-5554", "emulator-5556", etc.

## Emulator pros and cons

### Pros
 * **Compiles are faster.** Many physical devices are arm64, whereas emulators
   are typically x86 (32-bit). 64-bit builds may require 2 copies of the native
   library (32-bit and 64-bit), so compiling for an arm64 phone is ~twice as
   much work as for an emulator (for targets which support WebView).
 * **APKs install faster.** Since emulators run on your workstation, adb can
   push the APK onto the emulator without being [bandwidth-constrained by
   USB](https://youtu.be/Mzop8bXZI3E).
 * Emulators can be nice for working remotely. Physical devices usually require
   `scp` or ssh port forwarding to copy the APK from your workstation and
   install on a local device. Emulators run on your workstation, so there's **no
   ssh slow-down**.

### Cons
 * If you're investigating a hardware-specific bug report, you'll need a
   physical device with the actual hardware to repro that issue.
 * x86 emulators need a separate out directory, so building for both physical
   devices and emulators takes up more disk space (not a problem if you build
   exclusively for the emulator).
 * `userdebug`/`eng` emulators don't come with the Play Store installed, so you
   can't install third party applications. Sideloading is tricky, as not all
   third-party apps support x86.
