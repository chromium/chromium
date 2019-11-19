# GPU Synchronization in Chrome

Chrome supports multiple mechanisms for sequencing GPU drawing operations, this
document provides a brief overview. The main focus is a high-level explanation
of when synchronization is needed and which mechanism is appropriate.

[TOC]

## Glossary

**GL Sync Object**: Generic GL-level synchronization object that can be in a
"unsignaled" or "signaled" state. The only current implementation of this is a
GL fence.

**GL Fence**: A GL sync object that is inserted into the GL command stream. It
starts out unsignaled and becomes signaled when the GPU reaches this point in the
command stream, implying that all previous commands have completed.

**Client Wait**: Block the client thread until a sync object becomes signaled,
or until a timeout occurs.

**Server Wait**: Tells the GPU to defer executing commands issued after a fence
until the fence signals. The client thread continues executing immediately and
can continue submitting GL commands.

**CHROMIUM fence sync**: A command buffer specific GL fence that sequences
operations among command buffer GL contexts without requiring driver-level
execution of previous commands.

**Native GL Fence**: A GL Fence backed by a platform-specific cross-process
synchronization mechanism.

**GPU Fence Handle**: An IPC-transportable object (typically a file descriptor)
that can be used to duplicate a native GL fence into a different process's
context.

**GPU Fence**: A Chrome abstraction that owns a GPU fence handle representing a
native GL fence, usable for cross-process synchronization.

## Use case overview

The core scenario is synchronizing read and write access to a shared resorce,
for example drawing an image into an offscreen texture and compositing the
result into a final image. The drawing operations need to be completed before
reading to ensure correct output. A typical effect of wrong synchronization is
that the output contains blank or incomplete results instead of the expected
rendered sub-images, causing flickering or tearing.

"Completed" in this case means that the end result of using a resource as input
will be equivalent to waiting for everything to finish rendering, but it does
not necessarily mean that the GPU has fully finished all drawing operations at
that time.

## Single GL context: no synchronization needed

If all access to the shared resource happens in the same GL context, there is no
need for explicit synchronization. GL guarantees that commands are logically
processed in the order they are submitted. This is true both for local GL
contexts (GL calls via ui/gl/ interfaces) and for a single command buffer GL
context.

## Multiple driver-level GL contexts in the same share group: use GLFence

A process can create multiple GL contexts that are part of the same share group.
These contexts can be created in different threads within this process.

In this case, GL fences must be used for sequencing, for example:

1. Context A: draw image, create GLFence
1. Context B: server wait or client wait for GLFence, read image

[gl::GLFence](/ui/gl/gl_fence.h) and its subclasses provide wrappers for
GL/EGL fence handling methods such as `eglFenceSyncKHR` and `eglWaitSyncKHR`.
These fence objects can be used cross-thread as long as both thread's GL
contexts are part of the same share group.

For more details, please refer to the underlying extension documentation, for example:

* https://www.khronos.org/opengl/wiki/Synchronization
* https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_fence_sync.txt
* https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_wait_sync.txt

## Implementation-dependent: same-thread driver-level GL contexts

Many GL driver implementations are based on a per-thread command queue,
with the effect that commands are processed in order even if they were issued
from different contexts on that thread without explicit synchronization.

This behavior is not part of the GL standard, and some driver implementations
use a per-context command queue where this assumption is not true.

See [issue 510232](http://crbug.com/510243#c23) for an example of a problematic
sequence:

```
// In one thread:
MakeCurrent(A);
Render1();
MakeCurrent(B);
Render2();
CreateSync(X);

// And in another thread:
MakeCurrent(C);
WaitSync(X);
Render3();
MakeCurrent(D);
Render4();
```

The only serialization guarantee is that Render2 will complete before Render3,
but Render4 could theoretically complete before Render1.

Chrome assumes that the render steps happen in order Render1, Render2, Render3,
and Render4, and requires this behavior to ensure security. If the driver doesn't
ensure this sequencing, Chrome has to emulate it using virtual contexts. (Or by
using explicit synchronization, but it doesn't do that today.) See also the
"CHROMIUM fence sync" section below.

## Command buffer GL clients: use CHROMIUM sync tokens

Chrome's command buffer IPC interface uses multiple layers. There are multiple
active IPC channels (typically one per process, i.e. one per Renderer and one
for Browser). Each IPC channel has multiple scheduling groups (also called
streams), and each stream can contain multiple command buffers, which in turn
contain a sequence of GL commands.

Command buffers in the same client-side share group must be in the same stream.
Command scheduling granuarity is at the stream level, and a client can choose to
create and use multiple streams with different stream priorities. Stream IDs are
arbitrary integers assigned by the client at creation time, see for example the
[viz::ContextProviderCommandBuffer](/services/viz/public/cpp/gpu/context_provider_command_buffer.h)
constructor.

The CHROMIUM sync token is intended to order operations among command buffer GL
instructions. It inserts an internal fence sync command in the stream, flushing
it appropriately (see below), and generating a sync token from it which is a
cross-context transportable reference to the underlying fence sync. A
WaitSyncTokenCHROMIUM call does **not** ensure that the underlying GL commands
have been executed at the GPU driver level, this mechanism is not suitable for
synchronizing command buffer GL operations with a local driver-level GL context.

See the
[CHROMIUM_sync_point](/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_sync_point.txt)
documentation for details.

Commands issued within a single command buffer don't need to be synchronized
explicitly, they will be executed in the same order that they were issued.

Multiple command buffers within the same stream can use an ordering barrier to
sequence their commands. Sync tokens are not necessary. Example:

```c++
// Command buffers gl1 and gl2 are in the same stream.
Render1(gl1);
gl1->OrderingBarrierCHROMIUM()
Render2(gl2);  // will happen after Render1.
```

Command buffers that are in different streams need to use sync tokens. If both
are using the same IPC channel (i.e. same client process), an unverified sync
token is sufficient, and commands do not need to be flushed to the server:

```c++
// stream A
Render1(glA);
glA->GenUnverifiedSyncTokenCHROMIUM(out_sync_token);

// stream B
glB->WaitSyncTokenCHROMIUM();
Render2(glB);  // will happen after Render1.
```

Command buffers that are using different IPC channels must use verified sync
tokens. Verification is a check that the underlying fence sync was flushed to
the server. Cross-process synchronization always uses verified sync tokens.
`GenSyncTokenCHROMIUM` will force a shallow flush as a side effect if necessary.
Example:

```c++
// IPC channel in process X
Render1(glX);
glX->GenSyncTokenCHROMIUM(out_sync_token);

// IPC channel in process Y
glY->WaitSyncTokenCHROMIUM();
Render2(glY);  // will happen after Render1.
```

Alternatively, unverified sync tokens can be converted to verified ones in bulk
by calling `VerifySyncTokensCHROMIUM`. This will wait for a flush to complete as
necessary. Use this to avoid multiple sequential flushes:

```c++
gl->GenUnverifiedSyncTokenCHROMIUM(out_sync_tokens[0]);
gl->GenUnverifiedSyncTokenCHROMIUM(out_sync_tokens[1]);
gl->VerifySyncTokensCHROMIUM(out_sync_tokens, 2);
```

### Implementation notes

Correctness of the CHROMIUM fence sync mechanism depends on the assumption that
commands issued from the command buffer service side happen in the order they
were issued in that thread. This is handled in different ways:

* Issue a glFlush on switching contexts on platforms where glFlush is sufficient
  to ensure ordering, i.e. MacOS. (This approach would not be well suited to
  tiling GPUs as used on many mobile GPUs where glFlush is an expensive
  operation, it may force content load/store between tile memory and main
  memory.) See for example
  [gl::GLContextCGL::MakeCurrent](/ui/gl/gl_context_cgl.cc):
```c++
  // It's likely we're going to switch OpenGL contexts at this point.
  // Before doing so, if there is a current context, flush it. There
  // are many implicit assumptions of flush ordering between contexts
  // at higher levels, and if a flush isn't performed, OpenGL commands
  // may be issued in unexpected orders, causing flickering and other
  // artifacts.
```

* Force context virtualization so that all commands are issued into a single
  driver-level GL context. This is used on Qualcomm/Adreno chipsets, see [issue
  691102](http://crbug.com/691102).

* Assume per-thread command queues without explicit synchronization. GLX
  effectively ensures this. On Windows, ANGLE uses a single D3D device
  underneath all contexts which ensures strong ordering.

GPU control tasks are processed out of band and are only partially ordered in
respect to GL commands. A gpu_control task always happens before any following
GL commands issued on the same IPC channel. It usually executes before any
preceding unflushed GL commands, but this is not guaranteed. A
`ShallowFlushCHROMIUM` ensures that any following gpu_control tasks will execute
after the flushed GL commands.

In this example, DoTask will execute after GLCommandA and before GLCommandD, but
there is no ordering guarantee relative to CommandB and CommandC:

```c++
  // gles2_implementation.cc

  helper_->GLCommandA();
  ShallowFlushCHROMIUM();

  helper_->GLCommandB();
  helper_->GLCommandC();
  gpu_control_->DoTask();

  helper_->GLCommandD();

  // Execution order is one of:
  //   A | DoTask B C | D
  //   A | B DoTask C | D
  //   A | B C DoTask | D
```

The shallow flush adds the pending GL commands to the service's task queue, and
this task queue is also used by incoming gpu control tasks and processed in
order. The `ShallowFlushCHROMIUM` command returns as soon as the tasks are
queued and does not wait for them to be processed.

## Cross-process transport: GpuFence and GpuFenceHandle

Some platforms such as Android (most devices N and above) and ChromeOS support
synchronizing a native GL context with a command buffer GL context through a
GpuFence.

Use the static `gl::GLFence::IsGpuFenceSupported()` method to check at runtime if
the current platform has support for the GpuFence mechanism including
GpuFenceHandle transport.

The GpuFence mechanism supports two use cases:

* Create a GLFence object in a local context, convert it to a client-side
GpuFence, duplicate it into a command buffer service-side gpu fence, and
issue a server wait on the command buffer service side. That service-side
wait will be unblocked when the *client-side* GpuFence signals.

* Create a new command buffer service-side gpu fence, request a GpuFenceHandle
from it, use this handle to create a native GL fence object in the local
context, then issue a server wait on the local GL fence object. This local
server wait will be unblocked when the *service-side* gpu fence signals.

The [CHROMIUM_gpu_fence
extension](/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_gpu_fence.txt) documents
the GLES API as used through the command buffer interface. This section contains
additional information about the integration with local GL contexts that is
needed to work with these objects.

### Driver-level wrappers

In general, you should use the static `gl::GLFence::CreateForGpuFence()` and
`gl::GLFence::CreateFromGpuFence()` factory methods to create a
platform-specific local fence object instead of using an implementation class
directly.

For Android and ChromeOS, the
[gl::GLFenceAndroidNativeFenceSync](/ui/gl/gl_fence_android_native_fence_sync.h)
implementation wraps the
[EGL_ANDROID_native_fence_sync](https://www.khronos.org/registry/EGL/extensions/ANDROID/EGL_ANDROID_native_fence_sync.txt)
extension that allows creating a special EGLFence object from which a file
descriptor can be extracted, and then creating a duplicate fence object from
that file descriptor that is synchronized with the original fence.

### GpuFence and GpuFenceHandle

A [gfx::GpuFence](/ui/gfx/gpu_fence.h) object owns a GPU fence handle
representing a native GL fence. The `AsClientGpuFence` method casts it to a
ClientGpuFence type for use with the [CHROMIUM_gpu_fence
extension](/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_gpu_fence.txt)'s
`CreateClientGpuFenceCHROMIUM` call.

A [gfx::GpuFenceHandle](/ui/gfx/gpu_fence_handle.h) is an IPC-transportable
wrapper for a file descriptor or other underlying primitive object, and is used
to duplicate a native GL fence into another process. It has value semantics and
can be copied multiple times, and then consumed exactly one time. Consumers take
ownership of the underlying resource. Current GpuFenceHandle consumers are:

* The `gfx::GpuFence(gpu_fence_handle)` constructor takes ownership of the
  handle's resources without constructing a local fence.

* The IPC subsystem closes resources after sending. The typical idiom is to call
  `gfx::CloneHandleForIPC(handle)` on a GpuFenceHandle retrieved from a
  scope-lifetime object to create a copied handle that will be owned by the IPC
  subsystem.

### Sample Code

A usage example for two-process synchronization is to sequence access to a
globally shared drawable such as an AHardwareBuffer on Android, where the
writer uses a local GL context and the reader is a command buffer context in
the GPU process. The writer process draws into an AHardwareBuffer-backed
GLImage in the local GL context, then creates a gpu fence to mark the end of
drawing operations:

```c++
    // This example assumes that GpuFence is supported. If not, the application
    // should fall back to a different transport or synchronization method.
    DCHECK(gl::GLFence::IsGpuFenceSupported())

    // ... write to the shared drawable in local context, then create
    // a local fence.
    std::unique_ptr<gl::GLFence> local_fence = gl::GLFence::CreateForGpuFence();

    // Convert to a GpuFence.
    std::unique_ptr<gfx::GpuFence> gpu_fence = local_fence->GetGpuFence();
    // It's ok for local_fence to be destroyed now, the GpuFence remains valid.

    // Create a matching gpu fence on the command buffer context, issue
    // server wait, and destroy it.
    GLuint id = gl->CreateClientGpuFenceCHROMIUM(gpu_fence.AsClientGpuFence());
    // It's ok for gpu_fence to be destroyed now.
    gl->WaitGpuFenceCHROMIUM(id);
    gl->DestroyGpuFenceCHROMIUM(id);

    // ... read from the shared drawable via command buffer. These reads
    // will happen after the local_fence has signalled. The local
    // fence and gpu_fence dn't need to remain alive for this.
```

If a process wants to consume a drawable that was produced through a command
buffer context in the GPU process, the sequence is as follows:

```c++
    // Set up callback that's waiting for the drawable to be ready.
    void callback(std::unique_ptr<gfx::GpuFence> gpu_fence) {
        // Create a local context GL fence from the GpuFence.
        std::unique_ptr<gl::GLFence> local_fence =
            gl::GLFence::CreateFromGpuFence(*gpu_fence);
        local_fence->ServerWait();
        // ... read from the shared drawable in the local context.
    }

    // ... write to the shared drawable via command buffer, then
    // create a gpu fence:
    GLuint id = gl->CreateGpuFenceCHROMIUM();
    context_support->GetGpuFenceHandle(id, base::BindOnce(callback));
    gl->DestroyGpuFenceCHROMIUM(id);
```

It is legal to create the GpuFence on a separate command buffer context instead
of on the command buffer channel that did the drawing operations, but in that
case gl->WaitSyncTokenCHROMIUM() or equivalent must be used to sequence the
operations between the distinct command buffer contexts as usual.
