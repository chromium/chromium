// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/bitrate.h"
#include "media/base/media_util.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/color_space.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Values;
using ::testing::WithArgs;

namespace media {

class VideoEncodeAcceleratorAdapterTest
    : public ::testing::TestWithParam<VideoPixelFormat> {
 public:
  VideoEncodeAcceleratorAdapterTest() = default;

  void SetUp() override {
    vea_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

    vea_ = new FakeVideoEncodeAccelerator(vea_runner_);
    sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
    gpu_factories_ =
        std::make_unique<MockGpuVideoAcceleratorFactories>(sii_.get());
    supported_profiles_ = {
        VideoEncodeAccelerator::SupportedProfile(
            profile_,
            /*max_resolution=*/gfx::Size(3840, 2160),
            /*max_framerate_numerator=*/30,
            /*max_framerate_denominator=*/1,
            /*rc_modes=*/VideoEncodeAccelerator::kConstantMode |
                VideoEncodeAccelerator::kVariableMode),
    };

    EXPECT_CALL(*gpu_factories_.get(),
                GetVideoEncodeAcceleratorSupportedProfiles())
        .WillRepeatedly(Return(supported_profiles_));
    EXPECT_CALL(*gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
        .WillRepeatedly(Return(vea_.get()));
    EXPECT_CALL(*gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(vea_runner_));

    auto media_log = std::make_unique<NullMediaLog>();
    callback_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    vae_adapter_ = std::make_unique<VideoEncodeAcceleratorAdapter>(
        gpu_factories_.get(), media_log->Clone(), callback_runner_);
  }

  void TearDown() override {
    vea_runner_->DeleteSoon(FROM_HERE, std::move(vae_adapter_));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  VideoEncodeAcceleratorAdapter* adapter() { return vae_adapter_.get(); }
  FakeVideoEncodeAccelerator* vea() { return vea_; }

  scoped_refptr<VideoFrame> CreateGreenGpuFrame(gfx::Size size,
                                                base::TimeDelta timestamp) {
    auto gmb = gpu_factories_->CreateGpuMemoryBuffer(
        size, gfx::BufferFormat::YUV_420_BIPLANAR,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);

    if (!gmb || !gmb->Map())
      return nullptr;

    // Green NV12 frame (Y:0x96, U:0x40, V:0x40)
    const auto gmb_size = gmb->GetSize();
    memset(static_cast<uint8_t*>(gmb->memory(0)), 0x96,
           gmb->stride(0) * gmb_size.height());
    memset(static_cast<uint8_t*>(gmb->memory(1)), 0x28,
           gmb->stride(1) * gmb_size.height() / 2);
    gmb->Unmap();

    auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
        gfx::Rect(gmb_size), size, std::move(gmb), timestamp);
    frame->set_color_space(kYUVColorSpace);
    return frame;
  }

  scoped_refptr<VideoFrame> CreateGreenCpuFrame(gfx::Size size,
                                                base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                         gfx::Rect(size), size, timestamp);

    // Green I420 frame (Y:0x96, U:0x40, V:0x40)
    libyuv::I420Rect(frame->writable_data(VideoFrame::Plane::kY),
                     frame->stride(VideoFrame::Plane::kY),
                     frame->writable_data(VideoFrame::Plane::kU),
                     frame->stride(VideoFrame::Plane::kU),
                     frame->writable_data(VideoFrame::Plane::kV),
                     frame->stride(VideoFrame::Plane::kV),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
                     0x96,                            // Y color
                     0x40,                            // U color
                     0x40);                           // V color

    frame->set_color_space(kYUVColorSpace);
    return frame;
  }

  scoped_refptr<VideoFrame> CreateGreenCpuFrameARGB(gfx::Size size,
                                                    base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_XRGB, size,
                                         gfx::Rect(size), size, timestamp);

    // Green XRGB frame (R:0x3B, G:0xD9, B:0x24)
    libyuv::ARGBRect(frame->writable_data(VideoFrame::Plane::kARGB),
                     frame->stride(VideoFrame::Plane::kARGB),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
                     0x24D93B00);                     // V color

    frame->set_color_space(kRGBColorSpace);
    return frame;
  }

  scoped_refptr<VideoFrame> CreateGreenFrame(gfx::Size size,
                                             VideoPixelFormat format,
                                             base::TimeDelta timestamp) {
    switch (format) {
      case PIXEL_FORMAT_I420:
        return CreateGreenCpuFrame(size, timestamp);
      case PIXEL_FORMAT_NV12:
        return CreateGreenGpuFrame(size, timestamp);
      case PIXEL_FORMAT_XRGB:
        return CreateGreenCpuFrameARGB(size, timestamp);
      default:
        EXPECT_TRUE(false) << "not supported pixel format";
        return nullptr;
    }
  }

  gfx::ColorSpace ExpectedColorSpace(VideoPixelFormat src_format,
                                     VideoPixelFormat dst_format) {
    // Converting between YUV formats doesn't change the color space.
    if (IsYuvPlanar(src_format) && IsYuvPlanar(dst_format)) {
      return kYUVColorSpace;
    }

    // libyuv's RGB to YUV methods always output BT.601.
    if (IsRGB(src_format) && IsYuvPlanar(dst_format)) {
      return gfx::ColorSpace::CreateREC601();
    }

    EXPECT_TRUE(false) << "unexpected formats: src=" << src_format
                       << ", dst=" << dst_format;
    return gfx::ColorSpace();
  }

  VideoEncoder::EncoderStatusCB ValidatingStatusCB(
      base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    return base::BindLambdaForTesting(
        [this, enforcer{std::move(enforcer)}](EncoderStatus s) {
          EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
          EXPECT_TRUE(s.is_ok()) << " Callback created: " << enforcer->location
                                 << " Error: " << s.message();
          enforcer->called = true;
        });
  }

 protected:
  VideoCodecProfile profile_ = VP8PROFILE_ANY;
  static constexpr gfx::ColorSpace kRGBColorSpace =
      gfx::ColorSpace::CreateSRGB();
  static constexpr gfx::ColorSpace kYUVColorSpace =
      gfx::ColorSpace::CreateREC709();
  std::vector<VideoEncodeAccelerator::SupportedProfile> supported_profiles_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<FakeVideoEncodeAccelerator, AcrossTasksDanglingUntriaged>
      vea_;  // owned by |vae_adapter_|
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<VideoEncodeAcceleratorAdapter> vae_adapter_;
  scoped_refptr<base::SequencedTaskRunner> vea_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
};

TEST_F(VideoEncodeAcceleratorAdapterTest, PreInitialize) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);

  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        /*output_cb=*/base::DoNothing(), ValidatingStatusCB());
  RunUntilIdle();
}

TEST_F(VideoEncodeAcceleratorAdapterTest, InitializeAfterFirstFrame) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto pixel_format = PIXEL_FORMAT_I420;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);

  bool info_cb_called = false;
  VideoEncoder::EncoderInfoCB info_cb = base::BindLambdaForTesting(
      [&](const VideoEncoderInfo& info) { info_cb_called = true; });

  int outputs_count = 0;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(keyframe, true);
        EXPECT_EQ(frame->format(), pixel_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, std::move(info_cb),
                        std::move(output_cb), ValidatingStatusCB());

  auto frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));

  adapter()->Encode(frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  vea()->NotifyEncoderInfoChange(VideoEncoderInfo());
  RunUntilIdle();
  EXPECT_TRUE(info_cb_called);
  EXPECT_EQ(outputs_count, 1);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, TemporalSvc) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  options.scalability_mode = SVCScalabilityMode::kL1T3;
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        if (output.timestamp == base::Milliseconds(1))
          EXPECT_EQ(output.temporal_id, 1);
        else if (output.timestamp == base::Milliseconds(2))
          EXPECT_EQ(output.temporal_id, 1);
        else if (output.timestamp == base::Milliseconds(3))
          EXPECT_EQ(output.temporal_id, 2);
        else if (output.timestamp == base::Milliseconds(4))
          EXPECT_EQ(output.temporal_id, 2);
        else
          EXPECT_EQ(output.temporal_id, 2);

        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        BitstreamBufferMetadata result(1, keyframe, frame->timestamp());
        if (frame->timestamp() == base::Milliseconds(1)) {
          result.h264 = H264Metadata();
          result.h264->temporal_idx = 1;
        } else if (frame->timestamp() == base::Milliseconds(2)) {
          result.vp8 = Vp8Metadata();
          result.vp8->temporal_idx = 1;
        } else if (frame->timestamp() == base::Milliseconds(3)) {
          result.vp9 = Vp9Metadata();
          result.vp9->temporal_idx = 2;
        } else if (frame->timestamp() == base::Milliseconds(4)) {
          result.av1 = Av1Metadata();
          result.av1->temporal_idx = 2;
        } else {
          result.h265 = H265Metadata();
          result.h265->temporal_idx = 2;
        }
        return result;
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  auto frame1 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  auto frame2 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(2));
  auto frame3 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(3));
  auto frame4 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(4));
  auto frame5 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(5));
  adapter()->Encode(frame1, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  adapter()->Encode(frame2, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  adapter()->Encode(frame3, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  adapter()->Encode(frame4, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  adapter()->Encode(frame5, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 5);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, FlushDuringInitialize) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(keyframe, true);
        EXPECT_EQ(frame->format(), pixel_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  auto frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  adapter()->Flush(base::BindLambdaForTesting([&](EncoderStatus s) {
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(outputs_count, 1);
  }));
  RunUntilIdle();
}

TEST_F(VideoEncodeAcceleratorAdapterTest, InitializationError) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, std::optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  VideoEncoder::EncoderStatusCB expect_error_done_cb =
      base::BindLambdaForTesting(
          [&](EncoderStatus s) { EXPECT_FALSE(s.is_ok()); });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_TRUE(false) << "should never come here";
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(
      VIDEO_CODEC_PROFILE_UNKNOWN, options, /*info_cb=*/base::DoNothing(),
      std::move(output_cb), base::BindLambdaForTesting([](EncoderStatus s) {
        EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderUnsupportedProfile);
      }));

  auto frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(frame, VideoEncoder::EncodeOptions(true),
                    std::move(expect_error_done_cb));
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 0);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, EncodingError) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, std::optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  VideoEncoder::EncoderStatusCB expect_error_done_cb =
      base::BindLambdaForTesting(
          [&](EncoderStatus s) { EXPECT_FALSE(s.is_ok()); });

  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  vea()->SetWillEncodingSucceed(false);

  auto frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(frame, VideoEncoder::EncodeOptions(true),
                    std::move(expect_error_done_cb));
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 0);
}

TEST_P(VideoEncodeAcceleratorAdapterTest, TwoFramesResize) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  gfx::Size small_size(480, 320);
  gfx::Size large_size(800, 600);
  auto pixel_format = GetParam();
  auto small_frame =
      CreateGreenFrame(small_size, pixel_format, base::Milliseconds(1));
  auto large_frame =
      CreateGreenFrame(large_size, pixel_format, base::Milliseconds(2));

  VideoPixelFormat expected_input_format = PIXEL_FORMAT_I420;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (pixel_format != PIXEL_FORMAT_I420 || !small_frame->IsMappable())
    expected_input_format = PIXEL_FORMAT_NV12;
#endif
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, expected_input_format);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(frame->format(), expected_input_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  adapter()->Encode(small_frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  adapter()->Encode(large_frame, VideoEncoder::EncodeOptions(false),
                    ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 2);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, AutomaticResizeSupport) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  gfx::Size small_size(480, 320);
  auto pixel_format = PIXEL_FORMAT_NV12;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(frame->coded_size(), small_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  vea()->SupportResize();
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  auto frame1 =
      CreateGreenFrame(small_size, pixel_format, base::Milliseconds(1));
  auto frame2 =
      CreateGreenFrame(small_size, pixel_format, base::Milliseconds(2));
  adapter()->Encode(frame1, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  adapter()->Encode(frame2, VideoEncoder::EncodeOptions(false),
                    ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 2);
}

TEST_P(VideoEncodeAcceleratorAdapterTest, RunWithAllPossibleInputConversions) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  gfx::Size small_size(480, 320);
  gfx::Size same_size = options.frame_size;
  gfx::Size large_size(800, 600);
  int frames_to_encode = 33;
  auto pixel_format = GetParam();
  auto input_kind =
      (pixel_format == PIXEL_FORMAT_NV12)
          ? VideoEncodeAcceleratorAdapter::InputBufferKind::GpuMemBuf
          : VideoEncodeAcceleratorAdapter::InputBufferKind::CpuMemBuf;
  adapter()->SetInputBufferPreferenceForTesting(input_kind);

  const VideoPixelFormat expected_input_format =
      input_kind == VideoEncodeAcceleratorAdapter::InputBufferKind::GpuMemBuf
          ? PIXEL_FORMAT_NV12
          : PIXEL_FORMAT_I420;

  constexpr auto get_source_format = [](int i) {
    // Every 4 frames switch between the 3 supported formats.
    const int rem = i % 12;
    auto format = PIXEL_FORMAT_XRGB;
    if (rem < 4)
      format = PIXEL_FORMAT_I420;
    else if (rem < 8)
      format = PIXEL_FORMAT_NV12;
    return format;
  };

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        VideoPixelFormat source_frame_format = get_source_format(outputs_count);
        const gfx::ColorSpace expected_color_space =
            ExpectedColorSpace(source_frame_format, expected_input_format);
        EXPECT_EQ(output.color_space, expected_color_space);
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(frame->format(), expected_input_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  for (int frame_index = 0; frame_index < frames_to_encode; frame_index++) {
    gfx::Size size;
    if (frame_index % 4 == 0)
      size = large_size;
    else if (frame_index % 4 == 1)
      size = small_size;
    else
      size = same_size;

    const auto format = get_source_format(frame_index);
    bool key = frame_index % 9 == 0;
    auto frame =
        CreateGreenFrame(size, format, base::Milliseconds(frame_index));
    adapter()->Encode(frame, VideoEncoder::EncodeOptions(key),
                      ValidatingStatusCB());
  }

  RunUntilIdle();
  EXPECT_EQ(outputs_count, frames_to_encode);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, DroppedFrame) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto pixel_format = PIXEL_FORMAT_I420;
  std::vector<base::TimeDelta> output_timestamps;
  std::vector<base::TimeDelta> dropped_output_timestamps;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        if (output.data.empty()) {
          dropped_output_timestamps.push_back(output.timestamp);
          return;
        }
        EXPECT_EQ(output.color_space, expected_color_space);
        output_timestamps.push_back(output.timestamp);
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        size_t size = keyframe ? 1 : 0;  // Drop non-key frame
        return BitstreamBufferMetadata(size, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(output_cb), ValidatingStatusCB());

  auto frame1 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  auto frame2 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(2));
  auto frame3 =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(3));
  adapter()->Encode(frame1, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  adapter()->Encode(frame2, VideoEncoder::EncodeOptions(false),
                    ValidatingStatusCB());
  adapter()->Encode(frame3, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();
  ASSERT_EQ(output_timestamps.size(), 2u);
  EXPECT_EQ(output_timestamps[0], base::Milliseconds(1));
  EXPECT_EQ(output_timestamps[1], base::Milliseconds(3));
  ASSERT_EQ(dropped_output_timestamps.size(), 1u);
  EXPECT_EQ(dropped_output_timestamps[0], base::Milliseconds(2));
}

TEST_F(VideoEncodeAcceleratorAdapterTest,
       ChangeOptions_ChangeVariableBitrateSmokeTest) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  options.bitrate = Bitrate::VariableBitrate(1111u, 2222u);
  auto pixel_format = PIXEL_FORMAT_I420;
  int output_count_before_change = 0;
  int output_count_after_change = 0;
  const gfx::ColorSpace expected_color_space =
      ExpectedColorSpace(pixel_format, pixel_format);
  VideoEncoder::OutputCB first_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        output_count_before_change++;
      });
  VideoEncoder::OutputCB second_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.color_space, expected_color_space);
        output_count_after_change++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(keyframe, true);
        EXPECT_EQ(frame->format(), pixel_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(first_output_cb), ValidatingStatusCB());
  // We must encode one frame before we can change options.
  auto first_frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(first_frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();

  options.bitrate = Bitrate::VariableBitrate(12345u, 23456u);
  adapter()->ChangeOptions(
      options, std::move(second_output_cb),
      base::BindLambdaForTesting([&](EncoderStatus s) {
        auto second_frame = CreateGreenFrame(options.frame_size, pixel_format,
                                             base::Milliseconds(2));
        adapter()->Encode(second_frame, VideoEncoder::EncodeOptions(true),
                          ValidatingStatusCB());
      }));
  RunUntilIdle();

  EXPECT_EQ(output_count_before_change, 1);
  EXPECT_EQ(output_count_after_change, 1);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, ChangeOptions_ChangeFrameSize) {
  VideoEncoder::Options options;
  auto first_frame_size = gfx::Size(640, 480);
  auto second_frame_size = gfx::Size(1280, 720);
  auto pixel_format = PIXEL_FORMAT_I420;
  int output_count_before_change = 0;
  int output_count_after_change = 0;
  options.frame_size = first_frame_size;
  options.bitrate = Bitrate::VariableBitrate(1111u, 2222u);

  VideoEncoder::OutputCB first_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.encoded_size, first_frame_size);
        output_count_before_change++;
      });
  VideoEncoder::OutputCB second_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.encoded_size, second_frame_size);
        output_count_after_change++;
      });

  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(first_output_cb), ValidatingStatusCB());
  auto first_frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(first_frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();

  options.frame_size = second_frame_size;
  adapter()->ChangeOptions(
      options, std::move(second_output_cb),
      base::BindLambdaForTesting([&](EncoderStatus s) {
        EXPECT_TRUE(s.is_ok());
        auto second_frame = CreateGreenFrame(options.frame_size, pixel_format,
                                             base::Milliseconds(2));
        adapter()->Encode(second_frame, VideoEncoder::EncodeOptions(true),
                          ValidatingStatusCB());
      }));
  RunUntilIdle();

  EXPECT_EQ(output_count_before_change, 1);
  EXPECT_EQ(output_count_after_change, 1);
}

TEST_F(VideoEncodeAcceleratorAdapterTest,
       ChangeOptions_ChangeFrameSizeNotSupported) {
  VideoEncoder::Options options;
  auto first_frame_size = gfx::Size(640, 480);
  auto second_frame_size = gfx::Size(1280, 720);
  auto pixel_format = PIXEL_FORMAT_I420;
  int output_count_before_change = 0;
  int output_count_after_change = 0;
  options.frame_size = first_frame_size;
  options.bitrate = Bitrate::VariableBitrate(1111u, 2222u);

  VideoEncoder::OutputCB first_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.encoded_size, first_frame_size);
        output_count_before_change++;
      });
  VideoEncoder::OutputCB second_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription>) {
        EXPECT_EQ(output.encoded_size, second_frame_size);
        output_count_after_change++;
      });

  adapter()->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                        std::move(first_output_cb), ValidatingStatusCB());
  auto first_frame =
      CreateGreenFrame(options.frame_size, pixel_format, base::Milliseconds(1));
  adapter()->Encode(first_frame, VideoEncoder::EncodeOptions(true),
                    ValidatingStatusCB());
  RunUntilIdle();

  vea()->SetSupportFrameSizeChange(false);
  options.frame_size = second_frame_size;
  adapter()->ChangeOptions(options, std::move(second_output_cb),
                           base::BindLambdaForTesting([](EncoderStatus s) {
                             EXPECT_FALSE(s.is_ok());
                           }));

  EXPECT_EQ(output_count_before_change, 1);
  EXPECT_EQ(output_count_after_change, 0);
}

INSTANTIATE_TEST_SUITE_P(VideoEncodeAcceleratorAdapterTest,
                         VideoEncodeAcceleratorAdapterTest,
                         ::testing::Values(PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_XRGB));

}  // namespace media
