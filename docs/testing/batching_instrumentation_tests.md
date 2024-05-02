# Instrumentation Test Batching Guide

## What is Test Batching?

Outside of Chromium, it is most common to run all tests of a test suite using a
single `adb shell am instrument` command (a single execution / OS process).
However, Chromium's test runner runs each test using a separate command, which
means tests cannot interfere with one another, but also that tests take much
longer to run. Test batching is a way to make our tests run faster by not
restarting the process between every test.

All on-device tests would ideally be annotated with one of:

* `@Batch(Batch.UNIT_TESTS)`: For tests the do not rely on global application
  state.
* `@Batch(Batch.PER_CLASS)`: For test classes where the process does not need
   to be restarted between `@Test`s within the class, but should be restarted
   before and after the suite runs.
* `@DoNotBatch(reason = "..."`: For tests classes that require the process to be
  restarted for each test or are infeasible to batch.

Tests that are not annotated are treated as `@DoNotBatch` and are assumed to
have not yet been assessed.

## How to Batch a Test

Add the `@Batch` annotation to the test class, and ensure that each test within
the chosen batch doesn't leave behind state that could cause other tests in the
batch to fail.

For some tests, batching won’t be as useful (tests that test Activity
startup, for example), and tests that test process startup shouldn’t be batched
at all.

If a few tests within a larger batched suite cannot be batched (eg. it tests
process initialization), you may add the
[@RequiresRestart](https://source.chromium.org/chromium/chromium/src/+/main:base/test/android/javatests/src/org/chromium/base/test/util/RequiresRestart.java;bpv=1;bpt=1;l=19?q=RequiresRestart&ss=chromium%2Fchromium%2Fsrc&originalUrl=https:%2F%2Fcs.chromium.org%2F&gsn=RequiresRestart&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Djava%3Fpath%3Dorg.chromium.base.test.util.RequiresRestart%23b5e85d5c8071e18f350b7f2c5014310bd2cabd0e0d3d176949c991ea18403f55)
annotation to test methods to exclude them from the batch.

## Types of Batched tests

### [UNIT_TESTS](https://source.chromium.org/chromium/chromium/src/+/main:base/test/android/javatests/src/org/chromium/base/test/util/Batch.java;bpv=1;bpt=1;l=51?q=Batch.java&ss=chromium%2Fchromium%2Fsrc&originalUrl=https:%2F%2Fcs.chromium.org%2F&gsn=UNIT_TESTS&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Djava%3Fpath%3Dorg.chromium.base.test.util.Batch%2319ebd2758adfaed0bda0e97542f70ca5b1564e7c1fa0f8c2bcb9e8170b75684d)

Tests that belong in this category are tests that are effectively unit tests.
They may be written as instrumentation tests rather than junit tests for a
variety of reasons such as needing to use real Android APIs, or needing to
use the native library.

Batching Unit Test style tests is usually fairly simple
([example](https://chromium-review.googlesource.com/c/chromium/src/+/2216044)).
It requires adding the `@Batch(Batch.UNIT_TESTS)` annotation, and ensuring no
global state, like test overrides, persists across tests. Unit Tests should also
not start the browser process, but may load the native library. Note that even
with Batched tests, the test fixture (the class) is recreated for each test.

Note that since the browser isn't initialized for unit tests, if you would like
to take advantage of feature annotations in your test you will have to use
`Features.JUnitProcessor` instead of `Features.InstrumentationProcessor`.


### [PER_CLASS](https://source.chromium.org/chromium/chromium/src/+/main:base/test/android/javatests/src/org/chromium/base/test/util/Batch.java;bpv=1;bpt=1;l=39?q=Batch.java&ss=chromium%2Fchromium%2Fsrc&originalUrl=https:%2F%2Fcs.chromium.org%2F&gsn=PER_CLASS&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Djava%3Fpath%3Dorg.chromium.base.test.util.Batch%23780b702db42a1901f05647fd29f75d443bc4efd2db588848b4aedf826ddf9e21)

This batching type is typically for larger and more complex test suites, and
will run the suite in its own batch. This will limit side-effects and reduce
the complexity of managing state from these tests as you only have to think
about tests within the suite.

Tests with different `@Features` annotations (`@EnableFeatures` and
`@DisableFeatures`) or `@CommandLineFlags` will be run in separate batches.

### Custom

This batching type is best for smaller and less complex test suites, that
require browser initialization, or something else that prevents them from being
unit tests. Custom batches allow you to pay the process startup cost once per
batch instead of once per test suite. To put multiple test suites into the same
batch, you will have to use a shared custom batch name
([example](https://chromium-review.googlesource.com/c/chromium/src/+/2307650)).
When batching across suites you’ll want to use something like
[BlankCTATabInitialStateRule](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/android/javatests/src/org/chromium/chrome/test/batch/BlankCTATabInitialStateRule.java?q=BlankCTATabInitialStateRule&ss=chromium&originalUrl=https:%2F%2Fcs.chromium.org%2F)
to persist static state (like the Activity) between test suites and perform any
necessary state cleanup between tests.

Note that there is an inherent tradeoff here between batch size and
debuggability - the larger your batch, the harder it will be to diagnose one
test causing a different test to fail/flake. I would recommend grouping tests
semantically to make it easier to understand relationships between the tests and
which shared state is relevant.

### Running Test Batches

Run all tests with `@Batch=UnitTests`:

```shell
out/<dir>/bin/run_chrome_public_unit_test_apk -A Batch=UnitTests

out/<dir>/bin/run_chrome_public_test_apk -A Batch=UnitTests
```

Run all tests in a custom batch:
```shell
./tools/autotest.py -C out/Debug BluetoothChooserDialogTest \
--gtest_filter="*" -A Batch=device_dialog
```

## Things worth noting

* Activities won’t be automatically finished for you, if your test requires
that. Other common state like SharedPreferences
[issue 1086663](https://crbug.com/1086663) also won’t be automatically reset.
* @ClassRule and @BeforeClass/@AfterClass run during test listing, so don’t do
any heavy work in them (and will run twice for parameterized tests). See
[issue 1090043](https://crbug.com/1090043).
* Sometimes it can be very difficult to figure out which test in a batch is
causing another test to fail. A good first step is to minimize [_TEST_BATCH_MAX_GROUP_SIZE](https://source.chromium.org/chromium/chromium/src/+/main:build/android/pylib/local/device/local_device_instrumentation_test_run.py;drc=3ab9a142091516aa57f10feebc46dee649ae4589;l=109)
to minimize the number of tests within the batch while still reproducing the
failure. Then, you can use multiple gtest filter patterns to control which tests
run together. Ex:
  ```shell
  ./tools/autotest.py -C out/Debug ExternalNavigationHandlerTest \
  --gtest_filter="*#testOrdinaryIncognitoUri:*#testChromeReferrer"
  ```
