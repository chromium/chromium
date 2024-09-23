# Video Decoder tests

The video decoder tests are a set of tests used to validate various video
decoder implementations. Multiple scenarios are tested, and the resulting
decoded frames are validated against known checksums. These tests run directly
on top of the video decoder implementation, and don't require the full Chrome
browser stack. They can be very useful when adding support for a new codec or
platform, or to make sure code changes don't break existing functionality. They
are build on top of the
[GoogleTest](https://github.com/google/googletest/blob/main/README.md)
framework.

[TOC]

## Running from Tast
The Tast framework provides an easy way to run the video decoder tests from a
ChromeOS chroot. Test data is automatically deployed to the device being tested.
To run all video decoder tests use:

    tast run $HOST video.ChromeStackDecoder*

Wildcards can be used to run specific sets of tests:
* Run all VP8 tests: `tast run $HOST video.ChromeStackDecoder.vp8*`
* Run all VP9 profile 2 tests: `tast run $HOST video.ChromeStackDecoder.vp9_2*`

Check the
[tast video folder](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/refs/heads/main/src/go.chromium.org/tast-tests/cros/local/bundles/cros/video/)
for a list of all available tests.
See the
[Tast quickstart guide](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/quickstart.md)
for more information about the Tast framework.

## Running manually
To run the video decoder tests manually the _video_decode_accelerator_tests_
target needs to be built and deployed to the device being tested. Running
the video decoder tests can be done by executing:

    ./video_decode_accelerator_tests [<video path>] [<video metadata path>]

e.g.: `./video_decode_accelerator_tests test-25fps.h264`

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
dimensions, number of frames and a list of md5 frame checksums to validate
decoded frames. These frame checksums can be generated using ffmpeg, e.g.:
`ffmpeg -i test-25fps.h264 -f framemd5 test-25fps.h264.frames.md5`.

## Command line options
Multiple command line arguments can be given to the command:

    -v                    enable verbose mode, e.g. -v=2.
    --vmodule             enable verbose mode for the specified module,
                          e.g. --vmodule=*media/gpu*=2.

    --validator_type      validate decoded frames, possible values are
                          md5 (default, compare against md5hash of expected
                          frames), ssim (compute SSIM against expected
                          frames, currently allowed for AV1 streams only)
                          and none (disable frame validation).
    --use-legacy          use the legacy VDA-based video decoders.
    --use_vd_vda          use the new VD-based video decoders with a
                          wrapper that translates to the VDA interface,
                          used to test interaction with older components
    --linear_output       use linear buffers as the final output of the
                          decoder which may require the use of an image
                          processor internally. This flag only works in
                          conjunction with --use_vd_vda.
                          Disabled by default.
    --output_frames       write the selected video frames to disk, possible
                          values are "all|corrupt".
    --output_format       set the format of frames saved to disk, supported
                          formats are "png" (default) and "yuv".
    --output_limit        limit the number of frames saved to disk.
    --output_folder       set the folder used to store frames, defaults to
                          "<testname>".
    --use-gl              specify which GPU backend to use, possible values
                          include desktop (GLX), egl (GLES w/ ANGLE), and
                          swiftshader (software rendering)
    --ozone-platform      specify which Ozone platform to use, possible values
                          depend on build configuration but normally include
                          x11, drm, wayland, and headless

    --gtest_help          display the gtest help and exit.
    --help                display this help and exit.

## Source code
See the video decoder tests [source code](https://cs.chromium.org/chromium/src/media/gpu/test/video_decode_accelerator_tests.cc).
