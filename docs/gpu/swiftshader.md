# Using Chromium with SwiftShader

SwiftShader is an open-source high-performance implementation of the Vulkan and OpenGL ES graphics APIs which runs purely on the CPU. Thus no graphics processor (GPU) is required for advanced (3D) graphics.

When requested, Chromium uses SwiftShader in two different ways:

1) **As the OpenGL ES driver**

When Chromium uses SwiftShader as the OpenGL ES driver, Chromium behaves as if it was running a on regular GPU, while actually running on SwiftShader. This allows Chromium to exercise hardware only code paths on GPU-less bots.

2) **As the WebGL fallback**

When Chromium uses SwiftShader as the WebGL fallback, Chromium runs in all software mode and only uses SwiftShader to render WebGL content.

## Automatic SwiftShader WebGL fallback is deprecated

Allowing automatic fallback to WebGL backed by SwiftShader has been deprecated and WebGL context creation will soon fail instead of falling back to SwiftShader. This was done for two primary reasons:
1) SwiftShader is a high security risk due to JIT-ed code running in Chromium's GPU process.
2) Users have a poor experience when falling back from a high-performance GPU-backed WebGL to a CPU-backed implementation. Users have no control over this behavior and it is difficult to describe in bug reports.

SwiftShader is a useful tool for web developers to test their sites on systems that are headless or do not have a supported GPU. This use case will still be supported by opting in but is not intended for running untrusted content.

To opt-in to lower security guarantees and allow SwiftShader for WebGL, run the **chrome** executable with the following command line switch:
>**\-\-enable-unsafe-swiftshader**

During the deprecation period, a warning will appear in the javascript console when a WebGL context is created and backed with SwiftShader. Passing **\-\-enable-unsafe-swiftshader** will remove this warning message.

Chromium and other browsers do not guarantee WebGL availability. Please test and handle WebGL context creation failure and fall back to other web APIs such as Canvas2D or an appropriate message to the user.

## Relevant Chromium command line switches

When running the **chrome** executable from the command line, SwiftShader can be enabled using the following Switches:
1) As the OpenGL ES driver, SwANGLE (ANGLE + SwiftShader Vulkan)
>**\-\-use-gl=angle \-\-use-angle=swiftshader**
2) As the **unsafe** WebGL fallback, SwANGLE (ANGLE + SwiftShader Vulkan)
>**\-\-use-gl=angle \-\-use-angle=swiftshader-webgl \-\-enable-unsafe-swiftshader***
3) As the Vulkan driver (requires the [enable_swiftshader_vulkan](https://source.chromium.org/chromium/chromium/src/+/main:gpu/vulkan/features.gni;l=16) feature)
>**--use-vulkan=swiftshader**
