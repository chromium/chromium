# GWP-ASan

GWP-ASan is a debug tool intended to detect heap memory errors in the wild. It
samples allocations to a debug allocator, similar to ElectricFence or Page Heap,
causing memory errors to crash and report additional debugging context about
the error.

It is also known by its recursive backronym, GWP-ASan Will Provide Allocation
Sanity.

To read a more in-depth explanation of GWP-ASan see [this post](https://sites.google.com/a/chromium.org/dev/Home/chromium-security/articles/gwp-asan).

## Allocator

The GuardedPageAllocator returns allocations on pages buffered on both sides by
guard pages. The allocations are either left- or right-aligned to detect buffer
overflows and underflows. When an allocation is freed, the page is marked
inaccessible so use-after-frees cause an exception (until that page is reused
for another allocation.)

The allocator saves stack traces on every allocation and deallocation to
preserve debug context if that allocation results in a memory error.

The allocator implements a quarantine mechanism by allocating virtual memory for
more allocations than the total number of physical pages it can return at any
given time. The difference forms a rudimentary quarantine.

Because pages are re-used for allocations, it's possible that a long-lived
use-after-free will cause a crash long after the original allocation has been
replaced. In order to decrease the likelihood of incorrect stack traces being
reported, we allocate a lot of virtual memory but don't store metadata for every
allocation. That way though we may not be able to report the metadata for an old
allocation, we will not report incorrect stack traces.

## Crash handler

The allocator is designed so that memory errors with GWP-ASan allocations
intentionally trigger invalid access exceptions. A hook in the crashpad crash
handler process inspects crashes, determines if they are GWP-ASan exceptions,
and adds additional debug information to the crash minidump if so.

The crash handler hook determines if the exception was related to GWP-ASan by
reading the allocator internals and seeing if the exception address was within
the bounds of the allocator region. If it is, the crash handler hook extracts
debug information about that allocation, such as thread IDs and stack traces
for allocation (and deallocation, if relevant) and writes it to the crash dump.

The crash handler runs with elevated privileges so parsing information from a
lesser-privileged process is security sensitive. The GWP-ASan hook is specially
structured to minimize the amount of allocator logic it relies on and to
validate the allocator internals before reasoning about them.

## Status

GWP-ASan is implemented for malloc and PartitionAlloc. It is enabled by default
on Windows and macOS. The allocator parameters can be manually modified by using
an invocation like the following:

```shell
chrome --enable-features="GwpAsanMalloc<Study" \
       --force-fieldtrials=Study/Group1 \
       --force-fieldtrial-params=Study.Group1:MaxAllocations/128/MaxMetadata/255/TotalPages/4096/AllocationSamplingFrequency/1000/ProcessSamplingProbability/1.0
```

GWP-ASan is tuned more aggressively in canary/dev, to increase the likelihood we
catch newly introduced bugs, and for specific processes depending on the
particular allocator.

A [hotlist of bugs discovered by by GWP-ASan](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=Hotlist%3DGWP-ASan)
exists, though GWP-ASan crashes are filed Bug-Security, e.g. without external
visibility, by default.

## Limitations

- GWP-ASan is configured with a small fixed-size amount of memory, so
  long-lived allocations can quickly deplete the page pool and lead the
  allocator to run out of memory. Depending on the sampling frequency and
  distribution of allocation lifetimes this may lead to only allocations early
  in the process lifetime being sampled.
- Allocations over a page in size are not sampled.
- The allocator skips zero-size allocations. Zero-size allocations on some
  platforms return valid pointers and may be subject to lifetime and bounds
  issues.
- GWP-ASan does not intercept allocations for Oilpan or the v8 GC.
- GWP-ASan does not hook PDFium's fork of PartitionAlloc.
- Right-aligned allocations to catch overflows are not perfectly right-aligned,
  so small out-of-bounds accesses may be missed.
- GWP-ASan does not sample some early allocations that occur before field trial
  initialization.
- Depending on the platform, GWP-ASan may or may not hook malloc allocations
  that occur in code not linked directly against Chrome.

## Testing

There is [not yet](https://crbug.com/910751) a way to intentionally trigger a
GWP-ASan exception.

There is [not yet](https://crbug.com/910749) a way to inspect GWP-ASan data in
a minidump (crash report) without access to Google's crash service.
