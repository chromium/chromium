# VA-API

This page documents tracing and debugging the Video Acceleration API (VaAPI or
VA-API) on ChromeOS. VA-API is an open-source library and API specification,
providing access to graphics hardware acceleration capabilities for video and
image processing. VA-API is used on ChromeOS on both Intel and AMD platforms.

[TOC]

VA-API is implemented by a generic `libva` library, developed upstream on
the [VaAPI GitHub repository], from which ChromeOS is a downstream client via
the [libva] package. Several backends are available for it, notably the legacy
[Intel i965], the modern [Intel iHD] and the [AMD].

![](https://i.imgur.com/skS8Ged.png)

[VaAPI GitHub repository]: https://github.com/intel/libva
[libva]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/x11-libs/libva/
[Intel i965]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/x11-libs/libva-intel-driver/
[Intel iHD]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/x11-libs/libva-intel-media-driver/
[AMD]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/media-libs/libva-amdgpu-driver/

## `libva` tracing

The environment variable `LIBVA_TRACE=/path/to/file` can be exposed to libva to
store tracing information (see [va_trace.c]). `libva` will create a number of
e.g. `libva.log.033807.thd-0x00000b25 ` files (one per thread) with a list of
the actions taken and semi-disassembled parameters.

[va_trace.c]: https://github.com/intel/libva/blob/64520e9ec90ed30e016d7c633d746b3bf538c702/va/va_trace.c#L59

### `libva` logging

The environment variable `LIBVA_MESSAGING_LEVEL=0` (or `1` or `2`) can be used
to configure increasing logging verbosity to stdout (see [va.c]). Chromium uses
a level `0` by default in `vaapi_wrapper.cc`.

[va.c]: https://github.com/intel/libva/blob/2ece7099061ba4ea821545c8b6712b5c421c4dea/va/va.c#L194

### Tracing power consumption

Power consumption is available on ChromeOS test/dev images via the command line
binary [`dump_intel_rapl_consumption`]; this tool averages the power
consumption of the four SoC domains over a configurable period of time, usually
a few seconds. These domains are, in the order presented by the tool:

* `pkg`: estimated power consumption of the whole SoC; in particular, this is a
  superset of pp0 and pp1, including all accessory silicon, e.g. video
  processing.
* `pp0`: CPU set.
* `pp1`/`gfx`: Integrated GPU or GPUs.
* `dram`: estimated power consumption of the DRAM, from the bus activity.


`dump_intel_rapl_consumption` results should be a subset and of the same
numerical value as those produced with e.g. `turbostat`. Note that despite the
name, the tool works on AMD platforms as well, since they provide the same type
of measurement registers, albeit a subset of Intel's. Googlers can read more
about this topic under [go/power-consumption-meas-in-intel].

`dump_intel_rapl_consumption` is usually run while a given workload is active
(e.g. a video playback) with an interval larger than a second to smooth out all
kinds of system services that would show up in smaller periods, e.g. WiFi.

```shell
dump_intel_rapl_consumption --interval_ms=2000 --repeat --verbose
```

E.g. on a nocturne main1, the average power consumption while playing back the
first minute of a 1080p VP9 [video], the average consumptions in watts are:

|`pkg` |`pp0` |`pp1`/`gfx` |`dram`|
| ---: | ---: | ---:       | ---: |
| 2.63 | 1.44 | 0.29       | 0.87 |

As can be seen, `pkg` ~= `pp0` + `pp1` + 1W, this extra watt is the cost of all
the associated silicon, e.g. bridges, bus controllers, caches, and the media
processing engine.


[`dump_intel_rapl_consumption`]: https://chromium.googlesource.com/chromiumos/platform2/+/main/power_manager/tools/dump_intel_rapl_consumption.cc
[video]: https://commons.wikimedia.org/wiki/File:Big_Buck_Bunny_4K.webm
[go/power-consumption-meas-in-intel]: http://go/power-consumption-meas-in-intel

## Tracing VaAPI video decoding (**LEGACY VDA API**)

A simplified diagram of the buffer circulation is provided below. The "client"
is always a Renderer process via a Mojo/IPC communication. Essentially the VaAPI
Video Decode Accelerator ([VaVDA]) receives encoded BitstreamBuffers from the
"client", and sends them to the "va internals", which eventually produces
decoded video in PictureBuffers. The VaVDA may or may not use the `Vpp` unit for
pixel format adaptation, depending on the codec used, silicon generation and
other specifics.

```
      K BitstreamBuffers   +-----+    +-------------------+
 C   --------------------->| Va  | ----->                 |
 L   <---------------------| VDA | <----     va internals |
 I      (encoded stuff)    |     |    |                   |
 E                         |     |    | +-----+       +----+
 N   <---------------------|     | <----|     |<------| lib|
 T   --------------------->|     | ---->| Vpp |------>| va |
                 N         +-----+    +-+-----+   M   +----+
           PictureBuffers                      VASurfaces
           (decoded stuff)
```
*** aside
PictureBuffers are created by the "client" but allocated and filled in by the
VaVDA. `K` is unrelated to both `M` and `N`.
***

[VaVDA]: https://cs.chromium.org/chromium/src/media/gpu/vaapi/vaapi_video_decode_accelerator.h?type=cs&q=vaapivideodecodeaccelerator&sq=package:chromium&g=0&l=57

### Tracing memory consumption

Tracing memory consumption is done via the [MemoryInfra] system. Please take a
minute and read that document (in particular the [difference between
`effective_size` and `size`]).  The VaAPI lives inside the GPU process (a.k.a.
Viz process), so please familiarize yourself with the [GPU Memory Tracing]
document. The VaVDA provides information by implementing the [Memory Dump
Provider] interface, but the information provided varies with the executing mode
as explained next.

#### Internal VASurfaces accountancy

The usage of the `Vpp` unit is controlled by the member variable
[`|decode_using_client_picture_buffers_|`] and is very advantageous in terms of
CPU, power and memory consumption (see [crbug.com/822346]).

* When [`|decode_using_client_picture_buffers_|`] is false, `libva` uses a set
  of internally allocated VASurfaces that are accounted for in the
  `gpu/vaapi/decoder` tracing category (see screenshot below). Each of these
  VASurfaces is backed by a Buffer Object large enough to hold, at least, the
  decoded image in YUV semiplanar format. In the diagram above, `M` varies: 4
  for VP8, 9 for VP9, 4-12 for H264/AVC1 (see [`GetNumReferenceFrames()`]).

![](https://i.imgur.com/UWAuAli.png)

* When [`|decode_using_client_picture_buffers_|`] is true, `libva` can decode
  directly on the client's PictureBuffers, `M = 0`, and the `gpu/vaapi/decoder`
  category is not present in the GPU MemoryInfra.

[MemoryInfra]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/memory-infra/README.md#memoryinfra
[difference between `effective_size` and `size`]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/memory-infra#effective_size-vs_size
[GPU Memory Tracing]: ../memory-infra/probe-gpu.md
[Memory Dump Provider]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/memory-infra/adding_memory_infra_tracing.md
[`|decode_using_client_picture_buffers_|`]: https://cs.chromium.org/search/?q=decode_using_client_picture_buffers_&sq=package:chromium&type=cs
[crbug.com/822346]: https://crbug.com/822346
[`GetNumReferenceFrames()`]: https://cs.chromium.org/search/?q=GetNumReferenceFrames+file:%5Esrc/media/gpu/+package:%5Echromium$+file:%5C.cc&type=cs

#### PictureBuffers accountancy

VaVDA allocates storage for the N PictureBuffers provided by the client by means
of VaapiPicture{NativePixmapOzone}s, backed by NativePixmaps, themselves backed
by DmaBufs (the client only knows about the client Texture IDs). The GPU's
TextureManager accounts for these textures, but:
- They are not correctly identified as being backed by NativePixmaps (see
  [crbug.com/514914]).
- They are not correctly linked back to the Renderer or ARC++ client on behalf
  of whom the allocation took place, like e.g. [the probe-gpu example] (see
  [crbug.com/721674]).

See e.g. the following ToT example for 10 1920x1080p textures (32bpp); finding
the desired `context_group` can be tricky.

![](https://i.imgur.com/3tJThzL.png)

[crbug.com/514914]: https://crbug.com/514914
[the probe-gpu example]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/memory-infra/probe-gpu.md#example
[crbug.com/721674]: https://crbug.com/721674

### Tracing CPU cycles and instantaneous buffer usage

TODO(mcasas): fill in this section.

## Verifying VaAPI installation and usage

### <a name="verify-driver"></a> Verify the VaAPI is correctly installed and can be loaded

`vainfo` is a small command line utility used to enumerate the supported
operation modes; it's developed in the [libva-utils] repository, but more
concretely available on ChromeOS dev images ([media-video/libva-utils] package)
and under Debian systems ([vainfo]). `vainfo` will try to load the appropriate
backend driver for the system and/or GPUs and fail if it cannot find/load it.

[libva-utils]: https://github.com/intel/libva-utils
[media-video/libva-utils]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/media-video/libva-utils
[vainfo]: https://packages.debian.org/sid/main/vainfo

### <a name="verify-vaapi"></a> Verify the VaAPI supports and/or uses a given codec

A few steps are customary to verify the support and use of a given codec.

To verify that the build and platform supports video acceleration, launch
Chromium and navigate to `chrome://gpu`, then:
* Search for the "Video Acceleration Information" Section: this should
   enumerate the available accelerated codecs and resolutions.
* If this section is empty, oftentimes the "Log Messages" Section immediately
  below might indicate an associated error, e.g.:

    > vaInitialize failed: unknown libva error

  that can usually be reproduced with `vainfo`, see the [previous
  section](#verify-driver).

To verify that a given video is being played back using the accelerated video
decoding backend:
* Navigate to a url that causes a video to be played. Leave it playing.
* Navigate to the `chrome://media-internals` tab.
 * Find the entry associated to the video-playing tab.
 * Scroll down to "`Player Properties`" and check the "`video_decoder`" entry:
   it should say "GpuVideoDecoder".

### VaAPI on Linux

This configuration is **unsupported** (see [docs/linux/hw_video_decode.md]), the
following instructions are provided only as a reference for developers to test
the code paths on a Linux machine.

* Follow the instructions under the [Linux build setup] document, adding the GN
  argument `use_vaapi=true` in the args.gn file (please refer to the [Setting up
  the build]) Section).
* To support proprietary codecs such as, e.g. H264/AVC1, add the options
  `proprietary_codecs = true` and `ffmpeg_branding = "Chrome"` to the GN args.
* Build Chromium as usual.

At this point you should make sure the appropriate VA driver backend is working
correctly; try running `vainfo` from the command line and verify no errors show
up.

To run Chromium using VaAPI three arguments are necessary:
* `--enable-features=VaapiVideoDecoder`
* `--ignore-gpu-blocklist`
* `--use-gl=desktop` or `--use-gl=egl`

```shell
./out/gn/chrome --ignore-gpu-blocklist --use-gl=egl
```

Note that you can set the environment variable `MESA_GLSL_CACHE_DISABLE=false`
if you want the gpu process to run in sandboxed mode, see
[crbug.com/264818](https://crbug.com/264818). To check if the running gpu
process is sandboxed or not, just open `chrome://gpu` and search for
`Sandboxed` in the driver information table. In addition, passing
`--gpu-sandbox-failures-fatal=yes` will prevent the gpu process to run in
non-sandboxed mode.

Refer to the [previous section](#verify-vaapi) to verify support and use of
the VaAPI.

[docs/linux/hw_video_decode.md]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/hw_video_decode.md
[Linux build setup]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md
[Setting up the build]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md#setting-up-the-build
