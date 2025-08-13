# Shared Image Infrastructure

This document provides a high-level overview of the Shared Image system, a core component of Chromium's GPU architecture designed for sharing graphical resources across different parts of the browser and between various graphics APIs.

## Core Components

The Shared Image system is built around four primary components that work together to manage the lifecycle and accessibility of shared graphical memory.

*   **`SharedImageManager`**: The central authority for all shared images. It is the sole owner of the underlying `SharedImageBacking` objects and manages their creation and destruction. All access to shared images is brokered through the manager.

*   **`SharedImageFactory`**: The entry point for creating shared images. Clients request shared images through interface which goes into this factory, specifying desired format, size, and usage flags. The factory's main role is to dispatch the creation request to the most appropriate backing-specific factory based on the platform, hardware capabilities, and intended use.

*   **`SharedImageBacking`**: Represents the actual allocated graphics memory (the "backing store"). This can be a native platform resource like a D3D11 texture or an `AHardwareBuffer`, or a more generic resource like a GL texture or a block of shared memory. The backing is an internal implementation detail, never directly accessed by client code.

*   **`SharedImageRepresentation`**: A lightweight, API-specific "view" into a `SharedImageBacking`. Representations are the means by which clients interact with a shared image's data. For example, a `DawnImageRepresentation` allows a backing to be used as a `wgpu::Texture`, while a `SkiaImageRepresentation` allows it to be drawn to or from using Skia.

## Key Architectural Concepts

### Separation of Storage and Access

The fundamental design principle of the Shared Image system is the strict separation of the underlying memory storage (`SharedImageBacking`) from the API-specific access mechanisms (`SharedImageRepresentation`). This allows a single graphical resource to be accessed by different parts of the code (e.g., the compositor, the Skia renderer, the WebGPU implementation) without needing to know the details of its underlying storage.

### Interoperability Across Graphics APIs

This separation is what enables seamless interoperability. For instance, an image can be created using the Vulkan backend (`ExternalVkImageBackingFactory`) and then be used for rendering in both Skia and Dawn. To achieve this, a client would request a `SkiaImageRepresentation` to draw with Skia, and later a `DawnImageRepresentation` to use the same image as a texture in a WebGPU application. The backing must ensure that access is properly synchronized.

## Platform-Specific Implementations

To leverage native capabilities, the Shared Image system uses specialized backing factories on different operating systems.

*   **Windows**: The `D3DImageBackingFactory` is used to create backings from D3D11 textures. This provides the most efficient path for interoperability with D3D11, D3D12, and DXGI swap chains, which are fundamental to Windows graphics.

*   **Android**: The `AHardwareBufferImageBackingFactory` creates backings that wrap `AHardwareBuffer` objects. `AHardwareBuffer` is Android's native mechanism for sharing buffers between system components, making it ideal for zero-copy sharing with the media decoder, camera, and other hardware.

*   **macOS/iOS**: The `IOSurfaceImageBackingFactory` utilizes `IOSurface` objects, which are the native, optimized buffer-sharing mechanism on Apple platforms.

*   **Linux/ChromeOS (Ozone)**: The `OzoneImageBackingFactory` creates backings from `NativePixmap` objects, integrating with the Ozone platform abstraction layer for windowing and graphics.

## Generic and Fallback Backings

For cross-platform use cases or when a native backing is not suitable, several generic factories are available.

*   **`SharedMemoryImageBackingFactory`**: Creates backings from CPU-accessible shared memory. This is highly versatile but may require an extra upload step to be used on the GPU.

*   **`WrappedSkImageBackingFactory`**: Creates a backing that wraps a Skia-owned resource (`SkImage`). This is useful when no other specialized native factory is needed to satisfy the requested usage flags.

*   **`WrappedGraphiteTextureBacking`**: Creates a backing that holds a `BackendTexture` allocated and managed by Skia's Graphite rendering backend.
