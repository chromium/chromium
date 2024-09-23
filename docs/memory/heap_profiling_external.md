# Heap Profiling with External Tools (on Linux)

For finding memory leaks or generally analyzing native heap usage of Chrome,
external off-the-shelf tools such as [heaptrack] can sometimes be useful to
complement the built-in
[MemoryInfra heap profiler](../memory-infra/heap_profiler.md).
E.g., heaptrack has a convenient view for finding temporary allocations, and
produces a nice flamegraph of the call stacks of all allocation sites.
Alternatively, the `tcmalloc` allocator as part of [gperftools] can produce heap
dumps at arbitrary points in time, which can be visualized and zoomed in,
filtered, etc. with [pprof].

This requires hooking into or replacing the allocator used by Chrome (typically
PartitionAlloc). In this guide, we describe how to do this on Linux. (It might
work similarly on other systems, feel free to extend this document with
instructions for macOS and Windows.)

Note that not all memory usage comes from the allocator or the hooking into it
may be incomplete, so this may underreport allocations as described in [the
blindspots here](tools.md#heap-dumps).

[TOC]

## <a name="heaptrack"></a> Using heaptrack

1. Build or install [heaptrack] and its GUI.
2. [Build Chrome](../get_the_code.md) with the following added to your
   `args.gn`:
```
forward_through_malloc = true   # so that all C++ allocations go to malloc
symbol_level = 2                # for stacktraces
is_component_build = true       # so that the allocation functions are dl-exported
                                # and can be intercepted
```
Since [PartitionAlloc Everywhere](https://docs.google.com/document/d/1R1H9z5IVUAnXJgDjnts3nTJVcRbufWWT9ByXLgecSUM/preview),
you should additionally disable PartitionAlloc and use the system allocator
instead so that more allocations are captured in heaptrack. (Note that
PartitionAlloc is still used in Blink, just not for `malloc` in other places
anymore.) Add these build flags additionally:
```
use_partition_alloc_as_malloc = false
enable_backup_ref_ptr_support = false
```
3. Run Chrome with `chrome --no-sandbox --renderer-cmd-prefix='heaptrack
--record-only' <other args...>`. The sandbox needs to be disabled such that the
heap dump can be written to disk. The other argument is for attaching heaptrack
to each new renderer process (which also disables the
[Zygote](../linux/zygote.md)). This will write (several) heapdump file(s) such
as `heaptrack.chrome.$pid.zst`. (Check the Chrome task manager for which tab
corresponds to which process ID.)
4. Analyze the heap dump with `heaptrack --analyze heaptrack.chrome.$pid.zst`.

## <a name="tcmalloc"></a> Using tcmalloc + pprof

Motivation: An alternative to heaptrack is to use the [heap
profiler](https://gperftools.github.io/gperftools/heapprofile.html) which is
part of the tcmalloc allocator. This has the advantage of allowing to take
heapdumps at different triggers (e.g., every N seconds, every N new bytes
allocated, if in-use memory increases by N bytes, or manually triggered via a
signal) and that its dumps can be visualized and analyzed with [pprof].

1. Build or install [gperftools]. As of 2024-03 the TL;DR of building yourself
   is:
```bash
git clone https://github.com/gperftools/gperftools
cd gperftools
./autogen.sh
./configure
make
```
2. Build Chrome as [described above](#heaptrack).
3. Save the following `heapdump-renderer.sh` script and `chmod +x` it. It
   `LD_PRELOAD`s the tcmalloc allocator into every spawned renderer processes in
   Chrome:
```bash
#!/bin/sh
echo "Renderer $$ starting..."
LD_PRELOAD=path/to/gperftools/.libs/libtcmalloc_and_profiler.so MALLOCSTATS=1 HEAPPROFILE=tcmalloc.renderer$$ HEAPPROFILESIGNAL=12 exec $*
```
4. You can also trigger heapdumps with, e.g.,
   `HEAP_PROFILE_ALLOCATION_INTERVAL=...` instead of `HEAPPROFILESIGNAL`, see
   the [gperftools
   documentation](https://gperftools.github.io/gperftools/heapprofile.html).
5. Run Chrome with `chrome --no-sandbox
   --renderer-cmd-prefix=./heapdump-renderer.sh <other args...>`.
6. Instruct the renderer process of your liking to take a heapdump with `kill -n
   12 $pid`. (Find the process ID in the Chrome task manager or check the
   console output.) This produces a `tcmalloc.renderer$pid.heap` file.
7. Analyze the heapdump with `pprof -flame tcmalloc.renderer$pid.heap`
   (Googlers-only) or `pprof -http tcmalloc.renderer$pid.heap`. Note that you
   can switch the `metric`/`Sample` between `alloc_objects`, `alloc_space`,
   `inuse_objects`, and `inuse_space`.

[heaptrack]: https://github.com/KDE/heaptrack
[gperftools]: https://github.com/gperftools/gperftools
[pprof]: https://github.com/google/pprof
