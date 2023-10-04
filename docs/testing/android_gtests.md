# How GTests work on Android

gtests are [googletest](https://github.com/google/googletest)-based C++ tests.
On Android, they run on a device. In most cases, they're packaged as APKs, but
there are a few cases where they're run as raw executables. The latter is
necessary in a few cases, particularly when manipulating signal handlers, but
isn't possible when the suite needs to call back through the JNI into Java code.

[TOC]

## APKs

### GN

Gtest APKs are built by default by the
[test](https://codesearch.chromium.org/chromium/src/testing/test.gni?type=cs&q=file:%5Esrc%5C/testing%5C/test.gni$+template%5C("test"%5C)&sq=package:chromium)
template, e.g.

```python
test("sample_gtest") {
  # ...
}
```

This uses gn's native
[shared_library](https://chromium.googlesource.com/chromium/src/+/main/tools/gn/docs/reference.md#shared_library_Declare-a-shared-library-target)
target type along with the
[unittest_apk](https://codesearch.chromium.org/chromium/src/build/config/android/rules.gni?type=cs&q=file:%5Esrc%5C/build%5C/config%5C/android%5C/rules.gni$+template%5C(%5C"unittest_apk%5C"%5C)&sq=package:chromium)
template to build an APK containing:

 - One or more .so files containing the native code on which the test suite
depends
 - One or more .dex files containing the Java code on which the test suite
depends
 - A [manifest](https://developer.android.com/guide/topics/manifest/manifest-intro.html)
file that contains `<instrumentation>` and `<activity>` elements (among others).

### Harness

GTest APKs are packaged with a harness that consists of:

  - [NativeTestInstrumentationTestRunner], an instrumentation entry point that
handles running one or more sequential instances of a test Activity. Typically,
unit test suites will only use one instance of the Activity and will run all of
the specified tests in it, while browser test suites will use multiple instances
and will only run one test per instance.
  - Three [Activity](https://developer.android.com/reference/android/app/Activity.html)-based
classes
([NativeUnitTestActivity](https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeUnitTestActivity.java),
[NativeUnitTestNativeActivity](https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeUnitTestNativeActivity.java),
and
[NativeBrowserTestActivity](https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeBrowserTestActivity.java))
that primarily act as process entry points for individual test shards.
Only one is used in any given suite.
  - [NativeTest] and [NativeUnitTest],
which handle formatting arguments for googletest and transferring control across
the JNI.
  - [testing::android::RunTests](https://codesearch.chromium.org/chromium/src/testing/android/native_test/native_test_launcher.cc?q=file:%5Esrc%5C/testing%5C/android%5C/native_test%5C/native_test_launcher.cc$+RunTests&sq=package:chromium),
the function on the native side, which initializes the native command-line,
redirects stdout either to a FIFO or a regular file, optionally waits for a
debugger to attach to the process, sets up the test data directories, and then
dispatches to googletest's `main` function.

### Runtime

 1. The test runner calls `am instrument` with a bunch of arguments,
    includes several extras that are arguments to either
    [NativeTestInstrumentationTestRunner] or [NativeTest]. This results in an
    intent being sent to [NativeTestInstrumentationTestRunner].
 2. [NativeTestInstrumentationTestRunner] is created. In its onCreate, it
    parses its own arguments from the intent and retains all other arguments
    to be passed to the Activities it'll start later. It also creates a
    temporary file in the external storage directory for stdout. It finally
    starts itself.
 3. [NativeTestInstrumentationTestRunner] is started. In its onStart, it prepares
    to receive notifications about the start and end of the test run from the
    Activities it's about to start. It then creates [ShardStarter]
    that will start the first test shard and adds that to the current
    [Handler](https://developer.android.com/reference/android/os/Handler.html).
 4. The [ShardStarter] is executed, starting the test Activity.
 5. The Activity starts, possibly doing some process initialization, and hands
    off to the [NativeTest].
 6. The [NativeTest] handles some initialization and informs the
    [NativeTestInstrumentationTestRunner] that it has started. On hearing this,
    the [NativeTestInstrumentationTestRunner] creates a [ShardMonitor] 
    that will monitor the execution of the test Activity.
 7. The [NativeTest] hands off to testing::android::RunTests. The tests run.
 8. The [NativeTest] informs the [NativeTestInstrumentationTestRunner] that is has
    completed. On hearing this, the [ShardMonitor] creates a [ShardEnder].
 9. The [ShardEnder] is executed, killing the child process (if applicable),
    parsing the results from the stdout file, and either launching the next
    shard via [ShardStarter] (in which case the process returns to #4) or sending
    the results out to the test runner and finishing the instrumentation.

## Executables

### GN

Gtest executables are built by passing
`use_raw_android_executable = True` to the 
[test](https://codesearch.chromium.org/chromium/src/testing/test.gni?type=cs&q=file:%5Esrc%5C/testing%5C/test.gni$+template%5C("test"%5C)&sq=package:chromium)
template, e.g.

```python
test("sample_gtest_executable") {
  if (is_android) {
    use_raw_android_executable = true
  }
  # ...
}
```

This uses gn's native
[executable](https://chromium.googlesource.com/chromium/src/+/main/tools/gn/docs/reference.md#executable_Declare-an-executable-target)
target type, then copies the resulting executable and any requisite shared libraries
to ```${root_out_dir}/${target_name}__dist``` (e.g. ```out/Debug/breakpad_unittests__dist```).

### Harness

Unlike APKs, gtest suites built as executables require no Android-specific harnesses.

### Runtime

The test runner simply executes the binary on the device directly and parses the
stdout on its own.

[NativeTest]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeTest.java
[NativeTestInstrumentationTestRunner]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeTestInstrumentationTestRunner.java
[NativeUnitTest]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeUnitTest.java
[ShardEnder]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeTestInstrumentationTestRunner.java?q=file:NativeTestInstrumentationTestRunner.java+class:ShardEnder&sq=package:chromium
[ShardMonitor]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeTestInstrumentationTestRunner.java?q=file:NativeTestInstrumentationTestRunner.java+class:ShardMonitor&sq=package:chromium
[ShardStarter]: https://codesearch.chromium.org/chromium/src/testing/android/native_test/java/src/org/chromium/native_test/NativeTestInstrumentationTestRunner.java?q=file:NativeTestInstrumentationTestRunner.java+class:ShardStarter&sq=package:chromium
