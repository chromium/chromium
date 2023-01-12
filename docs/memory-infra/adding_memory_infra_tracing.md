# Adding MemoryInfra Tracing to a Component

If you have a component that manages memory allocations, you should be
registering and tracking those allocations with Chrome's MemoryInfra system.
This lets you:

 * See an overview of your allocations, giving insight into total size and
   breakdown.
 * Understand how your allocations change over time and how they are impacted by
   other parts of Chrome.
 * Catch regressions in your component's allocations size by setting up
   telemetry tests which monitor your allocation sizes under certain
   circumstances.

Some existing components that use MemoryInfra:

 * **Discardable Memory**: Tracks usage of discardable memory throughout Chrome.
 * **GPU**: Tracks OpenGL and other GPU object allocations.
 * **V8**: Tracks the heap size for JS.

[TOC]

## Overview

In order to hook into Chrome's MemoryInfra system, your component needs to do
two things:

 1. Create a [`MemoryDumpProvider`][mdp] for your component.
 2. Register and unregister you dump provider with the
    [`MemoryDumpManager`][mdm].

[mdp]: https://chromium.googlesource.com/chromium/src/+/main/base/trace_event/memory_dump_provider.h
[mdm]: https://chromium.googlesource.com/chromium/src/+/main/base/trace_event/memory_dump_manager.h

## Creating a Memory Dump Provider

You can implement a [`MemoryDumpProvider`][mdp] as a stand-alone class, or as an
additional interface on an existing class. For example, this interface is
frequently implemented on classes which manage a pool of allocations (see
[`cc::ResourcePool`][resource-pool] for an example).

A `MemoryDumpProvider` has one basic job, to implement `OnMemoryDump`. This
function is responsible for iterating over the resources allocated or tracked by
your component, and creating a [`MemoryAllocatorDump`][mem-alloc-dump] for each
using [`ProcessMemoryDump::CreateAllocatorDump`][pmd]. A simple example:

```cpp
bool MyComponent::OnMemoryDump(const MemoryDumpArgs& args,
                               ProcessMemoryDump* process_memory_dump) {
  for (const auto& allocation : my_allocations_) {
    auto* dump = process_memory_dump->CreateAllocatorDump(
        "path/to/my/component/allocation_" + allocation.id().ToString());
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    allocation.size_bytes());

    // While you will typically have a kNameSize entry, you can add additional
    // entries to your dump with free-form names. In this example we also dump
    // an object's "free_size", assuming the object may not be entirely in use.
    dump->AddScalar("free_size",
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    allocation.free_size_bytes());
  }
}
```

For many components, this may be all that is needed. See
[Handling Shared Memory Allocations](#Handling-Shared-Memory-Allocations) and
[Suballocations](#Suballocations) for information on more complex use cases.

[resource-pool]:  https://chromium.googlesource.com/chromium/src/+/main/cc/resources/resource_pool.h
[mem-alloc-dump]: https://chromium.googlesource.com/chromium/src/+/main/base/trace_event/memory_allocator_dump.h
[pmd]:            https://chromium.googlesource.com/chromium/src/+/main/base/trace_event/process_memory_dump.h

## Registering a Memory Dump Provider

Once you have created a [`MemoryDumpProvider`][mdp], you need to register it
with the [`MemoryDumpManager`][mdm] before the system can start polling it for
memory information. Registration is generally straightforward, and involves
calling `MemoryDumpManager::RegisterDumpProvider`:

```cpp
// Each process uses a singleton |MemoryDumpManager|.
base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
    my_memory_dump_provider_, my_single_thread_task_runner_);
```

In the above code, `my_memory_dump_provider_` is the `MemoryDumpProvider`
outlined in the previous section. `my_single_thread_task_runner_` is more
complex and may be a number of things:

 * Most commonly, if your component is always used from the main message loop,
   `my_single_thread_task_runner_` may just be
   [`base::SingleThreadTaskRunner::GetCurrentDefault()`][task-runner-handle].
 * If your component already uses a custom `base::SingleThreadTaskRunner` for
   executing tasks on a specific thread, you should likely use this runner.

[task-runner-current-default-handle]: https://chromium.googlesource.com/chromium/src/+/main/base/task/single_thread_task_runner.h

## Unregistration

Unregistration must happen on the thread belonging to the
`SingleThreadTaskRunner` provided at registration time. Unregistering on another
thread can lead to race conditions if tracing is active when the provider is
unregistered.

```cpp
base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      my_memory_dump_provider_);
```

## Handling Shared Memory Allocations

When an allocation is shared between two components, it may be useful to dump
the allocation in both components, but you also want to avoid double-counting
the allocation. This can be achieved using the concept of _ownership edges_.
An ownership edge represents that the _source_ memory allocator dump owns a
_target_ memory allocator dump. If multiple source dumps own a single target,
then the cost of that target allocation will be split between the sources.
Additionally, importance can be added to a specific ownership edge, allowing
the highest importance source of that edge to claim the entire cost of the
target.

In the typical case, you will use [`ProcessMemoryDump`][pmd] to create a shared
global allocator dump. This dump will act as the target of all
component-specific dumps of a specific resource:

```cpp
// Component 1 is going to create a dump, source_mad, for an allocation,
// alloc_, which may be shared with other components / processes.
MyAllocationType* alloc_;
base::trace_event::MemoryAllocatorDump* source_mad;

// Component 1 creates and populates source_mad;
...

// In addition to creating a source dump, we must create a global shared
// target dump. This dump should be created with a unique global ID which can be
// generated any place the allocation is used. I recommend adding a global ID
// generation function to the allocation type.
base::trace_event::MemoryAllocatorDumpGUID guid(alloc_->GetGUIDString());

// From this global ID we can generate the parent allocator dump.
base::trace_event::MemoryAllocatorDump* target_mad =
    process_memory_dump->CreateSharedGlobalAllocatorDump(guid);

// We now create an ownership edge from the source dump to the target dump.
// When creating an edge, you can assign an importance to this edge. If all
// edges have the same importance, the size of the allocation will be split
// between all sources which create a dump for the allocation. If one
// edge has higher importance than the others, its source will be assigned the
// full size of the allocation.
const int kImportance = 1;
process_memory_dump->AddOwnershipEdge(
    source_mad->guid(), target_mad->guid(), kImportance);
```

If an allocation is being shared across process boundaries, it may be useful to
generate a global ID which incorporates the ID of the local process, preventing
two processes from generating colliding IDs. As it is not recommended to pass a
process ID between processes for security reasons, a function
`MemoryDumpManager::GetTracingProcessId` is provided which generates a unique ID
per process that can be passed with the resource without security concerns.
Frequently this ID is used to generate a global ID that is based on the
allocated resource's ID combined with the allocating process' tracing ID.

## Suballocations

Another advanced use case involves tracking sub-allocations of a larger
allocation. For instance, this is used in
[`gpu::gles2::TextureManager`][texture-manager] to dump both the suballocations
which make up a texture. To create a suballocation, instead of calling
[`ProcessMemoryDump::CreateAllocatorDump`][pmd] to create a
[`MemoryAllocatorDump`][mem-alloc-dump], you call
[`ProcessMemoryDump::AddSubAllocation`][pmd], providing the ID of the parent
allocation as the first parameter.

[texture-manager]: https://chromium.googlesource.com/chromium/src/+/main/gpu/command_buffer/service/texture_manager.cc
