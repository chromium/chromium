# Orderfile

An orderfile is a list of symbols that defines an ordering of functions which is
respected by the linker when generating a binary. It can improve performance by
reducing the number of random reads and improving the usefulness of readahead.

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

## Orderfile Pipeline

The `orderfile_generator_backend.py` script runs several key steps:

1. **Build and install Chrome with orderfile instrumentation.** This uses the
[`-finstrument-function-entry-bare`](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#index-finstrument-functions)
gcc flag to insert instrumentation for function entry. The build will be
generated in `out/arm_instrumented_out/` or `out/arm64_instrumented_out`,
depending on the architecture.


2. **Run the benchmarks and collect profiles.** These benchmarks can be found
in [orderfile.py](../tools/perf/contrib/orderfile/orderfile.py). These profiles
are a list of function offsets into the binary that were called during execution
of the benchmarks.

3. **Cluster the symbols from the profiles to generate the unpatched orderfile.**
The offsets are processed and merged using a
[clustering](../tools/cygprofile/cluster.py) algorithm to produce an orderfile.

4. **Build an uninstrumented Chrome and patch the orderfile with it.** The
orderfile based on an instrumented build cannot be applied directly to an
uninstrumented build. The orderfile needs to be
[patched](../tools/cygprofile/patch_orderfile.py) with an uninstrumented build
because the instrumentation has a non-trivial impact on inlining decisions and
has identical code folding disabled. The patching step produces the final
orderfile which will be in `clank/orderfiles/` for internal builds, or in
`orderfiles/` if running the generator script with `--public`. The
uninstrumented build will be in `out/orderfile_arm64_uninstrumented_out`.

5. **Run benchmarks on the final orderfile.** We run some benchmarks to compare
the performance with/without the orderfile. You can supply the `--no-benchmark`
flag to skip this step.