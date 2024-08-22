# Debugging Memory Issues

This page is designed to help Chromium developers debug memory issues.

When in doubt, reach out to memory-dev@chromium.org.

[TOC]

## Investigating Reproducible Memory Regression

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
      [Poisson process sampling](https://bugs.chromium.org/p/chromium/issues/detail?id=810748)
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

## Investigating Memory Corruption

In case you can reproduce the corruption locally,
you are advised to run sanitizers (e.g.
[ASan](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/asan.md))
to locate and fix UB.

Otherwise, you can look into
[minidump](https://sites.google.com/a/google.com/crash/users/how-to/manually-debug-a-minidump)
(link Googlers-only) if available.

### Known Memory Poisoning Patterns

Memory allocation goes through multiple states,
and its payload sometimes has a distinctive pattern.
You may also see some variance on lower bits, introduced by
e.g. an offset within `struct`.

#### Memory held by the OS

* All memory comes from the OS and returns back to the OS at some point.
* Access to memory that is already returned to the OS is likely a crash.
* Large allocations (>= ~1 MiB) tend to go back to the OS quickly when
  freed, while smaller allocations are mostly reused.

#### Memory held by the allocator

* The allocator holds the memory region borrowed from the OS in a free-list.
* Payload and behavior are implementation-specific.
* In Chrome, we use
  [PartitionAlloc](/base/allocator/partition_allocator/PartitionAlloc.md) as the
  main allocator.
  * We embed some data on payload and the original payload before `free()` may
    or may not be overwritten.
  * Writes to `free()`d memory may be caught as "free-list corruption".
* Following patterns can be written at this stage:
  * `0xCDCDCDCDCDCDCDCD`: when allocation gets returned to PartitionAlloc.
    * Shows up only in `PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)` builds.

#### Quarantined Memory

* Optionally, the allocator may keep `free()`d memory in quarantine
  for a while before returning it into a free-list to detect and mitigate
  UaF bugs.
* Following patterns can be written at this stage:
  * `0xCDCDCDCDCDCDCDCD`: PartitionAlloc's `FreeFlags::kZap`.
    * As of Aug. 2024 this is used by only [AMSC](https://docs.google.com/document/d/12OM0CSKgKv6NhM9YylSqAAXiV_f4uMgYgaH8KABUe-o/edit?usp=sharing).
  * `0xEFEFEFEFEFEFEFEF`: In [BRP](https://chromium.googlesource.com/chromium/src/+/HEAD/base/memory/raw_ptr.md) quarantine.
    * You are using a dangling pointer to access invalidated memory region.
  * `0xEFED????????8000`: In [LUD](https://docs.google.com/document/d/1xfGa_IMtFZiQ3beOmkncEafODwn4U90ZyL4NfPaAtDY/edit?usp=sharing&resourcekey=0-89BZl1SVILB6ylOHula0IA) quarantine.
    * (Googlers-only) You may have an access to `free()` stack trace on crashpad.
  * `0xECEC????????8000`: In [E-LUD](https://docs.google.com/document/d/1_9TSOtQuPR3NjorLDjAkuloi8lYqblb6Ykt5nbVnh9I/edit?usp=sharing) quarantine.


#### Memory allocation you officially own

In principle, once initialized you should only see values written
by your code while your allocation is alive.
However, in rare case, you may see values from Write-after-Free.

```txt
void YourFunc() {              | void TheirFunc() {
                               |   int* p1 = new int;
                               |   delete p1;
  // The allocator may         |
  // redistribute `p1` to `p2` |
  int* p2 = new int;           |
  *p2 = 123;                   |
                               |   // Write-after-Free
                               |   *p1 = 456;
  // 456 may show up           |
  printf("%d\n", *p2);         |
}                              | }
```

...or values from Double-Free.

```
void YourFunc() {              | void TheirFunc() {
                               |   int* p1 = new int;
                               |   delete p1;
  // The allocator may         |
  // redistribute `p1` to `p2` |
  int* p2 = new int;           |
  *p2 = 123;                   |
                               |   // Double-Free
                               |   delete p1;
                               |
                               |   // The allocator may
                               |   // redistribute `p2` to `p3`
                               |   int* p3 = new int;
                               |   *p3 = 456;
  // 456 may show up           |
  printf("%d\n", *p2);         |
}                              | }
```

* Following patterns can be written at this stage:
  * `0x0000000000000000`: [zero initialization](https://en.cppreference.com/w/cpp/language/zero_initialization).
  * `0x0000000000000000`: PartitionAlloc's `AllocFlags::kZeroFill`.
    * This payload is written as a part of memory allocation but requires
      explicit opt-in e.g. `calloc()`.
  - `0xABABABABABABABAB`: PartitionAlloc's newly allocated memory.
    * Shows up only in `PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)` builds.
    * MSan should be capable of catching this kind of reads to uninitialized
      regions.


#### Memory allocation owned by someone else

You may see random values written by someone else
if you keep using pointers to `free()`d region.

```
void YourFunc() {              | void TheirFunc() {
  int* p1 = new int;           |
  *p1 = 123;                   |
  delete p1;                   |
                               |   // The allocator may
                               |   // redistribute `p1` to `p2`
                               |   int* p2 = new int;
                               |   *p2 = 456;
  // Use-after-Free;           |
  // 456 may show up           |
  printf("%d\n", *p1);         |
}                              | }
```
