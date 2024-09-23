# Running Web Platform Tests with Chrome Android

[TOC]

## Initial Setup

Please follow the steps at ["Checking out and building Chromium for Android"](/docs/android_build_instructions.md) to
setup your local environment. Once that is done, you need to build the
`chrome_public_wpt` target to get `Chrome Android`, `chromedriver` and all of
the other needed binaries.

```bash
autoninja -C out/Default chrome_public_wpt
```

## Running the Tests

Once you have `Chrome Android` and `chromedriver` built, you can launch
`run_wpt_tests.py` to run WPTs. You can either run the tests on an Android
emulator or a real Android device.

### Running WPTs on an Android emulator

You will need to follow the steps in ["Using an Android Emulator"](/docs/android_emulator.md) to be ready
to run the Android Emulator. Passing the `--avd-config` option to `run_wpt_tests.py` will launch an emulator
that will be shut down after running the tests. The example below runs
`external/wpt/badging/badge-success.https.html` on Android 11:

```bash
$ third_party/blink/tools/run_wpt_tests.py -t Default -p clank --avd-config=tools/android/avd/proto/generic_android30.textpb external/wpt/badging/badge-success.https.html
```

Alternatively, you can launch the emulator yourself and `run_wpt_tests.py` will
detect and connect to the emulator and run WPTs with it. This can save you the
time to repeatedly launch the emulator. The commands below show how this works.

```bash
$ tools/android/avd/avd.py start --avd-config=tools/android/avd/proto/generic_android30.textpb
$ third_party/blink/tools/run_wpt_tests.py -t Default -p clank external/wpt/badging/badge-success.https.html
```

### Running WPTs on a real Android device

`run_wpt_tests.py` should be able to work with a real device as long as the
device can be found by `adb devices`. You will need to make sure the ABI matches
and these
[steps](/docs/android_build_instructions.md#installing-and-running-chromium-on-a-device)
are followed.

## Test expectations and Baselines

The
[MobileTestExpectations](../../third_party/blink/web_tests/MobileTestExpectations) file contains the list of all known Chrome Android
specific test failures, and it inherits or overrides test expectations from the default [TestExpectations](../../third_party/blink/web_tests/TestExpectations) file.

Chrome Android specific baselines reside at `third_party/blink/web_tests/platform/android`, and
falls back to `third_party/blink/web_tests/platform/linux`. To update baselines for Chrome Androids,
you should trigger `android-chrome-13-x64-wpt-android-specific` and run [rebaseline tool](./web_test_expectations.md#How-to-rebaseline) after the results are ready.

## Test List

The builder `android-webview-13-x64-wpt-android-specific` runs tests specified by the [android.filter](../../third_party/blink/web_tests/TestLists/android.filter) file, which tests Android specific behaviors. Developers can add additional tests to the list when necessary.

Please [file bugs and feature requests](https://crbug.com/new) against
[`Blink>Infra` with the `wptrunner`
label](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EInfra%20label%3Awptrunner&can=2).
