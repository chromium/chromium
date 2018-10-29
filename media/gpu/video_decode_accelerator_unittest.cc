// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The bulk of this file is support code; sorry about that.  Here's an overview
// to hopefully help readers of this code:
// - RenderingHelper is charged with interacting with X11/{EGL/GLES2,GLX/GL} or
//   Win/EGL.
// - ClientState is an enum for the state of the decode client used by the test.
// - ClientStateNotification is a barrier abstraction that allows the test code
//   to be written sequentially and wait for the decode client to see certain
//   state transitions.
// - GLRenderingVDAClient is a VideoDecodeAccelerator::Client implementation
// - Finally actual TEST cases are at the bottom of this file, using the above
//   infrastructure.

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/fake_video_decode_accelerator.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/test/rendering_helper.h"
#include "media/gpu/test/video_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/video/h264_parser.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_image.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"
#endif  // defined(OS_WIN)
#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if defined(OS_CHROMEOS)
#include "ui/ozone/public/ozone_platform.h"
#endif  // defined(OS_CHROMEOS)

namespace media {

namespace {

// Values optionally filled in from flags; see main() below.
// The syntax of multiple test videos is:
//  test-video1;test-video2;test-video3
// where only the first video is required and other optional videos would be
// decoded by concurrent decoders.
// The syntax of each test-video is:
//  filename:width:height:numframes:numfragments:minFPSwithRender:minFPSnoRender
// where only the first field is required.  Value details:
// - |filename| must be an h264 Annex B (NAL) stream or an IVF VP8/9 stream.
// - |width| and |height| are in pixels.
// - |numframes| is the number of picture frames in the file.
// - |numfragments| NALU (h264) or frame (VP8/9) count in the stream.
// - |minFPSwithRender| and |minFPSnoRender| are minimum frames/second speeds
//   expected to be achieved with and without rendering to the screen, resp.
//   (the latter tests just decode speed).
// - |profile| is the VideoCodecProfile set during Initialization.
// An empty value for a numeric field means "ignore".
const base::FilePath::CharType* g_test_video_data =
    // FILE_PATH_LITERAL("test-25fps.vp8:320:240:250:250:50:175:11");
    FILE_PATH_LITERAL("test-25fps.h264:320:240:250:258:50:175:1");

// The file path of the test output log. This is used to communicate the test
// results to CrOS autotests. We can enable the log and specify the filename by
// the "--output_log" switch.
const base::FilePath::CharType* g_output_log = NULL;

// The value is set by the switch "--rendering_fps".
double g_rendering_fps = 60;

bool g_use_gl_renderer = true;

// Validate each decoded frame on thumbnail test case.
// TODO(crbug.com/856562): Enable VideoFrameValidator by default if
// |g_test_import| is true.
bool g_frame_validator = false;

// The value is set by the switch "--num_play_throughs". The video will play
// the specified number of times. In different test cases, we have different
// values for |num_play_throughs|. This setting will override the value. A
// special value "0" means no override.
size_t g_num_play_throughs = 0;

// Fake decode
bool g_fake_decoder = 0;

// Test buffer import into VDA, providing buffers allocated by us, instead of
// requesting the VDA itself to allocate buffers.
bool g_test_import = false;

// This is the location of the test files. If empty, they're in the current
// working directory.
base::FilePath g_test_file_path;

// The location to output bad thumbnail image. If empty or invalid, fallback to
// the original location.
base::FilePath g_thumbnail_output_dir;

// Environment to store rendering thread.
media::test::VideoDecodeAcceleratorTestEnvironment* g_env;

constexpr size_t kMaxResetAfterFrameNum = 100;
constexpr size_t kMaxFramesToDelayReuse = 64;
const base::TimeDelta kReuseDelay = base::TimeDelta::FromSeconds(1);
// Simulate WebRTC and call VDA::Decode 30 times per second.
constexpr size_t kWebRtcDecodeCallsPerSecond = 30;
// Simulate an adjustment to a larger number of pictures to make sure the
// decoder supports an upwards adjustment.
constexpr size_t kExtraPictureBuffers = 2;
constexpr size_t kNoMidStreamReset = std::numeric_limits<size_t>::max();

const gfx::Size kThumbnailsPageSize(1600, 1200);
const gfx::Size kThumbnailSize(160, 120);

// We assert a minimal number of concurrent decoders we expect to succeed.
// Different platforms can support more concurrent decoders, so we don't assert
// failure above this.
constexpr size_t kMinSupportedNumConcurrentDecoders = 3;

// Magic constants for differentiating the reasons for NotifyResetDone being
// called.
enum ResetPoint {
  // Reset() right after calling Flush() (before getting NotifyFlushDone()).
  RESET_BEFORE_NOTIFY_FLUSH_DONE,
  // Reset() just after calling Decode() with a fragment containing config info.
  RESET_AFTER_FIRST_CONFIG_INFO,
  // Reset() just after finishing Initialize().
  START_OF_STREAM_RESET,
  // Reset() after a specific number of Decode() are executed.
  MID_STREAM_RESET,
  // Reset() after NotifyFlushDone().
  END_OF_STREAM_RESET,
  // This is the state that Reset() by RESET_AFTER_FIRST_CONFIG_INFO
  // is executed().
  DONE_RESET_AFTER_FIRST_CONFIG_INFO,
};

// State of the GLRenderingVDAClient below.  Order matters here as the test
// makes assumptions about it.
enum ClientState {
  CS_CREATED = 0,
  CS_DECODER_SET = 1,
  CS_INITIALIZED = 2,
  CS_FLUSHING = 3,
  CS_FLUSHED = 4,
  CS_RESETTING = 5,
  CS_RESET = 6,
  CS_ERROR = 7,
  CS_DESTROYED = 8,
  CS_MAX,  // Must be last entry.
};

struct TestVideoFile {
  explicit TestVideoFile(base::FilePath::StringType file_name)
      : file_name(file_name),
        width(0),
        height(0),
        num_frames(0),
        num_fragments(0),
        min_fps_render(0),
        min_fps_no_render(0),
        profile(VIDEO_CODEC_PROFILE_UNKNOWN),
        reset_after_frame_num(std::numeric_limits<size_t>::max()) {}

  base::FilePath::StringType file_name;
  int width;
  int height;
  size_t num_frames;
  size_t num_fragments;
  double min_fps_render;
  double min_fps_no_render;
  VideoCodecProfile profile;
  size_t reset_after_frame_num;
  std::string data_str;
};

base::FilePath GetTestDataFile(const base::FilePath& input_file) {
  if (input_file.IsAbsolute())
    return input_file;
  // input_file needs to be existed, otherwise base::MakeAbsoluteFilePath will
  // return an empty base::FilePath.
  base::FilePath abs_path =
      base::MakeAbsoluteFilePath(g_test_file_path.Append(input_file));
  LOG_IF(ERROR, abs_path.empty())
      << g_test_file_path.Append(input_file).value().c_str()
      << " is not an existing path.";
  return abs_path;
}

// Client that can accept callbacks from a VideoDecodeAccelerator and is used by
// the TESTs below.
class GLRenderingVDAClient
    : public VideoDecodeAccelerator::Client,
      public base::SupportsWeakPtr<GLRenderingVDAClient> {
 public:
  // |window_id| the window_id of the client, which is used to identify the
  // rendering area in the |rendering_helper_|.
  // |num_in_flight_decodes| is the number of concurrent in-flight Decode()
  // calls per decoder.
  // |num_play_throughs| indicates how many times to play through the video.
  // |reset_point| indicates the timing of executing Reset().
  // |reset_after_frame_num| can be a frame number >=0 indicating a mid-stream
  // Reset() should be done.  This member argument is only meaningful and must
  // not be less than 0 if |reset_point| == MID_STREAM_RESET.
  // Unless |reset_point| == MID_STREAM_RESET, it must be kNoMidStreamReset.
  // |delete_decoder_state| indicates when the underlying decoder should be
  // Destroy()'d and deleted and can take values: N<0: delete after -N Decode()
  // calls have been made, N>=0 means interpret as ClientState.
  // Both |reset_after_frame_num| & |delete_decoder_state| apply only to the
  // last play-through (governed by |num_play_throughs|).
  // |frame_size| is the frame size of the video file.
  // |profile| is video codec profile of the video file.
  // |fake_decoder| indicates decoder_ would be fake_video_decode_accelerator.
  // After |delay_reuse_after_frame_num| frame has been delivered, the client
  // will start delaying the call to ReusePictureBuffer() for kReuseDelay.
  // |decode_calls_per_second| is the number of VDA::Decode calls per second.
  // If |decode_calls_per_second| > 0, |num_in_flight_decodes| must be 1.
  // |num_frames| is the number of frames that must be verified to be decoded
  // during the test.
  struct Config {
    size_t window_id = 0;
    size_t num_in_flight_decodes = 1;
    size_t num_play_throughs = 1;
    ResetPoint reset_point = END_OF_STREAM_RESET;
    size_t reset_after_frame_num = kNoMidStreamReset;
    // TODO(hiroh): Refactor as delete_decoder_state can be enum class.
    // This can be set to not only ClientState, but also an integer in
    // TearDownTiming test case.
    int delete_decoder_state = CS_RESET;
    gfx::Size frame_size;
    VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
    bool fake_decoder = false;
    size_t delay_reuse_after_frame_num = std::numeric_limits<size_t>::max();
    size_t decode_calls_per_second = 0;
    size_t num_frames = 0;
  };

  // Doesn't take ownership of |rendering_helper| or |note|, which must outlive
  // |*this|.
  GLRenderingVDAClient(
      Config config,
      std::string encoded_data,
      RenderingHelper* rendering_helper,
      std::unique_ptr<media::test::VideoFrameValidator> video_frame_validator,
      ClientStateNotification<ClientState>* note);
  ~GLRenderingVDAClient() override;
  void CreateAndStartDecoder();

  // VideoDecodeAccelerator::Client implementation.
  // The heart of the Client.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  // Simple state changes.
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  void OutputFrameDeliveryTimes(base::File* output);

  std::vector<media::test::VideoFrameValidator::MismatchedFrameInfo>
  GetMismatchedFramesInfo();

  // Simple getters for inspecting the state of the Client.
  size_t num_done_bitstream_buffers() { return num_done_bitstream_buffers_; }
  size_t num_skipped_fragments() {
    return encoded_data_helper_->num_skipped_fragments();
  }
  size_t num_queued_fragments() { return num_queued_fragments_; }
  size_t num_decoded_frames() { return num_decoded_frames_; }
  double frames_per_second();
  // Return the median of the decode time of all decoded frames.
  base::TimeDelta decode_time_median();
  bool decoder_deleted() { return !decoder_.get(); }

 private:
  typedef std::map<int32_t, scoped_refptr<media::test::TextureRef>>
      TextureRefMap;

  void SetState(ClientState new_state);
  void FinishInitialization();
  void ReturnPicture(int32_t picture_buffer_id);
  bool IsLastPlayThrough() {
    return config_.num_play_throughs - completed_play_throughs_ == 1;
  }

  // Delete the associated decoder helper.
  void DeleteDecoder();
  // Reset the associated decoder after flushing.
  void ResetDecoderAfterFlush();

  // Request decode of the next fragment in the encoded data.
  void DecodeNextFragment();

  const Config config_;
  RenderingHelper* const rendering_helper_;
  gfx::Size frame_size_;
  size_t outstanding_decodes_;
  int next_bitstream_buffer_id_;
  ClientStateNotification<ClientState>* const note_;
  std::unique_ptr<VideoDecodeAccelerator> decoder_;
  base::WeakPtr<VideoDecodeAccelerator> weak_vda_;
  std::unique_ptr<base::WeakPtrFactory<VideoDecodeAccelerator>>
      weak_vda_ptr_factory_;
  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> vda_factory_;
  size_t completed_play_throughs_;
  ResetPoint reset_point_;
  ClientState state_;
  size_t num_queued_fragments_;
  size_t num_decoded_frames_;
  size_t num_done_bitstream_buffers_;
  base::TimeTicks initialize_done_ticks_;
  GLenum texture_target_;
  VideoPixelFormat pixel_format_;
  std::vector<base::TimeTicks> frame_delivery_times_;
  // A map from bitstream buffer id to the decode start time of the buffer.
  std::map<int, base::TimeTicks> decode_start_time_;
  // The decode time of all decoded frames.
  std::vector<base::TimeDelta> decode_time_;

  // A map of the textures that are currently active for the decoder, i.e.,
  // have been created via AssignPictureBuffers() and not dismissed via
  // DismissPictureBuffer(). The keys in the map are the IDs of the
  // corresponding picture buffers, and the values are TextureRefs to the
  // textures.
  TextureRefMap active_textures_;

  // A map of the textures that are still pending in the renderer.
  // The texture might be sent multiple times to the renderer in the case of VP9
  // show_existing_frame feature, so we track it by multimap.
  // We check this to ensure all frames are rendered before entering the
  // CS_RESET_State.
  std::multimap<int32_t, scoped_refptr<media::test::TextureRef>>
      pending_textures_;

  int32_t next_picture_buffer_id_;

  const std::unique_ptr<media::test::EncodedDataHelper> encoded_data_helper_;
  const std::unique_ptr<media::test::VideoFrameValidator>
      video_frame_validator_;

  base::WeakPtr<GLRenderingVDAClient> weak_this_;
  base::WeakPtrFactory<GLRenderingVDAClient> weak_this_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GLRenderingVDAClient);
};

static bool DummyBindImage(uint32_t client_texture_id,
                           uint32_t texture_target,
                           const scoped_refptr<gl::GLImage>& image,
                           bool can_bind_to_sampler) {
  return true;
}

GLRenderingVDAClient::GLRenderingVDAClient(
    Config config,
    std::string encoded_data,
    RenderingHelper* rendering_helper,
    std::unique_ptr<media::test::VideoFrameValidator> video_frame_validator,
    ClientStateNotification<ClientState>* note)
    : config_(std::move(config)),
      rendering_helper_(rendering_helper),
      frame_size_(config_.frame_size),
      outstanding_decodes_(0),
      next_bitstream_buffer_id_(0),
      note_(note),
      completed_play_throughs_(0),
      reset_point_(config_.reset_point),
      state_(CS_CREATED),
      num_queued_fragments_(0),
      num_decoded_frames_(0),
      num_done_bitstream_buffers_(0),
      texture_target_(0),
      pixel_format_(PIXEL_FORMAT_UNKNOWN),
      next_picture_buffer_id_(1),
      encoded_data_helper_(std::make_unique<media::test::EncodedDataHelper>(
          std::move(encoded_data),
          config_.profile)),
      video_frame_validator_(std::move(video_frame_validator)),
      weak_this_factory_(this) {
  DCHECK_NE(config.profile, VIDEO_CODEC_PROFILE_UNKNOWN);
  LOG_ASSERT(config_.num_in_flight_decodes > 0);
  LOG_ASSERT(config_.num_play_throughs > 0);
  // |num_in_flight_decodes_| is unsupported if |decode_calls_per_second_| > 0.
  if (config_.decode_calls_per_second > 0)
    LOG_ASSERT(1 == config_.num_in_flight_decodes);
  weak_this_ = weak_this_factory_.GetWeakPtr();
  if (config_.reset_point == MID_STREAM_RESET) {
    EXPECT_NE(config_.reset_after_frame_num, kNoMidStreamReset)
        << "reset_ater_frame_num_ must not be kNoMidStreamReset "
        << "when reset_point = MID_STREAM_RESET";
  } else {
    EXPECT_EQ(config_.reset_after_frame_num, kNoMidStreamReset);
  }
}

GLRenderingVDAClient::~GLRenderingVDAClient() {
  DeleteDecoder();  // Clean up in case of expected error.
  LOG_ASSERT(decoder_deleted());
  SetState(CS_DESTROYED);
}

void GLRenderingVDAClient::CreateAndStartDecoder() {
  LOG_ASSERT(decoder_deleted());
  LOG_ASSERT(!decoder_.get());

  VideoDecodeAccelerator::Config vda_config(config_.profile);

  if (config_.fake_decoder) {
    decoder_.reset(new FakeVideoDecodeAccelerator(
        frame_size_, base::Bind([]() { return true; })));
    LOG_ASSERT(decoder_->Initialize(vda_config, this));
  } else {
    if (!vda_factory_) {
      if (g_use_gl_renderer) {
        vda_factory_ = GpuVideoDecodeAcceleratorFactory::Create(
            base::Bind(&RenderingHelper::GetGLContext,
                       base::Unretained(rendering_helper_)),
            base::Bind([]() { return true; }), base::Bind(&DummyBindImage));
      } else {
        vda_factory_ = GpuVideoDecodeAcceleratorFactory::CreateWithNoGL();
      }

      LOG_ASSERT(vda_factory_);
    }

    if (g_test_import) {
      vda_config.output_mode =
          VideoDecodeAccelerator::Config::OutputMode::IMPORT;
    }
    gpu::GpuDriverBugWorkarounds workarounds;
    gpu::GpuPreferences gpu_preferences;
    decoder_ =
        vda_factory_->CreateVDA(this, vda_config, workarounds, gpu_preferences);
  }

  LOG_ASSERT(decoder_) << "Failed creating a VDA";

  decoder_->TryToSetupDecodeOnSeparateThread(
      weak_this_, base::ThreadTaskRunnerHandle::Get());

  weak_vda_ptr_factory_.reset(
      new base::WeakPtrFactory<VideoDecodeAccelerator>(decoder_.get()));
  weak_vda_ = weak_vda_ptr_factory_->GetWeakPtr();

  SetState(CS_DECODER_SET);
  FinishInitialization();
}

void GLRenderingVDAClient::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat pixel_format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  if (decoder_deleted())
    return;
  LOG_ASSERT(textures_per_buffer == 1u);
  std::vector<PictureBuffer> buffers;

  requested_num_of_buffers += static_cast<uint32_t>(kExtraPictureBuffers);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    pixel_format = PIXEL_FORMAT_ARGB;

  LOG_ASSERT((pixel_format_ == PIXEL_FORMAT_UNKNOWN) ||
             (pixel_format_ == pixel_format));
  pixel_format_ = pixel_format;
  frame_size_ = dimensions;

  texture_target_ = texture_target;
  for (uint32_t i = 0; i < requested_num_of_buffers; ++i) {
    auto texture_ref = rendering_helper_->CreateTexture(
        texture_target_, g_test_import, pixel_format, dimensions);
    LOG_ASSERT(texture_ref);
    int32_t picture_buffer_id = next_picture_buffer_id_++;
    int irrelevant_id = picture_buffer_id;
    LOG_ASSERT(
        active_textures_.insert(std::make_pair(picture_buffer_id, texture_ref))
            .second);

    PictureBuffer::TextureIds texture_ids(1, texture_ref->texture_id());
    buffers.push_back(PictureBuffer(picture_buffer_id, dimensions,
                                    PictureBuffer::TextureIds{irrelevant_id++},
                                    texture_ids, texture_target, pixel_format));
  }
  decoder_->AssignPictureBuffers(buffers);

  if (g_test_import) {
    for (const auto& buffer : buffers) {
      TextureRefMap::iterator texture_it = active_textures_.find(buffer.id());
      ASSERT_NE(active_textures_.end(), texture_it);

      const gfx::GpuMemoryBufferHandle& handle =
          texture_it->second->ExportGpuMemoryBufferHandle();
      LOG_ASSERT(!handle.is_null()) << "Failed producing GMB handle";
      decoder_->ImportBufferForPicture(buffer.id(), pixel_format, handle);
    }
  }
}

void GLRenderingVDAClient::DismissPictureBuffer(int32_t picture_buffer_id) {
  LOG_ASSERT(1U == active_textures_.erase(picture_buffer_id));
}

void GLRenderingVDAClient::PictureReady(const Picture& picture) {
  if (decoder_deleted())
    return;
  // We shouldn't be getting pictures delivered after Reset has completed.
  LOG_ASSERT(state_ < CS_RESET);

  gfx::Rect visible_rect = picture.visible_rect();
  if (!visible_rect.IsEmpty())
    EXPECT_TRUE(gfx::Rect(frame_size_).Contains(visible_rect));

  base::TimeTicks now = base::TimeTicks::Now();

  frame_delivery_times_.push_back(now);

  // Save the decode time of this picture.
  std::map<int, base::TimeTicks>::iterator it =
      decode_start_time_.find(picture.bitstream_buffer_id());
  ASSERT_NE(decode_start_time_.end(), it);
  decode_time_.push_back(now - it->second);
  decode_start_time_.erase(it);

  LOG_ASSERT(picture.bitstream_buffer_id() <= next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  // Mid-stream reset applies only to the last play-through per constructor
  // comment.
  if (IsLastPlayThrough() && reset_point_ == MID_STREAM_RESET &&
      config_.reset_after_frame_num == num_decoded_frames_) {
    decoder_->Reset();
    // Re-start decoding from the beginning of the stream to avoid needing to
    // know how to find I-frames and so on in this test.
    encoded_data_helper_->Rewind();
  }

  TextureRefMap::iterator texture_it =
      active_textures_.find(picture.picture_buffer_id());
  ASSERT_NE(active_textures_.end(), texture_it);

  scoped_refptr<VideoFrameTexture> video_frame_texture = new VideoFrameTexture(
      texture_target_, texture_it->second->texture_id(),
      base::Bind(&GLRenderingVDAClient::ReturnPicture, AsWeakPtr(),
                 picture.picture_buffer_id()));
  pending_textures_.insert(*texture_it);
  if (video_frame_validator_) {
    auto video_frame = texture_it->second->CreateVideoFrame(visible_rect);
    ASSERT_NE(video_frame.get(), nullptr);
    video_frame_validator_->EvaluateVideoFrame(std::move(video_frame));
  }
  rendering_helper_->ConsumeVideoFrame(config_.window_id,
                                       std::move(video_frame_texture));
}

void GLRenderingVDAClient::ReturnPicture(int32_t picture_buffer_id) {
  auto it = pending_textures_.find(picture_buffer_id);
  LOG_ASSERT(it != pending_textures_.end());
  pending_textures_.erase(it);

  if (decoder_deleted())
    return;

  if (active_textures_.find(picture_buffer_id) == active_textures_.end()) {
    // The picture associated with picture_buffer_id is dismissed.
    // Do not execute ReusePictureBuffer().
    return;
  }

  if (pending_textures_.empty() && state_ == CS_RESETTING) {
    SetState(CS_RESET);
    DeleteDecoder();
    return;
  }

  if (num_decoded_frames_ > config_.delay_reuse_after_frame_num) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::ReusePictureBuffer, weak_vda_,
                       picture_buffer_id),
        kReuseDelay);
  } else {
    decoder_->ReusePictureBuffer(picture_buffer_id);
  }
}

void GLRenderingVDAClient::ResetDecoderAfterFlush() {
  // SetState(CS_RESETTING) should be called before decoder_->Reset(), because
  // VDA can call NotifyFlushDone() from Reset().
  // TODO(johnylin): call SetState() before all decoder Flush() and Reset().
  SetState(CS_RESETTING);
  // It is necessary to check decoder deleted here because it is possible to
  // delete decoder in SetState() in some cases.
  if (decoder_deleted())
    return;
  decoder_->Reset();
}

void GLRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  if (decoder_deleted())
    return;

  // TODO(fischman): this test currently relies on this notification to make
  // forward progress during a Reset().  But the VDA::Reset() API doesn't
  // guarantee this, so stop relying on it (and remove the notifications from
  // VaapiVideoDecodeAccelerator::FinishReset()).
  LOG_ASSERT(outstanding_decodes_ != 0);
  ++num_done_bitstream_buffers_;
  --outstanding_decodes_;

  // Flush decoder after all BitstreamBuffers are processed.
  if (encoded_data_helper_->ReachEndOfStream()) {
    if (state_ != CS_FLUSHING) {
      decoder_->Flush();
      SetState(CS_FLUSHING);
      if (reset_point_ == RESET_BEFORE_NOTIFY_FLUSH_DONE) {
        SetState(CS_FLUSHED);
        ResetDecoderAfterFlush();
      }
    }
  } else if (config_.decode_calls_per_second == 0) {
    DecodeNextFragment();
  }
}

void GLRenderingVDAClient::NotifyFlushDone() {
  if (decoder_deleted())
    return;

  if (reset_point_ == RESET_BEFORE_NOTIFY_FLUSH_DONE) {
    // In ResetBeforeNotifyFlushDone case client is not necessary to wait for
    // NotifyFlushDone(). But if client gets here, it should be always before
    // NotifyResetDone().
    ASSERT_EQ(state_, CS_RESETTING);
    return;
  }

  // Check all the Decode()-ed frames are returned by PictureReady() in
  // END_OF_STREAM_RESET case.
  if (config_.reset_point == END_OF_STREAM_RESET)
    EXPECT_EQ(num_decoded_frames_, config_.num_frames);

  SetState(CS_FLUSHED);
  ResetDecoderAfterFlush();
}

void GLRenderingVDAClient::NotifyResetDone() {
  if (decoder_deleted())
    return;

  switch (reset_point_) {
    case DONE_RESET_AFTER_FIRST_CONFIG_INFO:
    case MID_STREAM_RESET:
      reset_point_ = END_OF_STREAM_RESET;
      // Because VDA::Decode() is executed if |reset_point_| is
      // MID_STREAM_RESET or RESET_AFTER_FIRST_CONFIG_INFO,
      // NotifyEndOfBitstreamBuffer() will be invoked. Next VDA::Decode() is
      // triggered from NotifyEndOfBitstreamBuffer().
      return;
    case START_OF_STREAM_RESET:
      EXPECT_EQ(num_decoded_frames_, 0u);
      EXPECT_EQ(encoded_data_helper_->AtHeadOfStream(), true);
      reset_point_ = END_OF_STREAM_RESET;
      for (size_t i = 0; i < config_.num_in_flight_decodes; ++i)
        DecodeNextFragment();
      return;
    case END_OF_STREAM_RESET:
    case RESET_BEFORE_NOTIFY_FLUSH_DONE:
      break;
    case RESET_AFTER_FIRST_CONFIG_INFO:
      NOTREACHED();
      break;
  }

  completed_play_throughs_++;
  DCHECK_GE(config_.num_play_throughs, completed_play_throughs_);

  if (completed_play_throughs_ < config_.num_play_throughs) {
    encoded_data_helper_->Rewind();
    FinishInitialization();
    return;
  }

  // completed_play_throughs == config.num_play_throughs.
  rendering_helper_->Flush(config_.window_id);

  if (pending_textures_.empty()) {
    SetState(CS_RESET);
    DeleteDecoder();
  }
}

void GLRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  SetState(CS_ERROR);
}

void GLRenderingVDAClient::OutputFrameDeliveryTimes(base::File* output) {
  std::string s = base::StringPrintf("frame count: %" PRIuS "\n",
                                     frame_delivery_times_.size());
  output->WriteAtCurrentPos(s.data(), s.length());
  base::TimeTicks t0 = initialize_done_ticks_;
  for (size_t i = 0; i < frame_delivery_times_.size(); ++i) {
    s = base::StringPrintf("frame %04" PRIuS ": %" PRId64 " us\n", i,
                           (frame_delivery_times_[i] - t0).InMicroseconds());
    t0 = frame_delivery_times_[i];
    output->WriteAtCurrentPos(s.data(), s.length());
  }
}

std::vector<media::test::VideoFrameValidator::MismatchedFrameInfo>
GLRenderingVDAClient::GetMismatchedFramesInfo() {
  if (!video_frame_validator_) {
    return {};
  }
  return video_frame_validator_->GetMismatchedFramesInfo();
}

void GLRenderingVDAClient::SetState(ClientState new_state) {
  note_->Notify(new_state);
  state_ = new_state;
  if (IsLastPlayThrough() && new_state == config_.delete_decoder_state) {
    // If config_.delete_decoder_state is CS_RESET, IsLastPlayThrough() is
    // false. But it does not matter, because DeleteDecoder() is executed after
    // SetState(CS_RESET) in NotifyResetDone().
    ASSERT_NE(config_.delete_decoder_state, CS_RESET);
    LOG_ASSERT(!decoder_deleted());
    DeleteDecoder();
  }
}

void GLRenderingVDAClient::FinishInitialization() {
  SetState(CS_INITIALIZED);
  initialize_done_ticks_ = base::TimeTicks::Now();
  EXPECT_EQ(encoded_data_helper_->AtHeadOfStream(), true);
  num_decoded_frames_ = 0;
  if (decoder_deleted())
    return;

  if (reset_point_ == START_OF_STREAM_RESET) {
    decoder_->Reset();
    return;
  }

  for (size_t i = 0; i < config_.num_in_flight_decodes; ++i)
    DecodeNextFragment();
  EXPECT_EQ(outstanding_decodes_, config_.num_in_flight_decodes);
}

void GLRenderingVDAClient::DeleteDecoder() {
  if (decoder_deleted())
    return;
  weak_vda_ptr_factory_->InvalidateWeakPtrs();
  decoder_.reset();

  active_textures_.clear();

  // Set state to CS_DESTROYED after decoder is deleted.
  SetState(CS_DESTROYED);
}

void GLRenderingVDAClient::DecodeNextFragment() {
  if (decoder_deleted())
    return;
  if (encoded_data_helper_->ReachEndOfStream())
    return;
  std::string next_fragment_bytes;
  next_fragment_bytes = encoded_data_helper_->GetBytesForNextData();
  size_t next_fragment_size = next_fragment_bytes.size();
  if (next_fragment_size == 0)
    return;

  num_queued_fragments_++;
  // Call Reset() just after Decode() if the fragment contains config info.
  // This tests how the VDA behaves when it gets a reset request before it has
  // a chance to ProvidePictureBuffers().
  bool reset_here = false;
  if (reset_point_ == RESET_AFTER_FIRST_CONFIG_INFO) {
    reset_here = media::test::EncodedDataHelper::HasConfigInfo(
        reinterpret_cast<const uint8_t*>(next_fragment_bytes.data()),
        next_fragment_size, config_.profile);
    // Set to DONE_RESET_AFTER_FIRST_CONFIG_INFO, to only Reset() for the first
    // time.
    if (reset_here)
      reset_point_ = DONE_RESET_AFTER_FIRST_CONFIG_INFO;
  }

  // Populate the shared memory buffer w/ the fragment, duplicate its handle,
  // and hand it off to the decoder.
  base::SharedMemory shm;
  LOG_ASSERT(shm.CreateAndMapAnonymous(next_fragment_size));
  memcpy(shm.memory(), next_fragment_bytes.data(), next_fragment_size);
  base::SharedMemoryHandle dup_handle = shm.handle().Duplicate();
  LOG_ASSERT(dup_handle.IsValid());

  // TODO(erikchen): This may leak the SharedMemoryHandle.
  // https://crbug.com/640840.
  BitstreamBuffer bitstream_buffer(next_bitstream_buffer_id_, dup_handle,
                                   next_fragment_size);
  decode_start_time_[next_bitstream_buffer_id_] = base::TimeTicks::Now();
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  decoder_->Decode(bitstream_buffer);
  ++outstanding_decodes_;
  if (IsLastPlayThrough() &&
      -config_.delete_decoder_state == next_bitstream_buffer_id_) {
    DeleteDecoder();
  }

  if (reset_here) {
    decoder_->Reset();
    // Restart from the beginning to re-Decode() the SPS we just sent.
    encoded_data_helper_->Rewind();
  }

  if (config_.decode_calls_per_second > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GLRenderingVDAClient::DecodeNextFragment, AsWeakPtr()),
        base::TimeDelta::FromSeconds(1) / config_.decode_calls_per_second);
  } else {
    // Unless DecodeNextFragment() is posted from the above PostDelayedTask(),
    // all the DecodeNextFragment() will be executed from
    // NotifyEndOfBitstreamBuffer(). The number of Decode()s in flight must be
    // less than or equal to the specified times.
    EXPECT_LE(outstanding_decodes_, config_.num_in_flight_decodes);
  }
}

double GLRenderingVDAClient::frames_per_second() {
  base::TimeDelta delta = frame_delivery_times_.back() - initialize_done_ticks_;
  return num_decoded_frames_ / delta.InSecondsF();
}

base::TimeDelta GLRenderingVDAClient::decode_time_median() {
  if (decode_time_.size() == 0)
    return base::TimeDelta();
  std::sort(decode_time_.begin(), decode_time_.end());
  size_t index = decode_time_.size() / 2;
  if (decode_time_.size() % 2 != 0)
    return decode_time_[index];

  return (decode_time_[index] + decode_time_[index - 1]) / 2;
}

class VideoDecodeAcceleratorTest : public ::testing::Test {
 protected:
  using TestFilesVector = std::vector<std::unique_ptr<TestVideoFile>>;

  VideoDecodeAcceleratorTest();
  void SetUp() override;
  void TearDown() override;

  // Parse |data| into its constituent parts, set the various output fields
  // accordingly, and read in video stream. CHECK-fails on unexpected or
  // missing required data. Unspecified optional fields are set to -1.
  void ParseAndReadTestVideoData(base::FilePath::StringType data,
                                 TestFilesVector* test_video_files);

  // Update the parameters of |test_video_files| according to
  // |num_concurrent_decoders| and |reset_point|. Ex: the expected number of
  // frames should be adjusted if decoder is reset in the middle of the stream.
  void UpdateTestVideoFileParams(size_t num_concurrent_decoders,
                                 ResetPoint reset_point,
                                 TestFilesVector* test_video_files);

  void InitializeRenderingHelper(const RenderingHelperParams& helper_params);
  void CreateAndStartDecoder(GLRenderingVDAClient* client,
                             ClientStateNotification<ClientState>* note);

  // Wait until decode finishes and return the last state.
  ClientState WaitUntilDecodeFinish(ClientStateNotification<ClientState>* note);

  void WaitUntilIdle();
  void OutputLogFile(const base::FilePath::CharType* log_path,
                     const std::string& content);

  TestFilesVector test_video_files_;
  RenderingHelper rendering_helper_;

 protected:
  // Must be static because this method may run after the destructor.
  template <typename T>
  static void Delete(T item) {
    // |item| is cleared when the scope of this function is left.
  }
  using NotesVector =
      std::vector<std::unique_ptr<ClientStateNotification<ClientState>>>;
  using ClientsVector = std::vector<std::unique_ptr<GLRenderingVDAClient>>;

  NotesVector notes_;
  ClientsVector clients_;

 private:
  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorTest);
};

VideoDecodeAcceleratorTest::VideoDecodeAcceleratorTest() {}

void VideoDecodeAcceleratorTest::SetUp() {
  ParseAndReadTestVideoData(g_test_video_data, &test_video_files_);
}

void VideoDecodeAcceleratorTest::TearDown() {
  // |clients_| must be deleted first because |clients_| use |notes_|.
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Delete<ClientsVector>, base::Passed(&clients_)));

  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Delete<NotesVector>, base::Passed(&notes_)));

  WaitUntilIdle();

  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&Delete<TestFilesVector>, base::Passed(&test_video_files_)));

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RenderingHelper::UnInitialize,
                                base::Unretained(&rendering_helper_), &done));
  done.Wait();
}

void VideoDecodeAcceleratorTest::ParseAndReadTestVideoData(
    base::FilePath::StringType data,
    TestFilesVector* test_video_files) {
  std::vector<base::FilePath::StringType> entries =
      base::SplitString(data, base::FilePath::StringType(1, ';'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  LOG_ASSERT(entries.size() >= 1U) << data;
  for (size_t index = 0; index < entries.size(); ++index) {
    std::vector<base::FilePath::StringType> fields =
        base::SplitString(entries[index], base::FilePath::StringType(1, ':'),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    LOG_ASSERT(fields.size() >= 1U) << entries[index];
    LOG_ASSERT(fields.size() <= 8U) << entries[index];
    std::unique_ptr<TestVideoFile> video_file =
        std::make_unique<TestVideoFile>(fields[0]);
    if (!fields[1].empty())
      LOG_ASSERT(base::StringToInt(fields[1], &video_file->width));
    if (!fields[2].empty())
      LOG_ASSERT(base::StringToInt(fields[2], &video_file->height));
    if (!fields[3].empty())
      LOG_ASSERT(base::StringToSizeT(fields[3], &video_file->num_frames));
    if (!fields[4].empty())
      LOG_ASSERT(base::StringToSizeT(fields[4], &video_file->num_fragments));
    if (!fields[5].empty()) {
      std::string field(fields[5].begin(), fields[5].end());
      LOG_ASSERT(base::StringToDouble(field, &video_file->min_fps_render));
    }
    if (!fields[6].empty()) {
      std::string field(fields[5].begin(), fields[5].end());
      LOG_ASSERT(base::StringToDouble(field, &video_file->min_fps_no_render));
    }
    // Default to H264 baseline if no profile provided.
    int profile = static_cast<int>(H264PROFILE_BASELINE);
    if (!fields[7].empty())
      LOG_ASSERT(base::StringToInt(fields[7], &profile));
    video_file->profile = static_cast<VideoCodecProfile>(profile);

    // Read in the video data.
    base::FilePath filepath(video_file->file_name);
    LOG_ASSERT(base::ReadFileToString(GetTestDataFile(filepath),
                                      &video_file->data_str))
        << "test_video_file: " << filepath.MaybeAsASCII();

    test_video_files->push_back(std::move(video_file));
  }
}

void VideoDecodeAcceleratorTest::UpdateTestVideoFileParams(
    size_t num_concurrent_decoders,
    ResetPoint reset_point,
    TestFilesVector* test_video_files) {
  for (size_t i = 0; i < test_video_files->size(); i++) {
    TestVideoFile* video_file = (*test_video_files)[i].get();
    if (reset_point == MID_STREAM_RESET) {
      // Reset should not go beyond the last frame;
      // reset in the middle of the stream for short videos.
      video_file->reset_after_frame_num = kMaxResetAfterFrameNum;
      if (video_file->num_frames <= video_file->reset_after_frame_num)
        video_file->reset_after_frame_num = video_file->num_frames / 2;

      video_file->num_frames += video_file->reset_after_frame_num;
    } else {
      video_file->reset_after_frame_num = kNoMidStreamReset;
    }

    if (video_file->min_fps_render != -1)
      video_file->min_fps_render /= num_concurrent_decoders;
    if (video_file->min_fps_no_render != -1)
      video_file->min_fps_no_render /= num_concurrent_decoders;
  }
}

void VideoDecodeAcceleratorTest::InitializeRenderingHelper(
    const RenderingHelperParams& helper_params) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(&rendering_helper_), helper_params, &done));
  done.Wait();
}

void VideoDecodeAcceleratorTest::CreateAndStartDecoder(
    GLRenderingVDAClient* client,
    ClientStateNotification<ClientState>* note) {
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GLRenderingVDAClient::CreateAndStartDecoder,
                                base::Unretained(client)));
  ASSERT_EQ(note->Wait(), CS_DECODER_SET);
}

ClientState VideoDecodeAcceleratorTest::WaitUntilDecodeFinish(
    ClientStateNotification<ClientState>* note) {
  ClientState state = CS_DESTROYED;
  for (int i = 0; i < CS_MAX; i++) {
    state = note->Wait();
    if (state == CS_DESTROYED || state == CS_ERROR)
      break;
  }
  return state;
}

void VideoDecodeAcceleratorTest::WaitUntilIdle() {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();
}

void VideoDecodeAcceleratorTest::OutputLogFile(
    const base::FilePath::CharType* log_path,
    const std::string& content) {
  base::File file(base::FilePath(log_path),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(content.data(), content.length());
}

// Test parameters:
// - Number of concurrent decoders. The value takes effect when there is only
//   one input stream; otherwise, one decoder per input stream will be
//   instantiated.
// - Number of concurrent in-flight Decode() calls per decoder.
// - Number of play-throughs.
// - reset_after_frame_num: see GLRenderingVDAClient ctor.
// - delete_decoder_phase: see GLRenderingVDAClient ctor.
// - whether to test slow rendering by delaying ReusePictureBuffer().
// - whether the video frames are rendered as thumbnails.
class VideoDecodeAcceleratorParamTest
    : public VideoDecodeAcceleratorTest,
      public ::testing::WithParamInterface<std::tuple<size_t,
                                                      size_t,
                                                      size_t,
                                                      ResetPoint,
                                                      ClientState,
                                                      bool,
                                                      bool>> {};

// Wait for |note| to report a state and if it's not |expected_state| then
// assert |client| has deleted its decoder.
static void AssertWaitForStateOrDeleted(
    ClientStateNotification<ClientState>* note,
    GLRenderingVDAClient* client,
    ClientState expected_state) {
  // Skip waiting state if decoder of |client| is already deleted.
  if (client->decoder_deleted())
    return;
  ClientState state = note->Wait();
  if (state == expected_state)
    return;
  ASSERT_TRUE(client->decoder_deleted())
      << "Decoder not deleted but Wait() returned " << state
      << ", instead of " << expected_state;
}

std::unique_ptr<media::test::VideoFrameValidator>
CreateAndInitializeVideoFrameValidator(
    const base::FilePath::StringType& video_file) {
  // TODO(crbug.com/856562): Add a command line option to stand for outputting
  // decoded yuv.
  // Currently decoded yuv is not output.
  constexpr bool output_yuv = false;
  // Initialize prefix of yuv files.
  base::FilePath prefix_output_yuv;

  base::FilePath filepath(video_file);
  if (output_yuv) {
    if (!g_thumbnail_output_dir.empty() &&
        base::DirectoryExists(g_thumbnail_output_dir)) {
      prefix_output_yuv = g_thumbnail_output_dir.Append(filepath.BaseName());
    } else {
      prefix_output_yuv = GetTestDataFile(filepath);
    }
  }
  return media::test::VideoFrameValidator::CreateVideoFrameValidator(
      prefix_output_yuv,
      filepath.AddExtension(FILE_PATH_LITERAL(".frames.md5")));
}

// Fails on Win only. crbug.com/849368
#if defined(OS_WIN)
#define MAYBE_TestSimpleDecode DISABLED_TestSimpleDecode
#else
#define MAYBE_TestSimpleDecode TestSimpleDecode
#endif
// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST_P(VideoDecodeAcceleratorParamTest, MAYBE_TestSimpleDecode) {
  size_t num_concurrent_decoders = std::get<0>(GetParam());
  const size_t num_in_flight_decodes = std::get<1>(GetParam());
  size_t num_play_throughs = std::get<2>(GetParam());
  const ResetPoint reset_point = std::get<3>(GetParam());
  const int delete_decoder_state = std::get<4>(GetParam());
  bool test_reuse_delay = std::get<5>(GetParam());
  const bool render_as_thumbnails = std::get<6>(GetParam());

  // We cannot render thumbnails without GL
  if (!g_use_gl_renderer && render_as_thumbnails) {
    LOG(WARNING) << "Skipping thumbnail test because GL is deactivated by "
                    "--disable_rendering";
    return;
  }

  if (test_video_files_.size() > 1)
    num_concurrent_decoders = test_video_files_.size();

  if (g_num_play_throughs > 0)
    num_play_throughs = g_num_play_throughs;

  UpdateTestVideoFileParams(num_concurrent_decoders, reset_point,
                            &test_video_files_);

  notes_.resize(num_concurrent_decoders);
  clients_.resize(num_concurrent_decoders);

  // TODO(crbug.com/856562): Use Frame Validator in every test case, not
  // limited to thumbnail test case.
  bool use_video_frame_validator =
      render_as_thumbnails && g_frame_validator && g_test_import;
  if (use_video_frame_validator) {
    LOG(INFO) << "Using Frame Validator..";
#if !defined(OS_CHROMEOS)
    LOG(FATAL) << "FrameValidator (g_frame_validator) cannot be used on "
               << "non-Chrome OS platform.";
    return;
#endif  // !defined(OS_CHROMEOS)
  }

  // First kick off all the decoders.
  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    TestVideoFile* video_file =
        test_video_files_[index % test_video_files_.size()].get();
    std::unique_ptr<ClientStateNotification<ClientState>> note =
        std::make_unique<ClientStateNotification<ClientState>>();
    notes_[index] = std::move(note);

    size_t delay_reuse_after_frame_num = std::numeric_limits<size_t>::max();
    if (test_reuse_delay &&
        kMaxFramesToDelayReuse * 2 < video_file->num_frames) {
      delay_reuse_after_frame_num =
          video_file->num_frames - kMaxFramesToDelayReuse;
    }
    GLRenderingVDAClient::Config config;
    config.window_id = index;
    config.num_in_flight_decodes = num_in_flight_decodes;
    config.num_play_throughs = num_play_throughs;
    config.reset_point = reset_point;
    config.reset_after_frame_num = video_file->reset_after_frame_num;
    config.delete_decoder_state = delete_decoder_state;
    config.frame_size = gfx::Size(video_file->width, video_file->height);
    config.profile = video_file->profile;
    config.fake_decoder = g_fake_decoder;
    config.delay_reuse_after_frame_num = delay_reuse_after_frame_num;
    config.num_frames = video_file->num_frames;

    std::unique_ptr<media::test::VideoFrameValidator> video_frame_validator;
    if (use_video_frame_validator) {
      video_frame_validator =
          CreateAndInitializeVideoFrameValidator(video_file->file_name);
      ASSERT_NE(video_frame_validator.get(), nullptr);
    }
    clients_[index] = std::make_unique<GLRenderingVDAClient>(
        std::move(config), video_file->data_str, &rendering_helper_,
        std::move(video_frame_validator), notes_[index].get());
  }

  RenderingHelperParams helper_params;
  helper_params.rendering_fps = g_rendering_fps;
  helper_params.render_as_thumbnails = render_as_thumbnails;
  helper_params.num_windows = num_concurrent_decoders;
  if (render_as_thumbnails) {
    // Only one decoder is supported with thumbnail rendering
    LOG_ASSERT(num_concurrent_decoders == 1U);
    helper_params.thumbnails_page_size = kThumbnailsPageSize;
    helper_params.thumbnail_size = kThumbnailSize;
  }
  InitializeRenderingHelper(helper_params);

  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    CreateAndStartDecoder(clients_[index].get(), notes_[index].get());
  }

  // Then wait for all the decodes to finish.
  // Only check performance & correctness later if we play through only once.
  bool skip_performance_and_correctness_checks = num_play_throughs > 1;
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    ClientStateNotification<ClientState>* note = notes_[i].get();
    ClientState state = note->Wait();
    EXPECT_TRUE(delete_decoder_state != CS_DECODER_SET ||
                state == CS_DESTROYED);
    if (delete_decoder_state != CS_DECODER_SET && state != CS_INITIALIZED) {
      skip_performance_and_correctness_checks = true;
      // We expect initialization to fail only when more than the supported
      // number of decoders is instantiated.  Assert here that something else
      // didn't trigger failure.
      ASSERT_GT(num_concurrent_decoders,
                static_cast<size_t>(kMinSupportedNumConcurrentDecoders));
      continue;
    }
    for (size_t n = 0; n < num_play_throughs; ++n) {
      // For play-throughs other than the first, we expect initialization to
      // succeed unconditionally.
      if (n > 0) {
        ASSERT_NO_FATAL_FAILURE(AssertWaitForStateOrDeleted(
            note, clients_[i].get(), CS_INITIALIZED));
      }
      // InitializeDone kicks off decoding inside the client, so we just need to
      // wait for Flush.
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients_[i].get(), CS_FLUSHING));
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients_[i].get(), CS_FLUSHED));
      // FlushDone requests Reset().
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients_[i].get(), CS_RESETTING));
    }
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients_[i].get(), CS_RESET));
    // ResetDone requests Destroy().
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients_[i].get(), CS_DESTROYED));
  }
  // Finally assert that decoding went as expected.
  for (size_t i = 0;
       i < num_concurrent_decoders && !skip_performance_and_correctness_checks;
       ++i) {
    // We can only make performance/correctness assertions if the decoder was
    // allowed to finish.
    if (delete_decoder_state < CS_FLUSHED)
      continue;
    GLRenderingVDAClient* client = clients_[i].get();
    TestVideoFile* video_file =
        test_video_files_[i % test_video_files_.size()].get();
    if (video_file->num_frames > 0) {
      // Expect the decoded frames may be more than the video frames as frames
      // could still be returned until resetting done.
      if (reset_point == MID_STREAM_RESET)
        EXPECT_GE(client->num_decoded_frames(), video_file->num_frames);
      // In ResetBeforeNotifyFlushDone case the decoded frames may be less than
      // the video frames because decoder is reset before flush done.
      else if (reset_point != RESET_BEFORE_NOTIFY_FLUSH_DONE)
        EXPECT_EQ(client->num_decoded_frames(), video_file->num_frames);
    }
    if (reset_point == END_OF_STREAM_RESET) {
      EXPECT_EQ(video_file->num_fragments, client->num_skipped_fragments() +
                                               client->num_queued_fragments());
      EXPECT_EQ(client->num_done_bitstream_buffers(),
                client->num_queued_fragments());
    }
    LOG(INFO) << "Decoder " << i << " fps: " << client->frames_per_second();
    if (!render_as_thumbnails) {
      double min_fps = g_rendering_fps == 0 ? video_file->min_fps_no_render
                                            : video_file->min_fps_render;
      if (min_fps > 0 && !test_reuse_delay)
        EXPECT_GT(client->frames_per_second(), min_fps);
    }
  }

  if (render_as_thumbnails) {
    std::vector<unsigned char> rgba;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    g_env->GetRenderingTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&RenderingHelper::GetThumbnailsAsRGBA,
                   base::Unretained(&rendering_helper_), &rgba, &done));
    done.Wait();

    std::vector<unsigned char> rgb;
    EXPECT_EQ(media::test::ConvertRGBAToRGB(rgba, &rgb), true)
        << "RGBA frame had incorrect alpha";

    std::string md5_string = base::MD5String(
        base::StringPiece(reinterpret_cast<char*>(&rgb[0]), rgb.size()));
    base::FilePath filepath(test_video_files_[0]->file_name);
    auto golden_md5s = media::test::ReadGoldenThumbnailMD5s(
        filepath.AddExtension(FILE_PATH_LITERAL(".md5")));
    bool is_valid_thumbnail = base::ContainsValue(golden_md5s, md5_string);

    // Convert raw RGBA into PNG for export.
    std::vector<unsigned char> png;
    gfx::PNGCodec::Encode(&rgba[0], gfx::PNGCodec::FORMAT_RGBA,
                          kThumbnailsPageSize, kThumbnailsPageSize.width() * 4,
                          true, std::vector<gfx::PNGCodec::Comment>(), &png);

    if (!g_thumbnail_output_dir.empty() &&
        base::DirectoryExists(g_thumbnail_output_dir)) {
      // Write thumbnails image to where --thumbnail_output_dir assigned.
      filepath = g_thumbnail_output_dir.Append(filepath.BaseName());
    } else {
      // Fallback to write to test data directory.
      // Note: test data directory is not writable by vda_unittest while
      //       running by autotest. It should assign its resultsdir as output
      //       directory.
      filepath = GetTestDataFile(filepath);
    }

    if (is_valid_thumbnail) {
      filepath =
          filepath.AddExtension(FILE_PATH_LITERAL(".good_thumbnails.png"));
      LOG(INFO) << "Write good thumbnails image to: "
                << filepath.value().c_str();
    } else {
      filepath =
          filepath.AddExtension(FILE_PATH_LITERAL(".bad_thumbnails.png"));
      LOG(INFO) << "Write bad thumbnails image to: "
                << filepath.value().c_str();
    }
    int num_bytes =
        base::WriteFile(filepath, reinterpret_cast<char*>(&png[0]), png.size());
    LOG_ASSERT(num_bytes != -1);
    EXPECT_EQ(static_cast<size_t>(num_bytes), png.size());
    EXPECT_EQ(is_valid_thumbnail, true)
        << "Unknown thumbnails MD5: " << md5_string;
  }

  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    auto mismatched_frames = clients_[i]->GetMismatchedFramesInfo();
    for (const auto& info : mismatched_frames) {
      LOG(ERROR) << "Frame " << std::setw(4) << info.frame_index << " "
                 << info.computed_md5 << " (expected: " << info.expected_md5
                 << " )";
    }
    EXPECT_TRUE(mismatched_frames.empty())
        << "# of MD5 mismatched frames (Decoder #" << i
        << " ): " << mismatched_frames.size();
  }
  // Output the frame delivery time to file
  // We can only make performance/correctness assertions if the decoder was
  // allowed to finish.
  if (g_output_log != NULL && delete_decoder_state >= CS_FLUSHED) {
    base::File output_file(
        base::FilePath(g_output_log),
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    for (size_t i = 0; i < num_concurrent_decoders; ++i) {
      clients_[i]->OutputFrameDeliveryTimes(&output_file);
    }
  }
};

// Test that replay after EOS works fine.
INSTANTIATE_TEST_CASE_P(
    ReplayAfterEOS,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 4, END_OF_STREAM_RESET, CS_RESET, false, false)));

// Test that Reset() before the first Decode() works fine.
INSTANTIATE_TEST_CASE_P(ResetBeforeDecode,
                        VideoDecodeAcceleratorParamTest,
                        ::testing::Values(std::make_tuple(1,
                                                          1,
                                                          1,
                                                          START_OF_STREAM_RESET,
                                                          CS_RESET,
                                                          false,
                                                          false)));

// Test Reset() immediately after Decode() containing config info.
INSTANTIATE_TEST_CASE_P(
    ResetAfterFirstConfigInfo,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(std::make_tuple(1,
                                      1,
                                      1,
                                      RESET_AFTER_FIRST_CONFIG_INFO,
                                      CS_RESET,
                                      false,
                                      false)));

// Test Reset() immediately after Flush() and before NotifyFlushDone().
INSTANTIATE_TEST_CASE_P(
    ResetBeforeNotifyFlushDone,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(std::make_tuple(1,
                                      1,
                                      1,
                                      RESET_BEFORE_NOTIFY_FLUSH_DONE,
                                      CS_RESET,
                                      false,
                                      false)));

// Test that Reset() mid-stream works fine and doesn't affect decoding even when
// Decode() calls are made during the reset.
INSTANTIATE_TEST_CASE_P(
    MidStreamReset,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, MID_STREAM_RESET, CS_RESET, false, false)));

INSTANTIATE_TEST_CASE_P(
    SlowRendering,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, true, false)));

// Test that Destroy() mid-stream works fine (primarily this is testing that no
// crashes occur).
INSTANTIATE_TEST_CASE_P(
    TearDownTiming,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_DECODER_SET,
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_INITIALIZED,
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_FLUSHING,
                        false,
                        false),
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHED, false, false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESETTING,
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-1),
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-10),
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-100),
                        false,
                        false)));

// Test that decoding various variation works with multiple in-flight decodes.
INSTANTIATE_TEST_CASE_P(
    DecodeVariations,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, false, false),
        std::make_tuple(1, 10, 1, END_OF_STREAM_RESET, CS_RESET, false, false),
        // Tests queuing.
        std::make_tuple(1,
                        15,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESET,
                        false,
                        false)));

// Find out how many concurrent decoders can go before we exhaust system
// resources.
INSTANTIATE_TEST_CASE_P(
    ResourceExhaustion,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(std::make_tuple(kMinSupportedNumConcurrentDecoders,
                                      1,
                                      1,
                                      END_OF_STREAM_RESET,
                                      CS_RESET,
                                      false,
                                      false),
                      std::make_tuple(kMinSupportedNumConcurrentDecoders + 1,
                                      1,
                                      1,
                                      END_OF_STREAM_RESET,
                                      CS_RESET,
                                      false,
                                      false)));

// Allow MAYBE macro substitution.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(a, b, c) \
  INSTANTIATE_TEST_CASE_P(a, b, c)

#if defined(OS_WIN)
// There are no reference images for windows.
#define MAYBE_Thumbnail DISABLED_Thumbnail
#else
#define MAYBE_Thumbnail Thumbnail
#endif
// Thumbnailing test
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Thumbnail,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, false, true)));

// Measure the median of the decode time when VDA::Decode is called 30 times per
// second.
TEST_F(VideoDecodeAcceleratorTest, TestDecodeTimeMedian) {
  notes_.push_back(std::make_unique<ClientStateNotification<ClientState>>());

  const TestVideoFile* video_file = test_video_files_[0].get();
  GLRenderingVDAClient::Config config;
  EXPECT_EQ(video_file->reset_after_frame_num, kNoMidStreamReset);
  config.frame_size = gfx::Size(video_file->width, video_file->height);
  config.profile = video_file->profile;
  config.fake_decoder = g_fake_decoder;
  config.decode_calls_per_second = kWebRtcDecodeCallsPerSecond;
  config.num_frames = video_file->num_frames;

  clients_.push_back(std::make_unique<GLRenderingVDAClient>(
      std::move(config), video_file->data_str, &rendering_helper_, nullptr,
      notes_[0].get()));
  RenderingHelperParams helper_params;
  helper_params.num_windows = 1;
  InitializeRenderingHelper(helper_params);
  CreateAndStartDecoder(clients_[0].get(), notes_[0].get());
  ClientState last_state = WaitUntilDecodeFinish(notes_[0].get());
  EXPECT_NE(CS_ERROR, last_state);

  base::TimeDelta decode_time_median = clients_[0]->decode_time_median();
  std::string output_string =
      base::StringPrintf("Decode time median: %" PRId64 " us",
                         decode_time_median.InMicroseconds());
  LOG(INFO) << output_string;

  if (g_output_log != NULL)
    OutputLogFile(g_output_log, output_string);
}

// This test passes as long as there is no crash. If VDA notifies an error, it
// is not considered as a failure because the input may be unsupported or
// corrupted videos.
TEST_F(VideoDecodeAcceleratorTest, NoCrash) {
  notes_.push_back(std::make_unique<ClientStateNotification<ClientState>>());

  const TestVideoFile* video_file = test_video_files_[0].get();
  GLRenderingVDAClient::Config config;
  EXPECT_EQ(video_file->reset_after_frame_num, kNoMidStreamReset);
  config.frame_size = gfx::Size(video_file->width, video_file->height);
  config.profile = video_file->profile;
  config.fake_decoder = g_fake_decoder;
  config.num_frames = video_file->num_frames;

  clients_.push_back(std::make_unique<GLRenderingVDAClient>(
      std::move(config), video_file->data_str, &rendering_helper_, nullptr,
      notes_[0].get()));
  RenderingHelperParams helper_params;
  helper_params.num_windows = 1;
  InitializeRenderingHelper(helper_params);
  CreateAndStartDecoder(clients_[0].get(), notes_[0].get());
  WaitUntilDecodeFinish(notes_[0].get());
}

// TODO(fischman, vrk): add more tests!  In particular:
// - Test life-cycle: Seek/Stop/Pause/Play for a single decoder.
// - Test alternate configurations
// - Test failure conditions.
// - Test frame size changes mid-stream

class VDATestSuite : public base::TestSuite {
 public:
  VDATestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  int Run() {
#if defined(OS_WIN) || defined(OS_CHROMEOS)
    // For windows the decoding thread initializes the media foundation decoder
    // which uses COM. We need the thread to be a UI thread.
    // On Ozone, the backend initializes the event system using a UI
    // thread.
    base::test::ScopedTaskEnvironment scoped_task_environment(
        base::test::ScopedTaskEnvironment::MainThreadType::UI);
#else
    base::test::ScopedTaskEnvironment scoped_task_environment;
#endif  // OS_WIN || OS_CHROMEOS

    media::g_env =
        reinterpret_cast<media::test::VideoDecodeAcceleratorTestEnvironment*>(
            testing::AddGlobalTestEnvironment(
                new media::test::VideoDecodeAcceleratorTestEnvironment(
                    g_use_gl_renderer)));

#if defined(OS_CHROMEOS)
    ui::OzonePlatform::InitParams params;
    params.single_process = false;
    ui::OzonePlatform::InitializeForUI(params);
#endif

#if BUILDFLAG(USE_VAAPI)
    media::VaapiWrapper::PreSandboxInitialization();
#elif defined(OS_WIN)
    media::DXVAVideoDecodeAccelerator::PreSandboxInitialization();
#endif
    return base::TestSuite::Run();
  }
};

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  mojo::core::Init();
  media::VDATestSuite test_suite(argc, argv);

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_video_data") {
      media::g_test_video_data = it->second.c_str();
      continue;
    }
    // The output log for VDA performance test.
    if (it->first == "output_log") {
      media::g_output_log = it->second.c_str();
      continue;
    }
    if (it->first == "rendering_fps") {
      // On Windows, CommandLine::StringType is wstring. We need to convert
      // it to std::string first
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToDouble(input, &media::g_rendering_fps));
      continue;
    }
    if (it->first == "disable_rendering") {
      media::g_use_gl_renderer = false;
      continue;
    }

    if (it->first == "num_play_throughs") {
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToSizeT(input, &media::g_num_play_throughs));
      continue;
    }
    if (it->first == "fake_decoder") {
      media::g_fake_decoder = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "ozone-platform" || it->first == "ozone-use-surfaceless")
      continue;
    if (it->first == "test_import") {
      media::g_test_import = true;
      continue;
    }
    if (it->first == "frame_validator") {
      media::g_frame_validator = true;
      continue;
    }
    if (it->first == "use-test-data-path") {
      media::g_test_file_path = media::GetTestDataFilePath("");
      continue;
    }
    if (it->first == "thumbnail_output_dir") {
      media::g_thumbnail_output_dir = base::FilePath(it->second.c_str());
    }
  }

  base::ShadowingAtExitManager at_exit_manager;

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::Bind(&media::VDATestSuite::Run, base::Unretained(&test_suite)));
}
