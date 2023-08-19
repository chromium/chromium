# //media/gpu

This directory contains `//media` code that runs in the GPU process; this
includes video, audio, and image decoders and encoders.

Common classes shared between platforms are here:

* `AcceleratedVideoDecoder` and its implementations `AV1Decoder`, `H264Decoder`,
  `H265Decoder`, `VP8Decoder`, and `VP9Decoder`.
* `CommandBufferHelper`, which provides utilities for operating inside of a
  renderer's `CommandBuffer` from inside the GPU process.
* `GpuVideoAcceleratorFactory`, which provides `VideoDecodeAccelerator`
  implementations to the IPC layer.

# //media/gpu/ipc

Glue to use `VideoDecodeAccelerator` implementations via
`MojoVideoDecoderService`.

# Platform-specific directories

There are platform-specific directories for `android`, `chromeos`, `mac`, and
`windows`, as well as `v4l2` and `vaapi` which are can be built on chromeos and
linux. (Video decoding on fuchsia does not go through the GPU process.)