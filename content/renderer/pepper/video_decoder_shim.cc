// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_decoder_shim.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/media_buildflags.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_errors.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace content {

namespace {

bool IsCodecSupported(media::VideoCodec codec) {
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == media::kCodecVP9)
    return true;
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return media::FFmpegVideoDecoder::IsCodecSupported(codec);
#else
  return false;
#endif
}

}  // namespace

struct VideoDecoderShim::PendingDecode {
  PendingDecode(uint32_t decode_id,
                const scoped_refptr<media::DecoderBuffer>& buffer);
  ~PendingDecode();

  const uint32_t decode_id;
  const scoped_refptr<media::DecoderBuffer> buffer;
};

VideoDecoderShim::PendingDecode::PendingDecode(
    uint32_t decode_id,
    const scoped_refptr<media::DecoderBuffer>& buffer)
    : decode_id(decode_id), buffer(buffer) {
}

VideoDecoderShim::PendingDecode::~PendingDecode() {
}

struct VideoDecoderShim::PendingFrame {
  explicit PendingFrame(uint32_t decode_id);
  PendingFrame(uint32_t decode_id, scoped_refptr<media::VideoFrame> frame);
  ~PendingFrame();

  const uint32_t decode_id;
  scoped_refptr<media::VideoFrame> video_frame;

 private:
  // This could be expensive to copy, so guard against that.
  DISALLOW_COPY_AND_ASSIGN(PendingFrame);
};

VideoDecoderShim::PendingFrame::PendingFrame(uint32_t decode_id)
    : decode_id(decode_id) {
}

VideoDecoderShim::PendingFrame::PendingFrame(
    uint32_t decode_id,
    scoped_refptr<media::VideoFrame> frame)
    : decode_id(decode_id), video_frame(std::move(frame)) {}

VideoDecoderShim::PendingFrame::~PendingFrame() {
}

// DecoderImpl runs the underlying VideoDecoder on the media thread, receiving
// calls from the VideoDecodeShim on the main thread and sending results back.
// This class is constructed on the main thread, but used and destructed on the
// media thread.
class VideoDecoderShim::DecoderImpl {
 public:
  explicit DecoderImpl(const base::WeakPtr<VideoDecoderShim>& proxy);
  ~DecoderImpl();

  void Initialize(media::VideoDecoderConfig config);
  void Decode(uint32_t decode_id, scoped_refptr<media::DecoderBuffer> buffer);
  void Reset();
  void Stop();

 private:
  void OnInitDone(media::Status status);
  void DoDecode();
  void OnDecodeComplete(media::Status status);
  void OnOutputComplete(scoped_refptr<media::VideoFrame> frame);
  void OnResetComplete();

  // WeakPtr is bound to main_message_loop_. Use only in shim callbacks.
  base::WeakPtr<VideoDecoderShim> shim_;
  media::NullMediaLog media_log_;
  std::unique_ptr<media::VideoDecoder> decoder_;
  bool initialized_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  // Queue of decodes waiting for the decoder.
  using PendingDecodeQueue = base::queue<PendingDecode>;
  PendingDecodeQueue pending_decodes_;
  bool awaiting_decoder_ = false;
  // VideoDecoder returns pictures without information about the decode buffer
  // that generated it, but VideoDecoder implementations used in this class
  // (media::FFmpegVideoDecoder and media::VpxVideoDecoder) always generate
  // corresponding frames before decode is finished. |decode_id_| is used to
  // store id of the current buffer while Decode() call is pending.
  uint32_t decode_id_ = 0;

  base::WeakPtrFactory<DecoderImpl> weak_ptr_factory_{this};
};

VideoDecoderShim::DecoderImpl::DecoderImpl(
    const base::WeakPtr<VideoDecoderShim>& proxy)
    : shim_(proxy), main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

VideoDecoderShim::DecoderImpl::~DecoderImpl() {
  DCHECK(pending_decodes_.empty());
}

void VideoDecoderShim::DecoderImpl::Initialize(
    media::VideoDecoderConfig config) {
  DCHECK(!decoder_);
#if BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (config.codec() == media::kCodecVP9) {
    decoder_.reset(new media::VpxVideoDecoder());
  } else
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  {
    std::unique_ptr<media::FFmpegVideoDecoder> ffmpeg_video_decoder(
        new media::FFmpegVideoDecoder(&media_log_));
    ffmpeg_video_decoder->set_decode_nalus(true);
    decoder_ = std::move(ffmpeg_video_decoder);
  }
#endif  //  BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // VpxVideoDecoder and FFmpegVideoDecoder support only one pending Decode()
  // request.
  DCHECK_EQ(decoder_->GetMaxDecodeRequests(), 1);

  decoder_->Initialize(
      config, true /* low_delay */, nullptr,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnInitDone,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoDecoderShim::DecoderImpl::OnOutputComplete,
                          weak_ptr_factory_.GetWeakPtr()),
      base::NullCallback());
#else
  OnInitDone(media::StatusCode::kDecoderFailedInitialization);
#endif  // BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
}

void VideoDecoderShim::DecoderImpl::Decode(
    uint32_t decode_id,
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK(decoder_);
  pending_decodes_.push(PendingDecode(decode_id, buffer));
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::Reset() {
  DCHECK(decoder_);
  // Abort all pending decodes.
  while (!pending_decodes_.empty()) {
    const PendingDecode& decode = pending_decodes_.front();
    std::unique_ptr<PendingFrame> pending_frame(
        new PendingFrame(decode.decode_id));
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                                  PP_OK, decode.decode_id));
    pending_decodes_.pop();
  }
  // Don't need to call Reset() if the |decoder_| hasn't been initialized.
  if (!initialized_) {
    OnResetComplete();
    return;
  }

  decoder_->Reset(
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnResetComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecoderShim::DecoderImpl::Stop() {
  DCHECK(decoder_);
  // Clear pending decodes now. We don't want OnDecodeComplete to call DoDecode
  // again.
  while (!pending_decodes_.empty())
    pending_decodes_.pop();
  decoder_.reset();
  // This instance is deleted once we exit this scope.
}

void VideoDecoderShim::DecoderImpl::OnInitDone(media::Status status) {
  if (!status.is_ok()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecoderShim::OnInitializeFailed, shim_));
    return;
  }

  initialized_ = true;
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::DoDecode() {
  if (!initialized_ || pending_decodes_.empty() || awaiting_decoder_)
    return;

  awaiting_decoder_ = true;
  const PendingDecode& decode = pending_decodes_.front();
  decode_id_ = decode.decode_id;
  decoder_->Decode(
      decode.buffer,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnDecodeComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  pending_decodes_.pop();
}

void VideoDecoderShim::DecoderImpl::OnDecodeComplete(media::Status status) {
  DCHECK(awaiting_decoder_);
  awaiting_decoder_ = false;

  int32_t result;
  switch (status.code()) {
    case media::StatusCode::kOk:
    case media::StatusCode::kAborted:
      result = PP_OK;
      break;
    default:
      result = PP_ERROR_RESOURCE_FAILED;
      break;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                                result, decode_id_));

  DoDecode();
}

void VideoDecoderShim::DecoderImpl::OnOutputComplete(
    scoped_refptr<media::VideoFrame> frame) {
  // Software decoders are expected to generated frames only when a Decode()
  // call is pending.
  DCHECK(awaiting_decoder_);

  std::unique_ptr<PendingFrame> pending_frame;
  if (!frame->metadata()->end_of_stream)
    pending_frame.reset(new PendingFrame(decode_id_, std::move(frame)));
  else
    pending_frame.reset(new PendingFrame(decode_id_));

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnOutputComplete, shim_,
                                std::move(pending_frame)));
}

void VideoDecoderShim::DecoderImpl::OnResetComplete() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnResetComplete, shim_));
}

VideoDecoderShim::VideoDecoderShim(PepperVideoDecoderHost* host,
                                   uint32_t texture_pool_size)
    : state_(UNINITIALIZED),
      host_(host),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()),
      context_provider_(
          RenderThreadImpl::current()->SharedMainThreadContextProvider()),
      texture_pool_size_(texture_pool_size),
      num_pending_decodes_(0) {
  DCHECK(host_);
  DCHECK(media_task_runner_.get());
  DCHECK(context_provider_.get());
  decoder_impl_.reset(new DecoderImpl(weak_ptr_factory_.GetWeakPtr()));
}

VideoDecoderShim::~VideoDecoderShim() {
  DCHECK(RenderThreadImpl::current());
  texture_mailbox_map_.clear();

  FlushCommandBuffer();

  weak_ptr_factory_.InvalidateWeakPtrs();
  // No more callbacks from the delegate will be received now.

  // The callback now holds the only reference to the DecoderImpl, which will be
  // deleted when Stop completes.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Stop,
                                base::Owned(decoder_impl_.release())));
}

bool VideoDecoderShim::Initialize(const Config& vda_config, Client* client) {
  DCHECK_EQ(client, host_);
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, UNINITIALIZED);

  if (vda_config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported";
    return false;
  }

  media::VideoCodec codec = media::kUnknownVideoCodec;
  if (vda_config.profile <= media::H264PROFILE_MAX)
    codec = media::kCodecH264;
  else if (vda_config.profile <= media::VP8PROFILE_MAX)
    codec = media::kCodecVP8;
  else if (vda_config.profile <= media::VP9PROFILE_MAX)
    codec = media::kCodecVP9;
  DCHECK_NE(codec, media::kUnknownVideoCodec);

  if (!IsCodecSupported(codec))
    return false;

  media::VideoDecoderConfig video_decoder_config(
      codec, vda_config.profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation,
      gfx::Size(32, 24),  // Small sizes that won't fail.
      gfx::Rect(32, 24), gfx::Size(32, 24),
      // TODO(bbudge): Verify extra data isn't needed.
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Initialize,
                                base::Unretained(decoder_impl_.get()),
                                video_decoder_config));

  state_ = DECODING;

  // Return success, even though we are asynchronous, to mimic
  // media::VideoDecodeAccelerator.
  return true;
}

void VideoDecoderShim::Decode(media::BitstreamBuffer bitstream_buffer) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);

  // We need the address of the shared memory, so we can copy the buffer.
  const uint8_t* buffer = host_->DecodeIdToAddress(bitstream_buffer.id());
  DCHECK(buffer);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoDecoderShim::DecoderImpl::Decode,
          base::Unretained(decoder_impl_.get()), bitstream_buffer.id(),
          media::DecoderBuffer::CopyFrom(buffer, bitstream_buffer.size())));
  num_pending_decodes_++;
}

void VideoDecoderShim::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_NE(state_, UNINITIALIZED);
  if (buffers.empty()) {
    NOTREACHED();
    return;
  }
  std::vector<gpu::Mailbox> mailboxes = host_->TakeMailboxes();
  DCHECK_EQ(buffers.size(), mailboxes.size());
  GLuint num_textures = base::checked_cast<GLuint>(buffers.size());
  for (uint32_t i = 0; i < num_textures; i++) {
    DCHECK_EQ(1u, buffers[i].client_texture_ids().size());
    // Map the plugin texture id to the mailbox
    uint32_t plugin_texture_id = buffers[i].client_texture_ids()[0];
    texture_mailbox_map_[plugin_texture_id] = mailboxes[i];
    available_textures_.insert(plugin_texture_id);
  }
  SendPictures();
}

void VideoDecoderShim::ReusePictureBuffer(int32_t picture_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  uint32_t texture_id = static_cast<uint32_t>(picture_buffer_id);
  if (textures_to_dismiss_.find(texture_id) != textures_to_dismiss_.end()) {
    DismissTexture(texture_id);
  } else if (texture_mailbox_map_.find(texture_id) !=
             texture_mailbox_map_.end()) {
    available_textures_.insert(texture_id);
    SendPictures();
  } else {
    NOTREACHED();
  }
}

void VideoDecoderShim::Flush() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = FLUSHING;
}

void VideoDecoderShim::Reset() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = RESETTING;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Reset,
                                base::Unretained(decoder_impl_.get())));
}

void VideoDecoderShim::Destroy() {
  delete this;
}

void VideoDecoderShim::OnInitializeFailed() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
}

void VideoDecoderShim::OnDecodeComplete(int32_t result, uint32_t decode_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (result == PP_ERROR_RESOURCE_FAILED) {
    host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  num_pending_decodes_--;
  completed_decodes_.push(decode_id);

  // If frames are being queued because we're out of textures, don't notify
  // the host that decode has completed. This exerts "back pressure" to keep
  // the host from sending buffers that will cause pending_frames_ to grow.
  if (pending_frames_.empty())
    NotifyCompletedDecodes();
}

void VideoDecoderShim::OnOutputComplete(std::unique_ptr<PendingFrame> frame) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (frame->video_frame) {
    if (texture_size_ != frame->video_frame->coded_size()) {
      // If the size has changed, all current textures must be dismissed. Add
      // all textures to |textures_to_dismiss_| and dismiss any that aren't in
      // use by the plugin. We will dismiss the rest as they are recycled.
      for (IdToMailboxMap::const_iterator it = texture_mailbox_map_.begin();
           it != texture_mailbox_map_.end(); ++it) {
        textures_to_dismiss_.insert(it->first);
      }
      for (auto it = available_textures_.begin();
           it != available_textures_.end(); ++it) {
        DismissTexture(*it);
      }
      available_textures_.clear();
      FlushCommandBuffer();

      host_->ProvidePictureBuffers(texture_pool_size_, media::PIXEL_FORMAT_ARGB,
                                   1, frame->video_frame->coded_size(),
                                   GL_TEXTURE_2D);
      texture_size_ = frame->video_frame->coded_size();
    }

    pending_frames_.push(std::move(frame));
    SendPictures();
  }
}

void VideoDecoderShim::SendPictures() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);
  while (!pending_frames_.empty() && !available_textures_.empty()) {
    const std::unique_ptr<PendingFrame>& frame = pending_frames_.front();

    auto it = available_textures_.begin();
    uint32_t texture_id = *it;
    available_textures_.erase(it);

    gpu::MailboxHolder destination_holder;
    destination_holder.mailbox = texture_mailbox_map_[texture_id];
    destination_holder.texture_target = GL_TEXTURE_2D;
    media::VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
        frame->video_frame.get(), context_provider_.get(), destination_holder);
    host_->PictureReady(media::Picture(texture_id, frame->decode_id,
                                       frame->video_frame->visible_rect(),
                                       gfx::ColorSpace(), false));
    pending_frames_.pop();
  }

  FlushCommandBuffer();

  if (pending_frames_.empty()) {
    // If frames aren't backing up, notify the host of any completed decodes so
    // it can send more buffers.
    NotifyCompletedDecodes();

    if (state_ == FLUSHING && !num_pending_decodes_) {
      state_ = DECODING;
      host_->NotifyFlushDone();
    }
  }
}

void VideoDecoderShim::OnResetComplete() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  while (!pending_frames_.empty())
    pending_frames_.pop();
  NotifyCompletedDecodes();

  // Dismiss any old textures now.
  while (!textures_to_dismiss_.empty())
    DismissTexture(*textures_to_dismiss_.begin());

  state_ = DECODING;
  host_->NotifyResetDone();
}

void VideoDecoderShim::NotifyCompletedDecodes() {
  while (!completed_decodes_.empty()) {
    host_->NotifyEndOfBitstreamBuffer(completed_decodes_.front());
    completed_decodes_.pop();
  }
}

void VideoDecoderShim::DismissTexture(uint32_t texture_id) {
  DCHECK(host_);
  textures_to_dismiss_.erase(texture_id);
  DCHECK(texture_mailbox_map_.find(texture_id) != texture_mailbox_map_.end());
  texture_mailbox_map_.erase(texture_id);
  host_->DismissPictureBuffer(texture_id);
}

void VideoDecoderShim::FlushCommandBuffer() {
  context_provider_->RasterInterface()->Flush();
}

}  // namespace content
