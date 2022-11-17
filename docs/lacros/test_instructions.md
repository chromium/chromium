# Test Lacros

Please read [Build instructions](build_instructions.md) first.
This document focuses on testing related topics.

## Instructions for Google Employees

Are you a Google employee? See
[go/lacros-test](https://goto.google.com/lacros-test) instead.


## Overview

Depends on which workflow you use, they support different test targets.

### Building and running on a Linux workstation.

  See [Lacros: Test instructions(linux)](test_linux_lacros.md).

  These tests are running on CI: linux-lacros-tester-rel and CQ: linux-lacros-rel.

*    Lacros gtest unit tests
*    Lacros browser tests
*    Lacros Interactive_ui_tests
*    crosapi tests: lacros_chrome_browsertests,
lacros_chrome_browsertests_run_in_series
*    Ash browser tests that requires Lacros: Only On Linux ChromeOS.
(linux-chromeos-rel).
*    Version skew testing: lacros_chrome_browsertests,
lacros_chrome_browsertests_run_in_series, interactive_ui_tests.

### Building on a Linux workstation and running on a physical ChromeOS device.

  See [Lacros: Test instructions(DUT)](test_dut_lacros.md)

*    Lacros gtest unit tests
*    Lacros Tast tests: On ChromeOS DUT/VM(lacros-amd64-generic-rel,
lacros-amd64-generic-chrome, etc)
*    Version skew testing:
     Lacros Tast tests on ChromeOS DUT/VM(lacros-amd64-generic-chrome-skylab,
lacros-arm-generic-chrome-skylab)

### Building on a Linux workstation and running on a ChromeOS VM running on the same Linux workstation.

  Same as above.

## What tests should you provide for your feature?

### gtest unit test

Unit tests should always be provided. We’re targeting per file unit test
coverage of 70%.

### Lacros browser tests

This refers to the tests in the browser_tests target. If your test is primarily
testing that your feature should work on Lacros, and your test will not cause
Lacros to call mojo crosapi, you should add a Lacros browser tests.
crosapi browser tests (lacros_chrome_browsertests)
If you’re testing crosapi, you need to add your test to
lacros_chrome_browsertests.

### Ash browser tests require Lacros

If your test is primarily testing Ash, but also requires Lacros to present,
you should add ash browser tests. The main difference is that browser() call
in the test will return an ash browser instance.

Note: if it’s fine to use lacros browser test or ash browser test, then please
add lacros browser test. Lacros browser test only start Ash once, then create
new Lacros for every test case.

However for ash browser test, for every test case it starts Ash as wayland
server every time, and then starts an ash browser and a Lacros browser. So in
theory, for the same test written in different approaches, Lacros browser test
is faster and more stable.

### Tast test

If your feature is hard/impossible to test on Linux, you should provide a Tast
test running on real cros devices or cros VM.

## Lacros related builders

*    https://ci.chromium.org/p/chromium/builders/ci/linux-lacros-builder-rel
*    https://ci.chromium.org/p/chromium/builders/ci/linux-lacros-tester-rel
*    https://ci.chromium.org/p/chromium/builders/try/linux-lacros-rel

CI/CQ linux lacros builder runs unit tests, browser tests, version skew tests.

*    https://ci.chromium.org/p/chrome/builders/ci/lacros-amd64-generic-chrome

CI builder using official gn args and running some Tast tests on DUT.

*    https://ci.chromium.org/p/chrome/builders/ci/lacros-arm-generic-chrome

Corresponding arm builder.

*    https://ci.chromium.org/p/chromium/builders/ci/linux-lacros-dbg
*    https://ci.chromium.org/p/chromium/builders/try/linux-lacros-dbg

CI and optional tryjob for running lacros tests with dbg gn args.

And also various of ash builders like chromeos-eve-chrome. If your cl only
changes ash, you can check ash builders which ensure your ash change doesn’t
break lacros.
