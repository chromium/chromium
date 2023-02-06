# Run Tests on Fuchsia with CFv2 Test Scripts

**If you are looking for information on _running_ tests, see
[Deploying and running gtests on Fuchsia](gtests.md).**

[TOC]

This page documents the individual scripts that support running various Chromium
[test types](build_instructions.md#running-test-suites).
The scripts can be found [here](../../build/fuchsia/test/) The new scripts currently support:

## Run CFv2 gtest binaries on Fuchsia

To build gtest binaries for Fuchsia follow
[build instructions](build_instructions.md).

#### E2E Testing Script (recommended)

Once the emulator is running, you can run tests on this emulator instance by
adding the command line arguments indicated above:

```bash
$ ./build/fuchsia/test/run_test.py [TEST_BINARY] -C [OUTPUT_DIR] --target-id [EMULATOR_NAME]
```

For gtests, there are scripts that further abstract the command line above.
See [Deploying and running gtests on Fuchsia](gtests.md).

#### Step by step approach

Alternatively, testing can be done step by step. The following steps are
equivalent to the [E2E testing script](#e2e-testing-script).

Before starting the steps, it is recommended to set the device that will be used
as the default device that `ffx` uses:

```bash
$ ./third_party/fuchsia_sdk/sdk/tools/x64/ffx target default set [EMULATOR_NAME]
```

If the device is not set as default, all the steps other than package publishing
will require an extra `--target-id [EMULATOR_NAME]` flag.

##### Publish packages

Before devices can run the packages built, a directory needs to be initialized
to serve as a package repository and the packages must be published to it:

```bash
$ ./build/fuchsia/test/publish_package.py -C [OUTPUT_DIR] --repo [REPO_DIR] --packages [TEST_BINARY]
```

##### Package installation

The packages need to be installed on the device:

```bash
$ ./build/fuchsia/test/serve_repo.py start --serve-repo [REPO_DIR]
```

##### Stream system logs

System logs can be obtained via:

```bash
$ ./build/fuchsia/test/log_manager.py start --packages [TEST_BINARY] -C [OUTPUT_DIR]
```

Specifying the test binary and the output directory allows the logs to be
properly symbolized.

##### Run test package

```bash
$ ./build/fuchsia/test/run_executable_test.py --test-name [TEST_BINARY]
```
