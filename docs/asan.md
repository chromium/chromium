# AddressSanitizer (ASan)

[AddressSanitizer](https://github.com/google/sanitizers) (ASan) is a fast memory
error detector based on compiler instrumentation (LLVM). It is fully usable for
Chrome on Android, Chrome OS, iOS simulator, Linux, Mac, and 64-bit Windows.
Additional info on the tool itself is available at
https://clang.llvm.org/docs/AddressSanitizer.html.

For the memory leak detector built into ASan, see
[LeakSanitizer](https://www.chromium.org/developers/testing/leaksanitizer).
If you want to debug memory leaks, please refer to the instructions on that page
instead.

## Buildbots and trybots

The [Chromium Memory
waterfall](https://ci.chromium.org/p/chromium/g/chromium.memory/console)
contains buildbots running Chromium tests under ASan on Linux (Linux ASan/LSan
bots for the regular Linux build, Linux Chromium OS ASan for the chromeos=1
build running on Linux), macOS, Chromium OS. Linux and Linux Chromium OS bots
run with --no-sandbox, but there's an extra Linux bot that enables the sandbox
(but disables LeakSanitizer).

The trybots running Chromium tests on Linux and macOS are:
- linux\_chromium\_asan\_rel\_ng
- mac\_chromium\_asan\_rel\_ng
- linux\_chromium\_chromeos\_asan\_rel\_ng (the chromeos=1 build running on a
Linux machine)

## Pre-built Chrome binaries

You can grab fresh Chrome binaries built with ASan
[here](https://commondatastorage.googleapis.com/chromium-browser-asan/index.html).
The lists of ASan binaries are _very_ long, but you can filter down to more
specific releases by specifying a prefix like
[linux-debug/asan-linux-debug-83](https://commondatastorage.googleapis.com/chromium-browser-asan/index.html?prefix=linux-debug/asan-linux-debug-83).
This is useful for finding a build for a specific revision, since filenames are of
the form `asan-<platform>-<buildtype>-<revision>` (but not every revision has an
archived ASan build). The
[get_asan_chrome](https://source.chromium.org/chromium/chromium/src/+/main:tools/get_asan_chrome/get_asan_chrome.py)
helper script is a handy way to download builds; its --help flag provides
usage instructions.

## Build tests with ASan

Building with ASan is easy. Start by compiling `base_unittests` to verify the
build is working for you (see below). Then, you can compile `chrome`,
`browser_tests`, etc.. Make sure to compile release builds.

### Configuring the build

Create an asan build directory by running:
```shell
gn args out/asan
```

Enter the following build variables in the editor that will pop up:
```python
is_asan = true
is_debug = false  # Release build.
```

Build with:
```shell
ninja -C out/asan base_unittests
```

### Reclient build

ASan builds should work seamlessly with Reclient; just add
`use_remoteexec=true` in your "gn args".

### Build options

If you want your stack traces to be precise, you will have to disable inlining
by setting the GN arg:
```shell
enable_full_stack_frames_for_profiling = true
```

Note that this incurs a significant performance hit. Please do not do this on
buildbots.

If you're working on reproducing ClusterFuzz reports, you might want to add:
```shell
v8_enable_verify_heap = true
```
in order to enable the `--verify-heap` command line flag for v8 in Release builds.

## Verify the ASan tool works

**ATTENTION (Linux only)**: These instructions are for running ASan in a way
that is compatible with the sandbox. However, this is not compatible with
LeakSanitizer. If you want to debug memory leaks, please use the instructions on
the
[LeakSanitizer](https://www.chromium.org/developers/testing/leaksanitizer)
page instead.

Now, check that the tool works. Run the following:
```shell
out/asan/base_unittests \
    --gtest_filter=ToolsSanityTest.DISABLED_AddressSanitizerLocalOOBCrashTest \
    --gtest_also_run_disabled_tests
```

The test will crash with the following error report:
```shell
==26552== ERROR: AddressSanitizer stack-buffer-overflow on address \
0x7fff338adb14 at pc 0xac20a7 bp 0x7fff338adad0 sp 0x7fff338adac8
WRITE of size 4 at 0x7fff338adb14 thread T0
    #0 0xac20a7 in base::ToolsSanityTest_DISABLED_AddressSanitizerLocalOOBCrashTest_Test::TestBody() ???:0
    #1 0xcddbd6 in testing::Test::Run() testing/gtest/src/gtest.cc:2161
    #2 0xcdf63b in testing::TestInfo::Run() testing/gtest/src/gtest.cc:2338
... lots more stuff
Address 0x7fff338adb14 is located at offset 52 in frame \
base::ToolsSanityTest_DISABLED_AddressSanitizerLocalOOBCrashTest_Test::TestBody()> of T0's stack:
  This frame has 2 object(s):
    [32, 52) 'array'
    [96, 104) 'access'
==26552== ABORTING
... lots more stuff
```

Congrats, you have a working ASan build! &#x1F64C;

## Run chrome under ASan

And finally, have fun with the `out/Release/chrome` binary. The filter script
`tools/valgrind/asan/asan_symbolize.py` can be used to symbolize the output,
although it shouldn't be necessary on Linux and Windows, where Chrome uses the
llvm-symbolizer in its source tree by default.

ASan should perfectly work with Chrome's sandbox. You should only need to run
with `--no-sandbox` on Linux if you're debugging ASan. To get reports on Windows
from sandboxed processes you will have to run with both `--enable-logging` and
`--log-file=d:\valid\path.log` then inspect the logfile.

You may need to run with `--disable-gpu` on Linux with NVIDIA driver older than
295.20.

You will likely need to define environment variable
[`G_SLICE=always-malloc`](https://developer.gnome.org/glib/unstable/glib-running.html)
to avoid crashes inside gtk.
`NSS_DISABLE_ARENA_FREE_LIST=1` and `NSS_DISABLE_UNLOAD=1` are required as well.

When filing a bug found by AddressSanitizer, please add a label
`Stability-Memory-AddressSanitizer`.

## ASan runtime options

ASan's behavior can be changed by exporting the `ASAN_OPTIONS` env var. Some of
the useful options are listed on this page, others can be obtained from running
an ASanified binary with `ASAN_OPTIONS=help=1`. Note that Chromium sets its own
defaults for some options, so the default behavior may be different from that
observed in other projects.
See `build/sanitizers/sanitizer_options.cc` for more details.

## NaCl support under ASan

On Linux (and soon on macOS) you can build and run Chromium with NaCl under ASan.
Untrusted code (nexe) itself is not instrumented with ASan in this mode, but
everything else is.

To do this, remove `enable_nacl=false` from your `args.gn`, and define
`NACL_DANGEROUS_SKIP_QUALIFICATION_TEST=1` in your environment at run time.

Pipe chromium output (stderr) through ``tools/valgrind/asan/asan_symbolize.py
`pwd`/`` to get function names and line numbers in ASan reports.
If you're seeing crashes within `nacl_helper_bootstrap`, try deleting
`out/Release/nacl_helper`.

## Building on iOS

It's possible to build and run Chrome tests for iOS simulator (which are x86
binaries essentially) under ASan. Note that you'll need a Chrome iOS checkout
for that. It isn't currently possible to build iOS binaries targeting ARM.

Configure your build with `is_asan = true` as described above. Replace your
build directory as needed:
```shell
ninja -C out/Release-iphonesimulator base_unittests
out/Release-iphonesimulator/iossim -d "iPhone" -s 7.0 \
    out/Release-iphonesimulator/base_unittests.app/ \
    --gtest_filter=ToolsSanityTest.DISABLED_AddressSanitizerLocalOOBCrashTest \
    --gtest_also_run_disabled_tests 2>&1 |
tools/valgrind/asan/asan_symbolize.py
```

You'll see the same report as shown above (see the "Verify the ASan tool works"
section), with a number of iOS-specific frames.

## Building on Android

Follow [AndroidBuildInstructions](android_build_instructions.md) with minor
changes:

```python
target_os="android"
is_asan=true
is_debug=false
```

Use `build/android/asan_symbolize.py` to symbolize stack from `adb logcat`. It
needs the `--output-directory` argument and takes care of translating the device
path to the unstripped binary in the output directory.

## Building with v8\_target\_arch="arm"

This is needed to detect addressability bugs in the ARM code emitted by V8 and
running on an instrumented ARM emulator in a 32-bit x86 Linux Chromium. **You
probably don't want this, and these instructions have bitrotted because they
still reference GYP. If you do this successfully, please update!** See
https://crbug.com/324207 for some context.

First, you need to install the 32-bit chroot environment using the
`build/install-chroot.sh` script (as described in
https://code.google.com/p/chromium/wiki/LinuxBuild32On64). Second, install the
build deps:
```shell
precise32 build/install-build-deps.sh  \
    # assuming your schroot wrapper is called 'precise32'
```

You'll need to make two symlinks to avoid linking errors:
```shell
sudo ln -s $CHROOT/usr/lib/i386-linux-gnu/libc_nonshared.a \
    /usr/lib/i386-linux-gnu/libc_nonshared.a
sudo ln -s $CHROOT/usr/lib/i386-linux-gnu/libpthread_nonshared.a \
    /usr/lib/i386-linux-gnu/libpthread_nonshared.a
```

Now configure and build your Chrome:
```shell
GYP_GENERATOR_FLAGS="output_dir=out_asan_chroot" GYP_DEFINES="asan=1 \
    disable_nacl=1 v8_target_arch=arm sysroot=/var/lib/chroot/precise32bit/ \
    chroot_cmd=precise32 host_arch=x86_64 target_arch=ia32" gclient runhooks
ninja -C out_asan_chroot/Release chrome
```

**Note**: `disable_nacl=1` is needed for now.

## Running on Chrome OS

For the linux-chromeos "emulator" build, run Asan following the instructions
above, just like you would for Linux.

For Chromebook hardware, add `is_asan = true` to your args.gn and build.
`deploy_chrome` with `--mount` and `--nostrip`. ASan logs can be found in
`/var/log/asan/`.

To catch crashes in gdb:

-   Edit `/etc/chrome_dev.conf` and add `ASAN_OPTIONS=abort_on_error=1`
-   `restart ui`
-   gdb -p 12345  # Find the pid from /var/log/chrome/chrome

When you trigger the crash, you'll get a SIGABRT in gdb. `bt` will show the
stack.

See
[Chrome OS stack traces](https://chromium.googlesource.com/chromiumos/docs/+/main/stack_traces.md)
for more details.

## AsanCoverage

AsanCoverage is a minimalistic code coverage implementation built into ASan. For
general information see
[https://code.google.com/p/address-sanitizer/wiki/AsanCoverage](https://github.com/google/sanitizers)
To use AsanCoverage in Chromium, add `use_sanitizer_coverage = true` to your GN
args. See also the `sanitizer_coverage_flags` variable for configuring it.

Chrome must be terminated gracefully in order for coverage to work. Either close
the browser, or SIGTERM the browser process. Do not do `killall chrome` or send
SIGKILL.
```shell
kill <browser_process_pid>
ls
...
chrome.22575.sancov
gpu.6916123572022919124.sancov.packed
zygote.13651804083035800069.sancov.packed
...
```

The `gpu.*.sancov.packed` file contains coverage data for the GPU process,
whereas the `zygote.*.sancov.packed` file contains coverage data for the
renderers (but not the zygote process). Unpack them to regular `.sancov` files
like so:
```shell
$ $LLVM/projects/compiler-rt/lib/sanitizer_common/scripts/sancov.py unpack \
    *.sancov.packed
sancov.py: unpacking gpu.6916123572022919124.sancov.packed
sancov.py: extracting chrome.22610.sancov
sancov.py: unpacking zygote.13651804083035800069.sancov.packed
sancov.py: extracting libpdf.so.12.sancov
sancov.py: extracting chrome.12.sancov
sancov.py: extracting libpdf.so.10.sancov
sancov.py: extracting chrome.10.sancov
```

Now, e.g., to list the offsets of covered functions in the libpdf.so binary in
renderer with pid 10:
```shell
$ $LLVM/projects/compiler-rt/lib/sanitizer_common/scripts/sancov.py print \
    libpdf.so.10.sancov
```
