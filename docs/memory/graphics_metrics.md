# Graphics Memory Metrics

The intent of this page is to provide more context and pointers to assess the
cost of Chromium's rendering. It is also meant to be used to complement to
warning in `//tools/metrics/histograms/metadata/memory/histograms.xml`.

## GPU Process and Private Memory Footprint

**tl;dr:** Memory.Gpu.PrivateMemoryFootprint does **not** reflect memory used by
graphics buffers on all platforms. A rough breakdown of platforms follows:

- **Android (including Android WebView), Linux, Chrome OS:** None of the
  graphics buffers are accounted for. Only CPU-side memory is reflected.
- **Windows:** Graphics memory may be partially or fully accounted for.
- **macOS, Intel CPUs:** Graphics memory may be partially or fully accounted for.
- **macOS, ARM64:** We believe that graphics buffers are fully accounted for.

In summary, ARM64 macOS is the only platform where we are reasonably confident
that all memory allocations are properly reflected in
Memory.Gpu.PrivateMemoryFootprint.

**Caution:** Rendering is heavily platform-specific. Do not assume that changes
impacting a platform are reflected on another one. Even for macOS, there are
many hardware, OS and Chromium-level differences. For instance, as of early
2024, rendering is performed using Metal on ARM64, and OpenGL on Intel mac
devices.
