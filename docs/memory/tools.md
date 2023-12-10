# Description of Tools for developers trying to understand memory usage

This page provides an overview of the tools available for examining memory usage
in chrome.

## Which tool should I use?

No single tool can give a full view of memory usage in Chrome. There are too
many different context involved (JS heap, DOM objects, native allocations, GPU,
etc) that any tool that collected all that information likely would not be able
to provide an actionable analysis.

Here is a table of common area of inquiry and suggested tools for examining them.

| Topic/Area of Inquiry  | Tool(s) |
|----------------------- | ------- |
| Which subsystems consuming memory per process.  | [Global Memory Dumps](#global-memory-dumps), [Taking memory-infra trace](#memory-infra-trace) |
| Tracking C++ object allocation over time | [`diff_heap_profiler.py`](#diff-heap-profiler), [Heap Details in chrome://tracing](#heap-dumps-chrome-tracing) |
| Suspected DOM leaks in the Renderer | [Developer Tools Heap Snapshots](#dev-tools-heap-snapshots), [Real World Leak Detector](#real-world-leak-detector) |
| Kernel/Driver Memory and Resource Usage | [perfmon (win), ETW](#os-tools) |
| Blackbox examination of process memory | [VMMAP (win)](#os-tools) | Understanding fragmentation of the memory space |
| Symbolized Heap Dump data | [Heap Dumps](#heap-dumps) | Grabs raw data for analysis by other tools |

If that seems like a lot of tools and complexity, it is [but there's a reason](#no-one-true-metric).

-----------
## <a name="global-memory-dumps"> Global Memory Dumps
Many Chrome subsystems implement the
[`trace_event::MemoryDumpProvider`](../../base/trace_event/memory_dump_provider.h)
interface to provide self-reported stats detailing their memory usage. The
Global Memory Dump view provides a snapshot-oriented view of these subsystems
that can be collected and viewed via the chrome://tracing infrastructure.

In the Analysis split screen, a single roll-up number is provided for each of
these subsystems. This can give a quick feel for where memory is allocated. The
cells can then be clicked to drill into a more detailed view of the subsystem's
stats. The memory-infra docs have more [detailed descriptions for each column](../memory-infra#Columns).

To look a the delta between two dumps, control-click two different dark-purple M
circles.

### Blindspots
  * Statistics are self-reported. If the MemoryDumpProvider implementation does
    not fully cover the resource usage of the subsystem, those resources will
    not be accounted.

### Instructions
  1. Take a memory-infra trace
  2. Click on a *dark-purple* M circle. Each one of these corresponds to a heavy
     dump.
  3. Click on a (process, subsystem) cell in `Global Memory Dump` tab within the
     Analysis View in bottom split screen.
  4. *Scroll down* to the bottom of the lower split screen to see details of
     selection (process, subsystem)

Clicking on the cell pulls up a view that lets you examine the stats
collected by the given MemoryDumpProvider however that view is often way outside
the viewport of the analysis view. Be sure to scroll down.


-----------
## <a name="heap-dumps-chrome-tracing"> Heap Dumps in chrome://tracing
GUI method of exploring the heap dump for a process.

TODO(awong): Explain how to interpret + interact with the data. (e.g. threads,
bottom-up vs top-down, etc)

### Blindspots
  * As this is a viewer of [heap dump](#heap-dump) data, it has the same
    blindspots.
  * The tool is bound by the memory limits of chrome://tracing. Large dumps
    (which generate large JS strings) will not be loadable and may likely crash
    chrome://tracing.

### Instructions
  1. [Configure Out-of-process heap profiling](#configure-oophp)
  2. Take a memory-infra trace and symbolize it.
  3. Click on a *dark-purple* M circle.
  4. Find the cell corresponding to the allocator (list below) for the process of interest within the `Global Memory Dump` tab of the Analysis View.
  5. Click on "hotdog" menu icon next to the number. If no icon is shown, the
     trace does not contain a heap dump for that allocator.
  6. *Scroll down* to the bottom of the lower split screen. There should now
     be a "Heap details" section below the "Component details" section that
     shows a all heap allocations in a navigatable format.

On step 5, the `Component Details` and `Heap Dump` views that let you examine
the information collected by the given MemoryDumpProvider is often way outside
the current viewport of the Analysis View. Be sure to scroll down!

Currently supported allocators: malloc, PartitionAlloc, Oilpan.

Note: PartitionAlloc and Oilpan traces have unsymbolized Javascript frames
which often make exploration via this tool hard to consume.


-----------

## <a name="diff-heap-profiler"></a> `diff_heap_profiler.py`
This is most useful for examining allocations that occur during an interval of
time. This is often useful for finding leaks as one call-stack will rise to the
top as the leak is repeated triggered.

Multiple traces can be given at once to show incremental changes. A similar
analysis can be had via ctrl-clicking multiple Global Memory Dumps in the
chrome://tracing UI but loading multiiple detailed heapdumps can often crash the
chrome://tracing UI. This tool is more robust to large data sizes.

The source code can also be used as an example for manually processing heap dump
data in python.

TODO(awong): Write about options to script and the flame graph.

### Blindspots
  * As this is a viewer of [heap dump](#heap-dumps) data, it has the same
    blindspots.

### Instructions
  1. Get 2 or more [symbolized heap dump](#heap-dumps)
  3. Run resulting traces through [`diff_heap_profiler.py`](https://chromium.googlesource.com/catapult/+/main/experimental/tracing/bin/diff_heap_profiler.py) to show a list of new allocations.

-----------
## <a name="heap-dumps"></a>Heap Dumps
Heap dumps provide extremely detailed data about object allocations and is
useful for finding code locations that are generating a large number of live
allocations. Data is tracked and recorded using the [Out-of-process Heap
Profiler (OOPHP)](../../components/services/heap_profiling/README.md).

For the Browser and GPU process, this often quickly finds objects that leak over
time.

This is less useful in the Renderer process. Even though Oilpan and
PartitionAlloc are hooked into the data collection, many of the stacks end up
looking similar due to the nature of DOM node allocation.

### Blindspots
  * Heap dumps only catch allocations that pass through the allocator shim. In particular,
    calls made directly to the platform's VM subsystem (eg, via `mmap()` or
    `VirtualAlloc()`) will not be tracked.
  * Utility processes are currently not profiled.
  * Allocations are only recorded after the
    [HeapProfilingService](../../components/services/heap_profiling/heap_profiling_service.h)
    has spun up the profiling process and created a connection to the target
    process. The HeapProfilingService is a mojo service that can be configured to
    start early in browser startup but it still takes time to spin up and early
    allocations are thus lost.

### Instructions
#### <a name="configure-oophp"></a>Configuration and setup
  1. [Android Only] For native stack traces, a custom build with
     `enable_framepointers=true` is required.
  2. Configure OOPHP settings in about://flags. (See table below)
  3. Restart browser with new settings if necessary.
  4. Verify target processes are being profiled in chrome://memory-internals.
  5. [Optional] start profiling additional processes in chrome://memory-internals.

| Flag | Notes |
| ------- | ----- |
| Out of process heap profiling start mode. | This option is somewhat misnamed. It tells OOPHP which processes to profile at startup. Other processes can selected manually later via chrome://memory-internals even if this is set to "disabled". |
| Keep track of even the small allocations in memlog heap dumps. | By default, small allocations are not emitted in the heap dump to reduce dump size. Enabling this track _all_ allocations. |
| The type of stack to record for memlog heap dumps | If possible, use Native stack frames as that provides the best information. When those are not available either due to performance for build (eg, no frame-pointers on arm32 official) configurations, using trace events for a "pseudo stack" can give good information too. |
| Heap profiling | Deprecated. Enables the in-process heap profiler. Functionality should be fully subsumed by preceeding options. |

#### Saving a heap dump
  1. On Desktop, click "save dump" in chrome://memory-internals to save a
     dump of all the profiled processes. On Android, enable debugging via USB
     and use chrome://inspect/?tracing#devices to take a memory-infra trace
     which will have the heap dump embedded.
  2. Symbolize trace using  [`symbolize_trace.py`](../../third_party/catapult/tracing/bin/symbolize_trace). If the Chrome binary was built locally, pass the flag "--is-local-build".
  3. Analyze resuing heap dump using [`diff_heap_profiler.py`](#diff-heap-profiler), or [Heap Profile view in Chrome Tracing](#tracing-heap-profile)

On desktop, using chrome://memory-internals to take a heap dump is more reliable
as it directly saves the heapdump to a file instead of passing the serialized data
through the chrome://tracing renderer process which can easily OOM. For Android,
this native file saving was harder to implement and would still leave the
problem of getting the dump off the phone so memory-infra tracing is the
current recommended path.

-----------
## <a name="memory-infra-trace"></a> Taking a memory-infra trace.
Examining self-reported statistics from various subsystems on memory usages.
This is most useful for getting a high-level understanding of how memory is
distributed between the different heaps and subsystems in chrome.

It also provides a way to view heap dump allocation information collected per
process through a progressively expanding stack trace.

Though chrome://tracing itself is a timeline based plot, this data is snapshot
oriented. Thus the standard chrome://tracing plotting tools do not provide a
good means for measuring changes per snapshot.

### Blindspots
  * Statistics are self-reported via "Memory Dump Provider" interfaces. If there
    is an error in the data collection, or if there are privileged resources
    that cannot be easily measured from usermode, they will be missed.

### Instructions
  1. Visit chrome://tracing
  2. Start a trace for memory-infra
      1. Click the "Record" button
      2. Choose "Manually select settings"
      3. [optional] Clear out all other tracing categories.
      4. Select "memory-infra" from the "Disabled by Default Categories"
      5. Click record again.
  3. Wait for a few seconds for a Global Memory Dump to be taken.  If OOPHP
     is enabled, don't run for more than a few seconds to avoid crashing the
     chrome://tracing UI with an over-large trace.
  4. Wait for a few seconds for a Global Memory Dump to be taken.
  5. Click stop

This should produce a view of the trace file with periodic "light" and "heavy"
memory dumps. These dumps are created periodically so the time spent waiting
in step (3) determines how many dumps (which are snapshots) are taken.

**Warning:** If OOPHP is enabled, the tracing UI may not be able to handle
deserializing or rendering the memory dump. In this situation, save
the heap dump directly in chrome://memory-internals and use alternate tools to
analyze it.

TODO(ajwong): Add screenshot or at least reference the more detailed
memory-infra docs.

-----------
## <a name="dev-tools-heap-snapshots"></a> Developer Tools Heap Snapshots

Heap snapshots provide views of objects on the Oilpan and V8 heaps and retainer
relationships between them. General documentation is here:
https://developer.chrome.com/docs/devtools/memory-problems/heap-snapshots/

By default, many objects on the Oilpan heap will be labeled as "InternalNode".
To capture detailed symbol names for them, follow these steps:

1. Add the following to gn args and rebuild: `cppgc_enable_object_names = true` <br/>
   Or use [Chrome for Testing](https://googlechromelabs.github.io/chrome-for-testing/) prebuilt binaries; they have this flag enabled.

2. In Developer Tools, under Settings | Experiments, check "Show option to
expose internals in heap snapshots"

3. Reload Developer Tools (there will be a button for this at the top of the
window)

4. On the Memory pane, under Select profiling type | Heap snapshot, check
"Expose internals (includes additional implementation-specific details)"

-----------
## <a name="real-world-leak-detector"></a> Real World Leak Detector (Blink-only)
TODO(awong): Fill in.


-----------
## <a name="os-tools"></a> OS Tools: perfmon, ETW, VMMAP
Each OS provides specialized tools that give the closest to complete information
about resource usage. This is a list of commonly interesting tools per platform.
Use them as search terms to look up new ways to analyze data.

| Platform | Tools |
| -------- | ----- |
| Window | [SysInternals vmmap](https://docs.microsoft.com/en-us/sysinternals/downloads/vmmap), resmon (can track kernel resources like Paged Pool), perfmon, ETW, !heap in WinDbg |
| Mac | [vmmap](https://developer.apple.com/library/content/documentation/Performance/Conceptual/ManagingMemory/Articles/VMPages.html), `vm_stat` |
| Linux/Android | `cat /proc/pid/maps` |


-----------
## <a name="no-one-true-metric"></a> No really, I want one tool/metric that views everything. Can I has it plz?
Sorry. No.

There is a natural tradeoff between getting detailed information
and getting reliably complete information. Getting detailed information requires
instrumentation which adds complexity and selection bias to the measurement.
This reduces the reliability and completeness of the metric as code shifts over
time.

While it might be possible to instrument a specific Chrome heap
(eg, PartitionAlloc or Oilpan, or even shimming malloc()) to gather detailed
actionable data, this implicitly means the instrumentation code is making
assumptions about what process resources are used which may not be complete
or correct.

As an example of missed coverage, none of these collection methods
can notice kernel resources that are allocated (eg, GPU memory, or drive memory
such as the Windows Paged and Non-paged pools) as side effects of user mode
calls nor do they account for memory that does not go through new/malloc
(manulaly callling `mmap()`, or `VirtualAlloc()`). Querying a full view of
these allocations usually requires admin privileges, the semantics change
per platform, and the performance can vary from being "constant-ish" to
being dependent on virtual space size (eg, probing allocation via
VirtualQueryEx or parsing /proc/self/maps) or number of processes in the
system (NTQuerySystemInformation).

As an example of error in measurement, PartitionAlloc did not account for
the Windows Committed Memory model [bug](https://crbug.com/765406) leading to
a "commit leak" in Windows that was undetected in its self-reported stats.

Relying on a single metric or single tool will thus either selection bias
the data being read or not give enough detail to quickly act on problems.
