## GPU Process Fallback

When the GPU process crashes repeatedly, usually 3 crashes in a short timeframe,
a less desirable but hopefully more stable mode will be chosen for the next
startup. Where available, we want to use Vulkan over GL, and GL over software.
Vulkan is more experimental than GL and doesn't have the same level of support
or maturity. Software rendering should be more stable than accelerated graphics
because it doesn't rely on third party drivers, or on graphics hardware outside
our control.


### Crash Counting

GPU process crashes are counted in `GpuProcessHost::RecordProcessCrash()`.
Occasional crashes are permitted without triggering the fallback behaviour,
so one crash is forgiven per interval elapsed since the last crash. For example,
if the GPU crashes for a second time after N minutes, the previous crash is
forgiven. This interval is determined by `GetForgiveMinutes()`. If the crash
limit is hit, then fallback occurs.

The crash limit, determined by `kGpuFallbackCrashCount`, is higher on Android
and ChromeOS because these platforms don't support software rendering.
Additionally, on Android, the OS can kill the GPU process arbitrarily. So the
crash tolerance is a little bit wider here.


### Fallback Order

The order of GPU process modes is determined at startup based on the platform,
command line switches, and enabled features. This is then stored as a stack in
`GpuDataManagerImplPrivate::fallback_modes_`.

For example, on Linux with all options available, the stack of GPU modes will
look like this after initialization:

    +--------------------+
    | HARDWARE_VULKAN    |  <--
    +--------------------+
    | HARDWARE_GL        |
    +--------------------+
    | SWIFTSHADER        |
    +--------------------+
    | DISPLAY_COMPOSITOR |
    +--------------------+

After the order is determined, or when fallback occurs, the top element from the
stack is popped and used as the next GPU mode. If the stack is empty when
fallback occurs, then the GPU process is too unstable to use, and the Browser
process intentionally crashes.


### GPU Modes


#### `HARDWARE_GL`

The GPU process is running with OpenGL hardware acceleration enabled.


#### `HARDWARE_METAL` and `HARDWARE_VULKAN`

The GPU process is running with OpenGL hardware acceleration enabled, as well as
Metal or Vulkan.

This doesn't necessarily determine what Metal or Vulkan are being used for, just
that they are initialized. In particular, for Vulkan, `--enable-features=Vulkan`
will cause Vulkan to be used for compositing and rasterization, whereas
`--use-vulkan` by itself will only initialize Vulkan so that it can be used for
other purposes, such as WebGPU.


#### `SWIFTSHADER`

The GPU process is running with hardware acceleration disabled, but SwiftShader
will be initialized for software-backed WebGL.


#### `DISPLAY_COMPOSITOR`

The GPU process is running for the display compositor only, no acceleration is
enabled.


### Special Cases

There are a few platforms that expect hardware acceleration, with some
exceptions for certain circumstances.


#### Android Chromecast Audio-Only

Android requires hardware acceleration, except in the case of Chromecast
audio-only builds. These run with the flag `--disable-gpu`, and the GPU process
is launched in `DISPLAY_COMPOSITOR` mode.


#### Fuchsia

Fuchsia always expects Vulkan to be available, and doesn't support falling back
to using GL or software. For testing purposes, the flag `--disable-gpu` is
allowed, and the GPU process will be launched in `SWIFTSHADER` mode.
