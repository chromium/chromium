# Reproducing Crashes from ClusterFuzz

## Introduction

This guide provides step-by-step instructions for reproducing crashes found by
ClusterFuzz. Reproducing a crash locally is the first step toward debugging and
fixing it.

*** note
**Requirements:** For Windows, you must convert the forward slashes (/) to
backslashes (\\) in the commands below and use `set` command instead of `export`
to set the environment variable (step 4). Note that these commands are intended
to be used with cmd.exe, not PowerShell. Also, you may find [these tips] on how
to debug an ASAN instrumented binary helpful.
***

[TOC]

## Which Workflow Should I Use?

The first step is to identify how the crash was found. Look at the ClusterFuzz
bug report:

*   If the report specifies a "Fuzz Target" and mentions a fuzzing engine
    (like libFuzzer, AFL, Centipede, etc.), you should follow the steps under
    [Reproducing a Crash from a Fuzzing Engine](#reproducing-a-crash-from-a-fuzzing-engine).

*   Otherwise, the crash was likely caused by a blackbox fuzzer that produced
    a file to crash a larger application (like `content_shell`). You should
    follow the steps under
    [Reproducing a Crash from a Blackbox Fuzzer](#reproducing-a-crash-from-a-blackbox-fuzzer).

## Reproducing a Crash from a Fuzzing Engine
The majority of the bugs reported by ClusterFuzz have the **Reproducible**
label. This workflow applies to those cases.

1. Download the testcase from ClusterFuzz. If you are CCed on an issue filed by
   ClusterFuzz, a link to it is next to "Reproducer testcase" in the bug
   description.

   For the rest of this section, we call the path of this
   file: `$TESTCASE_PATH` and the fuzz target you want to reproduce a
   crash on: `$FUZZER_NAME` (provided as "Fuzz Target" in the bug
   description).

2. Generate gn build configuration:

   ```
   gn args out/fuzz
   ```

   This will open up an editor. Copy the gn configuration parameters from the
   values provided in `GN Config` section in the ClusterFuzz testcase report.


3. Build the fuzzer:

   ```
   autoninja -C out/fuzz $FUZZER_NAME
   ```

4. Set the `*SAN_OPTIONS` environment variable as provided in the
   `Crash Stacktrace` section in the testcase report.
   Here is an example value of `ASAN_OPTIONS` that is similar to its value on
   ClusterFuzz:

   ```
   export ASAN_OPTIONS=redzone=256:print_summary=1:handle_sigill=1:allocator_release_to_os_interval_ms=500:print_suppressions=0:strict_memcmp=1:allow_user_segv_handler=0:use_sigaltstack=1:handle_sigfpe=1:handle_sigbus=1:detect_stack_use_after_return=0:alloc_dealloc_mismatch=0:detect_leaks=0:print_scariness=1:allocator_may_return_null=1:handle_abort=1:check_malloc_usable_size=0:detect_container_overflow=0:quarantine_size_mb=256:detect_odr_violation=0:symbolize=1:handle_segv=1:fast_unwind_on_fatal=0
   ```

   Note: The sanitizer options may reference suppressions files. You can find those
      at go/crfuzz-clusterfuzz-suppressions (google-internal).

5. Run the fuzz target with the downloaded testcase. The `-runs=100` flag helps
ensure reproduction even if the crash has minor flakiness.

   ```
   out/fuzz/$FUZZER_NAME -runs=100 $TESTCASE_PATH
   ```

[File a bug] if you run into any issues.

## Reproducing a Crash from a Blackbox Fuzzer

1. Get info from the report.
    * Download the `Reproducer testcase`. We'll call its path `$TESTCASE_PATH`.
    * Identify the target application from the `Job Type` (e.g.,
    `linux_asan_content_shell_drt` means the target is `content_shell`).

2. Configure and build the application:

   Generate the build configuration using the arguments from the “GN Config”
   section.
   ```
   gn args out/fuzz
   ```

   Build the target application binary (e.g., content_shell).
   ```
   autoninja -C out/fuzz content_shell
   ```

3. Set environment variables:

   Export all variables listed under `[Environment]` in the stack trace of the
   report.
   ```
   export ASAN_OPTIONS=...
   # Also export any other variables listed.
   ```

4. Construct and run the command:

   In the stack trace, find the line beginning with `Command line:`.
   Construct your local command by replacing the executable path (e.g.,
   `/proc/self/exe` or `/mnt/.../content_shell`) with the path to your built
   binary (e.g., `out/fuzz/content_shell`), copying all the flags, and appending
   the `$TESTCASE_PATH` at the end.
   ```
   # Example command based on a content_shell crash
   out/fuzz/content_shell \
   --run-web-tests \
   --disable-in-process-stack-traces \
   --autoplay-policy=no-user-gesture-required \
   --lang=en-US \
   $TESTCASE_PATH
   ```

## Crashes reported as Unreproducible

ClusterFuzz generally does not report issues that it cannot reliably reproduce,
unless the following condition is met. If a certain crash is occurring often
enough, such a crash might be reported with **Unreproducible** label and an
explicit clarification that there is no convenient way to reproduce it. There
are two ways to work with such crashes.

### Option 1: Attempt a Speculative Fix
Try a speculative fix based on the stacktrace. Once the fix is landed, wait a
couple days and then check Crash Statistics section on the ClusterFuzz
testcase report page. If the fix works out, you will see that the crash is
not happening anymore. If the crash does not occur again for a little while,
ClusterFuzz will automatically close the issue as Verified.

### Option 2: Replay the Fuzzing Session (libFuzzer Only)

Try to reproduce the whole fuzzing session. This workflow is very similar to the
one described above for reproducing a crash from a fuzzing engine. The only
differences are:

* Instead of downloading a single testcase, you need to download corpus backup.
This can be done using the following command:
   ```
   gsutil cp gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/$FUZZER_NAME/latest.zip .
   ```

* Alternatively, you can navigate to the following URL in your browser and
download the `latest.zip` file:
   ```
   https://pantheon.corp.google.com/storage/browser/clusterfuzz-libfuzzer-backup/corpus/libfuzzer/$FUZZER_NAME
   ```
* Create an empty directory and unpack the corpus into it.
* Follow steps 2-4 in the reproducing a crash from a fuzzing engine section
above.
* On step 5, use the following command:

   ```
   out/fuzz/$FUZZER_NAME -timeout=25 -rss_limit_mb=2048 -print_final_stats=1 $CORPUS_DIRECTORY_FROM_THE_PREVIOUS_STEP
   ```

* Wait and hope that the fuzzer will crash.

Waiting for a crash to occur may take some time (up to 1hr), but if it happens,
you will be able to test the fix locally and/or somehow debug the issue.

## Symbolizing stack traces

Stack traces from ASAN builds are not symbolized by default. However, you
can symbolize them by piping the output into:

```
src/tools/valgrind/asan/asan_symbolize.py
```

## Minimizing a crash input (optional)

ClusterFuzz does crash input minimization automatically, and a typical crash
report has two testcases available for downloading:

* An original testcase that has triggered the crash;
* A minimized testcase that is smaller than the original but triggers the same
  crash.

If you would like to further minimize a testcase, run the fuzz target with the
two additional arguments:

* `-minimize_crash=1`
* `-exact_artifact_path=<output_filename_for_minimized_testcase>`

The full command would be:

```
out/fuzz/$FUZZER_NAME -minimize_crash=1 -exact_artifact_path=<minimized_testcase_path> $TESTCASE_PATH
```

This might be useful for large testcases that make it hard to identify a root
cause of a crash. You can leave the minimization running locally for a while
(e.g. overnight) for better results.


[File a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Tools%3EStability%3ElibFuzzer&comment=What%20problem%20are%20you%20seeing
[these tips]: https://github.com/google/sanitizers/wiki/AddressSanitizerWindowsPort#debugging
