# MemoryInfra

MemoryInfra is a timeline-based profiling system integrated in chrome://tracing.
It aims at creating Chrome-scale memory measurement tooling so that on any
Chrome in the world --- desktop, mobile, Chrome OS or any other --- with the
click of a button you can understand where memory is being used in your system.

[TOC]

## Taking a memory-infra trace

 1. [Record a trace as usual][record-trace]: open [chrome://tracing][tracing]
    on Desktop Chrome or [chrome://inspect][inspect-tracing] to trace
    Chrome for Android.

 2. Make sure to enable the **memory-infra** category on the right.

      ![Tick the memory-infra checkbox when recording a trace.][memory-infra-box]


[record-trace]:     https://sites.google.com/a/chromium.org/dev/developers/how-tos/trace-event-profiling-tool/recording-tracing-runs
[tracing]:          chrome://tracing
[inspect-tracing]:  chrome://inspect
[memory-infra-box]: https://storage.googleapis.com/chromium-docs.appspot.com/1c6d1886584e7cc6ffed0d377f32023f8da53e02

## Navigating a memory-infra trace

![Timeline View and Analysis View][tracing-views]

After recording a trace, you will see the **timeline view**. The **timeline
view** is primarily used for other tracing features. Click one of the
![M][m-purple] dots to bring up the **analysis view**. Click on a cell in
analysis view to reveal more information about its subsystem. PartitionAlloc for
instance, has more details about its partitions.

![Component details for PartitionAlloc][partalloc-details]

The full details of the MemoryInfra UI are explained in its [design
doc][mi-ui-doc].

[tracing-views]:     https://storage.googleapis.com/chromium-docs.appspot.com/db12015bd262385f0f8bd69133330978a99da1ca
[partalloc-details]: https://storage.googleapis.com/chromium-docs.appspot.com/02eade61d57c83f8ef8227965513456555fc3324
[m-purple]:          https://storage.googleapis.com/chromium-docs.appspot.com/d7bdf4d16204c293688be2e5a0bcb2bf463dbbc3
[mi-ui-doc]:         https://docs.google.com/document/d/1b5BSBEd1oB-3zj_CBAQWiQZ0cmI0HmjmXG-5iNveLqw/edit

## Columns

**Columns in blue** reflect the amount of actual physical memory used by the
process. This is what exerts memory pressure on the system.

 * **Total Resident**: The current working set size of the process, excluding
   the memory overhead of tracing. On Linux, this returns the resident set size.
 * **Peak Total Resident**: The overall peak working set size of the process on
   supported platforms. On Linux kernel versions >= 4.0 the peak usage between
   two memory dumps is shown.
 * **PSS**: POSIX only. The process's proportional share of total resident size.
 * **Private Dirty**: The total size of dirty pages which are not used by any
   other process.
 * **Swapped**: The total size of anonymous memory used by process, which is
   swapped out of RAM.

**Columns in black** reflect a best estimation of the amount of physical
memory used by various subsystems of Chrome.

 * **Blink GC**: Memory used by [Oilpan][oilpan].
 * **CC**: Memory used by the compositor.
   See [cc/memory][cc-memory] for the full details.
 * **Discardable**: Total [discardable][discardable] memory used by the process
   from various components like Skia caches and Web caches.
 * **Font Caches**: Size of cache that stores Font shapes and platform Fonts.
 * **GPU** and **GPU Memory Buffer**: GPU memory and RAM used for GPU purposes.
   See [GPU Memory Tracing][gpu-memory].
 * **LevelDB**: Memory used for LeveldbValueStore(s), IndexedDB databases and
   ProtoDatabase(s).
 * **Malloc**: Memory allocated by calls to `malloc`, or `new` for most
     non-Blink objects.
 * **PartitionAlloc**: Memory allocated via [PartitionAlloc][partalloc].
   Blink objects that are not managed by Oilpan are allocated with
   PartitionAlloc.
 * **Skia**: Memory used by all resources used by the Skia rendering system.
 * **SQLite**: Memory used for all sqlite databases.
 * **Sync**: Memory used by Chrome Sync when signed in.
 * **UI**: Android only. Memory used by Android java bitmaps for the UI.
 * **V8**: Memory used by V8 Javascript engine.
 * **Web Cache**: Memory used by resources downloaded from the Web, like images
   and scripts.

The **tracing column in gray** reports memory that is used to collect all of the
above information. This memory would not be used if tracing were not enabled,
and it is discounted from malloc and the blue columns.

<!-- TODO(primiano): Improve this. https://crbug.com/??? -->

[oilpan]:     /third_party/blink/renderer/platform/heap/BlinkGCDesign.md
[discardable]:base/memory/discardable_memory.h
[cc-memory]:  probe-cc.md
[gpu-memory]: probe-gpu.md
[partalloc]:  /base/allocator/partition_allocator/PartitionAlloc.md

## 'effective\_size' vs. 'size'

This is a little like the difference between 'self time' and 'cumulative time'
in a profiling tool. Size is the total amount of memory allocated/requested
by a subsystem whereas effective size is the total amount of memory
used/consumed by a subsystem. If Skia allocates 10mb via partition_alloc
that memory would show up in the size of both Skia and partition_alloc
but only in the effective size of Skia since although partition_alloc
allocates the 10mb it does so on behalf of Skia which is responsible
for the memory. Summing all effective sizes gives the total amount of
memory used whereas summing size would give a number larger than the total
amount of memory used.

## Related Pages

 * [Adding MemoryInfra Tracing to a Component](adding_memory_infra_tracing.md)
 * [GPU Memory Tracing](probe-gpu.md)
 * [Heap Profiling with MemoryInfra](heap_profiler.md)
 * [Startup Tracing with MemoryInfra](memory_infra_startup_tracing.md)

## Rationale

Another memory profiler? What is wrong with tool X?
Most of the existing tools:

 * Are hard to get working with Chrome. (Massive symbols, require OS-specific
   tricks.)
 * Lack Chrome-related context.
 * Don't deal with multi-process scenarios.

MemoryInfra leverages the existing tracing infrastructure in Chrome and provides
contextual data:

 * **It speaks Chrome slang.**
   The Chromium codebase is instrumented. Its memory subsystems (allocators,
   caches, etc.) uniformly report their stats into the trace in a way that can
   be understood by Chrome developers. No more
   `__gnu_cxx::new_allocator< std::_Rb_tree_node< std::pair< std::string const, base::Value*>>> ::allocate`.
 * **Timeline data that can be correlated with other events.**
   Did memory suddenly increase during a specific Blink / V8 / HTML parsing
   event? Which subsystem increased? Did memory not go down as expected after
   closing a tab? Which other threads were active during a bloat?
 * **Works out of the box on desktop and mobile.**
    No recompilations, no time-consuming symbolizations stages. All the
   logic is already in Chrome, ready to dump at any time.
 * **The same technology is used for telemetry and the ChromePerf dashboard.**
   See [the slides][chromeperf-slides] and take a look at
   [some ChromePerf dashboards][chromeperf] and
   [telemetry documentation][telemetry].

[chromeperf-slides]: https://docs.google.com/presentation/d/1OyxyT1sfg50lA36A7ibZ7-bBRXI1kVlvCW0W9qAmM_0/present?slide=id.gde150139b_0_137
[chromeperf]:        https://chromeperf.appspot.com/report?sid=3b54e60c9951656574e19252fadeca846813afe04453c98a49136af4c8820b8d
[telemetry]:         https://catapult.gsrc.io/telemetry

## Development

MemoryInfra is based on a simple and extensible architecture. See
[the slides][dp-slides] on how to get your subsystem reported in MemoryInfra,
or take a look at one of the existing examples such as
[malloc_dump_provider.cc][malloc-dp]. The crbug label is
[Hotlist-MemoryInfra][hotlist]. Don't hesitate to contact
[tracing@chromium.org][mailtracing] for questions and support.

[dp-slides]:   https://docs.google.com/presentation/d/1GI3HY3Mm5-Mvp6eZyVB0JiaJ-u3L1MMJeKHJg4lxjEI/present?slide=id.g995514d5c_1_45
[malloc-dp]:   https://chromium.googlesource.com/chromium/src.git/+/master/base/trace_event/malloc_dump_provider.cc
[hotlist]:     https://code.google.com/p/chromium/issues/list?q=label:Hotlist-MemoryInfra
[mailtracing]: mailto:tracing@chromium.org

## Design documents

Architectural:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfmp0cW1VcE5XVWNxZndxelV5T19kT2NFSndYZlNFbkFpc3pSa2VDN0hlMm8">
</iframe>

Chrome-side design docs:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfndSa2dleUQtMnZDeWpPZk1JV0QtbVM5STkwWms4YThzQ0pGTmU1QU9kNVk">
</iframe>

Catapult-side design docs:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfm10bXd5YmRNWUpKOElOWS0xdU1tMmV1S3F4aHo0ZDJLTmtGRy1qVnQtVWM">
</iframe>
