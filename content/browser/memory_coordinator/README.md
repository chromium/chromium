# Browser Memory Coordinator

This directory contains the browser-side implementation of the
MemoryCoordinator.

MemoryConsumers from the browser process are grouped by ID in the
`MemoryConsumerRegistry`. MemoryConsumers from child processes are
represented by a `ChildMemoryConsumerRegistryHost`.

Both components notify the `MemoryCoordinatorPolicyManager` when consumer
groups are added or removed.

`MemoryCoordinatorPolicy` is the base class to apply policy logic to these
registered groups.

`MemoryCoordinatorPolicyManager` tracks all active consumer groups in the
system and aggregates the requests from all `MemoryCoordinatorPolicy`
instances to ensure the most restrictive memory limit is applied.

Take a look at
[memory_consumer.h](/base/memory_coordinator/memory_consumer.h) for more
details on the `MemoryConsumer` API.
