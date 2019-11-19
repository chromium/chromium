# Debugging Memory Issues

This page is designed to help Chromium developers debug memory issues.

When in doubt, reach out to memory-dev@chromium.org.

[TOC]

## Investigating Reproducible Memory Issues

Let's say that there's a CL or feature that reproducibly increases memory usage
when it's landed/enabled, given a particular set of repro steps.

* Take a look at [the documentation](/docs/memory/README.md) for both
  taking and navigating memory-infra traces.
* Take two memory-infra traces. One with the reproducible memory regression, and
  one without.
* Load the memory-infra traces into two tabs.
* Compare the memory dump providers and look for the one that shows the
  regression. Follow the relevant link.
    * [The regression is in the Malloc MemoryDumpProvider.](#Investigating-Reproducible-Memory-Issues)
    * [The regression is in a non-Malloc
      MemoryDumpProvider.](#Regression-in-Non-Malloc-MemoryDumpProvider)
    * [The regression is only observed in **private
      footprint**.](#Regression-only-in-Private-Footprint)
    * [No regression is observed.](#No-observed-regression)

### Regression in Malloc MemoryDumpProvider

Repeat the above steps, but this time also [take a heap
dump](#Taking-a-Heap-Dump). Confirm that the regression is also visible in the
heap dump, and then compare the two heap dumps to find the difference. You can
also use
[diff_heap_profiler.py](https://cs.chromium.org/chromium/src/third_party/catapult/experimental/tracing/bin/diff_heap_profiler.py)
to perform the diff.

### Regression in Non-Malloc MemoryDumpProvider

Hopefully the MemoryDumpProvider has sufficient information to help diagnose the
leak. Depending on the whether the leaked object is allocated via malloc or new
- it usually should be, you can also use the steps for debugging a Malloc
MemoryDumpProvider regression.

### Regression only in Private Footprint

* Repeat the repro steps, but instead of taking a memory-infra trace, use
  the following tools to map the process's virtual space:
    * On macOS, use vmmap
    * On Windows, use SysInternal VMMap
    * On other OSes, use /proc/<pid\>/smaps.
* The results should help diagnose what's happening. Contact the
  memory-dev@chromium.org mailing list for more help.

### No observed regression

* If there isn't a regression in PrivateMemoryFootprint, then this might become
  a question of semantics for what constitutes a memory regression. Common
  problems include:
    * Shared Memory, which is hard to attribute, but is mostly accounted for in
      the memory-infra trace.
    * Binary size, which is currently not accounted for anywhere.

## Investigating Heap Dumps From the Wild

For a small set of Chrome users in the wild, Chrome will record and upload
anonymized heap dumps. This has the benefit of wider coverage for real code
paths, at the expense of reproducibility.

These heap dumps can take some time to grok, but frequently yield valuable
insight. At the time of this writing, heap dumps from the wild have resulted in
real, high impact bugs being found in Chrome code ~90% of the time.

For an example investigation of a real heap dump, see [this
link](/docs/memory/investigating_heap_dump_example.md).

* Raw heap dumps can be viewed in the trace viewer. [See detailed
  instructions.](/docs/memory-infra/heap_profiler.md#how-to-manually-browse-a-heap-dump).
  This interface surfaces all available information, but can be overwhelming and
  is usually unnecessary for investigating heap dumps.
    * Important note: Heap profiling in the field uses
      [poison process sampling](https://bugs.chromium.org/p/chromium/issues/detail?id=810748)
      with a rate parameter of 10000. This means that for large/frequent allocations
      [e.g. >100 MB], the noise will be quite small [much less than 1%]. But
      there is noise so counts will not be exact.
* The heap dump summary typically contains all information necessary to diagnose
  a memory issue.
  * The stack trace of the potential memory leak is almost always sufficient to
    tell the type of object being leaked, since most functions in Chrome
    have a limited number of calls to new and malloc.
* The next thing to do is to determine whether the memory usage is intentional.
  Very rarely, components in Chrome legitimately need to use many 100s of MBs of
  memory. In this case, it's important to create a
  [MemoryDumpProvider](https://cs.chromium.org/chromium/src/base/trace_event/memory_dump_provider.h)
  to report this memory usage, so that we have a better understanding of which
  components are using a lot of memory. For an example, see
  [Issue 813046](https://bugs.chromium.org/p/chromium/issues/detail?id=813046).
* Assuming the memory usage is not intentional, the next thing to do is to
  figure out what is causing the memory leak.
    * The most common cause is adding elements to a container with no limit.
      Usually the code makes assumptions about how frequently it will be called
      in the wild, and something breaks those assumptions. Or sometimes the code
      to clear the container is not called as frequently as expected [or at
      all]. [Example
      1](https://bugs.chromium.org/p/chromium/issues/detail?id=798012). [Example
      2](https://bugs.chromium.org/p/chromium/issues/detail?id=804440).
    * Retain cycles for ref-counted objects.
      [Example](https://bugs.chromium.org/p/chromium/issues/detail?id=814334#c23)
    * Straight up leaks resulting from incorrect use of APIs. [Example
      1](https://bugs.chromium.org/p/chromium/issues/detail?id=801702#c31).
      [Example
      2](https://bugs.chromium.org/p/chromium/issues/detail?id=814444#c17).

## Taking a Heap Dump

Navigate to chrome://flags and search for **memlog**. There are several options
that can be used to configure heap dumps. All of these options are also
available as command line flags, for automated test runs [e.g. telemetry].

* `#memlog` controls which processes are profiled. It's also possible to
  manually specify the process via the interface at `chrome://memory-internals`.
* `#memlog-in-process` makes the profiling service to be run within the
  Chrome browser process. Defaults to run the service as a separate dedicated
  process.
* `#memlog-sampling-rate` specifies the sampling interval in bytes. The lower
  the interval, the more precise is the profile. However it comes at the cost of
  performance. Default value is 100KB, that is enough to observe allocation
  sites that make allocations >500KB total, where total equals to a single
  allocation size times the number of such allocations at the same call site.
* `#memlog-stack-mode` describes the type of metadata recorded for each
  allocation. `native` stacks provide the most utility. The only time the other
  options should be considered is for Android official builds, most of which do
  not support `native` stacks.

Once the flags have been set appropriately, restart Chrome and take a
memory-infra trace. The results will have a heap dump.

