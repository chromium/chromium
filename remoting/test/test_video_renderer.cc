// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_video_renderer.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "remoting/test/rgb_value.h"
#include "remoting/test/video_frame_writer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace {

// Used to account for frame resizing and lossy encoding error in percentage.
// The average color usually only varies by 1 on each channel, so 0.01 is large
// enough to allow variations while not being flaky for false negative cases.
const double kMaxColorError = 0.01;

}  // namespace

namespace remoting {
namespace test {

// Implements video decoding functionality.
class TestVideoRenderer::Core {
 public:
  Core();
  ~Core();

  // Initializes the internal structures of the class.
  void Initialize();

  // Used to decode video packets.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          base::OnceClosure done);

  // Initialize a decoder to decode video packets.
  void SetCodecForDecoding(const protocol::ChannelConfig::Codec codec);

  // Returns a copy of the current frame.
  std::unique_ptr<webrtc::DesktopFrame> GetCurrentFrameForTest() const;

  // Set expected image pattern for comparison and the callback will be called
  // when the pattern is matched.
  void ExpectAverageColorInRect(
      const webrtc::DesktopRect& expected_rect,
      const RGBValue& expected_avg_color,
      const base::Closure& image_pattern_matched_callback);

  // Turn on/off saving video frames to disk.
  void save_frame_data_to_disk(bool save_frame_data_to_disk) {
    save_frame_data_to_disk_ = save_frame_data_to_disk;
  }

 private:
  // Returns average color of pixels fall within |rect| on the current frame.
  RGBValue CalculateAverageColorValue(const webrtc::DesktopRect& rect) const;

  // Compares |candidate_avg_value| to |expected_avg_color_|.
  // Returns true if the root mean square of the errors in the R, G and B
  // components does not exceed a given limit.
  bool ExpectedAverageColorIsMatched(const RGBValue& candidate_avg_value) const;

  // Used to ensure Core methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Used to decode video packets.
  std::unique_ptr<VideoDecoder> decoder_;

  // Used to post tasks back to main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Protects access to |frame_|.
  mutable base::Lock lock_;

  // Used to store decoded video frame.
  std::unique_ptr<webrtc::SharedDesktopFrame> frame_;

  // Used to store the expected image pattern.
  webrtc::DesktopRect expected_rect_;
  RGBValue expected_avg_color_;

  // Used to store the callback when expected pattern is matched.
  base::Closure image_pattern_matched_callback_;

  // Used to identify whether saving frame frame data to disk.
  bool save_frame_data_to_disk_;

  // Used to dump video frames and generate image patterns.
  VideoFrameWriter video_frame_writer;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

TestVideoRenderer::Core::Core()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      save_frame_data_to_disk_(false) {
  thread_checker_.DetachFromThread();
}

TestVideoRenderer::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestVideoRenderer::Core::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestVideoRenderer::Core::SetCodecForDecoding(
    const protocol::ChannelConfig::Codec codec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_) {
    LOG(WARNING) << "Decoder is set more than once";
  }

  switch (codec) {
    case protocol::ChannelConfig::CODEC_VP8: {
      VLOG(1) << "Test Video Renderer will use VP8 decoder";
      decoder_ = VideoDecoderVpx::CreateForVP8();
      break;
    }
    case protocol::ChannelConfig::CODEC_VP9: {
      VLOG(1) << "Test Video Renderer will use VP9 decoder";
      decoder_ = VideoDecoderVpx::CreateForVP9();
      break;
    }
    case protocol::ChannelConfig::CODEC_VERBATIM: {
      VLOG(1) << "Test Video Renderer will use VERBATIM decoder";
      decoder_.reset(new VideoDecoderVerbatim());
      break;
    }
    default: {
      NOTREACHED() << "Unsupported codec: " << codec;
    }
  }
}

std::unique_ptr<webrtc::DesktopFrame>
TestVideoRenderer::Core::GetCurrentFrameForTest() const {
  base::AutoLock auto_lock(lock_);
  DCHECK(frame_);
  return base::WrapUnique(webrtc::BasicDesktopFrame::CopyOf(*frame_));
}

void TestVideoRenderer::Core::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> packet,
    base::OnceClosure done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(decoder_);
  DCHECK(packet);

  VLOG(2) << "TestVideoRenderer::Core::ProcessVideoPacket() Called";

  // Screen size is attached on the first packet as well as when the
  // host screen is resized.
  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    webrtc::DesktopSize source_size(packet->format().screen_width(),
                                    packet->format().screen_height());
    if (!frame_ || !frame_->size().equals(source_size)) {
      base::AutoLock auto_lock(lock_);
      frame_.reset(webrtc::SharedDesktopFrame::Wrap(
          new webrtc::BasicDesktopFrame(source_size)));
    }
  }

  // Render the result into a new DesktopFrame instance that shares buffer with
  // |frame_|. updated_region() will be updated for |new_frame|, but not for
  // |frame_|.
  std::unique_ptr<webrtc::DesktopFrame> new_frame(frame_->Share());

  {
    base::AutoLock auto_lock(lock_);
    if (!decoder_->DecodePacket(*packet, new_frame.get())) {
      LOG(ERROR) << "Decoder::DecodePacket() failed.";
      return;
    }
  }

  main_task_runner_->PostTask(FROM_HERE, std::move(done));

  if (save_frame_data_to_disk_) {
    std::unique_ptr<webrtc::DesktopFrame> frame(
        webrtc::BasicDesktopFrame::CopyOf(*frame_));
    video_frame_writer.HighlightRectInFrame(frame.get(), expected_rect_);
    video_frame_writer.WriteFrameToDefaultPath(*frame);
  }

  // Check to see if a image pattern matched reply is passed in, and whether
  // the |expected_rect_| falls within the current frame.
  if (image_pattern_matched_callback_.is_null() ||
      expected_rect_.right() > frame_->size().width() ||
      expected_rect_.bottom() > frame_->size().height()) {
    return;
  }
  // Compare the expected image pattern with the corresponding rectangle
  // region
  // on the current frame.
  RGBValue accumulating_avg_value = CalculateAverageColorValue(expected_rect_);
  if (ExpectedAverageColorIsMatched(accumulating_avg_value)) {
    main_task_runner_->PostTask(FROM_HERE,
                                std::move(image_pattern_matched_callback_));
  }
}

void TestVideoRenderer::Core::ExpectAverageColorInRect(
    const webrtc::DesktopRect& expected_rect,
    const RGBValue& expected_avg_color,
    const base::Closure& image_pattern_matched_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  expected_rect_ = expected_rect;
  expected_avg_color_ = expected_avg_color;
  image_pattern_matched_callback_ = image_pattern_matched_callback;
}

RGBValue TestVideoRenderer::Core::CalculateAverageColorValue(
    const webrtc::DesktopRect& rect) const {
  int red_sum = 0;
  int green_sum = 0;
  int blue_sum = 0;

  // Loop through pixels that fall within |accumulating_rect_| to obtain the
  // average color value.
  for (int y = rect.top(); y < rect.bottom(); ++y) {
    uint8_t* frame_pos =
        frame_->data() + (y * frame_->stride() +
                          rect.left() * webrtc::DesktopFrame::kBytesPerPixel);

    // Pixels of decoded video frame are presented in ARGB format.
    for (int x = 0; x < rect.width(); ++x) {
      red_sum += frame_pos[2];
      green_sum += frame_pos[1];
      blue_sum += frame_pos[0];
      frame_pos += 4;
    }
  }

  int area = rect.width() * rect.height();
  RGBValue rgb_value(red_sum / area, green_sum / area, blue_sum / area);
  return rgb_value;
}

bool TestVideoRenderer::Core::ExpectedAverageColorIsMatched(
    const RGBValue& candidate_avg_value) const {
  double error_sum_squares = 0;
  double red_error = expected_avg_color_.red - candidate_avg_value.red;
  double green_error = expected_avg_color_.green - candidate_avg_value.green;
  double blue_error = expected_avg_color_.blue - candidate_avg_value.blue;
  error_sum_squares = red_error * red_error + green_error * green_error +
                      blue_error * blue_error;
  error_sum_squares /= (255.0 * 255.0);

  return sqrt(error_sum_squares / 3) < kMaxColorError;
}

TestVideoRenderer::TestVideoRenderer()
    : video_decode_thread_(
          new base::Thread("TestVideoRendererVideoDecodingThread")) {
  DCHECK(thread_checker_.CalledOnValidThread());

  core_.reset(new Core());
  if (!video_decode_thread_->Start()) {
    LOG(ERROR) << "Cannot start TestVideoRenderer";
  } else {
    video_decode_task_runner_ = video_decode_thread_->task_runner();
    video_decode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::Initialize, base::Unretained(core_.get())));
  }
}

TestVideoRenderer::~TestVideoRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_decode_task_runner_->DeleteSoon(FROM_HERE, core_.release());

  // The thread's message loop will run until it runs out of work.
  video_decode_thread_->Stop();
}

bool TestVideoRenderer::Initialize(
    const ClientContext& client_context,
    protocol::FrameStatsConsumer* stats_consumer) {
  return true;
}

void TestVideoRenderer::OnSessionConfig(const protocol::SessionConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(2) << "TestVideoRenderer::OnSessionConfig() Called";
  protocol::ChannelConfig::Codec codec = config.video_config().codec;
  SetCodecForDecoding(codec);
}

protocol::VideoStub* TestVideoRenderer::GetVideoStub() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(2) << "TestVideoRenderer::GetVideoStub() Called";
  return this;
}

protocol::FrameConsumer* TestVideoRenderer::GetFrameConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED();
  return nullptr;
}
protocol::FrameStatsConsumer* TestVideoRenderer::GetFrameStatsConsumer() {
  return nullptr;
}

void TestVideoRenderer::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    base::OnceClosure done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_decode_task_runner_) << "Failed to start video decode thread";

  if (video_packet->has_data() && video_packet->data().size() != 0) {
    VLOG(2) << "process video packet is called!";

    // Post video process task to the video decode thread.
    base::OnceClosure process_video_task =
        base::BindOnce(&TestVideoRenderer::Core::ProcessVideoPacket,
                       base::Unretained(core_.get()), std::move(video_packet),
                       std::move(done));
    video_decode_task_runner_->PostTask(FROM_HERE,
                                        std::move(process_video_task));
  } else {
    // Log at a high verbosity level as we receive empty packets frequently and
    // they can clutter up the debug output if the level is set too low.
    VLOG(3) << "Empty Video Packet received.";
    std::move(done).Run();
  }
}

void TestVideoRenderer::SetCodecForDecoding(
    const protocol::ChannelConfig::Codec codec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(2) << "TestVideoRenderer::SetDecoder() Called";
  video_decode_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetCodecForDecoding,
                                base::Unretained(core_.get()), codec));
}

std::unique_ptr<webrtc::DesktopFrame>
TestVideoRenderer::GetCurrentFrameForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return core_->GetCurrentFrameForTest();
}

void TestVideoRenderer::ExpectAverageColorInRect(
    const webrtc::DesktopRect& expected_rect,
    const RGBValue& expected_avg_color,
    const base::Closure& image_pattern_matched_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!expected_rect.is_empty()) << "Expected rect cannot be empty";

  DVLOG(2) << "TestVideoRenderer::SetImagePatternAndMatchedCallback() Called";
  video_decode_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::ExpectAverageColorInRect,
                     base::Unretained(core_.get()), expected_rect,
                     expected_avg_color, image_pattern_matched_callback));
}

void TestVideoRenderer::SaveFrameDataToDisk(bool save_frame_data_to_disk) {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_decode_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::save_frame_data_to_disk,
                     base::Unretained(core_.get()), save_frame_data_to_disk));
}

}  // namespace test
}  // namespace remoting
