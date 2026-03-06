# Generating Local Code Coverage for Fuzz Tests

This guide explains how to generate a local code coverage report for a FuzzTest
integrated into a gtest suite. This is useful for visualizing which code paths
your new fuzz test exercises, ensuring it covers the intended logic before you
upload a CL.

We will use the central
[coverage.py script](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/code_coverage.md#local-coverage-script),
which automates the process of building with coverage instrumentation, running
the test, and generating an HTML report.

## Step 1: Ensure Your Toolchain is Up to Date

The coverage script and build system depend on the Clang compiler and specific
LLVM tools. Before starting, ensure these are downloaded and current. From the
src directory, run:

```
# This downloads the main Clang compiler needed for the build.
vpython3 tools/clang/scripts/update.py
```

## Step 2: Configure the Build with GN

From the src directory, run:

```
# It's recommended to use a descriptive name like 'out/fuzz_coverage'
# to keep it separate from your regular builds.
gn gen out/fuzz_coverage --args='
use_clang_coverage=true
is_component_build=false
is_debug=false
dcheck_always_on=true
use_remoteexec=true
'
```

### Argument Breakdown:

*   `use_clang_coverage=true`: The essential flag that instructs the compiler to
    add coverage instrumentation.
*   `is_component_build=false`: Required for coverage builds to work correctly.
*   `is_debug=false`: Coverage works best with optimized builds.
*   `dcheck_always_on=true`: Recommended for catching issues during test runs.
*   `use_remoteexec=true`: Recommended for compiling Chromium fast.

## Step 3: Build the Instrumented Test Target

Compile the test suite that contains your FuzzTest. For example, if your fuzz
test `MyFuzzer.MyTest` is located in `//foo/my_fuzzer_unittest.cc`, and this
file is part of the `foo_unittests` target, you would build `foo_unittests`.

```
# Replace foo_unittests with your actual test target
autoninja -C out/fuzz_coverage foo_unittests
```

## Step 4: Run the Coverage Script

Now, execute the `coverage.py` script. The key is to use the `--gtest_filter`
argument to run only your specific fuzz test. This gives you a precise report on
the coverage contributed by that test alone.

```
# Customize the arguments below for your specific test.
vpython3 tools/code_coverage/coverage.py \
    foo_unittests \
    -b out/fuzz_coverage \
    -o out/my_fuzz_test_report \
    -c 'out/fuzz_coverage/foo_unittests --gtest_filter=MyFuzzTestSuite.*' \
    -f foo/ \
    --no-component-view
```

#### Command Breakdown:

*   `foo_unittests`: The name of the test target you are analyzing.
*   `-b out/fuzz_coverage`: Specifies the build directory from Step 2.
*   `-o out/my_fuzz_test_report`: A new directory where the final HTML report
    will be saved.
*   `-c '...'`: The exact command to execute.
    *   `out/fuzz_coverage/foo_unittests`: The path to the instrumented test
        binary.
    *   `--gtest_filter=MyFuzzTestSuite.*`: (Crucial) Isolates your fuzz test.
        Replace MyFuzzTestSuite with the name of your test suite. Using .* runs
        all tests within that suite.
*   `-f foo/`: (Optional, but highly recommended) Filters the report to only
    show files in the specified directory, making it easier to analyze your
    changes.
*   `--no-component-view`: Prevents the script from fetching a
    directory-to-component mapping from the network, which can avoid potential
    403 Forbidden errors in some network environments.

## Step 5: View the Report

The script will generate a set of HTML files in the output directory you
specified (-o). Open the main index.html file in a browser to view the report.

Navigate through the directory view to your changed files. Lines covered by your
fuzz test will be highlighted in green, giving you a clear visual confirmation
of its impact.
