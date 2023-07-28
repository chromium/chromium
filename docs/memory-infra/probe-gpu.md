# GPU Memory Tracing

This is an overview of the GPU column in [MemoryInfra][memory-infra].

[TOC]

## Quick Start

If you want an overview of total GPU memory usage, select the GPU process' GPU
category and look at the _size_ column. (Not _effective size_.)

![Look at the size column for total GPU memory][gpu-size-column]

[memory-infra]:    README.md
[gpu-size-column]: https://storage.googleapis.com/chromium-docs.appspot.com/c7d632c18d90d99e393ad0ade929f96e7d8243fe

## In Depth

GPU Memory in Chrome involves several different types of allocations. These
include, but are not limited to:

 * **Raw OpenGL Objects**: These objects are allocated by Chrome using the
   OpenGL API. Chrome itself has handles to these objects, but the actual
   backing memory may live in a variety of places (CPU side in the GPU process,
   CPU side in the kernel, GPU side). Because most OpenGL operations occur over
   IPC, communicating with Chrome's GPU process, these allocations are almost
   always shared between a renderer or browser process and the GPU process.
 * **GPU Memory Buffers**: These objects provide a chunk of writable memory
   which can be handed off cross-process. While GPUMemoryBuffers represent a
   platform-independent way to access this memory, they have a number of
   possible platform-specific implementations (EGL surfaces on Linux,
   IOSurfaces on Mac, or CPU side shared memory). Because of their cross
   process use case, these objects will almost always be shared between a
   renderer or browser process and the GPU process.
 * **SharedImages**: SharedImages are a platform-independent abstraction around GPU
   memory, similar to GPU Memory Buffers. In many cases, SharedImages are created
   from GPUMemoryBuffers.

GPU Memory can be found across a number of different processes, in a few
different categories.

Renderer or browser process:

 * **CC Category**: The CC category contains all resource allocations used in
   the Chrome Compositor. When GPU rasterization is enabled, these resource
   allocations will be GPU allocations as well. See also
   [docs/memory-infra/probe-cc.md][cc-memory].
 * **Skia/gpu_resources Category**: All GPU resources used by Skia.
 * **GPUMemoryBuffer Category**: All GPUMemoryBuffers in use in the current
   process.

GPU process:

 * **GPU Category**: All GPU allocations, many shared with other processes.
 * **GPUMemoryBuffer Category**: All GPUMemoryBuffers.

## Example

Many of the objects listed above are shared between multiple processes.
Consider a GL texture used by CC --- this texture is shared between a renderer
and the GPU process. Additionally, the texture may be backed by a SharedImage
which was created from a GPUMemoryBuffer, which is also shared between the
renderer and GPU process. This means that the single texture may show up in the
memory logs of two different processes multiple times.

To make things easier to understand, each GPU allocation is only ever "owned"
by a single process and category. For instance, in the above example, the
texture would be owned by the CC category of the renderer process. Each
allocation has (at least) two sizes recorded --- _size_ and _effective size_.
In the owning allocation, these two numbers will match:

![Matching size and effective size][owner-size]

Note that the allocation also gives information on what other processes it is
shared with (seen by hovering over the green arrow). If we navigate to the
other allocation (in this case, gpu/gl/textures/client_25/texture_216) we will
see a non-owning allocation. In this allocation the size is the same, but the
_effective size_ is 0:

![Effective size of zero][non-owner-size]

Other types, such as GPUMemoryBuffers and SharedImages have similar sharing
patterns.

When trying to get an overview of the absolute memory usage tied to the GPU,
you can look at the size column (not effective size) of just the GPU process'
GPU category. This will show all GPU allocations, whether or not they are owned
by another process.

[cc-memory]:      /docs/memory-infra/probe-cc.md
[owner-size]:     https://storage.googleapis.com/chromium-docs.appspot.com/a325c4426422e53394a322d31b652cfa34231189
[non-owner-size]: https://storage.googleapis.com/chromium-docs.appspot.com/b8cf464636940d0925f29a102e99aabb9af40b13
