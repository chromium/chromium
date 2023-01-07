# Heap Profiling with MemoryInfra

As of Chrome 48, MemoryInfra supports heap profiling. Chrome will track all live
allocations (calls to new or malloc without a subsequent call to delete or free)
along with sufficient metadata to identify the code that made the allocation.

By default, MemoryInfra traces will not contain heap dumps. Heap profiling must
be enabled via chrome://memory-internals or about://flags.

[TOC]

## How to obtain a heap dump (M66+, Linux, macOS, Windows)

 1. Navigate to chrome://memory-internals.
    * There will be an error message at the top if heap-profiling is not
      supported on the current configuration
 2. Enable heap profiling for the relevant processes. Future allocations will be
    tracked. Refresh the page to view tracked processes.
    * To enable tracking at process start, navigate to chrome://flags and search
      for `memlog`.
 3. To take a heap dump, click `save dump`. This is stored as a
    [MemoryInfra][memory-infra] trace.
 4. To symbolize the trace:
   * Windows only: build `addr2line-pdb` from the chromium repository. For subsequent commands, add the flag `--addr2line-executable=<path_to_addr2lin-pdb>`
   * If this is a local build, run the command `./third_party/catapult/tracing/bin/symbolize_trace --is-local-build <path_to_trace>`
   * If this is an official Chrome build,  run `./third_party/catapult/tracing/bin/symbolize_trace <path_to_trace>`. This will request authentication with google cloud storage to obtain symbol files [googlers only].
   * If this is an official macOS or Linux Chrome build, add the flag `--use-breakpad-symbols`.
   * If the trace is from a different device on the same operating system, add the flag
     `--only-symbolize-chrome-symbols`.
   * If you run into the error "Nothing to symbolize" then backtraces are not
     working properly. There are two mechanisms that Chrome attempts to use:
     frame pointers if they're present, and backtrace lib. The former can be
     forced on with enable_frame_pointers gn arg. This should work on all architectures except for
     arm 32. The latter depends on unwind tables.
 5. Load the (now symbolized) trace in chrome://tracing.

## How to obtain a heap dump (M66+, Android)

On arm64 and x86-64, you can build chrome normally and follow steps above to
obtain heap dumps.

To obtain native heap dumps on arm32, you will need a custom build of Chrome
with the GN arguments `enable_profiling = true`, `arm_use_thumb = false`,
`is_component_build = false` and `symbol_level=1`. All other steps are the same.

Alternatively, if you want to use an official build of Chrome, use
`is_official_build = true` for arm32. If you want to use a released build,
profiling only works on Dev and Canary on arm, and all channels on x86-64. In
this case, you also need to fetch symbols manually and pass to the
symbolize_trace script above.

## How to obtain a heap dump (M65 and older)

For the most part, the setting `enable-heap-profiling` in `chrome://flags` has a
similar effect to the various `memlog` flags.


## How to manually browse a heap dump

 1. Select a heavy memory dump indicated by a purple ![M][m-purple] dot.

 2. In the analysis view, cells marked with a triple bar icon (☰) contain heap
    dumps. Select such a cell.

      ![Cells containing a heap dump][cells-heap-dump]

 3. Scroll down all the way to _Heap Details_.

 4. To navigate allocations, select a frame in the right-side pane and press
    Enter/Return. To pop up the stack, press Backspace/Delete.

[memory-infra]:    README.md
[m-purple]:        https://storage.googleapis.com/chromium-docs.appspot.com/d7bdf4d16204c293688be2e5a0bcb2bf463dbbc3
[cells-heap-dump]: https://storage.googleapis.com/chromium-docs.appspot.com/a24d80d6a08da088e2e9c8b2b64daa215be4dacb

## How to automatically extract large allocations from a heap dump

 1. Run `python ./third_party/catapult/experimental/tracing/bin/diff_heap_profiler.py
    <path_to_trace>`

 2. This produces a directory `output`, which contains a JSON file.

 3. Load the contents of the JSON file in any JSON viewer, e.g.
    [jsonviewer](http://jsonviewer.stack.hu/).

 4. The JSON files shows allocations segmented by stacktrace, sorted by largest
    first.

## Heap Details

The heap details view contains a tree that represents the heap. The size of the
root node corresponds to the selected allocator cell.

*** aside
The size value in the heap details view will not match the value in the selected
analysis view cell exactly. There are three reasons for this. First, the heap
profiler reports the memory that _the program requested_, whereas the allocator
reports the memory that it _actually allocated_ plus its own bookkeeping
overhead. Second, allocations that happen early --- before Chrome knows that
heap profiling is enabled --- are not captured by the heap profiler, but they
are reported by the allocator. Third, tracing overhead is not discounted by the
heap profiler.
***

The heap can be broken down in two ways: by _backtrace_ (marked with an ƒ), and
by _type_ (marked with a Ⓣ). When tracing is enabled, Chrome records trace
events, most of which appear in the flame chart in timeline view. At every
point in time these trace events form a pseudo stack, and a vertical slice
through the flame chart is like a backtrace. This corresponds to the ƒ nodes in
the heap details view.  Hence enabling more tracing categories will give a more
detailed breakdown of the heap.

The other way to break down the heap is by object type. At the moment this is
only supported for PartitionAlloc.

*** aside
In official builds, only the most common type names are included due to binary
size concerns. Development builds have full type information.
***

To keep the trace log small, uninteresting information is omitted from heap
dumps. The long tail of small nodes is not dumped, but grouped in an `<other>`
node instead. Note that although these small nodes are insignificant on their
own, together they can be responsible for a significant portion of the heap. The
`<other>` node is large in that case.

## Example

In the trace below, `ParseAuthorStyleSheet` is called at some point.

![ParseAuthorStyleSheet pseudo stack][pseudo-stack]

The pseudo stack of trace events corresponds to the tree of ƒ nodes below. Of
the 23.5 MiB of memory allocated with PartitionAlloc, 1.9 MiB was allocated
inside `ParseAuthorStyleSheet`, either directly, or at a deeper level (like
`CSSParserImpl::parseStyleSheet`).

![Memory Allocated in ParseAuthorStyleSheet][break-down-by-backtrace]

By expanding `ParseAuthorStyleSheet`, we can see which types were allocated
there. Of the 1.9 MiB, 371 KiB was spent on `ImmutableStylePropertySet`s, and
238 KiB was spent on `StringImpl`s.

![ParseAuthorStyleSheet broken down by type][break-down-by-type]

It is also possible to break down by type first, and then by backtrace. Below
we see that of the 23.5 MiB allocated with PartitionAlloc, 1 MiB is spent on
`Node`s, and about half of the memory spent on nodes was allocated in
`HTMLDocumentParser`.

![The PartitionAlloc heap broken down by type first and then by backtrace][type-then-backtrace]

Heap dump diffs are fully supported by trace viewer. Select a heavy memory dump
(a purple dot), then with the control key select a heavy memory dump earlier in
time. Below is a diff of theverge.com before and in the middle of loading ads.
We can see that 4 MiB were allocated when parsing the documents in all those
iframes, almost a megabyte of which was due to JavaScript. (Note that this is
memory allocated by PartitionAlloc alone, the total renderer memory increase was
around 72 MiB.)

![Diff of The Verge before and after loading ads][diff]

[pseudo-stack]:            https://storage.googleapis.com/chromium-docs.appspot.com/058e50350836f55724e100d4dbbddf4b9803f550
[break-down-by-backtrace]: https://storage.googleapis.com/chromium-docs.appspot.com/ec61c5f15705f5bcf3ca83a155ed647a0538bbe1
[break-down-by-type]:      https://storage.googleapis.com/chromium-docs.appspot.com/2236e61021922c0813908c6745136953fa20a37b
[type-then-backtrace]:     https://storage.googleapis.com/chromium-docs.appspot.com/c5367dde11476bdbf2d5a1c51674148915573d11
[diff]:                    https://storage.googleapis.com/chromium-docs.appspot.com/802141906869cd533bb613da5f91bd0b071ceb24
