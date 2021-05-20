# Android Instrumentation Tests

Instrumentation tests are Java tests based on
[`android.app.Instrumentation`](https://developer.android.com/reference/android/app/Instrumentation.html).
They run on a device.

[TOC]

## Writing an instrumentation test

Currently, an instrumentation test is just a JUnit3-style test based on
[android.test.InstrumentationTestCase](https://developer.android.com/reference/android/test/InstrumentationTestCase.html).
(This will change starting in [Android N](https://en.wikipedia.org/wiki/Android_Nougat).)

Writing an instrumentation test case can be simple, e.g.

```java
package org.chromium.sample.test;

import android.test.InstrumentationTestCase;

public class MyInstrumentationTest extends InstrumentationTestCase {

    // Note that, because it's a JUnit3-style test, the test method *must*
    // start with "test".
    public void testTheFirst() {
        bool writingInstrumentationTestsCanBeEasy = true;

        // InstrumentationTestCase inherits the assert* methods through
        // junit.framework.TestCase.
        assertTrue(writingInstrumentationTestsCanBeEasy);
    }

    public void testTheSecond() {
        bool writingInstrumentationTestsIsAlwaysEasy = false;
        assertFalse(writingInstrumentationTestsIsAlwaysEasy);
    }
}
```

After writing a test, you can run it by:

 - Adding the file to the relevant gn target if the entire file is new.
Typically, the "relevant gn target" is simply the target containing the
other files in the same directory.
 - Rebuild.
 - Run the test using the process described [here](/testing/android/docs/todo.md).

## Instrumentation test features

In many cases, Chromium has extended the instrumentation test framework
classes to implement additional features.

### Tracing

Enabling tracing during a test run allows all the function calls involved to be
observed in a visual display (using Chrome's built-in chrome://tracing feature).
To run a test with tracing, add the `--trace-output` flag to the command used to
call the instrumentation test (either running the test_runner.py script, or a
generated binary such as `run_chrome_public_test_apk`). The `--trace-output` flag
takes a filename, which, after the test run, will contain a JSON file readable
by chrome://tracing.

By default, the trace includes only certain function calls important to the test
run, both within the Python test runner framework and the Java code running on
the device. For a more detailed look, add the (no-argument) `--trace-all` flag.
This causes every function called on the Python side to be added to the trace.

### Annotations

Instrumentation tests in Chromium use a wide variety of annotations to control
and manipulate test execution. Some of these are implemented in Chromium, while
others are pulled in from outside. They include:

#### Test Batching

The [`@Batch("group_name")`](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/Batch.java)
annotation is used to run all tests with the same batch group name in the same
instrumentation invocation. In other words, the browser process is not
restarted between these tests, and so any changes to global state, like
launching an Activity, will persist between tests within a batch group. The
benefit of this is that these tests run significantly faster - the per-test cost
of restarting the process can be as high as 10 seconds (usually around 2
seconds), and that doesn't count the cost of starting an Activity like
ChromeTabbedActivity.

#### Size annotations

Size annotations are used primarily by the test runner to determine the length
of time to wait before considering a test hung (i.e., its timeout duration).

Several of the annotations are Android APIs from
[android.test.suitebuilder.annotation](https://developer.android.com/reference/android/test/suitebuilder/annotation/package-summary.html)
(prior to [Android N](https://en.wikipedia.org/wiki/Android_Nougat)) or
[android.support.test.filters](https://developer.android.com/reference/android/support/test/filters/package-summary.html)
(starting in Android N). These are all fairly self-explanatory:

 - [`@SmallTest`](https://developer.android.com/reference/android/support/test/filters/SmallTest.html) (timeout: **10 seconds**)
 - [`@MediumTest`](https://developer.android.com/reference/android/support/test/filters/MediumTest.html) (timeout: **30 seconds**)
 - [`@LargeTest`](https://developer.android.com/reference/android/support/test/filters/LargeTest.html) (timeout: **2 minutes**)

A few additional size annotations are provided in
[//base](https://chromium.googlesource.com/chromium/src/+/main/base):

 - [`@EnormousTest`](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/EnormousTest.java)
(timeout: **5 minutes**) Typically used for tests that require WiFi.
 - [`@IntegrationTest`](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/IntegrationTest.java)
(timeout: **10 minutes**) Used for tests that run against real services.
 - [`@Manual`](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/Manual.java)
(timeout: **10 hours**) Used for manual tests.

Beware that the timeout durations for these annotations are subject to
change, though they rarely do. These values are defined
[here](https://chromium.googlesource.com/chromium/src/+/main/build/android/pylib/local/device/local_device_instrumentation_test_run.py#20).

#### Annotations that disable tests

There are several annotations that control whether or not a test runs.
Some are conditional, others are not.

##### Unconditional disabling

[**@DisabledTest**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/DisabledTest.java)
unconditionally disables a test.
```java
@DisabledTest(
    // Describes why the test is disabled. Typically includes a crbug link.
    message = ""
)
```

[**@FlakyTest**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/FlakyTest.java)
marks a test as flaky. This also unconditionally disables the test, though
tests marked with **@FlakyTest** are explicitly run on some bots.
```java
@FlakyTest(
    // Describes why the test is marked flaky. Typically includes a crbug link.
    message = ""
)
```

Note that there are Android versions of **@DisabledTest** and **@FlakyTest**
that do not allow message specification. These are no longer used in Chromium.

As alluded to above, tests marked with either **@DisabledTest** or
**@FlakyTest** can be explicitly run via the test runner's
[-A/--annotation](/testing/android/docs/todo.md)
flag. For example, this would run only the tests marked as flaky in
`chrome_public_test_apk`:

```bash
./out/Debug/bin/run_chrome_public_test_apk -A FlakyTest
```

##### Conditional disabling

There are two primary annotation categories that conditionally disable tests:
**@DisableIf** and **@Restriction**. The **@DisableIf** annotations are intended
to temporarily disable a test in certain scenarios where it *should* work but
doesn't. In contrast, the **@Restriction** annotation is intended to
permanently limit a test to specific configurations. It signifies that the test
was not, is not, and will not be intended to run beyond those configurations.
In both cases, conditional disabling manifests as a skipped test.

[**@DisableIf.Build**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/DisableIf.java#25)
allows for conditional test disabling based on values in
[`android.os.Build`](https://developer.android.com/reference/android/os/Build.html):

```java
@DisableIf.Build(

    // Describes why the test is disabled.
    message = "",

    // Disables the test on SDK levels that match the given conditions.
    // Checks against Build.VERSION.SDK_INT.
    sdk_is_greater_than = 0,
    sdk_is_less_than = Integer.MAX_VALUE,

    // Disables the test on devices that support the given ABI
    // (e.g. "arm64-v8a"). Checks against:
    //  - Build.SUPPORTED_ABIS on L+
    //  - Build.CPU_ABI and Build.CPU_ABI2 otherwise
    supported_abis_includes = "",

    // Disables the test on devices with hardware that matches the given
    // value. Checks against Build.HARDWARE.
    hardware_is = "",

    // Disables the test on devices with product names that contain the
    // given value. Checks against Build.PRODUCT.
    product_name_includes = "",

)
```

[**@DisableIf.Device**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/DisableIf.java#40)
allows for conditional test disabling based on whether
a device is a phone, a tablet, or a "large tablet" as determined by
[org.chromium.ui.base.DeviceFormFactor](https://chromium.googlesource.com/chromium/src/+/main/ui/android/java/src/org/chromium/ui/base/DeviceFormFactor.java).
This is available to tests in
[//ui](https://chromium.googlesource.com/chromium/src/+/main/ui/)
or code that uses //ui.

```java
@DisableIf.Device(
    // Disables the test on devices that match the given type(s) as described
    // above.
    type = {}
)
```

[**@Restriction**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/Restriction.java)
currently allows for conditional test disabling based on device
type, device performance, internet connectivity, whether Google Play Services is
up to date, and whether the build was an official one.

```java
@Restriction(
    // Possible values include:
    //
    // base:
    //  - Restriction.RESTRICTION_TYPE_LOW_END_DEVICE
    //    Restricts the test to low-end devices as determined by SysUtils.isLowEndDevice().
    //
    //  - Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
    //    Restricts the test to non-low-end devices as determined by SysUtils.isLowEndDevice().
    //
    //  - Restriction.RESTRICTION_TYPE_INTERNET
    //    Restricts the test to devices that have an internet connection.
    //
    // chrome:
    //  - ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES
    //    Restricts the test to devices with up-to-date versions of Google Play Services.
    //
    //  - ChromeRestriction.RESTRICTION_TYPE_OFFICIAL_BUILD
    //    Restricts the test to official builds as determined by ChromeVersionInfo.isOfficialBuild().
    //
    // ui:
    //  - UiRestriction.RESTRICTION_TYPE_PHONE
    //    Restricts the test to phones as determined by DeviceFormFactor.
    //
    //  - UiRestriction.RESTRICTION_TYPE_TABLET
    //    Restricts the test to tablets as determined by DeviceFormFactor.
    value = {}
)
```

[**@MinAndroidSdkLevel**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/MinAndroidSdkLevel.java)
is similar to **@Restriction** in purpose in that it's
intended to permanently limit a test to only recent versions of Android.

```java
@MinAndroidSdkLevel(
    // The minimum SDK level at which this test should be executed. Checks
    // against Build.VERSION.SDK_INT.
    value = 0
)
```

#### Annotations that affect how a test is run

Several annotations affect how a test is run in interesting or nontrivial ways.

[**@CommandLineFlags.Add**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/CommandLineFlags.java#46)
and
[**@CommandLineFlags.Remove**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/CommandLineFlags.java#58)
manipulate Chrome's
command-line flags on a per-test basis (i.e., the flags handled by
[`org.chromium.base.CommandLine`](https://chromium.googlesource.com/chromium/src/+/main/base/android/java/src/org/chromium/base/CommandLine.java) and
[`base::CommandLine`](https://chromium.googlesource.com/chromium/src/+/main/base/command_line.h)).

```java
@CommandLineFlags.Add(
    // The flags to add to the command line for this test. These can be
    // anything and typically should include the leading dashes (e.g. "--foo").
    value = {}
)

@CommandLineFlags.Remove(
    // The flags to remove from the command line for this test. These can only
    // be flags added via @CommandLineFlags.Add. Flags already present in the
    // command-line file on the device are only present in the native
    // CommandLine object and cannot be manipulated.
    value = {}
)
```

#### Feature annotations

[**@Feature**](https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/util/Feature.java)
has been used inconsistently in Chromium to group tests across
test cases according to the feature they're testing.

```java
@Feature(
    // The features associated with this test. These can be anything.
    value = {}
)
```

@Feature doesn't have an inherent function, but it can be used to filter tests
via the test runner's
[-A/--annotation](/testing/android/docs/todo.md) and
[-E/--exclude-annotation](/testing/android/docs/todo.md) flags. For example,
this would run only the tests with @Feature annotations containing at least
"Sync" in `chrome_public_test_apk`:

```bash
out/Debug/bin/run_chrome_public_test_apk -A Feature=Sync
```

## Advanced

 - [Creating a component for use in instrumentation tests.](/testing/android/docs/components_for_testing.md)
