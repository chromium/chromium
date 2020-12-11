// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/test_vda_video_decoder.h"

#include <utility>
#include <vector>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/waiting.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_player/frame_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/vd_video_decode_accelerator.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {
// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;
}  // namespace

TestVDAVideoDecoder::TestVDAVideoDecoder(
    AllocationMode allocation_mode,
    bool use_vd_vda,
    const gfx::ColorSpace& target_color_space,
    FrameRenderer* const frame_renderer,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : output_mode_(allocation_mode == AllocationMode::kAllocate
                       ? VideoDecodeAccelerator::Config::OutputMode::ALLOCATE
                       : VideoDecodeAccelerator::Config::OutputMode::IMPORT),
      use_vd_vda_(use_vd_vda),
      target_color_space_(target_color_space),
      frame_renderer_(frame_renderer),
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      decode_start_timestamps_(kTimestampCacheSize) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);

  // TODO(crbug.com/933632) Remove support for allocate mode, and always use
  // import mode. Support for allocate mode is temporary maintained for older
  // platforms that don't support import mode.

  vda_wrapper_task_runner_ = base::ThreadTaskRunnerHandle::Get();

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

std::string TestVDAVideoDecoder::GetDisplayName() const {
  return "TestVDAVideoDecoder";
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

  // Create decoder factory.
  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> decoder_factory;
  bool hasGLContext = frame_renderer_->GetGLContext() != nullptr;
  GpuVideoDecodeGLClient gl_client;
  if (hasGLContext) {
    gl_client.get_context = base::BindRepeating(
        &FrameRenderer::GetGLContext, base::Unretained(frame_renderer_));
    gl_client.make_context_current = base::BindRepeating(
        &FrameRenderer::AcquireGLContext, base::Unretained(frame_renderer_));
    gl_client.bind_image = base::BindRepeating(
        [](uint32_t, uint32_t, const scoped_refptr<gl::GLImage>&, bool) {
          return true;
        });
  }
  decoder_factory = GpuVideoDecodeAcceleratorFactory::Create(gl_client);

  if (!decoder_factory) {
    ASSERT_TRUE(decoder_) << "Failed to create VideoDecodeAccelerator factory";
    std::move(init_cb).Run(StatusCode::kCodeOnlyForTesting);
    return;
  }

  // Create Decoder.
  VideoDecodeAccelerator::Config vda_config(config.profile());
  vda_config.output_mode = output_mode_;
  vda_config.encryption_scheme = config.encryption_scheme();
  vda_config.is_deferred_initialization_allowed = false;
  vda_config.initial_expected_coded_size = config.coded_size();
  vda_config.container_color_space = config.color_space_info();
  vda_config.target_color_space = target_color_space_;
  vda_config.hdr_metadata = config.hdr_metadata();

  gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds;
  gpu::GpuPreferences gpu_preferences;
  if (use_vd_vda_) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    DVLOGF(2) << "Use VdVideoDecodeAccelerator";
    vda_config.is_deferred_initialization_allowed = true;
    decoder_ = media::VdVideoDecodeAccelerator::Create(
        base::BindRepeating(&media::ChromeosVideoDecoderFactory::Create), this,
        vda_config, base::SequencedTaskRunnerHandle::Get());
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  } else {
    DVLOGF(2) << "Use original VDA";
    decoder_ = decoder_factory->CreateVDA(
        this, vda_config, gpu_driver_bug_workarounds, gpu_preferences);
  }

  if (!decoder_) {
    ASSERT_TRUE(decoder_) << "Failed to create VideoDecodeAccelerator factory";
    std::move(init_cb).Run(StatusCode::kCodeOnlyForTesting);
    return;
  }

  if (!vda_config.is_deferred_initialization_allowed)
    std::move(init_cb).Run(OkStatus());
  else
    init_cb_ = std::move(init_cb);
}

void TestVDAVideoDecoder::NotifyInitializationComplete(Status status) {
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

void TestVDAVideoDecoder::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  NOTIMPLEMENTED() << "VDA must call ProvidePictureBuffersWithVisibleRect()";
}

void TestVDAVideoDecoder::ProvidePictureBuffersWithVisibleRect(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect,
    uint32_t texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  ASSERT_EQ(textures_per_buffer, 1u);
  DVLOGF(4) << "Requested " << requested_num_of_buffers
            << " picture buffers with size " << dimensions.width() << "x"
            << dimensions.height();

  // If using allocate mode the format requested here might be
  // PIXEL_FORMAT_UNKNOWN.
  if (format == PIXEL_FORMAT_UNKNOWN)
    format = PIXEL_FORMAT_ARGB;

  std::vector<PictureBuffer> picture_buffers;

  switch (output_mode_) {
    case VideoDecodeAccelerator::Config::OutputMode::IMPORT:
      // If using import mode, create a set of DMABuf-backed video frames.
      for (uint32_t i = 0; i < requested_num_of_buffers; ++i) {
        picture_buffers.emplace_back(GetNextPictureBufferId(), dimensions);
      }
      decoder_->AssignPictureBuffers(picture_buffers);

      // Create a video frame for each of the picture buffers and provide memory
      // handles to the video frame's data to the decoder.
      for (const PictureBuffer& picture_buffer : picture_buffers) {
        scoped_refptr<VideoFrame> video_frame;

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
        video_frame = CreatePlatformVideoFrame(
            gpu_memory_buffer_factory_, format, dimensions, visible_rect,
            visible_rect.size(), base::TimeDelta(),
            gfx::BufferUsage::SCANOUT_VDA_WRITE);
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

        ASSERT_TRUE(video_frame) << "Failed to create video frame";
        video_frames_.emplace(picture_buffer.id(), video_frame);
        gfx::GpuMemoryBufferHandle handle;

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
        handle = CreateGpuMemoryBufferHandle(video_frame.get());
        DCHECK(!handle.is_null());
#else
        NOTREACHED();
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

        ASSERT_TRUE(!handle.is_null()) << "Failed to create GPU memory handle";
        decoder_->ImportBufferForPicture(picture_buffer.id(), format,
                                         std::move(handle));
      }
      break;
    case VideoDecodeAccelerator::Config::OutputMode::ALLOCATE: {
      // If using allocate mode, request a set of texture-backed video frames
      // from the renderer.
      const gfx::Size texture_dimensions =
          texture_target == GL_TEXTURE_EXTERNAL_OES
              ? GetRectSizeFromOrigin(visible_rect)
              : dimensions;
      for (uint32_t i = 0; i < requested_num_of_buffers; ++i) {
        uint32_t texture_id;
        auto video_frame = frame_renderer_->CreateVideoFrame(
            format, texture_dimensions, texture_target, &texture_id);
        ASSERT_TRUE(video_frame) << "Failed to create video frame";
        int32_t picture_buffer_id = GetNextPictureBufferId();
        PictureBuffer::TextureIds texture_ids(1, texture_id);
        picture_buffers.emplace_back(picture_buffer_id, texture_dimensions,
                                     texture_ids, texture_ids, texture_target,
                                     format);
        video_frames_.emplace(picture_buffer_id, std::move(video_frame));
      }
      // The decoder requires an active GL context to allocate memory.
      decoder_->AssignPictureBuffers(picture_buffers);
      break;
    }
    default:
      LOG(ERROR) << "Unsupported output mode "
                 << static_cast<size_t>(output_mode_);
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

  scoped_refptr<VideoFrame> wrapped_video_frame = nullptr;

  // Wrap the video frame in another frame that calls ReusePictureBufferTask()
  // upon destruction. When the renderer and video frame processors are done
  // using the video frame, the associated picture buffer will automatically be
  // flagged for reuse. WrapVideoFrame() is not supported for texture-based
  // video frames (see http://crbug/362521) so we work around this by creating a
  // new video frame using the same mailbox.
  if (!picture.visible_rect().IsEmpty()) {
    if (!video_frame->HasTextures()) {
      wrapped_video_frame = VideoFrame::WrapVideoFrame(
          video_frame, video_frame->format(), picture.visible_rect(),
          picture.visible_rect().size());
    } else {
      gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
      mailbox_holders[0] = video_frame->mailbox_holder(0);
      wrapped_video_frame = VideoFrame::WrapNativeTextures(
          video_frame->format(), mailbox_holders,
          VideoFrame::ReleaseMailboxCB(), video_frame->coded_size(),
          picture.visible_rect(), picture.visible_rect().size(),
          video_frame->timestamp());
    }
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
  wrapped_video_frame->metadata()->power_efficient = true;

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
    base::Optional<base::WeakPtr<TestVDAVideoDecoder>> vda_video_decoder,
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

  std::move(it->second).Run(DecodeStatus::OK);
  decode_cbs_.erase(it);
}

void TestVDAVideoDecoder::NotifyFlushDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vda_wrapper_sequence_checker_);
  DCHECK(flush_cb_);

  std::move(flush_cb_).Run(DecodeStatus::OK);
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
