# Android Test Instructions

[TOC]

## Device Setup

### Physical Device Setup

#### Root Access

Running tests requires root access, which requires using a userdebug build on
your device.

To use a userdebug build, see
[Running Builds](https://source.android.com/setup/build/running.html). Googlers
can refer to [this page](https://goto.google.com/flashdevice).

If you can't run "adb root", you will get an error when trying to install
the test APKs like "adb: error: failed to copy" and
"remote secure_mkdirs failed: Operation not permitted" (use "adb unroot" to
return adb to normal).

#### ADB Debugging

The adb executable exists within the Android SDK:

```shell
third_party/android_sdk/public/platform-tools/adb
```

In order to allow the ADB to connect to the device, you must enable USB
debugging:

* Developer options are hidden by default. To unhide them:
    *   Go to "About phone"
    *   Tap 10 times on "Build number"
    *   The "Developer options" menu will now be available.
    *   Check "USB debugging".
    *   Un-check "Verify apps over USB".

#### Screen

You **must** ensure that the screen stays on while testing: `adb shell svc power
stayon usb` Or do this manually on the device: Settings -> Developer options ->
Stay Awake.

If this option is greyed out, stay awake is probably disabled by policy. In that
case, get another device or log in with a normal, unmanaged account (because the
tests will break in exciting ways if stay awake is off).

#### Disable Verify Apps

You may see a dialog like [this
one](http://www.samsungmobileusa.com/simulators/ATT_GalaxyMega/mobile/screens/06-02_12.jpg),
which states, _Google may regularly check installed apps for potentially harmful
behavior._ This can interfere with the test runner. To disable this dialog, run:

```shell
adb shell settings put global package_verifier_enable 0
```

### Using Emulators

Running tests on emulators is the same as on device. Refer to
[android_emulator.md](../android_emulator.md) for setting up emulators.

## Building Tests

If you're adding a new test file, you'll need to explicitly add it to a gn
target. If you're adding a test to an existing file, you won't need to make gn
changes, but you may be interested in where your test winds up. In either case,
here are some guidelines for where a test belongs:

### C++

C++ test files typically belong in `<top-level directory>_unittests` (e.g.
`base_unittests` for `//base`). There are a few exceptions -- browser tests are
typically their own target (e.g. `content_browsertests` for `//content`, or
`browser_tests` for `//chrome`), and some unit test suites are broken at the
second directory rather than the top-level one.

### Java

Java test files vary a bit more widely than their C++ counterparts:

-   Instrumentation test files -- i.e., tests that will run on a device --
    typically belong in either `<top-level directory>_javatests` or `<top-level
    directory>_test_java`. Regardless, they'll wind up getting packaged into one
    of a few test APKs:
    -   `webview_instrumentation_test_apk` for anything in `//android_webview`
    -   `content_shell_test_apk` for anything in `//content` or below
    -   `chrome_public_test_apk` for most things in `//chrome`
-   JUnit or Robolectric test files -- i.e., tests that will run on the host --
    typically belong in `<top-level directory>_junit_tests` (e.g.
    `base_junit_tests` for `//base`), though here again there are cases
    (particularly in `//components`) where suites are split at the second
    directory rather than the top-level one.

Once you know what to build, just do it like you normally would build anything
else, e.g.: `ninja -C out/Release chrome_public_test_apk`

## Running Tests

All functional tests should be runnable via the wrapper scripts generated at
build time:

```sh
<output directory>/bin/run_<target_name> [options]
```

Note that tests are sharded across all attached devices unless explicitly told
to do otherwise by `-d/--device`.

The commands used by the buildbots are printed in the logs. Look at
https://build.chromium.org/ to duplicate the same test command as a particular
builder.

### INSTALL\_FAILED\_CONTAINER\_ERROR or INSTALL\_FAILED\_INSUFFICIENT\_STORAGE

If you see this error when the test runner is attempting to deploy the test
binaries to the AVD emulator, you may need to resize your userdata partition
with the following commands:

```shell
# Resize userdata partition to be 1G
resize2fs android_emulator_sdk/sdk/system-images/android-25/x86/userdata.img 1G

# Set filesystem parameter to continue on errors; Android doesn't like some
# things e2fsprogs does.
tune2fs -e continue android_emulator_sdk/sdk/system-images/android-25/x86/userdata.img
```

## Symbolizing Crashes

Crash stacks are logged and can be viewed using `adb logcat`. To symbolize the
traces, define `CHROMIUM_OUTPUT_DIR=$OUTDIR` where `$OUTDIR` is the argument you
pass to `ninja -C`, and pipe the output through
`third_party/android_platform/development/scripts/stack`. If
`$CHROMIUM_OUTPUT_DIR` is unset, the script will search `out/Debug` and
`out/Release`. For example:

```shell
# If you build with
ninja -C out/Debug chrome_public_test_apk
# You can run:
adb logcat -d | third_party/android_platform/development/scripts/stack

# If you build with
ninja -C out/android chrome_public_test_apk
# You can run:
adb logcat -d | CHROMIUM_OUTPUT_DIR=out/android third_party/android_platform/development/scripts/stack
# or
export CHROMIUM_OUTPUT_DIR=out/android
adb logcat -d | third_party/android_platform/development/scripts/stack
```

## JUnit tests

JUnit tests are Java unittests running on the host instead of the target device.
They are faster to run and therefore are recommended over instrumentation tests
when possible.

The JUnits tests are usually following the pattern of *target*\_junit\_tests,
for example, `content_junit_tests` and `chrome_junit_tests`.

When adding a new JUnit test, the associated `BUILD.gn` file must be updated.
For example, adding a test to `chrome_junit_tests` requires to update
`chrome/android/BUILD.gn`.

```shell
# Build the test suite.
ninja -C out/Default chrome_junit_tests

# Run the test suite.
out/Default/run_chrome_junit_tests

# Run a subset of tests. You might need to pass the package name for some tests.
out/Default/run_chrome_junit_tests -f "org.chromium.chrome.browser.media.*"
```

### Debugging

Similar to [debugging apk targets](../android_debugging_instructions.md#debugging-java):

```shell
out/Default/bin/run_chrome_junit_tests --wait-for-java-debugger
out/Default/bin/run_chrome_junit_tests --wait-for-java-debugger  # Specify custom port via --debug-socket=9999
```

## Gtests

```shell
# Build a test suite
ninja -C out/Release content_unittests

# Run a test suite
out/Release/bin/run_content_unittests [-vv]

# Run a subset of tests and enable some "please go faster" options:
out/Release/bin/run_content_unittests --fast-local-dev -f "ByteStreamTest.*"
```

## Instrumentation Tests

In order to run instrumentation tests, you must leave your device screen ON and
UNLOCKED. Otherwise, the test will timeout trying to launch an intent.
Optionally you can disable screen lock under Settings -> Security -> Screen Lock
-> None.

Next, you need to build the app, build your tests, and then run your tests
(which will install the APK under test and the test APK automatically).

Examples:

ContentShell tests:

```shell
# Build the tests:
ninja -C out/Release content_shell_test_apk

# Run the test suite:
out/Release/bin/run_content_shell_test_apk [-vv]

# Run a subset of tests and enable some "please go faster" options:
out/Release/bin/run_content_shell_test_apk --fast-local-dev -f "*TestClass*"
```

Android WebView tests:

See [WebView's instructions](/android_webview/docs/test-instructions.md).

In order to run a subset of tests, use -f to filter based on test class/method
or -A/-E to filter using annotations.

More Filtering examples:

```shell
# Run a specific test class
out/Debug/bin/run_content_shell_test_apk -f "AddressDetectionTest.*"

# Run a specific test method
out/Debug/bin/run_content_shell_test_apk -f AddressDetectionTest#testAddressLimits

# Run a subset of tests by size (Smoke, SmallTest, MediumTest, LargeTest,
# EnormousTest)
out/Debug/bin/run_content_shell_test_apk -A Smoke

# Run a subset of tests by annotation, such as filtering by Feature
out/Debug/bin/run_content_shell_test_apk -A Feature=Navigation
```

You might want to add stars `*` to each as a regular expression, e.g.
`*`AddressDetectionTest`*`

### Debugging

Similar to [debugging apk targets](../android_debugging_instructions.md#debugging-java):

```shell
out/Debug/bin/run_content_shell_test_apk --wait-for-java-debugger
```

### Deobfuscating Java Stacktraces

If running with `is_debug=false`, Java stacks from logcat need to be fixed up:

```shell
out/Release/bin/java_deobfuscate out/Release/apks/ChromePublicTest.apk.mapping < stacktrace.txt
```

Any stacks produced by test runner output will already be deobfuscated.


## Running Blink Web Tests

See [Web Tests](web_tests.md).

## Running GPU tests

(e.g. the "Android Debug (Nexus 7)" bot on the chromium.gpu waterfall)

See https://www.chromium.org/developers/testing/gpu-testing for details. Use
`--browser=android-content-shell`. Examine the stdio from the test invocation on
the bots to see arguments to pass to `src/content/test/gpu/run_gpu_test.py`.
