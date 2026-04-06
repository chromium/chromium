# Running net_perftests

## Summary

`net_perftests` is a set of microbenchmarks that have been written for the net
stack. They measure the performance of things that are or were considered
important for performance.

## Tips

*   Build with `is_official_build` set to true, `dcheck_always_on` set to false,
    and `is_component_build` set to false. These three items have a huge impact
    on performance and your results will be meaningless if you set them any
    other way. `is_official_build` will make building take twice as long, but
    for perftests it is usually not too bad. Recommended gn settings:

    ```gn
    is_debug = false
    is_official_build = true
    is_component_build = false
    is_chrome_branded = false
    use_remoteexec = true
    dcheck_always_on = false
    chrome_pgo_phase = 0
    ```

*   Use the instructions at
    [reducing_variance.md](https://github.com/google/benchmark/blob/main/docs/reducing_variance.md)
    to disable processor features which make benchmarks noisy. To summarize (on
    Linux):

    ```bash
    sudo cpupower frequency-set --governor performance
    echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost
    taskset -c 0 ./net_perftests
    ```

*   There are two kinds of tests in `net_perftests`:

    1.  **Older tests** use the
        [GoogleTest](https://github.com/google/googletest) framework together
        with `perf_test::TestResultReporter`. The advantage of this format is
        that our testing infrastructure knows how to parse it. But you may need
        to manually edit the `_perftest.cc` files to increase the number of
        iterations to get a meaningful result. The default number of iterations
        is normally set to get an answer quickly, but you should aim to run each
        test for a minute or more for accuracy. Use the `--gtest_filter` options
        to select the test(s) to run. It takes a wildcard argument.
    2.  **Newer tests** use the
        [Google Benchmark](https://github.com/google/benchmark) framework. Use
        the `--benchmark_filter` option to select the test(s) to run. It takes a
        regular expression argument. By adding a `--benchmark_repetitions` flag
        you can cause each benchmark to be run multiple times and statistics to
        be generated.

*   By default, all available tests run, but this is probably not what you want.

    **Running a set of old-style tests:**

    ```bash
    taskset -c 0 ./net_perftests --gtest_filter=CookieMonsterTest.* --benchmark_filter=nomatch
    ```

    Because `--benchmark_filter=` takes a regular expression, passing it nothing
    results in all tests running, which is probably not what you want.

    **Running a set of new-style tests:**

    ```bash
    taskset -c 0 ./net_perftests --gtest_filter= --benchmark_filter=SpdyHeadersToHttpResponseHeaders
    ```

*   The Google Benchmark command-line options are not well-documented. At time
    of writing, the output from running `--help` was:

    ```text
    benchmark [--benchmark_list_tests={true|false}]
              [--benchmark_filter=<regex>]
              [--benchmark_min_time=`<integer>x` OR `<float>s` ]
              [--benchmark_min_warmup_time=<min_warmup_time>]
              [--benchmark_repetitions=<num_repetitions>]
              [--benchmark_enable_random_interleaving={true|false}]
              [--benchmark_report_aggregates_only={true|false}]
              [--benchmark_display_aggregates_only={true|false}]
              [--benchmark_format=<console|json|csv>]
              [--benchmark_out=<filename>]
              [--benchmark_out_format=<json|console|csv>]
              [--benchmark_color={auto|true|false}]
              [--benchmark_counters_tabular={true|false}]
              [--benchmark_context=<key>=<value>,...]
              [--benchmark_time_unit={ns|us|ms|s}]
              [--v=<verbosity>]
    ```

*   Double-check that you're testing the version of the code you think you are
    testing. This will save you time in the long run.

*   Note down your test results together with what version they apply to.

*   Run with `--benchmark_repetitions --benchmark_display_aggregates_only=true`
    to get a statistical summary of results.

## Tips for writing new tests

*   Use the new Google Benchmark framework.
*   If the compiler can determine some code has no side effects, it will remove
    it. Capture the output of whatever function you are testing with
    `benchmark::DoNotOptimize`. If the function writes something to the heap,
    use `benchmark::ClobberMemory()` after `DoNotOptimize` to ensure those
    writes are not optimized out.

## Running on Android

*   Benchmark on a real device. Don't benchmark an emulator. There's no point,
    the results will be meaningless.
*   (Googlers only) See [go/acid](http://goto.google.com/acid) for how to borrow
    a real Android device remotely in a lab somewhere.
*   See build instructions at
    [android_build_instructions.md](https://chromium.googlesource.com/chromium/src/+/main/docs/android_build_instructions.md#converting-an-existing-linux-checkout).
*   To see the worst-case performance, build with `target_cpu = "arm"` and run
    on a cheap 32-bit device. `arm64` devices will give performance more similar
    to a desktop CPU and so are less interesting.
*   Use the `adb devices` command to verify that the system can see exactly one
    device. If it can't see any the test won't work, and if it can see more than
    one it will pick the wrong one.
*   Run as:

    ```bash
    bin/run_net_perftests --verbose --verbose --gtest_filter=<test> --benchmark_filter=<test>
    ```

    This will take longer than you'd like. The `--fast-local-dev` option may
    help, or may not work. The doubled `--verbose` flag is needed to see the
    results of the benchmark. Setting `--gtest_filter` to a non-matching value
    crashes the test runner, so it is best to set it to a quick old-style
    benchmark like `CookieMonsterTest.TestGetKey` if you are testing a new-style
    benchmark.
