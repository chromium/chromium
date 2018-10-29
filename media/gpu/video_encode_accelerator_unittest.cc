// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/bits.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp8_parser.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/test/video_accelerator_unittest_helpers.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#elif defined(OS_WIN)
#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"
#endif

namespace media {
namespace {

// The absolute differences between original frame and decoded frame usually
// ranges aroud 1 ~ 7. So we pick 10 as an extreme value to detect abnormal
// decoded frames.
const double kDecodeSimilarityThreshold = 10.0;

// Arbitrarily chosen to add some depth to the pipeline.
const unsigned int kNumOutputBuffers = 4;
const unsigned int kNumExtraInputFrames = 4;
// Maximum delay between requesting a keyframe and receiving one, in frames.
// Arbitrarily chosen as a reasonable requirement.
const unsigned int kMaxKeyframeDelay = 4;
// Default initial bitrate.
const uint32_t kDefaultBitrate = 2000000;
// Default ratio of requested_subsequent_bitrate to initial_bitrate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentBitrateRatio = 2.0;
// Default initial framerate.
const uint32_t kDefaultFramerate = 30;
// Default ratio of requested_subsequent_framerate to initial_framerate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentFramerateRatio = 0.1;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
// Minimum required FPS throughput for the basic performance test.
const uint32_t kMinPerfFPS = 30;
// Minimum (arbitrary) number of frames required to enforce bitrate requirements
// over. Streams shorter than this may be too short to realistically require
// an encoder to be able to converge to the requested bitrate over.
// The input stream will be looped as many times as needed in bitrate tests
// to reach at least this number of frames before calculating final bitrate.
const unsigned int kMinFramesForBitrateTests = 300;
// The percentiles to measure for encode latency.
const unsigned int kLoggedLatencyPercentiles[] = {50, 75, 95};
// Timeout for the flush is completed. In the multiple encoder test case, the
// FPS might be lower than expected. Let us assume that the lowest FPS is 5,
// then the period per frame is 200 milliseconds. Here we set the timeout 10x
// periods considering that there might be some pending frames.
const unsigned int kFlushTimeoutMs = 2000;

// The syntax of multiple test streams is:
//  test-stream1;test-stream2;test-stream3
// The syntax of each test stream is:
// "in_filename:width:height:profile:out_filename:requested_bitrate
//  :requested_framerate:requested_subsequent_bitrate
//  :requested_subsequent_framerate:pixel_format"
// Instead of ":", "," can be used as a seperator as well. Note that ":" does
// not work on Windows as it interferes with file paths.
// - |in_filename| is YUV raw stream. Its format must be |pixel_format|
//   (see http://www.fourcc.org/yuv.php#IYUV).
// - |width| and |height| are in pixels.
// - |profile| to encode into (values of VideoCodecProfile).
// - |out_filename| filename to save the encoded stream to (optional). The
//   format for H264 is Annex-B byte stream. The format for VP8 is IVF. Output
//   stream is saved for the simple encode test only. H264 raw stream and IVF
//   can be used as input of VDA unittest. H264 raw stream can be played by
//   "mplayer -fps 25 out.h264" and IVF can be played by mplayer directly.
//   Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
// Further parameters are optional (need to provide preceding positional
// parameters if a specific subsequent parameter is required):
// - |requested_bitrate| requested bitrate in bits per second.
//   Bitrate is only forced for tests that test bitrate.
// - |requested_framerate| requested initial framerate.
// - |requested_subsequent_bitrate| bitrate to switch to in the middle of the
//                                  stream.
// - |requested_subsequent_framerate| framerate to switch to in the middle
//                                    of the stream.
// - |pixel_format| is the VideoPixelFormat of |in_filename|. Users needs to
//   set the value corresponding to the desired format. If it is not specified,
//   this would be PIXEL_FORMAT_I420.

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
const char* g_default_in_filename = "bear_320x192_40frames.yuv";
const base::FilePath::CharType* g_default_in_parameters =
    FILE_PATH_LITERAL(":320:192:1:out.h264:200000");
#elif defined(OS_MACOSX)
// VideoToolbox falls back to SW encoder with resolutions lower than this.
const char* g_default_in_filename = "bear_640x384_40frames.yuv";
const base::FilePath::CharType* g_default_in_parameters =
    FILE_PATH_LITERAL(":640:384:1:out.h264:200000");
#elif defined(OS_WIN)
const char* g_default_in_filename = "bear_320x192_40frames.yuv";
const base::FilePath::CharType* g_default_in_parameters =
    FILE_PATH_LITERAL(",320,192,0,out.h264,200000");
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

// Default params that can be overriden via command line.
std::unique_ptr<base::FilePath::StringType> g_test_stream_data(
    new base::FilePath::StringType(
        media::GetTestDataFilePath(media::g_default_in_filename).value() +
        media::g_default_in_parameters));

base::FilePath g_log_path;

base::FilePath g_frame_stats_path;

bool g_run_at_fps = false;

bool g_needs_encode_latency = false;

bool g_verify_all_output = false;

bool g_fake_encoder = false;

// Environment to store test stream data for all test cases.
class VideoEncodeAcceleratorTestEnvironment;
VideoEncodeAcceleratorTestEnvironment* g_env;

// The number of frames to be encoded. This variable is set by the switch
// "--num_frames_to_encode". Ignored if 0.
int g_num_frames_to_encode = 0;

#ifdef ARCH_CPU_ARMEL
// ARM performs CPU cache management with CPU cache line granularity. We thus
// need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
// Otherwise newer kernels will refuse to accept them, and on older kernels
// we'll be treating ourselves to random corruption.
// Moreover, some hardware codecs require 128-byte alignment for physical
// buffers.
const size_t kPlatformBufferAlignment = 128;
#else
const size_t kPlatformBufferAlignment = 8;
#endif

inline static size_t AlignToPlatformRequirements(size_t value) {
  return base::bits::Align(value, kPlatformBufferAlignment);
}

// An aligned STL allocator.
template <typename T, size_t ByteAlignment>
class AlignedAllocator : public std::allocator<T> {
 public:
  typedef size_t size_type;
  typedef T* pointer;

  template <class T1>
  struct rebind {
    typedef AlignedAllocator<T1, ByteAlignment> other;
  };

  AlignedAllocator() {}
  explicit AlignedAllocator(const AlignedAllocator&) {}
  template <class T1>
  explicit AlignedAllocator(const AlignedAllocator<T1, ByteAlignment>&) {}
  ~AlignedAllocator() {}

  pointer allocate(size_type n, const void* = 0) {
    return static_cast<pointer>(base::AlignedAlloc(n, ByteAlignment));
  }

  void deallocate(pointer p, size_type n) {
    base::AlignedFree(static_cast<void*>(p));
  }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }
};

struct TestStream {
  TestStream()
      : num_frames(0),
        aligned_buffer_size(0),
        requested_bitrate(0),
        requested_framerate(0),
        requested_subsequent_bitrate(0),
        requested_subsequent_framerate(0) {}
  ~TestStream() {}

  VideoPixelFormat pixel_format;
  gfx::Size visible_size;
  gfx::Size coded_size;
  unsigned int num_frames;

  // Original unaligned YUV input file name provided as an argument to the test.
  std::string in_filename;

  // A vector used to prepare aligned input buffers of |in_filename|. This
  // makes sure starting addresses of YUV planes are aligned to
  // kPlatformBufferAlignment bytes.
  std::vector<char, AlignedAllocator<char, kPlatformBufferAlignment>>
      aligned_in_file_data;

  // Byte size of a frame of |aligned_in_file_data|.
  size_t aligned_buffer_size;

  // Byte size for each aligned plane of a frame.
  std::vector<size_t> aligned_plane_size;

  std::string out_filename;
  VideoCodecProfile requested_profile;
  unsigned int requested_bitrate;
  unsigned int requested_framerate;
  unsigned int requested_subsequent_bitrate;
  unsigned int requested_subsequent_framerate;
};

// Return the |percentile| from a sorted vector.
static base::TimeDelta Percentile(
    const std::vector<base::TimeDelta>& sorted_values,
    unsigned int percentile) {
  size_t size = sorted_values.size();
  LOG_ASSERT(size > 0UL);
  LOG_ASSERT(percentile <= 100UL);
  // Use Nearest Rank method in http://en.wikipedia.org/wiki/Percentile.
  int index =
      std::max(static_cast<int>(ceil(0.01f * percentile * size)) - 1, 0);
  return sorted_values[index];
}

static bool IsH264(VideoCodecProfile profile) {
  return profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX;
}

static bool IsVP8(VideoCodecProfile profile) {
  return profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX;
}

// Helper functions to do string conversions.
static base::FilePath::StringType StringToFilePathStringType(
    const std::string& str) {
#if defined(OS_WIN)
  return base::UTF8ToWide(str);
#else
  return str;
#endif  // defined(OS_WIN)
}

static std::string FilePathStringTypeToString(
    const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif  // defined(OS_WIN)
}

// Some platforms may have requirements on physical memory buffer alignment.
// Since we are just mapping and passing chunks of the input file directly to
// the VEA as input frames, to avoid copying large chunks of raw data on each
// frame, and thus affecting performance measurements, we have to prepare a
// temporary file with all planes aligned to the required alignment beforehand.
static void CreateAlignedInputStreamFile(const gfx::Size& coded_size,
                                         TestStream* test_stream) {
  // Test case may have many encoders and memory should be prepared once.
  if (test_stream->coded_size == coded_size &&
      !test_stream->aligned_in_file_data.empty())
    return;

  // All encoders in multiple encoder test reuse the same test_stream, make
  // sure they requested the same coded_size
  ASSERT_TRUE(test_stream->aligned_in_file_data.empty() ||
              coded_size == test_stream->coded_size);
  test_stream->coded_size = coded_size;

  ASSERT_NE(test_stream->pixel_format, PIXEL_FORMAT_UNKNOWN);
  const VideoPixelFormat pixel_format = test_stream->pixel_format;
  size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  std::vector<size_t> padding_sizes(num_planes);
  std::vector<size_t> coded_bpl(num_planes);
  std::vector<size_t> visible_bpl(num_planes);
  std::vector<size_t> visible_plane_rows(num_planes);

  // Calculate padding in bytes to be added after each plane required to keep
  // starting addresses of all planes at a byte boundary required by the
  // platform. This padding will be added after each plane when copying to the
  // temporary file.
  // At the same time we also need to take into account coded_size requested by
  // the VEA; each row of visible_bpl bytes in the original file needs to be
  // copied into a row of coded_bpl bytes in the aligned file.
  for (size_t i = 0; i < num_planes; i++) {
    const size_t size =
        VideoFrame::PlaneSize(pixel_format, i, coded_size).GetArea();
    test_stream->aligned_plane_size.push_back(
        AlignToPlatformRequirements(size));
    test_stream->aligned_buffer_size += test_stream->aligned_plane_size.back();

    coded_bpl[i] = VideoFrame::RowBytes(i, pixel_format, coded_size.width());
    visible_bpl[i] = VideoFrame::RowBytes(i, pixel_format,
                                          test_stream->visible_size.width());
    visible_plane_rows[i] =
        VideoFrame::Rows(i, pixel_format, test_stream->visible_size.height());
    const size_t padding_rows =
        VideoFrame::Rows(i, pixel_format, coded_size.height()) -
        visible_plane_rows[i];
    padding_sizes[i] =
        padding_rows * coded_bpl[i] + AlignToPlatformRequirements(size) - size;
  }

  base::FilePath src_file(StringToFilePathStringType(test_stream->in_filename));
  int64_t src_file_size = 0;
  LOG_ASSERT(base::GetFileSize(src_file, &src_file_size));

  size_t visible_buffer_size =
      VideoFrame::AllocationSize(pixel_format, test_stream->visible_size);
  LOG_ASSERT(src_file_size % visible_buffer_size == 0U)
      << "Stream byte size is not a product of calculated frame byte size";

  test_stream->num_frames =
      static_cast<unsigned int>(src_file_size / visible_buffer_size);

  LOG_ASSERT(test_stream->aligned_buffer_size > 0UL);
  test_stream->aligned_in_file_data.resize(test_stream->aligned_buffer_size *
                                           test_stream->num_frames);

  base::File src(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::vector<char> src_data(visible_buffer_size);
  off_t src_offset = 0, dest_offset = 0;
  for (size_t frame = 0; frame < test_stream->num_frames; frame++) {
    LOG_ASSERT(src.Read(src_offset, &src_data[0],
                        static_cast<int>(visible_buffer_size)) ==
               static_cast<int>(visible_buffer_size));
    const char* src_ptr = &src_data[0];
    for (size_t i = 0; i < num_planes; i++) {
      // Assert that each plane of frame starts at required byte boundary.
      ASSERT_EQ(0u, dest_offset & (kPlatformBufferAlignment - 1))
          << "Planes of frame should be mapped per platform requirements";
      for (size_t j = 0; j < visible_plane_rows[i]; j++) {
        memcpy(&test_stream->aligned_in_file_data[dest_offset], src_ptr,
               visible_bpl[i]);
        src_ptr += visible_bpl[i];
        dest_offset += static_cast<off_t>(coded_bpl[i]);
      }
      dest_offset += static_cast<off_t>(padding_sizes[i]);
    }
    src_offset += static_cast<off_t>(visible_buffer_size);
  }
  src.Close();

  LOG_ASSERT(test_stream->num_frames > 0UL);
}

// Parse |data| into its constituent parts, set the various output fields
// accordingly, read in video stream, and store them to |test_streams|.
static void ParseAndReadTestStreamData(
    const base::FilePath::StringType& data,
    std::vector<std::unique_ptr<TestStream>>* test_streams) {
  // Split the string to individual test stream data.
  std::vector<base::FilePath::StringType> test_streams_data =
      base::SplitString(data, base::FilePath::StringType(1, ';'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  LOG_ASSERT(test_streams_data.size() >= 1U) << data;

  // Parse each test stream data and read the input file.
  for (size_t index = 0; index < test_streams_data.size(); ++index) {
    std::vector<base::FilePath::StringType> fields = base::SplitString(
        test_streams_data[index], base::FilePath::StringType(1, ','),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    // Try using ":" as the seperator if "," isn't used.
    if (fields.size() == 1U) {
      fields = base::SplitString(test_streams_data[index],
                                 base::FilePath::StringType(1, ':'),
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    }
    LOG_ASSERT(fields.size() >= 4U) << data;
    LOG_ASSERT(fields.size() <= 10U) << data;
    auto test_stream = std::make_unique<TestStream>();

    test_stream->in_filename = FilePathStringTypeToString(fields[0]);
    int width, height;
    bool result = base::StringToInt(fields[1], &width);
    LOG_ASSERT(result);
    result = base::StringToInt(fields[2], &height);
    LOG_ASSERT(result);
    test_stream->visible_size = gfx::Size(width, height);
    LOG_ASSERT(!test_stream->visible_size.IsEmpty());
    int profile;
    result = base::StringToInt(fields[3], &profile);
    LOG_ASSERT(result);
    LOG_ASSERT(profile > VIDEO_CODEC_PROFILE_UNKNOWN);
    LOG_ASSERT(profile <= VIDEO_CODEC_PROFILE_MAX);
    test_stream->requested_profile = static_cast<VideoCodecProfile>(profile);
    test_stream->pixel_format = PIXEL_FORMAT_I420;

    if (fields.size() >= 5 && !fields[4].empty())
      test_stream->out_filename = FilePathStringTypeToString(fields[4]);

    if (fields.size() >= 6 && !fields[5].empty())
      LOG_ASSERT(
          base::StringToUint(fields[5], &test_stream->requested_bitrate));

    if (fields.size() >= 7 && !fields[6].empty())
      LOG_ASSERT(
          base::StringToUint(fields[6], &test_stream->requested_framerate));

    if (fields.size() >= 8 && !fields[7].empty()) {
      LOG_ASSERT(base::StringToUint(
          fields[7], &test_stream->requested_subsequent_bitrate));
    }

    if (fields.size() >= 9 && !fields[8].empty()) {
      LOG_ASSERT(base::StringToUint(
          fields[8], &test_stream->requested_subsequent_framerate));
    }

    if (fields.size() >= 10 && !fields[9].empty()) {
      unsigned int format = 0;
      LOG_ASSERT(base::StringToUint(fields[9], &format));
      test_stream->pixel_format = static_cast<VideoPixelFormat>(format);
    }
    test_streams->push_back(std::move(test_stream));
  }
}

// Basic test environment shared across multiple test cases. We only need to
// setup it once for all test cases.
// It helps
// - maintain test stream data and other test settings.
// - clean up temporary aligned files.
// - output log to file.
class VideoEncodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  VideoEncodeAcceleratorTestEnvironment(
      std::unique_ptr<base::FilePath::StringType> data,
      const base::FilePath& log_path,
      const base::FilePath& frame_stats_path,
      bool run_at_fps,
      bool needs_encode_latency,
      bool verify_all_output)
      : test_stream_data_(std::move(data)),
        log_path_(log_path),
        frame_stats_path_(frame_stats_path),
        run_at_fps_(run_at_fps),
        needs_encode_latency_(needs_encode_latency),
        verify_all_output_(verify_all_output) {}

  virtual void SetUp() {
    if (!log_path_.empty()) {
      log_file_.reset(new base::File(
          log_path_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
      LOG_ASSERT(log_file_->IsValid());
    }
    ParseAndReadTestStreamData(*test_stream_data_, &test_streams_);
  }

  virtual void TearDown() {
    log_file_.reset();
  }

  // Log one entry of machine-readable data to file and LOG(INFO).
  // The log has one data entry per line in the format of "<key>: <value>".
  // Note that Chrome OS video_VEAPerf autotest parses the output key and value
  // pairs. Be sure to keep the autotest in sync.
  void LogToFile(const std::string& key, const std::string& value) {
    std::string s = base::StringPrintf("%s: %s\n", key.c_str(), value.c_str());
    LOG(INFO) << s;
    if (log_file_) {
      log_file_->WriteAtCurrentPos(s.data(), static_cast<int>(s.length()));
    }
  }

  // Feed the encoder with the input buffers at the requested framerate. If
  // false, feed as fast as possible. This is set by the command line switch
  // "--run_at_fps".
  bool run_at_fps() const { return run_at_fps_; }

  // Whether to measure encode latency. This is set by the command line switch
  // "--measure_latency".
  bool needs_encode_latency() const { return needs_encode_latency_; }

  // Verify the encoder output of all testcases. This is set by the command line
  // switch "--verify_all_output".
  bool verify_all_output() const { return verify_all_output_; }

  const base::FilePath& frame_stats_path() const { return frame_stats_path_; }

  std::vector<std::unique_ptr<TestStream>> test_streams_;

 private:
  std::unique_ptr<base::FilePath::StringType> test_stream_data_;
  base::FilePath log_path_;
  base::FilePath frame_stats_path_;
  std::unique_ptr<base::File> log_file_;
  bool run_at_fps_;
  bool needs_encode_latency_;
  bool verify_all_output_;
};

enum ClientState {
  CS_CREATED,      // Encoder is created.
  CS_INITIALIZED,  // Encoder initialization is finished.
  CS_ENCODING,     // Encoder is encoding.
  CS_FLUSHING,     // Ask encoder to flush.
  CS_FINISHED,     // Encoding has finished, all frames are encoded.
  CS_FLUSHED,      // Encoder notifies the flush is finished.
  CS_VALIDATED,    // Encoded frame quality has been validated.
  CS_ERROR,        // Any error occurs.
};

// Performs basic, codec-specific sanity checks on the stream buffers passed
// to ProcessStreamBuffer(): whether we've seen keyframes before non-keyframes,
// correct sequences of H.264 NALUs (SPS before PPS and before slices), etc.
// Calls given FrameFoundCallback when a complete frame is found while
// processing.
class StreamValidator {
 public:
  // To be called when a complete frame is found while processing a stream
  // buffer, passing true if the frame is a keyframe and the visible size.
  // Returns false if we are not interested in more frames and further
  // processing should be aborted.
  typedef base::Callback<bool(bool, const gfx::Size&)> FrameFoundCallback;

  virtual ~StreamValidator() {}

  // Provide a StreamValidator instance for the given |profile|.
  static std::unique_ptr<StreamValidator> Create(
      VideoCodecProfile profile,
      const FrameFoundCallback& frame_cb);

  // Process and verify contents of a bitstream buffer.
  virtual void ProcessStreamBuffer(const uint8_t* stream, size_t size) = 0;

 protected:
  explicit StreamValidator(const FrameFoundCallback& frame_cb)
      : frame_cb_(frame_cb) {}

  FrameFoundCallback frame_cb_;
  gfx::Size visible_size_;
};

class H264Validator : public StreamValidator {
 public:
  explicit H264Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb),
        seen_sps_(false),
        seen_pps_(false),
        seen_idr_(false),
        curr_pic_(new H264Picture) {}

  void ProcessStreamBuffer(const uint8_t* stream, size_t size) override;

 private:
  bool IsNewPicture(const H264SliceHeader& slice_hdr);
  bool UpdateCurrentPicture(const H264SliceHeader& slice_hdr);

  // Set to true when encoder provides us with the corresponding NALU type.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;

  scoped_refptr<H264Picture> curr_pic_;
  int curr_sps_id_ = -1;
  int curr_pps_id_ = -1;

  H264Parser h264_parser_;
};

void H264Validator::ProcessStreamBuffer(const uint8_t* stream, size_t size) {
  h264_parser_.SetStream(stream, static_cast<off_t>(size));

  while (1) {
    H264NALU nalu;
    H264Parser::Result result;

    result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;

    ASSERT_EQ(H264Parser::kOk, result);

    bool keyframe = false;

    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
        ASSERT_TRUE(seen_sps_);
        ASSERT_TRUE(seen_pps_);
        seen_idr_ = true;
        keyframe = true;
        FALLTHROUGH;
      case H264NALU::kNonIDRSlice: {
        ASSERT_TRUE(seen_idr_);
        H264SliceHeader slice_hdr;
        ASSERT_EQ(H264Parser::kOk,
                  h264_parser_.ParseSliceHeader(nalu, &slice_hdr));
        if (IsNewPicture(slice_hdr)) {
          if (!frame_cb_.Run(keyframe, visible_size_))
            return;
          ASSERT_TRUE(UpdateCurrentPicture(slice_hdr));
        }
        break;
      }

      case H264NALU::kSPS: {
        int sps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser_.ParseSPS(&sps_id));
        // Check the visible size.
        gfx::Rect visible_size =
            h264_parser_.GetSPS(sps_id)->GetVisibleRect().value_or(gfx::Rect());
        ASSERT_FALSE(visible_size.IsEmpty());
        visible_size_ = visible_size.size();
        seen_sps_ = true;
        break;
      }

      case H264NALU::kPPS: {
        ASSERT_TRUE(seen_sps_);
        int pps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser_.ParsePPS(&pps_id));
        seen_pps_ = true;
        break;
      }

      default:
        break;
    }
  }
}

bool H264Validator::IsNewPicture(const H264SliceHeader& slice_hdr) {
  if (!curr_pic_)
    return true;
  return H264Decoder::IsNewPrimaryCodedPicture(
      curr_pic_.get(), curr_pps_id_, h264_parser_.GetSPS(curr_sps_id_),
      slice_hdr);
}

bool H264Validator::UpdateCurrentPicture(const H264SliceHeader& slice_hdr) {
  curr_pps_id_ = slice_hdr.pic_parameter_set_id;
  const H264PPS* pps = h264_parser_.GetPPS(curr_pps_id_);
  if (!pps) {
    LOG(ERROR) << "Cannot parse pps.";
    return false;
  }

  curr_sps_id_ = pps->seq_parameter_set_id;
  const H264SPS* sps = h264_parser_.GetSPS(curr_sps_id_);
  if (!sps) {
    LOG(ERROR) << "Cannot parse sps.";
    return false;
  }

  if (!H264Decoder::FillH264PictureFromSliceHeader(sps, slice_hdr,
                                                   curr_pic_.get())) {
    LOG(FATAL) << "Cannot initialize current frame.";
    return false;
  }
  return true;
}

class VP8Validator : public StreamValidator {
 public:
  explicit VP8Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb), seen_keyframe_(false) {}

  void ProcessStreamBuffer(const uint8_t* stream, size_t size) override;

 private:
  // Have we already got a keyframe in the stream?
  bool seen_keyframe_;
};

void VP8Validator::ProcessStreamBuffer(const uint8_t* stream, size_t size) {
  // TODO(posciak): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
  Vp8Parser parser;
  Vp8FrameHeader header;
  EXPECT_TRUE(parser.ParseFrame(stream, size, &header));
  if (header.IsKeyframe()) {
    seen_keyframe_ = true;
    visible_size_.SetSize(header.width, header.height);
  }

  EXPECT_TRUE(seen_keyframe_);
  ASSERT_FALSE(visible_size_.IsEmpty());
  frame_cb_.Run(header.IsKeyframe(), visible_size_);
}

// static
std::unique_ptr<StreamValidator> StreamValidator::Create(
    VideoCodecProfile profile,
    const FrameFoundCallback& frame_cb) {
  std::unique_ptr<StreamValidator> validator;

  if (IsH264(profile)) {
    validator.reset(new H264Validator(frame_cb));
  } else if (IsVP8(profile)) {
    validator.reset(new VP8Validator(frame_cb));
  } else {
    LOG(FATAL) << "Unsupported profile: " << GetProfileName(profile);
  }

  return validator;
}

class VideoFrameQualityValidator
    : public base::SupportsWeakPtr<VideoFrameQualityValidator> {
 public:
  VideoFrameQualityValidator(const VideoCodecProfile profile,
                             const VideoPixelFormat pixel_format,
                             bool verify_quality,
                             const base::Closure& flush_complete_cb,
                             const base::Closure& decode_error_cb);
  void Initialize(const gfx::Size& coded_size, const gfx::Rect& visible_size);
  // Save original YUV frame to compare it with the decoded frame later.
  void AddOriginalFrame(scoped_refptr<VideoFrame> frame);
  void AddDecodeBuffer(scoped_refptr<DecoderBuffer> buffer);
  // Flush the decoder.
  void Flush();

 private:
  void InitializeCB(bool success);
  void DecodeDone(DecodeStatus status);
  void FlushDone(DecodeStatus status);
  void VerifyOutputFrame(const scoped_refptr<VideoFrame>& output_frame);
  void Decode();
  void WriteFrameStats();

  enum State { UNINITIALIZED, INITIALIZED, DECODING, DECODER_ERROR };
  struct FrameStats {
    int width;
    int height;
    double ssim[VideoFrame::kMaxPlanes];
    uint64_t mse[VideoFrame::kMaxPlanes];
  };

  FrameStats CompareFrames(const VideoFrame& original_frame,
                           const VideoFrame& output_frame);
  MediaLog media_log_;
  const VideoCodecProfile profile_;
  const VideoPixelFormat pixel_format_;
  const bool verify_quality_;
  std::unique_ptr<FFmpegVideoDecoder> decoder_;
  VideoDecoder::DecodeCB decode_cb_;
  // Decode callback of an EOS buffer.
  VideoDecoder::DecodeCB eos_decode_cb_;
  // Callback of Flush(). Called after all frames are decoded.
  const base::Closure flush_complete_cb_;
  const base::Closure decode_error_cb_;
  State decoder_state_;
  base::queue<scoped_refptr<VideoFrame>> original_frames_;
  base::queue<scoped_refptr<DecoderBuffer>> decode_buffers_;
  std::vector<FrameStats> frame_stats_;
  base::ThreadChecker thread_checker_;
};

VideoFrameQualityValidator::VideoFrameQualityValidator(
    const VideoCodecProfile profile,
    const VideoPixelFormat pixel_format,
    const bool verify_quality,
    const base::Closure& flush_complete_cb,
    const base::Closure& decode_error_cb)
    : profile_(profile),
      pixel_format_(pixel_format),
      verify_quality_(verify_quality),
      decoder_(new FFmpegVideoDecoder(&media_log_)),
      decode_cb_(base::BindRepeating(&VideoFrameQualityValidator::DecodeDone,
                                     AsWeakPtr())),
      eos_decode_cb_(base::BindRepeating(&VideoFrameQualityValidator::FlushDone,
                                         AsWeakPtr())),
      flush_complete_cb_(flush_complete_cb),
      decode_error_cb_(decode_error_cb),
      decoder_state_(UNINITIALIZED) {
  // Allow decoding of individual NALU. Entire frames are required by default.
  decoder_->set_decode_nalus(true);
}

void VideoFrameQualityValidator::Initialize(const gfx::Size& coded_size,
                                            const gfx::Rect& visible_size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Size natural_size(visible_size.size());
  // The default output format of ffmpeg video decoder is YV12.
  VideoDecoderConfig config;
  if (IsVP8(profile_))
    config.Initialize(kCodecVP8, VP8PROFILE_ANY, pixel_format_,
                      COLOR_SPACE_UNSPECIFIED, VIDEO_ROTATION_0, coded_size,
                      visible_size, natural_size, EmptyExtraData(),
                      Unencrypted());
  else if (IsH264(profile_))
    config.Initialize(kCodecH264, H264PROFILE_MAIN, pixel_format_,
                      COLOR_SPACE_UNSPECIFIED, VIDEO_ROTATION_0, coded_size,
                      visible_size, natural_size, EmptyExtraData(),
                      Unencrypted());
  else
    LOG_ASSERT(0) << "Invalid profile " << GetProfileName(profile_);

  decoder_->Initialize(
      config, false, nullptr,
      base::BindRepeating(&VideoFrameQualityValidator::InitializeCB,
                          base::Unretained(this)),
      base::BindRepeating(&VideoFrameQualityValidator::VerifyOutputFrame,
                          base::Unretained(this)),
      base::NullCallback());
}

void VideoFrameQualityValidator::InitializeCB(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    decoder_state_ = INITIALIZED;
    Decode();
  } else {
    decoder_state_ = DECODER_ERROR;
    if (IsH264(profile_))
      LOG(ERROR) << "Chromium does not support H264 decode. Try Chrome.";
    decode_error_cb_.Run();
    FAIL() << "Decoder initialization error";
  }
}

void VideoFrameQualityValidator::AddOriginalFrame(
    scoped_refptr<VideoFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  original_frames_.push(frame);
}

void VideoFrameQualityValidator::DecodeDone(DecodeStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (status == DecodeStatus::OK) {
    decoder_state_ = INITIALIZED;
    Decode();
  } else {
    decoder_state_ = DECODER_ERROR;
    decode_error_cb_.Run();
    FAIL() << "Unexpected decode status = " << status << ". Stop decoding.";
  }
}

void VideoFrameQualityValidator::FlushDone(DecodeStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());

  WriteFrameStats();
  flush_complete_cb_.Run();
}

void VideoFrameQualityValidator::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_state_ != DECODER_ERROR) {
    decode_buffers_.push(DecoderBuffer::CreateEOSBuffer());
    Decode();
  }
}

void VideoFrameQualityValidator::WriteFrameStats() {
  if (g_env->frame_stats_path().empty())
    return;

  base::File frame_stats_file(
      g_env->frame_stats_path(),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  LOG_ASSERT(frame_stats_file.IsValid());

  // TODO(pbos): Consider adding encoded bytes per frame + key/delta frame.
  std::string csv =
      "frame,width,height,ssim-y,ssim-u,ssim-v,mse-y,mse-u,mse-v\n";
  frame_stats_file.WriteAtCurrentPos(csv.data(), static_cast<int>(csv.size()));
  for (size_t frame_idx = 0; frame_idx < frame_stats_.size(); ++frame_idx) {
    const FrameStats& frame_stats = frame_stats_[frame_idx];
    csv = base::StringPrintf(
        "%d,%d,%d,%f,%f,%f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
        static_cast<int>(frame_idx), frame_stats.width, frame_stats.height,
        frame_stats.ssim[VideoFrame::kYPlane],
        frame_stats.ssim[VideoFrame::kUPlane],
        frame_stats.ssim[VideoFrame::kVPlane],
        frame_stats.mse[VideoFrame::kYPlane],
        frame_stats.mse[VideoFrame::kUPlane],
        frame_stats.mse[VideoFrame::kVPlane]);
    frame_stats_file.WriteAtCurrentPos(csv.data(),
                                       static_cast<int>(csv.size()));
  }
}

void VideoFrameQualityValidator::AddDecodeBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_state_ != DECODER_ERROR) {
    decode_buffers_.push(buffer);
    Decode();
  }
}

void VideoFrameQualityValidator::Decode() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_state_ == INITIALIZED && !decode_buffers_.empty()) {
    scoped_refptr<DecoderBuffer> next_buffer = decode_buffers_.front();
    decode_buffers_.pop();
    decoder_state_ = DECODING;
    if (next_buffer->end_of_stream())
      decoder_->Decode(next_buffer, eos_decode_cb_);
    else
      decoder_->Decode(next_buffer, decode_cb_);
  }
}

namespace {
// Gets SSIM (see below) parameters for a 8x8 block.
void GetSsimParams8x8(const uint8_t* orig,
                      size_t orig_stride,
                      const uint8_t* recon,
                      size_t recon_stride,
                      int* sum_orig,
                      int* sum_recon,
                      int* sum_sq_orig,
                      int* sum_sq_recon,
                      int* sum_orig_x_recon) {
  for (size_t i = 0; i < 8; ++i, orig += orig_stride, recon += recon_stride) {
    for (size_t j = 0; j < 8; ++j) {
      *sum_orig += orig[j];
      *sum_recon += recon[j];
      *sum_sq_orig += orig[j] * orig[j];
      *sum_sq_recon += recon[j] * recon[j];
      *sum_orig_x_recon += orig[j] * recon[j];
    }
  }
}

// Generates SSIM (see below) for a 8x8 block from input parameters.
// https://en.wikipedia.org/wiki/Structural_similarity
double GenerateSimilarity(int sum_orig,
                          int sum_recon,
                          int sum_sq_orig,
                          int sum_sq_recon,
                          int sum_orig_x_recon,
                          int count) {
  // Same constants as used inside libvpx.
  static const int64_t kC1 = 26634;   // (64^2*(.01*255)^2
  static const int64_t kC2 = 239708;  // (64^2*(.03*255)^2

  // scale the constants by number of pixels
  int64_t c1 = (kC1 * count * count) >> 12;
  int64_t c2 = (kC2 * count * count) >> 12;

  int64_t ssim_n = (2 * sum_orig * sum_recon + c1) *
                   (static_cast<int64_t>(2 * count) * sum_orig_x_recon -
                    static_cast<int64_t>(2 * sum_orig) * sum_recon + c2);

  int64_t ssim_d = (sum_orig * sum_orig + sum_recon * sum_recon + c1) *
                   (static_cast<int64_t>(count) * sum_sq_orig -
                    static_cast<int64_t>(sum_orig) * sum_orig +
                    static_cast<int64_t>(count) * sum_sq_recon -
                    static_cast<int64_t>(sum_recon) * sum_recon + c2);

  return static_cast<double>(ssim_n) / ssim_d;
}

// Generates SSIM for a 8x8 block.
double GenerateSsim8x8(const uint8_t* orig,
                       int orig_stride,
                       const uint8_t* recon,
                       int recon_stride) {
  int sum_orig = 0;
  int sum_recon = 0;
  int sum_sq_orig = 0;
  int sum_sq_recon = 0;
  int sum_orig_x_recon = 0;
  GetSsimParams8x8(orig, orig_stride, recon, recon_stride, &sum_orig,
                   &sum_recon, &sum_sq_orig, &sum_sq_recon, &sum_orig_x_recon);
  return GenerateSimilarity(sum_orig, sum_recon, sum_sq_orig, sum_sq_recon,
                            sum_orig_x_recon, 64);
}

// Generates SSIM: https://en.wikipedia.org/wiki/Structural_similarity
// We are using a 8x8 moving window with starting location of each 8x8 window
// on the 4x4 pixel grid. Such arrangement allows the windows to overlap
// block boundaries to penalize blocking artifacts.
double GenerateSsim(const uint8_t* img1,
                    size_t stride_img1,
                    const uint8_t* img2,
                    size_t stride_img2,
                    int width,
                    int height) {
  int num_samples = 0;
  double ssim_total = 0;

  // sample point start with each 4x4 location
  for (int i = 0; i <= height - 8;
       i += 4, img1 += stride_img1 * 4, img2 += stride_img2 * 4) {
    for (int j = 0; j <= width - 8; j += 4) {
      double v = GenerateSsim8x8(img1 + j, stride_img1, img2 + j, stride_img2);
      ssim_total += v;
      ++num_samples;
    }
  }
  return ssim_total / num_samples;
}

// Generates MSE that can later be used (outside this file) to generate PSNR
// either average per-frame or global average.
// https://en.wikipedia.org/wiki/Mean_squared_error
// https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio
static uint64_t GenerateMse(const uint8_t* orig,
                            int orig_stride,
                            const uint8_t* recon,
                            int recon_stride,
                            int cols,
                            int rows) {
  uint64_t total_sse = 0;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      int diff = orig[col] - recon[col];
      total_sse += diff * diff;
    }

    orig += orig_stride;
    recon += recon_stride;
  }

  return total_sse;
}

void GenerateMseAndSsim(double* ssim,
                        uint64_t* mse,
                        const uint8_t* buf0,
                        size_t stride0,
                        const uint8_t* buf1,
                        size_t stride1,
                        int w,
                        int h) {
  *ssim = GenerateSsim(buf0, stride0, buf1, stride1, w, h);
  *mse = GenerateMse(buf0, stride0, buf1, stride1, w, h);
}
}  // namespace

VideoFrameQualityValidator::FrameStats
VideoFrameQualityValidator::CompareFrames(const VideoFrame& original_frame,
                                          const VideoFrame& output_frame) {
  CHECK(original_frame.visible_rect().size() ==
        output_frame.visible_rect().size());

  FrameStats frame_stats;
  gfx::Size visible_size = original_frame.visible_rect().size();
  frame_stats.width = visible_size.width();
  frame_stats.height = visible_size.height();
  for (size_t plane :
       {VideoFrame::kYPlane, VideoFrame::kUPlane, VideoFrame::kVPlane}) {
    GenerateMseAndSsim(
        &frame_stats.ssim[plane], &frame_stats.mse[plane],
        original_frame.data(plane), original_frame.stride(plane),
        output_frame.data(plane), output_frame.stride(plane),
        VideoFrame::Columns(plane, pixel_format_, frame_stats.width),
        VideoFrame::Rows(plane, pixel_format_, frame_stats.height));
  }
  return frame_stats;
}

void VideoFrameQualityValidator::VerifyOutputFrame(
    const scoped_refptr<VideoFrame>& output_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<VideoFrame> original_frame = original_frames_.front();
  original_frames_.pop();
  gfx::Size visible_size = original_frame->visible_rect().size();

  if (!g_env->frame_stats_path().empty())
    frame_stats_.push_back(CompareFrames(*original_frame, *output_frame));

  // TODO(pbos): Consider rewriting similiarity thresholds to use standard
  // SSIM/PSNR metrics for thresholds instead of abs(difference) / size which
  // correspond less to perceptive distortion.
  if (verify_quality_) {
    int planes[] = {VideoFrame::kYPlane, VideoFrame::kUPlane,
                    VideoFrame::kVPlane};
    double difference = 0;
    for (int plane : planes) {
      uint8_t* original_plane = original_frame->data(plane);
      uint8_t* output_plane = output_frame->data(plane);

      size_t rows =
          VideoFrame::Rows(plane, pixel_format_, visible_size.height());
      size_t columns =
          VideoFrame::Columns(plane, pixel_format_, visible_size.width());
      size_t stride = original_frame->stride(plane);

      for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < columns; j++) {
          difference += std::abs(original_plane[stride * i + j] -
                                 output_plane[stride * i + j]);
        }
      }
    }

    // Divide the difference by the size of frame.
    difference /= VideoFrame::AllocationSize(pixel_format_, visible_size);
    EXPECT_TRUE(difference <= kDecodeSimilarityThreshold)
        << "difference = " << difference << "  > decode similarity threshold";
  }
}

// Base class for all VEA Clients in this file
class VEAClientBase : public VideoEncodeAccelerator::Client {
 public:
  ~VEAClientBase() override { LOG_ASSERT(!has_encoder()); }
  void NotifyError(VideoEncodeAccelerator::Error error) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    SetState(CS_ERROR);
  }

 protected:
  VEAClientBase(ClientStateNotification<ClientState>* note)
      : note_(note), next_output_buffer_id_(0) {}

  bool has_encoder() { return encoder_.get(); }

  virtual void SetState(ClientState new_state) = 0;

  std::unique_ptr<VideoEncodeAccelerator> encoder_;

  // Used to notify another thread about the state. VEAClientBase does not own
  // this.
  ClientStateNotification<ClientState>* note_;

  // All methods of this class should be run on the same thread.
  base::ThreadChecker thread_checker_;

  std::vector<std::unique_ptr<base::SharedMemory>> output_shms_;
  int32_t next_output_buffer_id_;
};

class VEAClient : public VEAClientBase {
 public:
  VEAClient(TestStream* test_stream,
            ClientStateNotification<ClientState>* note,
            bool save_to_file,
            unsigned int keyframe_period,
            bool force_bitrate,
            bool test_perf,
            bool mid_stream_bitrate_switch,
            bool mid_stream_framerate_switch,
            bool verify_output,
            bool verify_output_timestamp);
  void CreateEncoder();
  void DestroyEncoder();

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;

 private:
  // Return the number of encoded frames per second.
  double frames_per_second();

  void SetState(ClientState new_state) override;

  // Set current stream parameters to given |bitrate| at |framerate|.
  void SetStreamParameters(unsigned int bitrate, unsigned int framerate);

  // Called when encoder is done with a VideoFrame.
  void InputNoLongerNeededCallback(int32_t input_id);

  // Feed the encoder with one input frame.
  void FeedEncoderWithOneInput();

  // Provide the encoder with a new output buffer.
  void FeedEncoderWithOutput(base::SharedMemory* shm);

  // Called on finding a complete frame (with |keyframe| set to true for
  // keyframes, |visible_size| for the visible size of encoded frame) in the
  // stream, to perform codec-independent, per-frame checks and accounting.
  // Returns false once we have collected all frames we needed.
  bool HandleEncodedFrame(bool keyframe, const gfx::Size& visible_size);

  // Ask the encoder to flush the frame.
  void FlushEncoder();

  // Callback function of encoder_->Flush(). We add the number of received
  // frames at BitstreamBufferReady() and verify the number after flush is
  // completed.
  void FlushEncoderDone(bool success);
  void FlushEncoderSuccessfully();

  // Timeout function to check the flush callback function is called in the
  // short period.
  void FlushTimeout();

  // Verify the minimum FPS requirement.
  void VerifyMinFPS();

  // Verify that stream bitrate has been close to current_requested_bitrate_,
  // assuming current_framerate_ since the last time VerifyStreamProperties()
  // was called. Fail the test if |force_bitrate_| is true and the bitrate
  // is not within kBitrateTolerance.
  void VerifyStreamProperties();

  // Log the performance data.
  void LogPerf();

  // Write IVF file header to test_stream_->out_filename.
  void WriteIvfFileHeader();

  // Write an IVF frame header to test_stream_->out_filename.
  void WriteIvfFrameHeader(int frame_index, size_t frame_size);

  // Create and return a VideoFrame wrapping the data at |position| bytes in the
  // input stream.
  scoped_refptr<VideoFrame> CreateFrame(off_t position);

  // Prepare and return a frame wrapping the data at |position| bytes in the
  // input stream, ready to be sent to encoder.
  // The input frame id is returned in |input_id|.
  scoped_refptr<VideoFrame> PrepareInputFrame(off_t position,
                                              int32_t* input_id);

  // Update the parameters according to |mid_stream_bitrate_switch| and
  // |mid_stream_framerate_switch|.
  void UpdateTestStreamData(bool mid_stream_bitrate_switch,
                            bool mid_stream_framerate_switch);

  // Callback function of the |input_timer_|.
  void OnInputTimer();

  // Called when the quality validator has decoded all the frames.
  void DecodeCompleted();

  // Called when the quality validator fails to decode a frame.
  void DecodeFailed();

  // Verify that the output timestamp matches input timestamp.
  void VerifyOutputTimestamp(base::TimeDelta timestamp);

  ClientState state_;

  TestStream* test_stream_;

  // Ids assigned to VideoFrames.
  std::set<int32_t> inputs_at_client_;
  int32_t next_input_id_;

  // Encode start time of all encoded frames. The position in the vector is the
  // frame input id.
  std::vector<base::TimeTicks> encode_start_time_;
  // The encode latencies of all encoded frames. We define encode latency as the
  // time delay from input of each VideoFrame (VEA::Encode()) to output of the
  // corresponding BitstreamBuffer (VEA::Client::BitstreamBufferReady()).
  std::vector<base::TimeDelta> encode_latencies_;
  // The 0-based indices of frames that we force as key frames.
  std::queue<size_t> keyframe_indices_;

  // Ids for output BitstreamBuffers.
  typedef std::map<int32_t, base::SharedMemory*> IdToSHM;
  IdToSHM output_buffers_at_client_;

  // Current offset into input stream.
  off_t pos_in_input_stream_;
  gfx::Size input_coded_size_;
  // Requested by encoder.
  unsigned int num_required_input_buffers_;
  size_t output_buffer_size_;

  // Number of frames to encode. This may differ from the number of frames in
  // stream if we need more frames for bitrate tests.
  unsigned int num_frames_to_encode_;

  // Number of frames we've sent to the encoder thus far.
  size_t num_frames_submitted_to_encoder_;

  // Number of encoded frames we've got from the encoder thus far.
  unsigned int num_encoded_frames_;

  // Frames since last bitrate verification.
  unsigned int num_frames_since_last_check_;

  // True if we are to save the encoded stream to a file.
  bool save_to_file_;

  // Request a keyframe every keyframe_period_ frames.
  const unsigned int keyframe_period_;

  // True if we are asking encoder for a particular bitrate.
  bool force_bitrate_;

  // Current requested bitrate.
  unsigned int current_requested_bitrate_;

  // Current expected framerate.
  unsigned int current_framerate_;

  // Byte size of the encoded stream (for bitrate calculation) since last
  // time we checked bitrate.
  size_t encoded_stream_size_since_last_check_;

  // If true, verify performance at the end of the test.
  bool test_perf_;

  // Check the output frame quality of the encoder.
  bool verify_output_;

  // Check whether the output timestamps match input timestamps.
  bool verify_output_timestamp_;

  // Used to perform codec-specific sanity checks on the stream.
  std::unique_ptr<StreamValidator> stream_validator_;

  // Used to validate the encoded frame quality.
  std::unique_ptr<VideoFrameQualityValidator> quality_validator_;

  // The time when the first frame is submitted for encode.
  base::TimeTicks first_frame_start_time_;

  // The time when the last encoded frame is ready.
  base::TimeTicks last_frame_ready_time_;

  // Requested bitrate in bits per second.
  unsigned int requested_bitrate_;

  // Requested initial framerate.
  unsigned int requested_framerate_;

  // Bitrate to switch to in the middle of the stream.
  unsigned int requested_subsequent_bitrate_;

  // Framerate to switch to in the middle of the stream.
  unsigned int requested_subsequent_framerate_;

  // The timer used to feed the encoder with the input frames.
  std::unique_ptr<base::RepeatingTimer> input_timer_;

  // The FlushTimeout closure. It is cancelled when flush is finished.
  base::CancelableClosure flush_timeout_;

  // The timestamps for each frame in the order of CreateFrame() invocation.
  base::queue<base::TimeDelta> frame_timestamps_;

  // The last timestamp popped from |frame_timestamps_|.
  base::TimeDelta previous_timestamp_;
};

VEAClient::VEAClient(TestStream* test_stream,
                     ClientStateNotification<ClientState>* note,
                     bool save_to_file,
                     unsigned int keyframe_period,
                     bool force_bitrate,
                     bool test_perf,
                     bool mid_stream_bitrate_switch,
                     bool mid_stream_framerate_switch,
                     bool verify_output,
                     bool verify_output_timestamp)
    : VEAClientBase(note),
      state_(CS_CREATED),
      test_stream_(test_stream),
      next_input_id_(0),
      pos_in_input_stream_(0),
      num_required_input_buffers_(0),
      output_buffer_size_(0),
      num_frames_to_encode_(0),
      num_frames_submitted_to_encoder_(0),
      num_encoded_frames_(0),
      num_frames_since_last_check_(0),
      save_to_file_(save_to_file),
      keyframe_period_(keyframe_period),
      force_bitrate_(force_bitrate),
      current_requested_bitrate_(0),
      current_framerate_(0),
      encoded_stream_size_since_last_check_(0),
      test_perf_(test_perf),
      verify_output_(verify_output),
      verify_output_timestamp_(verify_output_timestamp),
      requested_bitrate_(0),
      requested_framerate_(0),
      requested_subsequent_bitrate_(0),
      requested_subsequent_framerate_(0) {
  if (keyframe_period_)
    LOG_ASSERT(kMaxKeyframeDelay < keyframe_period_);

  // Fake encoder produces an invalid stream, so skip validating it.
  if (!g_fake_encoder) {
    stream_validator_ = StreamValidator::Create(
        test_stream_->requested_profile,
        base::BindRepeating(&VEAClient::HandleEncodedFrame,
                            base::Unretained(this)));
    CHECK(stream_validator_);
    // VideoFrameQualityValidator is required to generate frame stats as well as
    // validating encoder quality.
    if (verify_output_ || !g_env->frame_stats_path().empty()) {
      quality_validator_.reset(new VideoFrameQualityValidator(
          test_stream_->requested_profile, test_stream_->pixel_format,
          verify_output_,
          base::BindRepeating(&VEAClient::DecodeCompleted,
                              base::Unretained(this)),
          base::BindRepeating(&VEAClient::DecodeFailed,
                              base::Unretained(this))));
    }
  }

  if (save_to_file_) {
    LOG_ASSERT(!test_stream_->out_filename.empty());
#if defined(OS_POSIX)
    base::FilePath out_filename(test_stream_->out_filename);
#elif defined(OS_WIN)
    base::FilePath out_filename(base::UTF8ToWide(test_stream_->out_filename));
#endif
    // This creates or truncates out_filename.
    // Without it, AppendToFile() will not work.
    EXPECT_EQ(0, base::WriteFile(out_filename, NULL, 0));
  }

  // Initialize the parameters of the test streams.
  UpdateTestStreamData(mid_stream_bitrate_switch, mid_stream_framerate_switch);

  thread_checker_.DetachFromThread();
}

// Helper function to create VEA.
static std::unique_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator(
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    const gpu::GpuPreferences& gpu_preferences) {
  if (g_fake_encoder) {
    std::unique_ptr<VideoEncodeAccelerator> encoder(
        new FakeVideoEncodeAccelerator(
            scoped_refptr<base::SingleThreadTaskRunner>(
                base::ThreadTaskRunnerHandle::Get())));
    if (encoder->Initialize(config, client))
      return encoder;
    return nullptr;
  } else {
    // TODO(johnylin): add level testing in video_encode_accelerator_unittest
    //                 crbug.com/863327
    return GpuVideoEncodeAcceleratorFactory::CreateVEA(config, client,
                                                       gpu_preferences);
  }
}

void VEAClient::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(!has_encoder());
  DVLOG(1) << "Profile: " << test_stream_->requested_profile
           << ", initial bitrate: " << requested_bitrate_;

  const VideoEncodeAccelerator::Config config(
      test_stream_->pixel_format, test_stream_->visible_size,
      test_stream_->requested_profile, requested_bitrate_,
      requested_framerate_);
  encoder_ = CreateVideoEncodeAccelerator(config, this, gpu::GpuPreferences());
  if (!encoder_) {
    LOG(ERROR) << "Failed creating a VideoEncodeAccelerator.";
    SetState(CS_ERROR);
    return;
  }
  SetStreamParameters(requested_bitrate_, requested_framerate_);
  SetState(CS_INITIALIZED);
}

void VEAClient::DecodeCompleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_VALIDATED);
}

void VEAClient::DecodeFailed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_ERROR);
}

void VEAClient::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;

  encoder_.reset();
  input_timer_.reset();
  quality_validator_.reset();
}

void VEAClient::UpdateTestStreamData(bool mid_stream_bitrate_switch,
                                     bool mid_stream_framerate_switch) {
  // Use defaults for bitrate/framerate if they are not provided.
  if (test_stream_->requested_bitrate == 0)
    requested_bitrate_ = kDefaultBitrate;
  else
    requested_bitrate_ = test_stream_->requested_bitrate;

  if (test_stream_->requested_framerate == 0)
    requested_framerate_ = kDefaultFramerate;
  else
    requested_framerate_ = test_stream_->requested_framerate;

  // If bitrate/framerate switch is requested, use the subsequent values if
  // provided, or, if not, calculate them from their initial values using
  // the default ratios.
  // Otherwise, if a switch is not requested, keep the initial values.
  if (mid_stream_bitrate_switch) {
    if (test_stream_->requested_subsequent_bitrate == 0)
      requested_subsequent_bitrate_ =
          requested_bitrate_ * kDefaultSubsequentBitrateRatio;
    else
      requested_subsequent_bitrate_ =
          test_stream_->requested_subsequent_bitrate;
  } else {
    requested_subsequent_bitrate_ = requested_bitrate_;
  }
  if (requested_subsequent_bitrate_ == 0)
    requested_subsequent_bitrate_ = 1;

  if (mid_stream_framerate_switch) {
    if (test_stream_->requested_subsequent_framerate == 0)
      requested_subsequent_framerate_ =
          requested_framerate_ * kDefaultSubsequentFramerateRatio;
    else
      requested_subsequent_framerate_ =
          test_stream_->requested_subsequent_framerate;
  } else {
    requested_subsequent_framerate_ = requested_framerate_;
  }
  if (requested_subsequent_framerate_ == 0)
    requested_subsequent_framerate_ = 1;
}

double VEAClient::frames_per_second() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(num_encoded_frames_ != 0UL);
  base::TimeDelta duration = last_frame_ready_time_ - first_frame_start_time_;
  return num_encoded_frames_ / duration.InSecondsF();
}

void VEAClient::RequireBitstreamBuffers(unsigned int input_count,
                                        const gfx::Size& input_coded_size,
                                        size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_EQ(CS_INITIALIZED, state_);
  SetState(CS_ENCODING);

  if (quality_validator_)
    quality_validator_->Initialize(input_coded_size,
                                   gfx::Rect(test_stream_->visible_size));

  CreateAlignedInputStreamFile(input_coded_size, test_stream_);

  num_frames_to_encode_ = test_stream_->num_frames;
  if (g_num_frames_to_encode > 0)
    num_frames_to_encode_ = g_num_frames_to_encode;

  // We may need to loop over the stream more than once if more frames than
  // provided is required for bitrate tests.
  if (force_bitrate_ && num_frames_to_encode_ < kMinFramesForBitrateTests) {
    DVLOG(1) << "Stream too short for bitrate test ("
             << test_stream_->num_frames << " frames), will loop it to reach "
             << kMinFramesForBitrateTests << " frames";
    num_frames_to_encode_ = kMinFramesForBitrateTests;
  }
  if (save_to_file_ && IsVP8(test_stream_->requested_profile))
    WriteIvfFileHeader();

  input_coded_size_ = input_coded_size;
  num_required_input_buffers_ = input_count;
  ASSERT_GT(num_required_input_buffers_, 0UL);

  output_buffer_size_ = output_size;
  ASSERT_GT(output_buffer_size_, 0UL);

  for (unsigned int i = 0; i < kNumOutputBuffers; ++i) {
    auto shm = std::make_unique<base::SharedMemory>();
    LOG_ASSERT(shm->CreateAndMapAnonymous(output_buffer_size_));
    FeedEncoderWithOutput(shm.get());
    output_shms_.push_back(std::move(shm));
  }

  if (g_env->run_at_fps()) {
    input_timer_.reset(new base::RepeatingTimer());
    input_timer_->Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1) / current_framerate_,
        base::BindRepeating(&VEAClient::OnInputTimer, base::Unretained(this)));
  } else {
    while (inputs_at_client_.size() <
           num_required_input_buffers_ + kNumExtraInputFrames)
      FeedEncoderWithOneInput();
  }
}

void VEAClient::VerifyOutputTimestamp(base::TimeDelta timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // One input frame may be mapped to multiple output frames, so the current
  // timestamp should be equal to previous timestamp or the top of
  // frame_timestamps_.
  if (timestamp != previous_timestamp_) {
    ASSERT_TRUE(!frame_timestamps_.empty());
    EXPECT_EQ(frame_timestamps_.front(), timestamp);
    previous_timestamp_ = frame_timestamps_.front();
    frame_timestamps_.pop();
  }
}

void VEAClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_LE(metadata.payload_size_bytes, output_buffer_size_);

  IdToSHM::iterator it = output_buffers_at_client_.find(bitstream_buffer_id);
  ASSERT_NE(it, output_buffers_at_client_.end());
  base::SharedMemory* shm = it->second;
  output_buffers_at_client_.erase(it);

  if (state_ == CS_FLUSHED || state_ == CS_VALIDATED)
    return;

  // When flush is completed, VEA may return an extra empty buffer. Skip
  // checking the buffer.
  if (verify_output_timestamp_ && metadata.payload_size_bytes > 0) {
    VerifyOutputTimestamp(metadata.timestamp);
  }

  encoded_stream_size_since_last_check_ += metadata.payload_size_bytes;

  const uint8_t* stream_ptr = static_cast<const uint8_t*>(shm->memory());
  if (metadata.payload_size_bytes > 0) {
    if (stream_validator_) {
      stream_validator_->ProcessStreamBuffer(stream_ptr,
                                             metadata.payload_size_bytes);
    } else {
      // We don't know the visible size of without stream validator, just
      // send the expected value to pass the check.
      HandleEncodedFrame(metadata.key_frame, test_stream_->visible_size);
    }

    if (quality_validator_) {
      scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
          static_cast<const uint8_t*>(shm->memory()),
          static_cast<int>(metadata.payload_size_bytes)));
      quality_validator_->AddDecodeBuffer(buffer);
    }
    // If the encoder does not support flush, pretend flush is done when all
    // frames are received.
    if (!encoder_->IsFlushSupported() &&
        num_encoded_frames_ == num_frames_to_encode_) {
      FlushEncoderSuccessfully();
    }

    if (save_to_file_) {
      if (IsVP8(test_stream_->requested_profile))
        WriteIvfFrameHeader(num_encoded_frames_ - 1,
                            metadata.payload_size_bytes);

      EXPECT_TRUE(base::AppendToFile(
          base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
          static_cast<char*>(shm->memory()),
          base::checked_cast<int>(metadata.payload_size_bytes)));
    }
  }

  FeedEncoderWithOutput(shm);
}

void VEAClient::SetState(ClientState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(4) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void VEAClient::SetStreamParameters(unsigned int bitrate,
                                    unsigned int framerate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_requested_bitrate_ = bitrate;
  current_framerate_ = framerate;
  LOG_ASSERT(current_requested_bitrate_ > 0UL);
  LOG_ASSERT(current_framerate_ > 0UL);
  encoder_->RequestEncodingParametersChange(bitrate, framerate);
  DVLOG(1) << "Switched parameters to " << current_requested_bitrate_
           << " bps @ " << current_framerate_ << " FPS";
}

void VEAClient::InputNoLongerNeededCallback(int32_t input_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::set<int32_t>::iterator it = inputs_at_client_.find(input_id);
  ASSERT_NE(it, inputs_at_client_.end());
  inputs_at_client_.erase(it);
  if (!g_env->run_at_fps())
    FeedEncoderWithOneInput();
}

scoped_refptr<VideoFrame> VEAClient::CreateFrame(off_t position) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_GT(current_framerate_, 0U);

  size_t num_planes = VideoFrame::NumPlanes(test_stream_->pixel_format);
  CHECK_LE(num_planes, 3u);
  uint8_t* frame_data[3] = {};
  size_t plane_stride[3] = {};
  frame_data[0] =
      reinterpret_cast<uint8_t*>(&test_stream_->aligned_in_file_data[0]) +
      position;
  for (size_t i = 1; i < num_planes; i++) {
    frame_data[i] = frame_data[i - 1] + test_stream_->aligned_plane_size[i - 1];
  }
  for (size_t i = 0; i < num_planes; i++) {
    plane_stride[i] = VideoFrame::RowBytes(i, test_stream_->pixel_format,
                                           input_coded_size_.width());
  }

  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
      test_stream_->pixel_format, input_coded_size_,
      gfx::Rect(test_stream_->visible_size), test_stream_->visible_size,
      plane_stride[0], plane_stride[1], plane_stride[2], frame_data[0],
      frame_data[1], frame_data[2],
      // Timestamp needs to avoid starting from 0.
      base::TimeDelta().FromMilliseconds((next_input_id_ + 1) *
                                         base::Time::kMillisecondsPerSecond /
                                         current_framerate_));
  EXPECT_NE(nullptr, video_frame.get());
  return video_frame;
}

scoped_refptr<VideoFrame> VEAClient::PrepareInputFrame(off_t position,
                                                       int32_t* input_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_LE(position + test_stream_->aligned_buffer_size,
           test_stream_->aligned_in_file_data.size());

  scoped_refptr<VideoFrame> frame = CreateFrame(position);
  EXPECT_TRUE(frame);
  frame->AddDestructionObserver(BindToCurrentLoop(
      base::BindOnce(&VEAClient::InputNoLongerNeededCallback,
                     base::Unretained(this), next_input_id_)));

  LOG_ASSERT(inputs_at_client_.insert(next_input_id_).second);

  *input_id = next_input_id_++;
  return frame;
}

void VEAClient::OnInputTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!has_encoder() || state_ != CS_ENCODING)
    input_timer_.reset();
  else if (inputs_at_client_.size() <
           num_required_input_buffers_ + kNumExtraInputFrames)
    FeedEncoderWithOneInput();
  else
    DVLOG(1) << "Dropping input frame";
}

void VEAClient::FeedEncoderWithOneInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder() || state_ != CS_ENCODING ||
      num_frames_submitted_to_encoder_ == num_frames_to_encode_) {
    return;
  }

  size_t bytes_left =
      test_stream_->aligned_in_file_data.size() - pos_in_input_stream_;
  if (bytes_left < test_stream_->aligned_buffer_size) {
    DCHECK_EQ(bytes_left, 0UL);
    // Rewind if at the end of stream and we are still encoding.
    // This is to flush the encoder with additional frames from the beginning
    // of the stream, or if the stream is shorter that the number of frames
    // we require for bitrate tests.
    pos_in_input_stream_ = 0;
  }

  if (quality_validator_)
    quality_validator_->AddOriginalFrame(CreateFrame(pos_in_input_stream_));

  int32_t input_id;
  scoped_refptr<VideoFrame> video_frame =
      PrepareInputFrame(pos_in_input_stream_, &input_id);
  frame_timestamps_.push(video_frame->timestamp());
  pos_in_input_stream_ += static_cast<off_t>(test_stream_->aligned_buffer_size);

  if (input_id == 0) {
    first_frame_start_time_ = base::TimeTicks::Now();
  }

  if (g_env->needs_encode_latency()) {
    LOG_ASSERT(input_id == static_cast<int32_t>(encode_start_time_.size()));
    encode_start_time_.push_back(base::TimeTicks::Now());
  }

  bool force_keyframe = (keyframe_period_ && input_id % keyframe_period_ == 0);
  if (force_keyframe) {
    // Because we increase |num_frames_submitted_to_encoder_| after calling
    // Encode(), the value here is actually 0-based frame index.
    keyframe_indices_.push(num_frames_submitted_to_encoder_);
  }
  encoder_->Encode(video_frame, force_keyframe);
  ++num_frames_submitted_to_encoder_;
  if (num_frames_submitted_to_encoder_ == num_frames_to_encode_) {
    FlushEncoder();
  }
}

void VEAClient::FeedEncoderWithOutput(base::SharedMemory* shm) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;

  if (state_ != CS_ENCODING && state_ != CS_FLUSHING)
    return;

  base::SharedMemoryHandle dup_handle = shm->handle().Duplicate();
  LOG_ASSERT(dup_handle.IsValid());

  // TODO(erikchen): This may leak the SharedMemoryHandle.
  // https://crbug.com/640840.
  BitstreamBuffer bitstream_buffer(next_output_buffer_id_++, dup_handle,
                                   output_buffer_size_);
  LOG_ASSERT(output_buffers_at_client_
                 .insert(std::make_pair(bitstream_buffer.id(), shm))
                 .second);

  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

bool VEAClient::HandleEncodedFrame(bool keyframe,
                                   const gfx::Size& visible_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This would be a bug in the test, which should not ignore false
  // return value from this method.
  LOG_ASSERT(num_encoded_frames_ <= num_frames_to_encode_);

  last_frame_ready_time_ = base::TimeTicks::Now();

  if (g_env->needs_encode_latency()) {
    LOG_ASSERT(num_encoded_frames_ < encode_start_time_.size());
    base::TimeTicks start_time = encode_start_time_[num_encoded_frames_];
    LOG_ASSERT(!start_time.is_null());
    encode_latencies_.push_back(last_frame_ready_time_ - start_time);
  }

  ++num_encoded_frames_;
  ++num_frames_since_last_check_;

  // Because the keyframe behavior requirements are loose, we give
  // the encoder more freedom here. It could either deliver a keyframe
  // immediately after we requested it, which could be for a frame number
  // before the one we requested it for (if the keyframe request
  // is asynchronous, i.e. not bound to any concrete frame, and because
  // the pipeline can be deeper than one frame), at that frame, or after.
  // So the only constraints we put here is that we get a keyframe not
  // earlier than we requested one (in time), and not later than
  // kMaxKeyframeDelay frames after the frame, for which we requested
  // it, comes back encoded.
  if (!keyframe_indices_.empty()) {
    // Convert to 0-based index for encoded frame.
    const unsigned int frame_index = num_encoded_frames_ - 1;
    if (keyframe) {
      EXPECT_LE(frame_index, keyframe_indices_.front() + kMaxKeyframeDelay);
      keyframe_indices_.pop();
    } else {
      EXPECT_LT(frame_index, keyframe_indices_.front() + kMaxKeyframeDelay);
    }
  }

  EXPECT_EQ(test_stream_->visible_size, visible_size);

  if (num_encoded_frames_ == num_frames_to_encode_ / 2) {
    VerifyStreamProperties();
    if (requested_subsequent_bitrate_ != current_requested_bitrate_ ||
        requested_subsequent_framerate_ != current_framerate_) {
      SetStreamParameters(requested_subsequent_bitrate_,
                          requested_subsequent_framerate_);
      if (g_env->run_at_fps() && input_timer_)
        input_timer_->Start(
            FROM_HERE, base::TimeDelta::FromSeconds(1) / current_framerate_,
            base::BindRepeating(&VEAClient::OnInputTimer,
                                base::Unretained(this)));
    }
  } else if (num_encoded_frames_ == num_frames_to_encode_) {
    LogPerf();
    VerifyMinFPS();
    VerifyStreamProperties();
    // We might receive the last frame before calling Flush(). In this case we
    // set the state to CS_FLUSHING first to bypass the state transition check.
    if (state_ == CS_ENCODING)
      SetState(CS_FLUSHING);
    SetState(CS_FINISHED);
    if (verify_output_timestamp_) {
      // There may be some timestamps left because we push extra frames to flush
      // encoder.
      EXPECT_LE(frame_timestamps_.size(),
                static_cast<size_t>(next_input_id_ - num_frames_to_encode_));
    }
    return false;
  }

  return true;
}

void VEAClient::LogPerf() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_env->LogToFile("Measured encoder FPS",
                   base::StringPrintf("%.3f", frames_per_second()));

  // Log encode latencies.
  if (g_env->needs_encode_latency()) {
    std::sort(encode_latencies_.begin(), encode_latencies_.end());
    for (const auto& percentile : kLoggedLatencyPercentiles) {
      base::TimeDelta latency = Percentile(encode_latencies_, percentile);
      g_env->LogToFile(
          base::StringPrintf("Encode latency for the %dth percentile",
                             percentile),
          base::StringPrintf("%" PRId64 " us", latency.InMicroseconds()));
    }
  }
}

void VEAClient::FlushEncoder() {
  // In order to guarantee the order between encoder.Encode() and
  // encoder.Flush(), this method should be called from the same thread as
  // encoder.Encode().
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(num_frames_submitted_to_encoder_ == num_frames_to_encode_);

  if (!encoder_->IsFlushSupported())
    return;

  encoder_->Flush(
      base::BindOnce(&VEAClient::FlushEncoderDone, base::Unretained(this)));
  // We might receive the last frame before calling Flush(). In this case we set
  // the state to CS_FLUSHING when receiving the last frame.
  if (state_ != CS_FINISHED)
    SetState(CS_FLUSHING);

  flush_timeout_.Reset(
      base::BindRepeating(&VEAClient::FlushTimeout, base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, flush_timeout_.callback(),
      base::TimeDelta::FromMilliseconds(kFlushTimeoutMs));
}

void VEAClient::FlushEncoderDone(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  flush_timeout_.Cancel();
  LOG_ASSERT(num_frames_submitted_to_encoder_ == num_frames_to_encode_);

  if (!success || num_encoded_frames_ != num_frames_to_encode_) {
    SetState(CS_ERROR);
    return;
  }
  FlushEncoderSuccessfully();
}

void VEAClient::FlushEncoderSuccessfully() {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetState(CS_FLUSHED);
  if (!quality_validator_) {
    SetState(CS_VALIDATED);
  } else {
    // Insert EOS buffer to flush the decoder.
    quality_validator_->Flush();
  }
}

void VEAClient::FlushTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << "Flush timeout.";
  SetState(CS_ERROR);
}

void VEAClient::VerifyMinFPS() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (test_perf_)
    EXPECT_GE(frames_per_second(), kMinPerfFPS);
}

void VEAClient::VerifyStreamProperties() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(num_frames_since_last_check_ > 0UL);
  LOG_ASSERT(encoded_stream_size_since_last_check_ > 0UL);
  unsigned int bitrate = static_cast<unsigned int>(
      encoded_stream_size_since_last_check_ * 8 * current_framerate_ /
      num_frames_since_last_check_);
  DVLOG(1) << "Current chunk's bitrate: " << bitrate
           << " (expected: " << current_requested_bitrate_ << " @ "
           << current_framerate_ << " FPS,"
           << " num frames in chunk: " << num_frames_since_last_check_;

  num_frames_since_last_check_ = 0;
  encoded_stream_size_since_last_check_ = 0;

  if (force_bitrate_) {
    EXPECT_NEAR(bitrate, current_requested_bitrate_,
                kBitrateTolerance * current_requested_bitrate_);
  }
}

void VEAClient::WriteIvfFileHeader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  IvfFileHeader header = {};
  memcpy(header.signature, kIvfHeaderSignature, sizeof(header.signature));
  header.version = 0;
  header.header_size = sizeof(header);
  header.fourcc = 0x30385056;  // VP80
  header.width =
      base::checked_cast<uint16_t>(test_stream_->visible_size.width());
  header.height =
      base::checked_cast<uint16_t>(test_stream_->visible_size.height());
  header.timebase_denum = requested_framerate_;
  header.timebase_num = 1;
  header.num_frames = num_frames_to_encode_;
  header.ByteSwap();

  EXPECT_TRUE(base::AppendToFile(
      base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
      reinterpret_cast<char*>(&header), sizeof(header)));
}

void VEAClient::WriteIvfFrameHeader(int frame_index, size_t frame_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  IvfFrameHeader header = {};

  header.frame_size = static_cast<uint32_t>(frame_size);
  header.timestamp = frame_index;
  header.ByteSwap();
  EXPECT_TRUE(base::AppendToFile(
      base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
      reinterpret_cast<char*>(&header), sizeof(header)));
}

// Base class for simple VEA Clients
class SimpleVEAClientBase : public VEAClientBase {
 public:
  void CreateEncoder();
  void DestroyEncoder();

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;

 protected:
  SimpleVEAClientBase(ClientStateNotification<ClientState>* note,
                      const int width,
                      const int height);

  void SetState(ClientState new_state) override;

  // Provide the encoder with a new output buffer.
  void FeedEncoderWithOutput(base::SharedMemory* shm, size_t output_size);

  const int width_;
  const int height_;
  const int bitrate_;
  const int fps_;
};

SimpleVEAClientBase::SimpleVEAClientBase(
    ClientStateNotification<ClientState>* note,
    const int width,
    const int height)
    : VEAClientBase(note),
      width_(width),
      height_(height),
      bitrate_(200000),
      fps_(30) {
  thread_checker_.DetachFromThread();
}

void SimpleVEAClientBase::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(!has_encoder());
  LOG_ASSERT(g_env->test_streams_.size());

  gfx::Size visible_size(width_, height_);
  const VideoEncodeAccelerator::Config config(
      g_env->test_streams_[0]->pixel_format, visible_size,
      g_env->test_streams_[0]->requested_profile, bitrate_, fps_);
  encoder_ = CreateVideoEncodeAccelerator(config, this, gpu::GpuPreferences());
  if (!encoder_) {
    LOG(ERROR) << "Failed creating a VideoEncodeAccelerator.";
    SetState(CS_ERROR);
    return;
  }
  encoder_->RequestEncodingParametersChange(bitrate_, fps_);
  SetState(CS_INITIALIZED);
}

void SimpleVEAClientBase::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;
  // Clear the objects that should be destroyed on the same thread as creation.
  encoder_.reset();
}

void SimpleVEAClientBase::SetState(ClientState new_state) {
  DVLOG(4) << "Changing state to " << new_state;
  note_->Notify(new_state);
}

void SimpleVEAClientBase::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_ENCODING);
  ASSERT_GT(output_size, 0UL);

  for (unsigned int i = 0; i < kNumOutputBuffers; ++i) {
    auto shm = std::make_unique<base::SharedMemory>();
    LOG_ASSERT(shm->CreateAndMapAnonymous(output_size));
    FeedEncoderWithOutput(shm.get(), output_size);
    output_shms_.push_back(std::move(shm));
  }
}

void SimpleVEAClientBase::FeedEncoderWithOutput(base::SharedMemory* shm,
                                                size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;

  base::SharedMemoryHandle dup_handle = shm->handle().Duplicate();
  LOG_ASSERT(dup_handle.IsValid());

  BitstreamBuffer bitstream_buffer(next_output_buffer_id_++, dup_handle,
                                   output_size);
  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

// This client is only used to make sure the encoder does not return an encoded
// frame before getting any input.
class VEANoInputClient : public SimpleVEAClientBase {
 public:
  explicit VEANoInputClient(ClientStateNotification<ClientState>* note);
  void DestroyEncoder();

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;

 private:
  // The timer used to monitor the encoder doesn't return an output buffer in
  // a period of time.
  std::unique_ptr<base::OneShotTimer> timer_;
};

VEANoInputClient::VEANoInputClient(ClientStateNotification<ClientState>* note)
    : SimpleVEAClientBase(note, 320, 240) {}

void VEANoInputClient::DestroyEncoder() {
  SimpleVEAClientBase::DestroyEncoder();
  // Clear the objects that should be destroyed on the same thread as creation.
  timer_.reset();
}

void VEANoInputClient::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SimpleVEAClientBase::RequireBitstreamBuffers(input_count, input_coded_size,
                                               output_size);

  // Timer is used to make sure there is no output frame in 100ms.
  timer_.reset(new base::OneShotTimer());
  timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(100),
                base::Bind(&VEANoInputClient::SetState, base::Unretained(this),
                           CS_FINISHED));
}

void VEANoInputClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_ERROR);
}

// This client is only used to test input frame with the size of U and V planes
// unaligned to cache line.
// To have both width and height divisible by 16 but not 32 will make the size
// of U/V plane (width * height / 4) unaligned to 128-byte cache line.
class VEACacheLineUnalignedInputClient : public SimpleVEAClientBase {
 public:
  explicit VEACacheLineUnalignedInputClient(
      ClientStateNotification<ClientState>* note);

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;

 private:
  // Feed the encoder with one input frame.
  void FeedEncoderWithOneInput(const gfx::Size& input_coded_size);
};

VEACacheLineUnalignedInputClient::VEACacheLineUnalignedInputClient(
    ClientStateNotification<ClientState>* note)
    : SimpleVEAClientBase(note, 368, 368) {
}  // 368 is divisible by 16 but not 32

void VEACacheLineUnalignedInputClient::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SimpleVEAClientBase::RequireBitstreamBuffers(input_count, input_coded_size,
                                               output_size);

  FeedEncoderWithOneInput(input_coded_size);
}

void VEACacheLineUnalignedInputClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // It's enough to encode just one frame. If plane size is not aligned,
  // VideoEncodeAccelerator::Encode will fail.
  SetState(CS_FINISHED);
}

void VEACacheLineUnalignedInputClient::FeedEncoderWithOneInput(
    const gfx::Size& input_coded_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;

  const VideoPixelFormat pixel_format = g_env->test_streams_[0]->pixel_format;
  size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  CHECK_LE(num_planes, 3u);
  std::vector<char, AlignedAllocator<char, kPlatformBufferAlignment>>
      aligned_data[3];
  uint8_t* frame_data[3] = {};
  uint8_t plane_stride[3] = {};
  for (size_t i = 0; i < num_planes; i++) {
    aligned_data[i].resize(
        VideoFrame::PlaneSize(pixel_format, i, input_coded_size).GetArea());
    frame_data[i] = reinterpret_cast<uint8_t*>(aligned_data[i].data());
    plane_stride[i] =
        VideoFrame::RowBytes(i, pixel_format, input_coded_size.width());
  }

  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
      pixel_format, input_coded_size, gfx::Rect(input_coded_size),
      input_coded_size, plane_stride[0], plane_stride[1], plane_stride[2],
      frame_data[0], frame_data[1], frame_data[2],
      base::TimeDelta().FromMilliseconds(base::Time::kMillisecondsPerSecond /
                                         fps_));

  encoder_->Encode(video_frame, false);
}

// Test parameters:
// - Number of concurrent encoders. The value takes effect when there is only
//   one input stream; otherwise, one encoder per input stream will be
//   instantiated.
// - If true, save output to file (provided an output filename was supplied).
// - Force a keyframe every n frames.
// - Force bitrate; the actual required value is provided as a property
//   of the input stream, because it depends on stream type/resolution/etc.
// - If true, measure performance.
// - If true, switch bitrate mid-stream.
// - If true, switch framerate mid-stream.
// - If true, verify the output frames of encoder.
// - If true, verify the timestamps of output frames.
class VideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<
          std::tuple<int, bool, int, bool, bool, bool, bool, bool, bool>> {};

TEST_P(VideoEncodeAcceleratorTest, TestSimpleEncode) {
  size_t num_concurrent_encoders = std::get<0>(GetParam());
  const bool save_to_file = std::get<1>(GetParam());
  const unsigned int keyframe_period = std::get<2>(GetParam());
  const bool force_bitrate = std::get<3>(GetParam());
  const bool test_perf = std::get<4>(GetParam());
  const bool mid_stream_bitrate_switch = std::get<5>(GetParam());
  const bool mid_stream_framerate_switch = std::get<6>(GetParam());
  const bool verify_output =
      std::get<7>(GetParam()) || g_env->verify_all_output();
  const bool verify_output_timestamp = std::get<8>(GetParam());

  std::vector<std::unique_ptr<ClientStateNotification<ClientState>>> notes;
  std::vector<std::unique_ptr<VEAClient>> clients;
  base::Thread vea_client_thread("EncoderClientThread");
  ASSERT_TRUE(vea_client_thread.Start());

  if (g_env->test_streams_.size() > 1)
    num_concurrent_encoders = g_env->test_streams_.size();

  // Create all encoders.
  for (size_t i = 0; i < num_concurrent_encoders; i++) {
    size_t test_stream_index = i % g_env->test_streams_.size();
    // Disregard save_to_file if we didn't get an output filename.
    bool encoder_save_to_file =
        (save_to_file &&
         !g_env->test_streams_[test_stream_index]->out_filename.empty());

    notes.push_back(std::make_unique<ClientStateNotification<ClientState>>());
    clients.push_back(std::make_unique<VEAClient>(
        g_env->test_streams_[test_stream_index].get(), notes.back().get(),
        encoder_save_to_file, keyframe_period, force_bitrate, test_perf,
        mid_stream_bitrate_switch, mid_stream_framerate_switch, verify_output,
        verify_output_timestamp));

    vea_client_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VEAClient::CreateEncoder,
                                  base::Unretained(clients.back().get())));
  }

  // All encoders must pass through states in this order.
  enum ClientState state_transitions[] = {CS_INITIALIZED, CS_ENCODING,
                                          CS_FLUSHING,    CS_FINISHED,
                                          CS_FLUSHED,     CS_VALIDATED};

  // Wait for all encoders to go through all states and finish.
  // Do this by waiting for all encoders to advance to state n before checking
  // state n+1, to verify that they are able to operate concurrently.
  // It also simulates the real-world usage better, as the main thread, on which
  // encoders are created/destroyed, is a single GPU Process ChildThread.
  // Moreover, we can't have proper multithreading on X11, so this could cause
  // hard to debug issues there, if there were multiple "ChildThreads".
  for (const auto& state : state_transitions) {
    for (size_t i = 0; i < num_concurrent_encoders && !HasFailure(); i++) {
      EXPECT_EQ(state, notes[i]->Wait());
    }
    if (HasFailure()) {
      break;
    }
  }

  for (size_t i = 0; i < num_concurrent_encoders; ++i) {
    vea_client_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VEAClient::DestroyEncoder,
                                  base::Unretained(clients[i].get())));
  }

  // This ensures all tasks have finished.
  vea_client_thread.Stop();
}

// Test parameters:
// - Test type
//   0: No input test
//   1: Cache line-unaligned test
class VideoEncodeAcceleratorSimpleTest : public ::testing::TestWithParam<int> {
};

template <class TestClient>
void SimpleTestFunc() {
  std::unique_ptr<ClientStateNotification<ClientState>> note(
      new ClientStateNotification<ClientState>());
  std::unique_ptr<TestClient> client(new TestClient(note.get()));
  base::Thread vea_client_thread("EncoderClientThread");
  ASSERT_TRUE(vea_client_thread.Start());

  vea_client_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestClient::CreateEncoder,
                                base::Unretained(client.get())));

  // Encoder must pass through states in this order.
  enum ClientState state_transitions[] = {CS_INITIALIZED, CS_ENCODING,
                                          CS_FINISHED};

  for (const auto& state : state_transitions) {
    EXPECT_EQ(state, note->Wait());
    if (testing::Test::HasFailure()) {
      break;
    }
  }

  vea_client_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestClient::DestroyEncoder,
                                base::Unretained(client.get())));

  // This ensures all tasks have finished.
  vea_client_thread.Stop();
}

TEST_P(VideoEncodeAcceleratorSimpleTest, TestSimpleEncode) {
  const int test_type = GetParam();
  ASSERT_LT(test_type, 2) << "Invalid test type=" << test_type;

  if (test_type == 0)
    SimpleTestFunc<VEANoInputClient>();
  else if (test_type == 1)
    SimpleTestFunc<VEACacheLineUnalignedInputClient>();
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// TODO(kcwu): add back test of verify_output=true after
// https://crbug.com/694131 fixed.
INSTANTIATE_TEST_CASE_P(
    SimpleEncode,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, true, 0, false, false, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    EncoderPerf,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, true, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(ForceKeyframes,
                        VideoEncodeAcceleratorTest,
                        ::testing::Values(std::make_tuple(1,
                                                          false,
                                                          10,
                                                          false,
                                                          false,
                                                          false,
                                                          false,
                                                          false,
                                                          false)));

INSTANTIATE_TEST_CASE_P(
    ForceBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, true, false, false, false)));

// TODO(kcwu): add back bitrate test after https://crbug.com/693336 fixed.
INSTANTIATE_TEST_CASE_P(
    DISABLED_MidStreamParamSwitchFPS,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, false, true, false, false)));

INSTANTIATE_TEST_CASE_P(
    MultipleEncoders,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(3, false, 0, false, false, false, false, false, false),
        std::make_tuple(3, false, 0, true, false, true, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    VerifyTimestamp,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, false, false, false, false, true)));

INSTANTIATE_TEST_CASE_P(NoInputTest,
                        VideoEncodeAcceleratorSimpleTest,
                        ::testing::Values(0));

INSTANTIATE_TEST_CASE_P(CacheLineUnalignedInputTest,
                        VideoEncodeAcceleratorSimpleTest,
                        ::testing::Values(1));

#elif defined(OS_MACOSX) || defined(OS_WIN)
INSTANTIATE_TEST_CASE_P(
    SimpleEncode,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, true, 0, false, false, false, false, false, false),
        std::make_tuple(1, true, 0, false, false, false, false, true, false)));

INSTANTIATE_TEST_CASE_P(
    EncoderPerf,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, true, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(MultipleEncoders,
                        VideoEncodeAcceleratorTest,
                        ::testing::Values(std::make_tuple(3,
                                                          false,
                                                          0,
                                                          false,
                                                          false,
                                                          false,
                                                          false,
                                                          false,
                                                          false)));

INSTANTIATE_TEST_CASE_P(
    VerifyTimestamp,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, false, false, false, false, true)));

#if defined(OS_WIN)
INSTANTIATE_TEST_CASE_P(
    ForceBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, false, false, false, false)));
#endif  // defined(OS_WIN)

#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

// TODO(posciak): more tests:
// - async FeedEncoderWithOutput
// - out-of-order return of outputs to encoder
// - multiple encoders + decoders
// - mid-stream encoder_->Destroy()

class VEATestSuite : public base::TestSuite {
 public:
  VEATestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  int Run() {
    base::test::ScopedTaskEnvironment scoped_task_environment;
    media::g_env =
        reinterpret_cast<media::VideoEncodeAcceleratorTestEnvironment*>(
            testing::AddGlobalTestEnvironment(
                new media::VideoEncodeAcceleratorTestEnvironment(
                    std::move(media::g_test_stream_data), media::g_log_path,
                    media::g_frame_stats_path, media::g_run_at_fps,
                    media::g_needs_encode_latency,
                    media::g_verify_all_output)));

#if BUILDFLAG(USE_VAAPI)
    media::VaapiWrapper::PreSandboxInitialization();
#elif defined(OS_WIN)
    media::MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization();
#endif
    return base::TestSuite::Run();
  }
};

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  mojo::core::Init();
  media::VEATestSuite test_suite(argc, argv);

  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_stream_data") {
      media::g_test_stream_data->assign(it->second.c_str());
      continue;
    }
    // Output machine-readable logs with fixed formats to a file.
    if (it->first == "output_log") {
      media::g_log_path = base::FilePath(
          base::FilePath::StringType(it->second.begin(), it->second.end()));
      continue;
    }
    if (it->first == "num_frames_to_encode") {
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToInt(input, &media::g_num_frames_to_encode));
      continue;
    }
    if (it->first == "measure_latency") {
      media::g_needs_encode_latency = true;
      continue;
    }
    if (it->first == "fake_encoder") {
      media::g_fake_encoder = true;
      continue;
    }
    if (it->first == "run_at_fps") {
      media::g_run_at_fps = true;
      continue;
    }
    if (it->first == "verify_all_output") {
      media::g_verify_all_output = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "ozone-platform" || it->first == "ozone-use-surfaceless")
      continue;

    // Output per-frame metrics to a csv file.
    if (it->first == "frame_stats") {
      media::g_frame_stats_path = base::FilePath(
          base::FilePath::StringType(it->second.begin(), it->second.end()));
      continue;
    }
  }

  if (media::g_needs_encode_latency && !media::g_run_at_fps) {
    // Encode latency can only be measured with --run_at_fps. Otherwise, we get
    // skewed results since it may queue too many frames at once with the same
    // encode start time.
    LOG(FATAL) << "--measure_latency requires --run_at_fps enabled to work.";
  }

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&media::VEATestSuite::Run, base::Unretained(&test_suite)));
}
