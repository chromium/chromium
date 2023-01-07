# CHROMIUM Sync Token Internals

Chrome uses a mechanism known as "sync tokens" to synchronize different command
buffers in the GPU process. This document discusses the internals of the sync
token system.

[TOC]

## Rationale

In Chrome, multiple processes, for example browser and renderer, submit work to
the GPU process asynchronously in command buffer. However, there are
dependencies between the work submitted by different processes, such as
SkiaRenderer in the display compositor in the GPU process rendering a tile
produced by the raster worker in the renderer process.

Sync tokens are used to synchronize the work contained in command buffers
without waiting for the work to complete. This improves pipelining, and with the
introduction of GPU scheduling, allows prioritization of work. Although
originally built for synchronizing command buffers, they can be used for other
work in the GPU process.

## Generation

Sync tokens are represented by a namespace, identifier, and the *fence release
count*. `CommandBufferId` is a 64-bit unsigned integer which is unique within a
`CommandBufferNamespace`. For example IPC command buffers are in the *GPU_IO*
CommandBufferNamespace, and are identified by CommandBufferId with process id as
the MSB and IPC route id as the LSB.

The fence release count marks completion of some work in a command buffer. Note:
this is CPU side work done that includes command decoding, validation, issuing
GL calls to the driver, etc. and not GPU side work. See
[gpu_synchronication.md](/docs/design/gpu_synchronization.md) for more
information about synchronizing GPU work.

Fences are typically generated or inserted on the client using a sequential
counter. The corresponding GL API is `GenSyncTokenCHROMIUM` which generates the
fence using `CommandBufferProxyImpl::GenerateFenceSyncRelease()`, and also adds
the fence to the command buffer using the internal `InsertFenceSync` command.

## Verification

Different client processes communicate with the GPU process using *channels*. A
channel wraps around a message pipe which doesn't provide ordering guarantees
with respect to other pipes. For example, a message from the browser process
containing a sync token wait can arrive before the message from the renderer
process that releases or fulfills the sync token promise.

To prevent the above problem, client processes must verify sync tokens before
sending to another process. Verification involves a synchronous nop IPC message,
`GpuChannelMsg_Nop`, to the GPU process which ensures that the GPU process has
read previous messages from the pipe.

Sync tokens used within a process do not need to be verified, and the
`GenSyncTokenUnverifiedCHROMIUM` GL API serves this common case. These sync
tokens need to be verified using `VerifySyncTokensCHROMIUM`. Sync tokens
generated using `GenSyncTokenCHROMIUM` are already verified. `SyncToken` has a
`verified_flush` bit that guards against accidentally sending unverified sync
tokens over IPC.

## Streams

In the GPU process, command buffers are organized into logical streams of
execution that are called *sequences*. Within a sequence tasks are ordered, but
are asynchronous with respect to tasks in other sequences. Dependencies between
tasks are specified as sync tokens. For IPC command buffers, this implies flush
ordering within a sequence.

A sequence can be created by `Scheduler::CreateSequence` which returns a
`SequenceId`. Tasks are posted to a sequence using `Scheduler::ScheduleTask`.
Typically there is one sequence per channel, but sometimes there are more like
raster, compositor, and media streams in renderer's channel.

The scheduler also provides a means for co-operative scheduling through
`Scheduler::ShouldYield` and `Scheduler::ContinueTask`. These allow a task to
yield and continue once higher priority work is complete. Together with the GPU
scheduler, multiple sequences provide the means for prioritization of UI work
over raster prepaint work.

## Waiting and Completion

Sync tokens are managed in the GPU process by `SyncPointManager`, and its helper
classes `SyncPointOrderData` and `SyncPointClientState`. `SyncPointOrderData`
holds state for a logical stream of execution, typically containing work of
multiple command buffers from one process. `SyncPointClientState` holds sync token
state for a client which generated sync tokens, typically an IPC command buffer.

GPU scheduler maintains a `SyncPointOrderData` per sequence. Clients must create
SyncPointClientState using `SyncPointManager::CreateSyncPointClientState` and
identify their namespace, id, and sequence.

Waiting on a sync token is done by calling `SyncPointManager::Wait()` with a
sync token, order number for the wait, and a callback. The callbacks are
enqueued with the `SyncPointClientState` of the target with the release count of
the sync token. The scheduler does this internally for sync token dependencies
for scheduled tasks, but the wait can also be performed when running the
`WaitSyncTokenCHROMIUM` GL command.

Sync tokens are completed when the fence is released in the GPU process by
calling `SyncPointClientState::ReleaseFenceSync()`. For GL command buffers, the
`InsertFenceSync` command, which contains the release count generated in the
client, calls this when executed in the service. This issues callbacks and
allows waiting command buffers to resume their work.

## Correctness

Correctness of waits and releases basically amounts to checking that there are
no indefinite waits because of broken promises or circular wait chains. This is
ensured by associating an order number with each wait and release and
maintaining the invariant that the order number of release is less than or equal
to the order number of wait.

Each task is assigned a global sequential order number generated by
`SyncPointOrderData::GenerateUnprocessedOrderNumber` which are stored in a queue
of unprocessed order numbers. In `SyncPointManager::Wait()`, the callbacks are
also enqueued with the order number of the waiting task in `SyncPointOrderData`
in a queue called `OrderFenceQueue`.

`SyncPointOrderData` maintains the invariant that all waiting callbacks must
have an order number greater than the sequence's next unprocessed order number.
This invariant is checked when enqueuing a new callback in
`SyncPointOrderData::ValidateReleaseOrderNumber`, and after completing a task in
`SyncPointOrderData::FinishProcessingOrderNumber`.


## See Also

[CHROMIUM_sync_point](/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_sync_point.txt)
[gpu_synchronication.md](/docs/design/gpu_synchronization.md)
[Lightweight GPU Sync Points](https://docs.google.com/document/d/1XwBYFuTcINI84ShNvqifkPREs3sw5NdaKzKqDDxyeHk/edit)
