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

## `libva` logging

The environment variable `LIBVA_MESSAGING_LEVEL=0` (or `1` or `2`) can be used
to configure increasing logging verbosity to stdout (see [va.c]). Chromium uses
a level `0` by default in `vaapi_wrapper.cc`.

[va.c]: https://github.com/intel/libva/blob/2ece7099061ba4ea821545c8b6712b5c421c4dea/va/va.c#L194

## Tracing power consumption

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

## VaAPI on Linux

VA-API on Linux is not supported, but it can be enabled using the flags below,
and it might work on certain configurations -- but there's no guarantees.

* To support proprietary codecs such as, e.g. H264/AVC1, add the options
  `proprietary_codecs = true` and `ffmpeg_branding = "Chrome"` to the GN args
  (please refer to the [Setting up the build] Section).
* Build Chromium as usual.

At this point you should make sure the appropriate VA driver backend is working
correctly; try running `vainfo` from the command line and verify no errors show
up, see the [previous section](#verify-driver).

The following feature switch controls video encoding (see [media
switches](https://source.chromium.org/chromium/chromium/src/+/main:media/base/media_switches.cc)
for more details):
* `--enable-features=AcceleratedVideoEncoder`

The following two arguments are optional:
* `--ignore-gpu-blocklist`
* `--disable-gpu-driver-bug-workaround`

The following feature can improve performance when using EGL/Wayland:
* `--enable-features=AcceleratedVideoDecodeLinuxZeroCopyGL`

The NVIDIA VaAPI drivers are known to not support Chromium (see
[crbug.com/1492880](https://crbug.com/1492880)). This feature switch is
provided for developers to test VaAPI drivers on NVIDIA GPUs:
* `--enable-features=VaapiOnNvidiaGPUs`, disabled by default

### VaAPI on Linux with OpenGL

```shell
./out/gn/chrome --use-gl=angle --use-angle=gl \
--enable-features=AcceleratedVideoEncoder,AcceleratedVideoDecodeLinuxGL,VaapiOnNvidiaGPUs \
--ignore-gpu-blocklist --disable-gpu-driver-bug-workaround
```

### VaAPI on Linux with Vulkan

```shell
./out/gn/chrome --use-gl=angle --use-angle=vulkan \
--enable-features=AcceleratedVideoEncoder,VaapiOnNvidiaGPUs,VaapiIgnoreDriverChecks,Vulkan,DefaultANGLEVulkan,VulkanFromANGLE \
--ignore-gpu-blocklist --disable-gpu-driver-bug-workaround
```

Refer to the [previous section](#verify-vaapi) to verify support and use of the VaAPI.

[Setting up the build]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md#setting-up-the-build

