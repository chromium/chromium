// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/test_vda_video_decoder.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/waiting.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/vd_video_decode_accelerator.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {
// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;
}  // namespace

TestVDAVideoDecoder::TestVDAVideoDecoder(
    bool use_vd_vda,
    OnProvidePictureBuffersCB on_provide_picture_buffers_cb,
    const gfx::ColorSpace& target_color_space,
    FrameRendererDummy* const frame_renderer,
    bool linear_output)
    : use_vd_vda_(use_vd_vda),
      on_provide_picture_buffers_cb_(std::move(on_provide_picture_buffers_cb)),
      target_color_space_(target_color_space),
      frame_renderer_(frame_renderer),
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      linear_output_(linear_output),
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      decode_start_timestamps_(kTimestampCacheSize) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  vda_wrapper_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

TestVDAVideoDecoder::~TestVDAVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  decoder_ = nullptr;

  // Delete all video frames and related textures and the decoder.
  video_frames_.clear();
}

VideoDecoderType TestVDAVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kTesting;
}

bool TestVDAVideoDecoder::IsPlatformDecoder() const {
  return true;
}

void TestVDAVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                     bool low_delay,
                                     CdmContext* cdm_context,
                                     InitCB init_cb,
                                     const OutputCB& output_cb,
                                     const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  output_cb_ = output_cb;

  // Create Decoder.
  VideoDecodeAccelerator::Config vda_config(config.profile());
  vda_config.output_mode = VideoDecodeAccelerator::Config::OutputMode::kImport;
  vda_config.encryption_scheme = config.encryption_scheme();
  vda_config.is_deferred_initialization_allowed = false;
  vda_config.initial_expected_coded_size = config.coded_size();
  vda_config.container_color_space = config.color_space_info();
  vda_config.target_color_space = target_color_space_;
  vda_config.hdr_metadata = config.hdr_metadata();

  if (use_vd_vda_) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    DVLOGF(2) << "Use VdVideoDecodeAccelerator";
    vda_config.is_deferred_initialization_allowed = true;
    decoder_ = media::VdVideoDecodeAccelerator::Create(
        base::BindRepeating(
            &media::VideoDecoderPipeline::CreateForVDAAdapterForARC),
        this, vda_config, base::SequencedTaskRunner::GetCurrentDefault());
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  } else {
    DVLOGF(2) << "Use original VDA";
    decoder_ = GpuVideoDecodeAcceleratorFactory::CreateVDA(
        this, vda_config, gpu::GpuPreferences());
  }

  if (!decoder_) {
    ASSERT_TRUE(decoder_) << "Failed to create VideoDecodeAccelerator factory";
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!vda_config.is_deferred_initialization_allowed)
    std::move(init_cb).Run(DecoderStatus::Codes::kOk);
  else
    init_cb_ = std::move(init_cb);
}

void TestVDAVideoDecoder::NotifyInitializationComplete(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DCHECK(init_cb_);

  std::move(init_cb_).Run(status);
}

void TestVDAVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                 DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  // If the |buffer| is an EOS buffer the decoder must be flushed.
  if (buffer->end_of_stream()) {
    flush_cb_ = std::move(decode_cb);
    decoder_->Flush();
    return;
  }

  int32_t bitstream_buffer_id = GetNextBitstreamBufferId();
  decode_cbs_[bitstream_buffer_id] = std::move(decode_cb);

  // Record picture buffer decode start time. A cache is used because not each
  // bitstream buffer decode will result in a call to PictureReady(). Pictures
  // can be delivered in a different order than the decode operations, so we
  // don't know when it's safe to purge old decode timestamps. Instead we use
  // a cache with a large enough size to account for frame reordering.
  decode_start_timestamps_.Put(bitstream_buffer_id, buffer->timestamp());

  decoder_->Decode(std::move(buffer), bitstream_buffer_id);
}

void TestVDAVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  reset_cb_ = std::move(reset_cb);
  decoder_->Reset();
}

bool TestVDAVideoDecoder::NeedsBitstreamConversion() const {
  return false;
}

bool TestVDAVideoDecoder::CanReadWithoutStalling() const {
  return true;
}

int TestVDAVideoDecoder::GetMaxDecodeRequests() const {
  return 4;
}

void TestVDAVideoDecoder::ProvidePictureBuffersWithVisibleRect(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DVLOGF(4) << "Requested " << requested_num_of_buffers
            << " picture buffers with size " << dimensions.width() << "x"
            << dimensions.height();

  const bool should_continue = on_provide_picture_buffers_cb_.Run();
  if (!should_continue) {
    DVLOGF(3) << "Abort the resolution change process.";
    return;
  }

  // Create a set of DMABuf-backed video frames.
  std::vector<PictureBuffer> picture_buffers;
  for (uint32_t i = 0; i < requested_num_of_buffers; ++i) {
    picture_buffers.emplace_back(GetNextPictureBufferId());
  }

  decoder_->AssignPictureBuffers(picture_buffers);

  // Create a video frame for each of the picture buffers and provide memory
  // handles to the video frame's data to the decoder.
  for (const PictureBuffer& picture_buffer : picture_buffers) {
    scoped_refptr<VideoFrame> video_frame;

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    video_frame = CreateGpuMemoryBufferVideoFrame(
        format, dimensions, visible_rect, visible_rect.size(),
        base::TimeDelta(),
        linear_output_ ? gfx::BufferUsage::SCANOUT_CPU_READ_WRITE
                       : gfx::BufferUsage::SCANOUT_VDA_WRITE);
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

    ASSERT_TRUE(video_frame) << "Failed to create video frame";
    video_frames_.emplace(picture_buffer.id(), video_frame);
    gfx::GpuMemoryBufferHandle handle;

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    handle = CreateGpuMemoryBufferHandle(video_frame.get());
    DCHECK(!handle.is_null());
#else
    NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

    ASSERT_TRUE(!handle.is_null()) << "Failed to create GPU memory handle";
    decoder_->ImportBufferForPicture(picture_buffer.id(), format,
                                     std::move(handle));
  }
}

void TestVDAVideoDecoder::DismissPictureBuffer(int32_t picture_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  // Drop reference to the video frame associated with the picture buffer, so
  // the video frame and related texture are automatically destroyed once the
  // renderer and video frame processors are done using them.
  ASSERT_EQ(video_frames_.erase(picture_buffer_id), 1u);
}

void TestVDAVideoDecoder::PictureReady(const Picture& picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DVLOGF(4) << "Picture buffer ID: " << picture.picture_buffer_id();

  auto it = video_frames_.find(picture.picture_buffer_id());
  ASSERT_TRUE(it != video_frames_.end());
  scoped_refptr<VideoFrame> video_frame = it->second;

  // Look up the time at which the decode started.
  auto timestamp_it =
      decode_start_timestamps_.Peek(picture.bitstream_buffer_id());
  ASSERT_NE(timestamp_it, decode_start_timestamps_.end());
  video_frame->set_timestamp(timestamp_it->second);

  scoped_refptr<VideoFrame> wrapped_video_frame;

  // Wrap the video frame in another frame that calls ReusePictureBufferTask()
  // upon destruction. When the renderer and video frame processors are done
  // using the video frame, the associated picture buffer will automatically be
  // flagged for reuse.
  if (!picture.visible_rect().IsEmpty()) {
    wrapped_video_frame = VideoFrame::WrapVideoFrame(
        video_frame, video_frame->format(), picture.visible_rect(),
        picture.visible_rect().size());
  } else {
    // This occurs in bitstream buffer in webrtc scenario. WrapNativeTexture()
    // fails if visible_rect() is empty. Although the client of
    // TestVdaVideoDecoder should ignore this frame, it is necessary to output
    // the dummy frame to count up the number of output video frames.
    wrapped_video_frame =
        VideoFrame::CreateFrame(PIXEL_FORMAT_UNKNOWN, gfx::Size(), gfx::Rect(),
                                gfx::Size(), video_frame->timestamp());
  }

  DCHECK(wrapped_video_frame);

  // Flag that the video frame was decoded in a power efficient way.
  wrapped_video_frame->metadata().power_efficient = true;

  // It's important to bind the original video frame to the destruction callback
  // of the wrapped frame, to avoid deleting it before rendering of the wrapped
  // frame is done. A reference to the video frame is already stored in
  // |video_frames_| to map between picture buffers and frames, but that
  // reference will be released when the decoder calls DismissPictureBuffer()
  // (e.g. on a resolution change).
  base::OnceClosure reuse_cb =
      base::BindOnce(&TestVDAVideoDecoder::ReusePictureBufferThunk, weak_this_,
                     vda_wrapper_task_runner_, picture.picture_buffer_id());
  wrapped_video_frame->AddDestructionObserver(std::move(reuse_cb));
  output_cb_.Run(std::move(wrapped_video_frame));
}

// static
void TestVDAVideoDecoder::ReusePictureBufferThunk(
    std::optional<base::WeakPtr<TestVDAVideoDecoder>> vda_video_decoder,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int32_t picture_buffer_id) {
  DCHECK(vda_video_decoder);
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&TestVDAVideoDecoder::ReusePictureBufferTask,
                                *vda_video_decoder, picture_buffer_id));
}

// Called when a picture buffer is ready to be re-used.
void TestVDAVideoDecoder::ReusePictureBufferTask(int32_t picture_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DCHECK(decoder_);
  DVLOGF(4) << "Picture buffer ID: " << picture_buffer_id;

  // Notify the decoder the picture buffer can be reused. The decoder will only
  // request a limited set of picture buffers, when it runs out it will wait
  // until picture buffers are flagged for reuse.
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

void TestVDAVideoDecoder::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  auto it = decode_cbs_.find(bitstream_buffer_id);
  ASSERT_TRUE(it != decode_cbs_.end())
      << "Couldn't find decode callback for picture buffer with id "
      << bitstream_buffer_id;

  std::move(it->second).Run(DecoderStatus::Codes::kOk);
  decode_cbs_.erase(it);
}

void TestVDAVideoDecoder::NotifyFlushDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DCHECK(flush_cb_);

  std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);
}

void TestVDAVideoDecoder::NotifyResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DCHECK(reset_cb_);

  std::move(reset_cb_).Run();
}

void TestVDAVideoDecoder::NotifyError(VideoDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  switch (error) {
    case VideoDecodeAccelerator::ILLEGAL_STATE:
      LOG(ERROR) << "ILLEGAL_STATE";
      break;
    case VideoDecodeAccelerator::INVALID_ARGUMENT:
      LOG(ERROR) << "INVALID_ARGUMENT";
      break;
    case VideoDecodeAccelerator::UNREADABLE_INPUT:
      LOG(ERROR) << "UNREADABLE_INPUT";
      break;
    case VideoDecodeAccelerator::PLATFORM_FAILURE:
      LOG(ERROR) << "PLATFORM_FAILURE";
      break;
    default:
      LOG(ERROR) << "Unknown error " << error;
      break;
  }
}

int32_t TestVDAVideoDecoder::GetNextBitstreamBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  // The bitstream buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_bitstream_buffer_id_;
}

int32_t TestVDAVideoDecoder::GetNextPictureBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  // The picture buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_picture_buffer_id_ = (next_picture_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_picture_buffer_id_;
}

}  // namespace test
}  // namespace media
