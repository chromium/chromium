# Orderfile

An orderfile is a list of symbols that defines an ordering of functions. One can
make a static linker, such as LLD, respect this ordering when generating a
binary.

Reordering code this way can improve startup performance by fetching machine
code to memory more efficiently, since it requires fetching fewer pages from
disk, and a big part of the I/O work is done sequentially by the readahead.

Code reordering can also improve memory usage by keeping the used code in a
smaller number of memory pages. It can also reduce TLB and L1i cache misses by
placing functions commonly called together closely in memory.

## Generating Orderfiles Manually

To generate an orderfile you can run the `orderfile_generator_backend.py`
script. You will need an Android device connected with
[adb](https://developer.android.com/tools/adb) to generate the orderfile as the
generation pipeline will need to run benchmarks on a device.

Example:
```
tools/cygprofile/orderfile_generator_backend.py --target-arch=arm64 --use-remoteexec
```

You can specify the architecture (arm or arm64) with `--target-arch`. For quick
local testing you can use `--streamline-for-debugging`. To build using Reclient,
use `--use-remoteexec` (Googlers only). There are several other options you can
use to configure/debug the orderfile generation. Use the `-h` option to view the
various options.

NB: If your checkout is non-internal you must use the `--public` option.

To build Chrome with a locally generated orderfile, use the
`chrome_orderfile_path=<path_to_orderfile>` GN arg.

## Orderfile Performance Testing

Orderfiles can be tested using
[Pinpoint](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/perf_trybots.md).
To do this, please create and upload a Gerrit change overriding the value of
[`chrome_orderfile_path`](https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/BUILD.gn;l=217-223;drc=3a829695d83990141babd25dee7f2f94c005cae4)
to, for instance, `//path/to/my_orderfile` (relative to `src`), where
`my_orderfile` is the orderfile that needs to be evaluated. The orderfile should
be added to the local branch and uploaded to Gerrit along with
`build/config/compiler/BUILD.gn`. This Gerrit change can then be used as an
"experiment patch" for a Pinpoint try job.

## Triaging Performance Regressions

Occasionally, an orderfile roll will cause performance problems on perfbots.
This typically triggers an alert in the form of a bug report, which contains a
group of related regressions like the one shown
[here](https://crbug.com/344654892).

In such cases it is important to keep in mind that effectiveness of the
orderfile is coupled with using a recent PGO profile when building the native
code. As a result some orderfile improvements (or effective no-ops) register as
regressions on perfbots using non-PGO builds, which is the most common perfbot
configuration.

If a new regression does not include alerts from the
[android-pixel6-perf-pgo](https://ci.chromium.org/ui/p/chrome/builders/luci.chrome.ci/android-pixel6-perf-pgo)
(the only Android PGO perfbot as of 2024-06) then the first thing to check is to
query the same benchmark+metric combinations for the PGO bot. If the graphs
demonstrate no regression, feel free to close the issue as WontFix(Intended
Behavior). However, not all benchmarks are exercised on the PGO bot
continuously. If there is no PGO coverage for a particular benchmark+metric
combination, this combination can be checked on Pinpoint with the right perfbot
choice ([example](https://crbug.com/344665295)).

Finally, the PGO+orderfile coupling exists only on arm64. Most speed
optimization efforts on Android are focused on this configuration. On arm32 the
most important orderfile optimization is for reducing memory used by machine
code. Only one benchmark measures it: `system_health.memory_mobile`.

## Orderfile Pipeline

The `orderfile_generator_backend.py` script runs several key steps:

1. **Build and install Chrome with orderfile instrumentation.** This uses the
[`-finstrument-function-entry-bare`](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-finstrument-function-entry-bare)
Clang command line option to insert instrumentation for function entry. The
build will be generated in `out/arm_instrumented_out/` or
`out/arm64_instrumented_out`, depending on the CPU architecture (instruction
set).

2. **Run the benchmarks and collect profiles.** These benchmarks can be found
in [orderfile.py](../tools/perf/contrib/orderfile/orderfile.py). These profiles
are a list of function offsets into the binary that were called during execution
of the benchmarks.

3. **Cluster the symbols from the profiles to generate the orderfile.**
The offsets are processed and merged using a
[clustering](../tools/cygprofile/cluster.py) algorithm to produce an orderfile.

4. **Run benchmarks on the final orderfile.** We run some benchmarks to compare
the performance with/without the orderfile. You can supply the `--no-benchmark`
flag to skip this step.
