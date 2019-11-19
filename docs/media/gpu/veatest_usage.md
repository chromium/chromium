# Using the Video Encode Accelerator Unittests Manually

The VEAtest (or `video_encode_accelerator_unittest`) is a set of unit tests that
embeds the Chrome video encoding stack without requiring the whole browser,
meaning they can work in a headless environment. It includes a variety of tests
to validate the encoding stack with h264, vp8 and vp9.

Running this test manually can be very useful when bringing up a new codec, or
in order to make sure that new code does not break hardware encoding. This
document is a walk though the prerequisites for running this program, as well
as the most common options.

## Prerequisites

The required kernel drivers should be loaded, and there should exist a
`/dev/video-enc0` symbolic link pointing to the encoder device node (e.g.
`/dev/video-enc0` → `/dev/video0`).

The unittests can be built by specifying the `video_encode_accelerator_unittest`
target to `ninja`. If you are building for an ARM board that is not yet
supported by the
[simplechrome](https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md)
workflow, use `arm-generic` as the board. It should work across all ARM targets.

For unlisted Intel boards, any other Intel target (preferably with the same
chipset) should be usable with libva. AMD targets can use `amd64-generic`.

## Basic VEA usage

The VEA test takes raw YUV files in I420 format as input and produces e.g. an
H.264 Annex-B byte stream. Sample raw YUV files can be found at the following
locations:

* [1080 Crowd YUV](http://commondatastorage.googleapis.com/chromiumos-test-assets-public/crowd/crowd1080-96f60dd6ff87ba8b129301a0f36efc58.yuv)
* [320x180 Bear YUV](http://commondatastorage.googleapis.com/chromiumos-test-assets-public/bear/bear-320x180-c60a86c52ba93fa7c5ae4bb3156dfc2a.yuv)

It is recommended to rename these files after downloading them to e.g.
`crowd1080.yuv` and `bear-320x180.yuv`.

The VEA can then be tested as follows:

    ./video_encode_accelerator_unittest --single-process-tests --disable_flush --gtest_filter=SimpleEncode/VideoEncodeAcceleratorTest.TestSimpleEncode/0 --test_stream_data=bear-320x180.yuv:320:180:1:bear.mp4:100000:30

for the `bear` file, and

    ./video_encode_accelerator_unittest --single-process-tests --disable_flush --gtest_filter=SimpleEncode/VideoEncodeAcceleratorTest.TestSimpleEncode/0 --test_stream_data=crowd1080.yuv:1920:1080:1:crowd.mp4:4000000:30

for the larger `crowd` file. These commands will put the encoded output into
`bear.mp4` and `crowd.mp4` respectively. They can then be copied on the host and
played with `mplayer -fps 25`.

## Test filtering options

`./video_encode_accelerator_unittest --help` will list all valid options.

The list of available tests can be retrieved using the `--gtest_list_tests`
option.

By default, all tests are run, which can be a bit too much, especially when
bringing up a new codec. The `--gtest_filter` option can be used to specify a
pattern of test names to run.

## Verbosity options

The `--vmodule` options allows to specify a set of source files that should be
more verbose about what they are doing. For basic usage, a useful set of vmodule
options could be:

    --vmodule=*/media/gpu/*=4

## Source code

The VEAtest's source code can be consulted here: [https://cs.chromium.org/chromium/src/media/gpu/video_encode_accelerator_unittest.cc](https://cs.chromium.org/chromium/src/media/gpu/video_encode_accelerator_unittest.cc).

V4L2 support: [https://cs.chromium.org/chromium/src/media/gpu/v4l2/](https://cs.chromium.org/chromium/src/media/gpu/v4l2/).

VAAPI support: [https://cs.chromium.org/chromium/src/media/gpu/vaapi/](https://cs.chromium.org/chromium/src/media/gpu/vaapi/).
