# Deploying content_shell and running web_tests on Fuchsia

[TOC]

General instruction on running and debugging web_tests can be found
[here](../testing/web_tests.md).

Unlike on other platforms, where tests are directly invoked via the
[blink test script](third_party/blink/tools/blinkpy/web_tests/run_web_tests.py),
Fuchsia layers on top [its own test script] (../../build/fuchsia/test/run_test.py),
which handles preparation such as installing the content_shell binary.

Currently, only
[a small subset of web tests](../../third_party/blink/web_tests/TestLists/Default.txt)
can be run on Fuchsia. Build the target `blink_web_tests` first before running any
of the commands below:

## Hermetic emulation

The test script brings up an emulator, runs the tests on it, and shuts the
emulator down when finished.
```bash
$ <output-dir>/bin/run_blink_web_tests
```

## Run on an physical device.

```bash
$ <output-dir>/bin/run_blink_web_tests --target-id=<device-target-id>
```
