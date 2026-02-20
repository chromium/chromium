# Blink renderer/core Development Guidelines for AI Agents

This document provides essential instructions and best practices for developing in the `third_party/blink/renderer/core` tree. Adhere to these guidelines to ensure consistency and quality.

## Building and Running tests
- General
    - In subsequent instructions, a build output directory of `out/Default` is assumed. Replace `out/Default` with the local output dir, as appropriate.
- Chromium
    - To build Chromium, you should use: `autoninja -C out/Default chrome`.
    - This is useful for making sure any code changes compile.
- WPTs (Web Platform Tests)
    - This is the most common test type for web platform features.
    - These tests are located in `third_party/blink/web_tests/external/wpt`.
    - To run them, you should use: `third_party/blink/tools/run_wpt_tests.py -t Default {testname}` where {testname} is something like `external/wpt/shadow-dom/some-test-name.html`. Do not include the leading `third_party/blink/web_tests/` part of the test path when running tests like this, but *do* include the `external/wpt/`.
- Web tests, a.k.a Layout tests
    - These are very similar to WPTs, in usage and structure, but they are separate internal Chromium-only tests.
    - When writing new tests, do not use these. Use WPTs instead, which are strongly preferred.
    - To run a web test, you should use: `third_party/blink/tools/run_web_tests.py -t Default {testname}` where {testname} is something like `fast/frames/some-test-name.html`. Do not include the leading `third_party/blink/web_tests/` part of the test path when running tests like this.
- Common things for WPTs and Web Tests
    - To build the tests, you should use: `autoninja -C out/Default blink_tests`.
    - When trying to reproduce flakiness, it is sometimes helpful to add `--iterations 10` to the `run_wpt_tests.py` or `run_web_tests.py` command line, to repeat the test 10 times.
    - You can examine test output with `cat out/Default/layout-test-results/[test path]/{testname}-stderr.txt` or `...{testname}-actual.txt`. For example, `cat out/Default/layout-test-results/external/wpt/shadow-dom/some-test-name-stderr.txt` or `cat out/Default/layout-test-results/fast/frames/some-test-name-actual.txt`.
    - A file called `third_party/blink/web_tests/TestExpectations` is used to modify the behavior of these test suites. Each line in this file has a prefix (bug number and operating systems impacted), a test path, and a suffix containing allowed test results. The results can be one or more of: Timeout, Crash, Pass, Failure, or Skip. If a test matches a line in this file, the actual test result is compared to the allowed test results list, and if *any* of them match, the test is reported as working "as expected". When debugging test failures, it is important to check this file first, to avoid false positives. E.g. If the test is listed with `[ Failure Pass ]`, then the test will appear to succeed if it passes *or* fails. Often the best course of action is to delete the line with the test in question before attempting to reproduce a failure or fix a bug.
    - These test suites can *also* include an "expectations file", which is a file located either directly next to the test, or within a platform-specific directory within `third_party/blink/web_tests/platform/`. The file will be named `{testname without extension}-expected.txt`. When trying to reproduce flakiness or failures, or fix bugs, it might be important to first delete or at least examine the expectation file. If the test outputs exactly what is contained in the expectation file, the test runner will report that the test ran "as expected".
- Unit tests
    - These are less common for web platform features. They execute C++ code to test internals for a specific unit of code.
    - To build the tests, use `autoninja -C out/Default blink_unittests`.
    - To run a unit test, you should use: `out/Default/blink_unittests --gtest_filter="{testname}"` where {testname} is the qualified test name. You can include asterisks to include Param variations. For example, `--gtest_filter="*TestSuiteName.TestPrefixOrName*"`.
- Browser tests
    - These are less common for web platform features. They run a full browser and execute C++ code to test browser internals.
    - To build the tests, use `autoninja -C out/Default browser_tests`.
    - To run a browser test, you should use: `out/Default/browser_tests --gtest_filter="{testname}"` where {testname} is the qualified test name. You can include asterisks to include Param variations. For example, `--gtest_filter="*TestSuiteName.TestPrefixOrName*"`.

## Feature flags
- The typical way to enable and disable features in Blink is through a "runtime enabled feature". These are all specified in the `third_party/blink/renderer/platform/runtime_enabled_features.json5` file, as an object in the array with `name: "FeatureName"`, and `status: {a string}`.
- When modifying the behavior of the browser in a way that developers or users might be able to detect, it is strongly advised that you add a runtime enabled feature, which is set to "stable" by default, but that flag-guards the new behavior. In this way, if a problem is detected later, the feature can be remotely disabled. Ensure the code is written so that with the feature *disabled*, the behavior is not changed.
- Checking the state of a runtime enabled feature in code is done via `RuntimeEnabledFeatures::FeaturenameEnabled()`. For example, for a feature with `name: "FeatureName"`, you can check via `RuntimeEnabledFeatures::FeatureNameEnabled()`.
- By default, all runtime enabled features automatically generate a corresponding `base::Feature` of the same name. That feature can be checked with `base::FeatureList::IsEnabled(features::kFeatureName)`. Typically, though, that shouldn't be done, and only the status of the runtime enabled feature should be used/checked.

## General instructions
- Unless specifically instructed otherwise, when fixing bugs, always try to run a test that confirms the failing behavior *before* proceeding to try to fix the bug. If no such test exists, start by writing one.
- When fixing bugs, be careful **NOT** to make changes to tests that simply mask actual underlying failures in the Chromium behavior. I.e. if the test is actually catching a Chromium bug, do not simply modify the test to disable it, or to avoid provoking the bug.
- One useful tool when debugging Chromium code is to add log statements to output helpful state. This can be done using `LOG(ERROR) << "Some interesting state " << some_variable;`. When it isn't clear what the code is doing, try adding debugging statements such as this before running tests, to confirm your understanding.
- When adding files, you'll need to create the standard copyright header including the year. To find the current year, execute `echo $(date +%Y)`.
