// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

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
    gpu_factories_ =
        std::make_unique<MockGpuVideoAcceleratorFactories>(nullptr);
    EXPECT_CALL(*gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
        .WillRepeatedly(Return(vea_));
    EXPECT_CALL(*gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(vea_runner_));

    callback_runner_ = base::SequencedTaskRunnerHandle::Get();
    vae_adapter_ = std::make_unique<VideoEncodeAcceleratorAdapter>(
        gpu_factories_.get(), callback_runner_);
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

    gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
    return VideoFrame::WrapExternalGpuMemoryBuffer(
        gfx::Rect(gmb_size), size, std::move(gmb), empty_mailboxes,
        base::NullCallback(), timestamp);
  }

  scoped_refptr<VideoFrame> CreateGreenCpuFrame(gfx::Size size,
                                                base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                         gfx::Rect(size), size, timestamp);

    // Green I420 frame (Y:0x96, U:0x40, V:0x40)
    libyuv::I420Rect(
        frame->data(VideoFrame::kYPlane), frame->stride(VideoFrame::kYPlane),
        frame->data(VideoFrame::kUPlane), frame->stride(VideoFrame::kUPlane),
        frame->data(VideoFrame::kVPlane), frame->stride(VideoFrame::kVPlane),
        0,                               // left
        0,                               // top
        frame->visible_rect().width(),   // right
        frame->visible_rect().height(),  // bottom
        0x96,                            // Y color
        0x40,                            // U color
        0x40);                           // V color

    return frame;
  }

  scoped_refptr<VideoFrame> CreateGreenCpuFrameARGB(gfx::Size size,
                                                    base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_XRGB, size,
                                         gfx::Rect(size), size, timestamp);

    // Green XRGB frame (R:0x3B, G:0xD9, B:0x24)
    libyuv::ARGBRect(frame->data(VideoFrame::kARGBPlane),
                     frame->stride(VideoFrame::kARGBPlane),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
                     0x24D93B00);                     // V color

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

  VideoEncoder::StatusCB ValidatingStatusCB(base::Location loc = FROM_HERE) {
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
        [this, enforcer{std::move(enforcer)}](Status s) {
          EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
          EXPECT_TRUE(s.is_ok()) << " Callback created: " << enforcer->location
                                 << " Error: " << s.message();
          enforcer->called = true;
        });
  }

 protected:
  VideoCodecProfile profile_ = VP8PROFILE_ANY;
  base::test::TaskEnvironment task_environment_;
  FakeVideoEncodeAccelerator* vea_;  // owned by |vae_adapter_|
  std::unique_ptr<MockGpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<VideoEncodeAcceleratorAdapter> vae_adapter_;
  scoped_refptr<base::SequencedTaskRunner> vea_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
};

TEST_F(VideoEncodeAcceleratorAdapterTest, PreInitialize) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
      });

  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());
  RunUntilIdle();
}

TEST_F(VideoEncodeAcceleratorAdapterTest, InitializeAfterFirstFrame) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(keyframe, true);
        EXPECT_EQ(frame->format(), pixel_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());

  auto frame = CreateGreenFrame(options.frame_size, pixel_format,
                                base::TimeDelta::FromMilliseconds(1));
  adapter()->Encode(frame, true, ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 1);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, FlushDuringInitialize) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  auto pixel_format = PIXEL_FORMAT_I420;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(keyframe, true);
        EXPECT_EQ(frame->format(), pixel_format);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());

  auto frame = CreateGreenFrame(options.frame_size, pixel_format,
                                base::TimeDelta::FromMilliseconds(1));
  adapter()->Encode(frame, true, ValidatingStatusCB());
  adapter()->Flush(base::BindLambdaForTesting([&](Status s) {
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
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  VideoEncoder::StatusCB expect_error_done_cb =
      base::BindLambdaForTesting([&](Status s) { EXPECT_FALSE(s.is_ok()); });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_TRUE(false) << "should never come here";
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(VIDEO_CODEC_PROFILE_UNKNOWN, options,
                        std::move(output_cb), ValidatingStatusCB());

  auto frame = CreateGreenFrame(options.frame_size, pixel_format,
                                base::TimeDelta::FromMilliseconds(1));
  adapter()->Encode(frame, true, std::move(expect_error_done_cb));
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
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
        EXPECT_EQ(frame->format(),
                  IsYuvPlanar(pixel_format) ? pixel_format : PIXEL_FORMAT_I420);
#else
        // Everywhere except on Linux resize switches frame into CPU mode.
        EXPECT_EQ(frame->format(), PIXEL_FORMAT_I420);
#endif
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());

  auto small_frame = CreateGreenFrame(small_size, pixel_format,
                                      base::TimeDelta::FromMilliseconds(1));
  auto large_frame = CreateGreenFrame(large_size, pixel_format,
                                      base::TimeDelta::FromMilliseconds(2));
  adapter()->Encode(small_frame, true, ValidatingStatusCB());
  adapter()->Encode(large_frame, false, ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 2);
}

TEST_F(VideoEncodeAcceleratorAdapterTest, AutomaticResizeSupport) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  int outputs_count = 0;
  gfx::Size small_size(480, 320);
  auto pixel_format = PIXEL_FORMAT_NV12;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(frame->coded_size(), small_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  vea()->SupportResize();
  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());

  auto frame1 = CreateGreenFrame(small_size, pixel_format,
                                 base::TimeDelta::FromMilliseconds(1));
  auto frame2 = CreateGreenFrame(small_size, pixel_format,
                                 base::TimeDelta::FromMilliseconds(2));
  adapter()->Encode(frame1, true, ValidatingStatusCB());
  adapter()->Encode(frame2, false, ValidatingStatusCB());
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

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        outputs_count++;
      });

  vea()->SetEncodingCallback(base::BindLambdaForTesting(
      [&](BitstreamBuffer&, bool keyframe, scoped_refptr<VideoFrame> frame) {
        EXPECT_EQ(frame->format(),
                  IsYuvPlanar(pixel_format) ? pixel_format : PIXEL_FORMAT_I420);
        EXPECT_EQ(frame->coded_size(), options.frame_size);
        return BitstreamBufferMetadata(1, keyframe, frame->timestamp());
      }));
  adapter()->Initialize(profile_, options, std::move(output_cb),
                        ValidatingStatusCB());

  for (int frame_index = 0; frame_index < frames_to_encode; frame_index++) {
    gfx::Size size;
    if (frame_index % 4 == 0)
      size = large_size;
    else if (frame_index % 4 == 1)
      size = small_size;
    else
      size = same_size;

    // Every 4 frames switch between the 3 supported formats.
    const int rem = frame_index % 12;
    auto format = PIXEL_FORMAT_XRGB;
    if (rem < 4)
      format = PIXEL_FORMAT_I420;
    else if (rem < 8)
      format = PIXEL_FORMAT_NV12;
    bool key = frame_index % 9 == 0;
    auto frame = CreateGreenFrame(
        size, format, base::TimeDelta::FromMilliseconds(frame_index));
    adapter()->Encode(frame, key, ValidatingStatusCB());
  }

  RunUntilIdle();
  EXPECT_EQ(outputs_count, frames_to_encode);
}

INSTANTIATE_TEST_SUITE_P(VideoEncodeAcceleratorAdapterTest,
                         VideoEncodeAcceleratorAdapterTest,
                         ::testing::Values(PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_XRGB));

}  // namespace media
