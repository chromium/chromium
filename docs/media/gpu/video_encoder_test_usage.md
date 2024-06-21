# Video Encoder tests
The video encoder tests are a set of tests that validate various video encoding
scenarios. They are accompanied by the video encoder performance tests that can
be used to measure a video encoder's performance.

These tests run directly on top of the video encoder implementation, and
don't require the full Chrome browser stack. They are built on top of the
[GoogleTest](https://github.com/google/googletest/blob/main/README.md)
framework.

[TOC]

## Running from Tast
The Tast framework provides an easy way to run the video encoder tests from a
ChromeOS chroot. Test data is automatically deployed to the device being tested.
To run all video encoder tests use:

    tast run $HOST video.EncodeAccel.*

Wildcards can be used to run specific sets of tests:
* Run all VP8 tests: `tast run $HOST video.EncodeAccel.vp8*`

Check the
[tast video folder](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/refs/heads/main/src/go.chromium.org/tast-tests/cros/local/bundles/cros/video/)
for a list of all available tests.
See the
[Tast quickstart guide](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/quickstart.md)
for more information about the Tast framework.

## Running manually
To run the video encoder tests manually the _video_encode_accelerator_tests_
target needs to be built and deployed to the device being tested. Running
the video encoder tests can be done by executing:

    ./video_encode_accelerator_tests [<video path>]

e.g.: `./video_encode_accelerator_tests bear_320x192_40frames.yuv.webm`

Running the video encoder performance tests can be done in a smilar way by
building, deploying and executing the _video_encode_accelerator_perf_tests_
target.

    ./video_encode_accelerator_perf_tests [<video path>]

e.g.: `./video_encode_accelerator_perf_tests bear_320x192_40frames.yuv.webm`

__Test videos:__ Various test videos are present in the
[_media/test/data_](https://cs.chromium.org/chromium/src/media/test/data/)
folder in Chromium's source tree (e.g.
[_bear_320x192_40frames.yuv.webm_](https://cs.chromium.org/chromium/src/media/test/data/bear_320x192_40frames.yuv.webm)).
These videos are stored in compressed format and extracted at the start of each
test run, as storing uncompressed videos requires a lot of disk space. Currently
only VP9 or uncompressed videos are supported as test input. If no video is
specified _bear_320x192_40frames.yuv.webm_ will be used.

__Video Metadata:__ These videos also have an accompanying metadata _.json_ file
that needs to be deployed alongside the test video. The video metadata file is a
simple json file that contains info about the video such as its pixel format,
dimensions, framerate and number of frames. These can also be found in the
_media/test/data_ folder (e.g.
[_bear_320x192_40frames.yuv.webm.json_](https://cs.chromium.org/chromium/src/media/test/data/bear_320x192_40frames.yuv.webm.json)).
The metadata file must be _\<video path\>.json_ in the same directory.

## Command line options
Multiple command line arguments can be given to the command:

    --codec              codec profile to encode, "h264 (baseline)",
                         "h264main, "h264high", "vp8" and "vp9"
    --output_folder      overwrite the output folder used to store
                         performance metrics, if not specified results
                         will be stored in the current working directory.

     -v                  enable verbose mode, e.g. -v=2.
    --vmodule            enable verbose mode for the specified module,
                         e.g. --vmodule=*media/gpu*=2.

    --gtest_help         display the gtest help and exit.
    --help               display this help and exit.

Non-performance tests only:

    --num_temporal_layers the number of temporal layers of the encoded
                          bitstream. Only used in --codec=vp9 currently.
    --disable_validator   disable validation of encoded bitstream.
    --output_bitstream    save the output bitstream in either H264 AnnexB
                          format (for H264) or IVF format (for vp8 and
                          vp9) to <output_folder>/<testname>.
    --output_images       in addition to saving the full encoded,
                          bitstream it's also possible to dump individual
                          frames to <output_folder>/<testname>, possible
                          values are \"all|corrupt\"
    --output_format       set the format of images saved to disk,
                          supported formats are \"png\" (default) and
                          \"yuv\".
    --output_limit        limit the number of images saved to disk.

## Source code
See the video encoder tests [source code](https://cs.chromium.org/chromium/src/media/gpu/test/video_encode_accelerator_tests.cc).
See the video encoder performance tests [source code](https://cs.chromium.org/chromium/src/media/gpu/test/video_encode_accelerator_perf_tests.cc).
