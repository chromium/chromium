# Android Testing in Chromium

## Concepts
  - **Native Unit Tests**: Normal Chromium unit tests based on the [gtest framework](/testing/android/docs/gtest_implementation.md). Tests for native code.
  - **Java Unit Tests**: JUnit tests that run on the host machine using [Robolectric](http://robolectric.org) to emulate Android APIs.
  - **Instrumentation Tests**: JUnit tests that run on Android devices (or emulators).
    - **Unit Instrumentation Tests**: Instrumentation tests that test an individual feature. They do not require starting up ContentShell (or Chrome browser). These use [BaseActivityTestRule](https://source.chromium.org/chromium/chromium/src/+/main:base/test/android/javatests/src/org/chromium/base/test/BaseActivityTestRule.java) or [BlankUiTestActivityTestCase](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/javatests/src/org/chromium/ui/test/util/BlankUiTestActivityTestCase.java) based on `BaseActivityTestRule`.
    - **Integration Instrumentation Tests**: Instrumentation tests that bring up ContentShell (or Chrome browser) to test a certain feature in the end-to-end flow. These typically use more specialized test rules such as [ContentShellActivityTestRule](https://source.chromium.org/chromium/chromium/src/+/main:content/shell/android/javatests/src/org/chromium/content_shell_apk/ContentShellActivityTestRule.java) or [ChromeActivityTestRule](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/android/javatests/src/org/chromium/chrome/test/ChromeActivityTestRule.java).

## How do I...

  - **set up**
    - [... set up a device for local testing?](/testing/android/docs/android_test_instructions.md#physical-device-setup)
    - [... set up an emulator for local testing?](/docs/android_emulator.md)
  - **writing tests**
    - [... write an instrumentation test?](/testing/android/docs/instrumentation.md)
    - [... write a JUnit or Robolectric test?](/testing/android/docs/junit.md)
    - [... write a test that needs to mock native calls?](/base/android/jni_generator/README.md#testing-mockable-natives)
  - **running tests**
    - [... run a gtest?](/testing/android/docs/android_test_instructions.md#gtests)
    - [... run an instrumentation test?](/testing/android/docs/android_test_instructions.md#instrumentation-tests)
    - [... run a JUnit test?](/testing/android/docs/android_test_instructions.md#junit-tests)
    - [... run the Web tests?](/docs/testing/web_tests.md)
    - [... run a telemetry test?](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md)
  - **debugging tests**
    - [... debug junit tests?](/testing/android/docs/android_test_instructions.md#junit-tests-debugging)
    - [... debug flaky tests?](/testing/android/docs/todo.md)
  - **miscellaneous**
    - [... use code coverage for Java tests?](/build/android/docs/coverage.md)

## How does it work on Android?

  - [gtests](/testing/android/docs/gtest_implementation.md)
  - [instrumentation tests](https://source.android.com/compatibility/tests/development/instrumentation)
  - [junit tests](/testing/android/docs/junit.md)
