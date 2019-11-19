# Using an Android Emulator
Always use x86 emulators (or x86\_64 for testing 64-bit APKs). Although arm
emulators exist, they are so slow that they are not worth your time.

*** note
**Note:** apps with native code must be compiled specifically for the device
architecture, so make sure your copy of the app supports x86. Also, be aware the
Play Store may not display ARM-only applications for an x86 emulator. The steps
below show how to locally compile chromium-based apps for x86.
***

## Building for Emulation
You need to target the correct architecture via GN args:
```gn
target_cpu = "x86"  # or "x64" if you have an x86_64 emulator
```

## Creating an Emulator Image
By far the easiest way to set up emulator images is to use Android Studio.
If you don't have an [Android Studio project](android_studio.md) already, you
can create a blank one to be able to reach the Virtual Device Manager screen.

Refer to: https://developer.android.com/studio/run/managing-avds.html

Where files live:
 * System partition images are stored within the sdk directory.
 * Emulator configs and data partition images are stored within
   `~/.android/avd/`.

### Choosing a Skin
Choose a skin with a small screen for better performance (unless you care about
testing large screens).

### Choosing an Image
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

### Configuration
"Show Advanced Settings" > scroll down:
* Set internal storage to 4000MB (component builds are really big).
* Set SD card to 1000MB (our tests push a lot of files to /sdcard).

### Known Issues
 * Our test & installer scripts do not work with pre-MR1 Jelly Bean.
 * Component builds do not work on pre-KitKat (due to the OS having a max
   number of shared libraries).
 * Jelly Bean and KitKat images sometimes forget to mount /sdcard :(.
   * This causes tests to fail.
   * To ensure it's there: `adb -s emulator-5554 shell mount` (look for /sdcard)
   * Can often be fixed by editing `~/.android/avd/YOUR_DEVICE/config.ini`.
     * Look for `hw.sdCard=no` and set it to `yes`

## Starting an Emulator from the Command Line
Refer to: https://developer.android.com/studio/run/emulator-commandline.html.

*** promo
Ctrl-C will gracefully close an emulator.
***

### Basic Command Line Use
```shell
$ # List virtual devices that you've created:
$ ~/Android/Sdk/emulator/emulator -list-avds
$ # Start a named device:
$ ~/Android/Sdk/emulator/emulator @EMULATOR_ID
```

### Running a Headless Emulator
You can run an emulator without creating a window on your desktop (useful for
`ssh`):
```shell
$ ~/Android/Sdk/emulator/emulator -no-window @EMULATOR_ID
$ # This also works for new enough emulator builds:
$ ~/Android/Sdk/emulator/emulator-headless @EMULATOR_ID
```

### Running Multiple Emulators
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

### Writable system partition
Unlike physical devices, an emulator's `/system` partition cannot be modified by
default (even on rooted devices). If you need to do so (such as to remove a
system app), you can start your emulator like so:
```shell
$ ~/Android/Sdk/emulator/emulator -writable-system @EMULATOR_ID
```

## Using an Emulator
 * Emulators show up just like devices via `adb devices`
   * Device serials will look like "emulator-5554", "emulator-5556", etc.

