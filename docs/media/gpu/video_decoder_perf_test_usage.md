# Video Decoder performance tests
The video decoder performance tests are a set of tests used to measure the
performance of various video decoder implementations. These tests run directly
on top of the video decoder implementation, and don't require the full Chrome
browser stack. They are build on top of the
[GoogleTest](https://github.com/google/googletest/blob/main/README.md)
framework.

[TOC]

## Running from Tast
The Tast framework provides an easy way to run the video decoder performance
tests from a ChromeOS chroot. Test data is automatically deployed to the device
being tested. To run all video decoder performance tests use:

    tast run $HOST video.ChromeStackDecoderPerf*

Wildcards can be used to run specific sets of tests:
* Run all VP8 performance tests: `tast run $HOST video.ChromeStackDecoderPerf.vp8*`
* Run all 1080p 60fps performance tests:
`tast run $HOST video.ChromeStackDecoderPerf.*_1080p_60fps*`

Check the
[tast video folder](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/refs/heads/main/src/go.chromium.org/tast-tests/cros/local/bundles/cros/video/)
for a list of all available tests.
See the
[Tast quickstart guide](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/quickstart.md)
for more information about the Tast framework.

## Running manually
To run the video decoder performance tests manually the
_video_decode_accelerator_perf_tests_ target needs to be built and deployed to
the device being tested. Running the video decoder performance tests can be done
by executing:

    ./video_decode_accelerator_perf_tests [<video path>] [<video metadata path>]

e.g.: `./video_decode_accelerator_perf_tests test-25fps.h264`

__Test videos:__ Test videos are present for multiple codecs in the
[_media/test/data_](https://cs.chromium.org/chromium/src/media/test/data/)
folder in Chromium's source tree (e.g.
[_test-25fps.vp8_](https://cs.chromium.org/chromium/src/media/test/data/test-25fps.vp8)).
If no video is specified _test-25fps.h264_ will be used.

__Video Metadata:__ These videos also have an accompanying metadata _.json_ file
that needs to be deployed alongside the test video. They can also be found in
the _media/test/data_ folder (e.g.
[_test-25fps.h264.json_](https://cs.chromium.org/chromium/src/media/test/data/test-25fps.h264.json)).
If no metadata file is specified _\<video path\>.json_ will be used. The video
metadata file contains info about the video such as its codec profile,
dimensions and number of frames.

__Note:__ Various CPU scaling and thermal throttling mechanisms might be active
on the device (e.g. _scaling_governor_, _intel_pstate_). As these can influence
test results it's advised to disable them.

## Performance metrics
To measure decoder performance two different test scenarios are used.

__Uncapped decoder performance:__ In this scenario the specified test video is
decoded from start to finish as fast as possible. This test scenario provides an
estimate of the decoder's maximum performance (e.g. the maximum FPS).

__Capped decoder performance:__ This scenario simulates a more realistic
environment by decoding a video from start to finish at its actual frame rate,
while simulating real-time rendering. Frames that are not decoded by the time
they should be rendered will be dropped.

Various performance metrics are collected by these tests:
* FPS: The average number of frames the decoder was able to decode per second.
* Frames Dropped: The number of frames that were dropped during playback, only
relevant for the capped performance test.
* Dropped frame percentage: The percentage of frames dropped, only relevant for
the capped performance test.
* Frame delivery time: The time between subsequent frame deliveries. The average
frame delivery time and 25/50/75 percentiles are calculated.
* Frame decode time: The time between scheduling a frame to be decoded and
getting the decoded frame. This metric provides a measure of the decoder's
latency. The average decode time and 25/50/75 percentiles are calculated.

All performance metrics are written to _perf_metrics/<test_name>.json_.

## Command line options
Multiple command line arguments can be given to the command:

     -v                   enable verbose mode, e.g. -v=2.
    --vmodule             enable verbose mode for the specified module,
                          e.g. --vmodule=*media/gpu*=2.

    --output_folder       overwrite the output folder used to store
                          performance metrics, if not specified results
                          will be stored in the current working directory.
    --use-legacy          use the legacy VDA-based video decoders.
    --use_vd_vda          use the new VD-based video decoders with a
                          wrapper that translates to the VDA interface,
                          used to test interaction with older components
                          expecting the VDA interface.
    --linear_output       use linear buffers as the final output of the
                          decoder which may require the use of an image
                          processor internally. This flag only works in
                          conjunction with --use_vd_vda.
                          Disabled by default.
    --use-gl              specify which GPU backend to use, possible values
                          include desktop (GLX), egl (GLES w/ ANGLE), and
                          swiftshader (software rendering)
    --ozone-platform      specify which Ozone platform to use, possible values
                          depend on build configuration but normally include
                          x11, drm, wayland, and headless

    --gtest_help          display the gtest help and exit.
    --help                display this help and exit.

## Source code
See the video decoder performance tests [source code](https://cs.chromium.org/chromium/src/media/gpu/test/video_decode_accelerator_perf_tests.cc).
