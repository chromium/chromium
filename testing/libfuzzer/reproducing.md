# Reproducing libFuzzer and AFL crashes

*** note
**Requirements:** For Windows, you must convert the forward slashes (/) to
backslashes (\\) in the commands below and use `set` command instead of `export`
to set the environment variable (step 4). Note that these commands are intended
to be used with cmd.exe, not PowerShell. Also, you may find [these tips] on how
to debug an ASAN instrumented binary helpful.
***

[TOC]

## Crashes reported as Reproducible

The majority of the bugs reported by ClusterFuzz have **Reproducible** label.
That means there is a testcase that can be used to reliably reproduce the crash.

1. Download the testcase from ClusterFuzz. If you are CCed on an issue filed by
   ClusterFuzz, a link to it is next to "Reproducer testcase" in the bug
   description.

   For the rest of this walkthrough, we call the path of this
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

5. Run the fuzz target:

```
out/fuzz/$FUZZER_NAME -runs=100 $TESTCASE_PATH
```

[File a bug] if you run into any issues.

## Symbolizing stack traces

Stack traces from ASAN builds are not symbolized by default. However, you
can symbolize them by piping the output into:

```
src/tools/valgrind/asan/asan_symbolize.py
```

## Crashes reported as Unreproducible

ClusterFuzz generally does not report issues that it cannot reliably reproduce,
unless the following condition is met. If a certain crash is occurring often
enough, such a crash might be reported with **Unreproducible** label and an
explicit clarification that there is no convenient way to reproduce it. There
are two ways to work with such crashes.

1. Try a speculative fix based on the stacktrace. Once the fix is landed, wait a
   couple days and then check Crash Statistics section on the ClusterFuzz
   testcase report page. If the fix works out, you will see that the crash is
   not happening anymore. If the crash does not occur again for a little while,
   ClusterFuzz will automatically close the issue as Verified.

2. (libFuzzer only) Try to reproduce the whole fuzzing session. This workflow is
   very similar to the one described above for the **Reproducible** crashes. The
   only differences are:

  * On step 1, instead of downloading a single testcase, you need to download
    corpus backup. This can be done using the following command:
```
gsutil cp gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/$FUZZER_NAME/latest.zip .
```

  * Alternatively, you can navigate to the following URL in your browser and
    download the `latest.zip` file:
```
https://pantheon.corp.google.com/storage/browser/clusterfuzz-libfuzzer-backup/corpus/libfuzzer/$FUZZER_NAME
```

  * Create an empty directory and unpack the corpus into it.
  * Follow steps 2-4 in the **Reproducible** section above.
  * On step 5, use the following command:

```
out/fuzz/$FUZZER_NAME -timeout=25 -rss_limit_mb=2048 -print_final_stats=1 $CORPUS_DIRECTORY_FROM_THE_PREVIOUS_STEP
```

  * Wait and hope that the fuzzer will crash.

Waiting for a crash to occur may take some time (up to 1hr), but if it happens,
you will be able to test the fix locally and/or somehow debug the issue.

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
