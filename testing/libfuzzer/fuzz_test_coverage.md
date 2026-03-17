# Generating Local Code Coverage for Fuzz Tests

This document explains how to generate a local code coverage report for a
FuzzTest integrated into a gtest suite. A local coverage report helps you
visualize which code paths your fuzz test exercises so you can verify the logic
before you upload a CL.

To complete this process, use the
[coverage.py script](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/code_coverage.md#local-coverage-script)
script to automate building with coverage instrumentation, running the test, and
generating an HTML report.

## Update the toolchain

The coverage script and build system require the Clang compiler and specific
LLVM tools.

In your terminal, navigate to the src directory and update the required tools:

```shell
vpython3 tools/clang/scripts/update.py
```

## Configure the build

Create a dedicated build configuration for your coverage build.

In your terminal, from the src directory, generate the configuration:

```shell
# It's recommended to use a descriptive name like 'out/fuzz_coverage'
# to keep it separate from your regular builds.
gn gen out/fuzz_coverage --args='
use_clang_coverage=true
is_component_build=false
is_debug=false
dcheck_always_on=true
use_remoteexec=true'
```

### Argument breakdown:

*   `use_clang_coverage=true`: Instructs the compiler to add coverage
    instrumentation.
*   `is_component_build=false`: (Recommended) Code coverage instrumentation
    works with both component and non-component builds, but setting this to true
    causes the tests to run significantly slower.
*   `is_debug=false`: (Recommended) Optimizes the build for coverage.
*   `dcheck_always_on=true`: (Recommended) Catches issues during test runs.
*   `use_remoteexec=true`: (Recommended) Compiles Chromium fast.

## Build the test target

Compile the test suite that contains your FuzzTest. For example, if your fuzz
test `MyFuzzer.MyTest` is located in `//foo/my_fuzzer_unittest.cc`, and this
file is part of the `foo_unittests` target, you would build `foo_unittests`.

In your terminal, compile the test target:

```shell
# Replace foo_unittests with your actual test target
autoninja -C out/fuzz_coverage foo_unittests
```

## Run the coverage script

Run the `coverage.py` script. Use the `--gtest_filter` argument to run only your
specific fuzz test. Isolating the test produces a precise report on the coverage
contributed by that test alone.

In your terminal, run the script:

```shell
# Customize the arguments below for your specific test.
vpython3 tools/code_coverage/coverage.py \
foo_unittests \
-b out/fuzz_coverage \
-o out/my_fuzz_test_report \
-c 'out/fuzz_coverage/foo_unittests --gtest_filter=MyFuzzTestSuite.*' \
-f foo/ \
--no-component-view
```

### Command breakdown:

*   `foo_unittests`: Specifies the test target you are analyzing.
*   `-b out/fuzz_coverage`: Specifies the build directory from Step 2.
*   `-o out/my_fuzz_test_report`: Specifies the output directory for the final
    HTML report.
*   `-c '...'`: Defines the command to execute the test binary.
    *   `out/fuzz_coverage/foo_unittests`: Specifies the path to the test
        binary.
    *   `--gtest_filter=MyFuzzTestSuite.*`: Isolates your fuzz test. Replace
        MyFuzzTestSuite with the name of your test suite. Using .* runs all
        tests within that suite.
*   `-f foo/`: (Recommended) Filters the report to only show files in the
    specified directory.
*   `--no-component-view`: Prevents the script from fetching a
    directory-to-component mapping from the network, which avoids a 403
    Forbidden error.

## View the report

The script generates a set of HTML files in your specified output directory. To
organize the data, the script creates a sub-directory named after your target
operating system (for example, `linux`, `mac`, or `win`). The index.html file is
located inside this platform sub-directory.

### View on a local machine

If you build the test on your local machine, open the
`out/my_fuzz_test_report/<PLATFORM>/index.html` file in a browser. Replace
`<PLATFORM>` with your target operating system

### View from a remote machine

If you build the test on a remote machine or virtual machine, start an HTTP
server to access the report from your local browser.

In your remote terminal, navigate to the out directory:

```shell
cd out/my_fuzz_test_report
```

Start an HTTP server:

```shell
python3 -m http.server 8000
```

On your local machine, open a browser and navigate to
`<REMOTE_IP>:8000/<PLATFORM>/index.html`. Replace `<REMOTE_IP>` with your remote
machine's IP address or hostname, and replace `<PLATFORM>` with your target
operating system (for example, `linux`).


When you open the report, navigate the directory view to your changed files. The
report highlights lines covered by your fuzz test in green so you can verify the
test's impact.
