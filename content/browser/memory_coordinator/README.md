# Browser Memory Coordinator

This directory contains the browser-side implementation of the
MemoryCoordinator.

MemoryConsumers from the browser process are directly registered with
`BrowserMemoryConsumerRegistry`. MemoryConsumers from child processes are
indirectly registered by `ChildMemoryConsumerRegistryHost`.

`MemoryCoordinatorPolicy` is the base class to apply policy logic to registered
MemoryConsumers.

`MemoryCoordinatorPolicyManager` aggregates the requests from all
MemoryCoordinatorPolicy instances to ensure the lowest memory limit is applied.

Take a look at
[memory_consumer.h](/base/memory_coordinator/memory_consumer.h) for more
details on the `MemoryConsumer` API.
