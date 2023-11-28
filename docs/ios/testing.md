# Automated testing for Chrome for iOS

See the [instructions] for how to check out and build Chromium for iOS.

Automated testing is a crucial part of ensuring the quality of Chromium.

## Unit testing

Unit testing is done via gtests. To run a unit test, simply run the test
target (ending in _unittest).

## Integration testing

[EarlGrey] (EG2) is the integration testing framework used by Chromium for iOS.

### Writing EarlGrey tests

#### Before you start

* Just write a unit test if the purpose of your test does not involve UI.
* Learn about EarlGrey test framework principles and APIs in [EarlGrey].
* Learn about [Defining Test Cases and Test Methods] from Apple.

#### Creating test files and writing EG2 tests

1. EG2 test files are ended with _egtest.mm, and usually located within the same
directory of the UI code you wish to test.
2. Basic imports of a EG2 test file:

    * You’ll have to include:
    ```
    #import "ios/chrome/test/earl_grey/chrome_test_case.h"
    ```
    * You’ll most likely find util functions in these files helpful.
    ```
    #import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
    #import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
    #import "ios/chrome/test/earl_grey/chrome_matchers.h"
    ```
    * Beside these, directly import an EG2 header for an EG2 API you are using.

3. TestCase/testMethods definitions. Create `SomeGreatTestCase` as a subclass of
`ChromeTestCase`. Create test methods, eg `-(void)testMyGreatUIFeature {...}`,
and put UI actions within the test methods. 
    * Put your setup and tear down code for the *TestCase* in 
`+(void)setUpForTestCase` and `+tearDown`. These will run once before and
after all tests for the test class.
    * Put your setup and tear down code for each *test method* in `-(void)setUp`
and `-(void)tearDown`. These will run before and after every
`-(void)testMethod` in the file.
4. Writing test contents. See the chrome helpers (imports in 2.) as well as
[EarlGrey APIs] to write a UI action/assertion in your testMethod.

#### Interacting with the app in a test

##### Relaunch app with different flags

In EG2 tests, the test process launches the host app process at the beginning,
then runs UI actions/assertions in the app. To pass args or feature flags to the
app at initial launching, or relaunch the app in the middle of your test, see
[this AppLaunchManager API].

##### Accessing app internals

EG2 test targets are built with test-related code but without app code.

To access anything from the app side, use an "app interface". App interface is
implemented as a class that lives in the app process, but can be accessed in the
test process through [eDO]. You can include the header in your test side code
and call class methods of the interface class. The methods will execute code in
the app process and can return basic Objective-C types. See this [Example of App
 Interface].

See `eg_test_support+eg2` (test side utilities) and `eg_app_support+eg2` (app
side utilities) targets in `BUILD.gn` files to learn how test utilities are
organized in targets. If you added an app side helper (app interface), you’ll
also need to include your new `eg_app_support+eg2` target in
`//ios/chrome/test/earl_grey/BUILD.gn`’s `eg_app_support+eg2` target. ([Example
 CL adding App Interface]).

Note that if you create an App interface, you can’t build the app interface
class in your eg2_tests target, but you need to include and refer to it. To
satisfy the linker, you'll need to create a `my_test_app_interface_stub.mm`
file with the following content in it and build it as a dependency of your
tests that use the app interface.

```objc
#import "ios_internal/chrome/test/earl_grey2/my_test_app_interface.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(MyTestAppInterface)

```

If you don't you'll get linker errors that read like “Undefined symbols for
architecture… MyTestAppInterface”

#### Creating test targets and adding the target to test suites

1. Create a test target. Add a target(`source_set`) named "eg2_tests" into the
closest `BUILD.gn` file. Put the test file into the `sources` array and put the
targets containing headers used in your test file into `deps` array. This is to
organize test source files and dependencies so that the GN build system can
correctly build the test module. The skeleton of the target:
```
source_set("eg2_tests") {
  configs += [
    "//build/config/ios:xctest_config",
  ]
  testonly = true
  sources = [
    "some_egtest.mm"
  ]
  deps = [
    "//ios/chrome/test/earl_grey:eg_test_support+eg2",
    "//ios/testing/earl_grey:eg_test_support+eg2",
  ]
  frameworks = [ "UIKit.framework" ]
}
```
2. Include your test target in the `deps` array of a suitable suite in
`//src/ios/chrome/test/earl_grey2/BUILD.gn`.
3. Optional: If you feel like your new test should be in a new suite, or you
want to delete an existing suite to make tests better organized, you’ll need to
change the suites in `//src/ios/chrome/test/earl_grey2/BUILD.gn` in the format
of existing ones. (Do not forget to [config the bots] so the new suite can run
in infra.)
4. Ensure your dependencies are correct.
```
$ gn gen --check out/Debug-iphonesimulator
```

### Running EarlGrey tests

EarlGrey tests are based on Apple's [XCUITest].

#### Running tests from Xcode

1. If you added a new test file / suite, run `gclient runhooks` to sync for the
list of tests in Xcode.
2. Run a test suite(module), TestCase or testMethod in test navigator.
Xcode will build the targets and run the test(s) you choose. Alternatively, 
use ⌘+U to run all the tests. See Apple's [Running Tests and Viewing Results].
3. You can pass extra arguments to the app process with `--extra-app-args`, e.g.
`--extra-app-args='--enable-features=Foo'`.
    * This might not work consistently as tests can re-launch the app with
    arbitrary command-line arguments.


#### Running from the command-line

EG2 tests can run in the command line with test runner scripts. You’ll need to
build the targets before running tests in cmd. This is used by continuous
integration infra and thus not user friendly. Running UI tests directly in Xcode
is recommended.

Important notes:
* The test runner can invoke mac_toolchain to install a new Xcode of the version
specified to the path specified. You may want to choose a different path from
your daily use Xcode.
* If test_cases is empty in --args-json, all tests will run. Specifying a
testMethod to run is currently not supported in the test runner.

Example:
```
src/ios/build/bots/scripts/run.py
    --app
    src/out/Debug-iphonesimulator/ios_chrome_ui_eg2tests_module-Runner.app
    --host-app
    src/out/Debug-iphonesimulator/ios_chrome_eg2tests.app
    --args-json
    {"test_args": [], "xctest": false, "test_cases": ["ReadingListTestCase"],
    "restart": false, "xcode_parallelization": true, "xcodebuild_device_runner":
    false}
    --out-dir
   path/to/output/dir
    --retries
    3
    --shards
    1
    --xcode-build-version
    11c29
    --mac-toolchain-cmd
    path/to/mac_toolchain
    --xcode-path
    path/to/Xcode.app
    --wpr-tools-path
    NO_PATH
    --replay-path
    NO_PATH
    --iossim
    src/out/Debug-iphonesimulator/iossim
    --platform
    iPad (6th generation)
    --version
    13.3
```
The invocation args are logged. You can find the latest arg format at the
beginning of stdout from an infra test shard if the above doesn't work.


[config the bots]: https://chromium.googlesource.com/chromium/src/testing/+/refs/heads/main/buildbot/README.md#buildbot-testing-configuration-files
[Defining Test Cases and Test Methods]: https://developer.apple.com/documentation/xctest/defining_test_cases_and_test_methods?language=objc
[EarlGrey]: https://github.com/google/EarlGrey/tree/earlgrey2
[EarlGrey APIs]: https://github.com/google/EarlGrey/blob/master/docs/api.md
[eDO]: https://github.com/google/eDistantObject
[Example of App Interface]: https://cs.chromium.org/chromium/src/ios/chrome/browser/metrics/model/metrics_app_interface.h
[Example CL adding App Interface]: https://chromium-review.googlesource.com/c/chromium/src/+/1919147
[instructions]: ./build_instructions.md
[Running Tests and Viewing Results]: https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/testing_with_xcode/chapters/05-running_tests.html
[this AppLaunchManager API]: https://source.chromium.org/chromium/chromium/src/+/main:ios/testing/earl_grey/app_launch_manager.h;drc=d0889865de20c5b3bc59d58674eb2dcc02dd2269;l=47
[XCUITest]: https://developer.apple.com/documentation/xctest
