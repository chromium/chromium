// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_decoder_shim.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_errors.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

namespace {

// Size of the timestamp cache. We don't want the cache to grow without bounds.
// The maximum size is chosen to be the same as in the VaapiVideoDecoder.
constexpr size_t kTimestampCacheSize = 128;

constexpr gfx::Size kDefaultSize(128, 128);

bool IsSoftwareCodecSupported(media::VideoCodec codec) {
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == media::VideoCodec::kVP9 || codec == media::VideoCodec::kVP8) {
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return media::FFmpegVideoDecoder::IsCodecSupported(codec);
#else
  return false;
#endif
}

}  // namespace

struct VideoDecoderShim::PendingDecode {
  PendingDecode(std::optional<uint32_t> decode_id,
                const scoped_refptr<media::DecoderBuffer>& buffer);
  ~PendingDecode();

  // |decode_id| is std::optional because it will be std::nullopt when the
  // decoder is being flushed.
  const std::optional<uint32_t> decode_id;
  const scoped_refptr<media::DecoderBuffer> buffer;
};

VideoDecoderShim::PendingDecode::PendingDecode(
    std::optional<uint32_t> decode_id,
    const scoped_refptr<media::DecoderBuffer>& buffer)
    : decode_id(decode_id), buffer(buffer) {}

VideoDecoderShim::PendingDecode::~PendingDecode() {
}

struct VideoDecoderShim::PendingFrame {
  explicit PendingFrame(std::optional<uint32_t> decode_id);
  PendingFrame(std::optional<uint32_t> decode_id,
               scoped_refptr<media::VideoFrame> frame);

  // This could be expensive to copy, so guard against that.
  PendingFrame(const PendingFrame&) = delete;
  PendingFrame& operator=(const PendingFrame&) = delete;

  ~PendingFrame();

  // |decode_id| is std::optional because it will be std::nullopt when the
  // decoder is being flushed.
  const std::optional<uint32_t> decode_id;
  scoped_refptr<media::VideoFrame> video_frame;
};

VideoDecoderShim::PendingFrame::PendingFrame(std::optional<uint32_t> decode_id)
    : decode_id(decode_id) {}

VideoDecoderShim::PendingFrame::PendingFrame(
    std::optional<uint32_t> decode_id,
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
  DecoderImpl(const base::WeakPtr<VideoDecoderShim>& proxy,
              bool use_hw_decoder);
  ~DecoderImpl();

  void InitializeSoftwareDecoder(media::VideoDecoderConfig config);
  void InitializeHardwareDecoder(
      media::VideoDecoderConfig config,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  void Decode(uint32_t decode_id, scoped_refptr<media::DecoderBuffer> buffer);
  void Flush();
  void Reset();
  void Stop();

 private:
  void OnInitDone(media::DecoderStatus status);
  void DoDecode();
  void OnDecodeComplete(std::optional<uint32_t> decode_id,
                        media::DecoderStatus status);
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
  // We can't assume that VideoDecoder always generates corresponding frames
  // before decode is finished. In that case, the frame can be output before
  // or after the decode completion callback is called. In order to allow the
  // Pepper plugin to associate Decode() calls with decoded frames we use
  // |decode_counter_| and |timestamp_to_id_cache_| to generate and store fake
  // timestamps. The corresponding timestamp will be put in the
  // media::DecoderBuffer that's sent to the VideoDecoder. When VideoDecoder
  // returns a VideoFrame we use its timestamp to look up the Decode() call id
  // in |timestamp_to_id_cache_|.
  base::LRUCache<base::TimeDelta, uint32_t> timestamp_to_id_cache_;
  base::TimeDelta decode_counter_ = base::Microseconds(0u);

  const bool use_hw_decoder_;

  base::WeakPtrFactory<DecoderImpl> weak_ptr_factory_{this};
};

VideoDecoderShim::DecoderImpl::DecoderImpl(
    const base::WeakPtr<VideoDecoderShim>& proxy,
    bool use_hw_decoder)
    : shim_(proxy),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      timestamp_to_id_cache_(kTimestampCacheSize),
      use_hw_decoder_(use_hw_decoder) {}

VideoDecoderShim::DecoderImpl::~DecoderImpl() {
  DCHECK(pending_decodes_.empty());
}

void VideoDecoderShim::DecoderImpl::InitializeSoftwareDecoder(
    media::VideoDecoderConfig config) {
  DCHECK(!use_hw_decoder_);
  DCHECK(!decoder_);
#if BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (config.codec() == media::VideoCodec::kVP9 ||
      config.codec() == media::VideoCodec::kVP8) {
    decoder_ = std::make_unique<media::VpxVideoDecoder>();
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
      config, /*low_delay=*/true, nullptr,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnInitDone,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoDecoderShim::DecoderImpl::OnOutputComplete,
                          weak_ptr_factory_.GetWeakPtr()),
      base::NullCallback());
#else
  OnInitDone(media::DecoderStatus::Codes::kUnsupportedCodec);
#endif  // BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
}

void VideoDecoderShim::DecoderImpl::InitializeHardwareDecoder(
    media::VideoDecoderConfig config,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(use_hw_decoder_);

  DCHECK(gpu_factories->GetTaskRunner()->RunsTasksInCurrentSequence());
  if (!gpu_factories->IsGpuVideoDecodeAcceleratorEnabled()) {
    OnInitDone(media::DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  decoder_ = gpu_factories->CreateVideoDecoder(
      &media_log_, /*request_overlay_info_cb=*/base::DoNothing());

  if (!decoder_) {
    OnInitDone(media::DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  decoder_->Initialize(
      config, /*low_delay=*/true, nullptr,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnInitDone,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoDecoderShim::DecoderImpl::OnOutputComplete,
                          weak_ptr_factory_.GetWeakPtr()),
      base::NullCallback());
}

void VideoDecoderShim::DecoderImpl::Decode(
    uint32_t decode_id,
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK(decoder_);
  pending_decodes_.push(PendingDecode(decode_id, buffer));
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::Flush() {
  DCHECK(decoder_);

  pending_decodes_.emplace(/*decode_id=*/std::nullopt,
                           media::DecoderBuffer::CreateEOSBuffer());

  DoDecode();
}

void VideoDecoderShim::DecoderImpl::Reset() {
  DCHECK(decoder_);
  // Abort all pending decodes.
  while (!pending_decodes_.empty()) {
    const PendingDecode& decode = pending_decodes_.front();

    // The PepperVideoDecoderHost validates that there's not a pending flush
    // when a reset request is received.
    DCHECK(decode.decode_id.has_value());
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
  // Clear pending decodes now. We don't want OnDecodeComplete to call DoDecode
  // again.
  while (!pending_decodes_.empty())
    pending_decodes_.pop();

  decoder_.reset();
  // This instance is deleted once we exit this scope.
}

void VideoDecoderShim::DecoderImpl::OnInitDone(media::DecoderStatus status) {
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

  if (!decode.buffer->end_of_stream()) {
    const base::TimeDelta new_counter =
        decode_counter_ + base::Microseconds(1u);
    if (new_counter == decode_counter_) {
      // We've reached the maximum base::TimeDelta.
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                         PP_ERROR_RESOURCE_FAILED, decode.decode_id));
      awaiting_decoder_ = false;
      pending_decodes_.pop();
      return;
    }
    decode_counter_ = new_counter;
    DCHECK(timestamp_to_id_cache_.Peek(decode_counter_) ==
           timestamp_to_id_cache_.end());
    DCHECK(decode.decode_id.has_value());
    timestamp_to_id_cache_.Put(decode_counter_, decode.decode_id.value());
    decode.buffer->set_timestamp(decode_counter_);
  }

  decoder_->Decode(
      decode.buffer,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnDecodeComplete,
                     weak_ptr_factory_.GetWeakPtr(), decode.decode_id));
  pending_decodes_.pop();
}

void VideoDecoderShim::DecoderImpl::OnDecodeComplete(
    std::optional<uint32_t> decode_id,
    media::DecoderStatus status) {
  DCHECK(awaiting_decoder_);
  awaiting_decoder_ = false;

  int32_t result;
  switch (status.code()) {
    case media::DecoderStatus::Codes::kOk:
    case media::DecoderStatus::Codes::kAborted:
      result = PP_OK;
      break;
    default:
      result = PP_ERROR_RESOURCE_FAILED;
      break;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                                result, decode_id));

  DoDecode();
}

void VideoDecoderShim::DecoderImpl::OnOutputComplete(
    scoped_refptr<media::VideoFrame> frame) {
  // Software decoders are expected to generate frames only when a Decode()
  // call is pending.
  DCHECK(use_hw_decoder_ || awaiting_decoder_);
  DCHECK(!frame->metadata().end_of_stream);

  uint32_t decode_id;

  base::TimeDelta timestamp = frame->timestamp();
  auto it = timestamp_to_id_cache_.Get(timestamp);
  if (it != timestamp_to_id_cache_.end()) {
    decode_id = it->second;
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  auto pending_frame =
      std::make_unique<PendingFrame>(decode_id, std::move(frame));

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnOutputComplete, shim_,
                                std::move(pending_frame)));
}

void VideoDecoderShim::DecoderImpl::OnResetComplete() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnResetComplete, shim_));
}

// static
std::unique_ptr<VideoDecoderShim> VideoDecoderShim::Create(
    PepperVideoDecoderHost* host,
    uint32_t texture_pool_size,
    bool use_hw_decoder) {
  scoped_refptr<viz::ContextProviderCommandBuffer>
      shared_main_thread_context_provider =
          RenderThreadImpl::current()->SharedMainThreadContextProvider();
  if (!shared_main_thread_context_provider) {
    return nullptr;
  }

  return base::WrapUnique(
      new VideoDecoderShim(host, texture_pool_size, use_hw_decoder,
                           std::move(shared_main_thread_context_provider)));
}

VideoDecoderShim::VideoDecoderShim(
    PepperVideoDecoderHost* host,
    uint32_t texture_pool_size,
    bool use_hw_decoder,
    scoped_refptr<viz::ContextProviderCommandBuffer>
        shared_main_thread_context_provider)
    : state_(UNINITIALIZED),
      host_(host),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaSequencedTaskRunner()),
      shared_main_thread_context_provider_(
          std::move(shared_main_thread_context_provider)),
      texture_pool_size_(texture_pool_size),
      num_pending_decodes_(0),
      use_hw_decoder_(use_hw_decoder) {
  DCHECK(host_);
  DCHECK(media_task_runner_.get());
  DCHECK(shared_main_thread_context_provider_.get());
  decoder_impl_ = std::make_unique<DecoderImpl>(weak_ptr_factory_.GetWeakPtr(),
                                                use_hw_decoder_);
}

VideoDecoderShim::~VideoDecoderShim() {
  DCHECK(RenderThreadImpl::current());

  CHECK(available_shared_images_.empty())
      << "VideoDecoderShim::Destroy() must be called before destructor";

  FlushCommandBuffer();

  weak_ptr_factory_.InvalidateWeakPtrs();
  // No more callbacks from the delegate will be received now.

  // The callback now holds the only reference to the DecoderImpl, which will be
  // deleted when Stop completes.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Stop,
                                base::Owned(decoder_impl_.release())));
}

bool VideoDecoderShim::Initialize(media::VideoCodecProfile profile) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, UNINITIALIZED);

  media::VideoCodec codec = media::VideoCodec::kUnknown;
  if (profile <= media::H264PROFILE_MAX) {
    codec = media::VideoCodec::kH264;
  } else if (profile <= media::VP8PROFILE_MAX) {
    codec = media::VideoCodec::kVP8;
  } else if (profile <= media::VP9PROFILE_MAX) {
    codec = media::VideoCodec::kVP9;
  }
  DCHECK_NE(codec, media::VideoCodec::kUnknown);

  // For hardware decoding, an unsupported codec is expected to manifest in an
  // initialization failure later on.
  if (!use_hw_decoder_ && !IsSoftwareCodecSupported(codec))
    return false;

  media::VideoDecoderConfig video_decoder_config(
      codec, profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
      gfx::Rect(kDefaultSize), kDefaultSize,
      // TODO(bbudge): Verify extra data isn't needed.
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);

  if (!use_hw_decoder_) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoDecoderShim::DecoderImpl::InitializeSoftwareDecoder,
            base::Unretained(decoder_impl_.get()), video_decoder_config));
  } else {
    media::GpuVideoAcceleratorFactories* gpu_factories =
        RenderThreadImpl::current()->GetGpuFactories();
    if (!gpu_factories)
      return false;

    video_renderer_ = std::make_unique<media::PaintCanvasVideoRenderer>();

    // It's safe to pass |gpu_factories| because the underlying instance is
    // managed by the RenderThreadImpl which doesn't destroy it until its
    // destructor which stops the media thread before destroying the
    // GpuVideoAcceleratorFactories.
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoDecoderShim::DecoderImpl::InitializeHardwareDecoder,
            base::Unretained(decoder_impl_.get()), video_decoder_config,
            gpu_factories));
  }

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
          media::DecoderBuffer::CopyFrom(
              // SAFETY: `buffer` is the address of `bitstream_buffer`'s shared
              // memory and is assumed to have the same size.
              UNSAFE_BUFFERS(base::span(buffer, bitstream_buffer.size())))));
  num_pending_decodes_++;
}

void VideoDecoderShim::ReuseSharedImage(const gpu::Mailbox& mailbox,
                                        gfx::Size size) {
  DCHECK(RenderThreadImpl::current());

  // Video resolution could have changed from the time the image was sent to
  // plugin and so this image is not reusable anymore. Delete it in this case.
  if (size != texture_size_) {
    host_->DestroySharedImage(mailbox);
    return;
  }

  available_shared_images_.push_back(mailbox);
  SendSharedImages();
}

void VideoDecoderShim::Flush() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);

  state_ = FLUSHING;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Flush,
                                base::Unretained(decoder_impl_.get())));
  num_pending_decodes_++;
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
  for (auto mailbox : available_shared_images_) {
    host_->DestroySharedImage(mailbox);
  }
  available_shared_images_.clear();

  FlushCommandBuffer();
}

void VideoDecoderShim::OnInitializeFailed() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
}

void VideoDecoderShim::OnDecodeComplete(int32_t result,
                                        std::optional<uint32_t> decode_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (result == PP_ERROR_RESOURCE_FAILED) {
    host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  num_pending_decodes_--;
  if (decode_id.has_value()) {
    completed_decodes_.push(decode_id.value());
  }

  // If frames are being queued because we're out of textures, don't notify
  // the host that decode has completed. This exerts "back pressure" to keep
  // the host from sending buffers that will cause pending_frames_ to grow.
  if (pending_frames_.empty())
    NotifyCompletedDecodes();

  if (!decode_id.has_value()) {
    // The flush request has been completed. This DCHECK is guaranteed by a
    // couple of facts:
    //
    // 1) The PepperVideoDecoderHost doesn't call VideoDecoderShim::Decode() or
    //    VideoDecoderShim::Flush() if there is a flush in progress, so
    //    |num_pending_decodes_| shouldn't increase after calling Flush() and
    //    before the flush is completed.
    //
    // 2) All pending decode callbacks should have been called.
    DCHECK(!num_pending_decodes_);
    pending_frames_.push(
        std::make_unique<PendingFrame>(/*decode_id=*/std::nullopt));
  }
}

void VideoDecoderShim::OnOutputComplete(std::unique_ptr<PendingFrame> frame) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);
  DCHECK(frame->video_frame);

  if (texture_size_ != frame->video_frame->coded_size()) {
    // If the size has changed, all current SharedImages must be destroyed.
    // Destroy images that aren't in use by the plugin. We will destroy the
    // rest as they are recycled.
    for (const auto& mailbox : available_shared_images_) {
      host_->DestroySharedImage(mailbox);
    }

    available_shared_images_.clear();

    for (uint32_t i = 0; i < texture_pool_size_; i++) {
      available_shared_images_.push_back(
          host_->CreateSharedImage(frame->video_frame->coded_size()));
    }

    texture_size_ = frame->video_frame->coded_size();
  }

  pending_frames_.push(std::move(frame));
  SendSharedImages();
}

void VideoDecoderShim::SendSharedImages() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  while (!pending_frames_.empty() && !available_shared_images_.empty()) {
    const std::unique_ptr<PendingFrame>& frame = pending_frames_.front();

    if (!frame->decode_id.has_value()) {
      // This signals the completion of a flush: all frames should have been
      // output by the underlying decoder (as required by the
      // media::VideoDecoder API) and the plugin should not have sent any other
      // decode requests while the flush was pending (this is validated by the
      // PepperVideoDecoderHost).
      pending_frames_.pop();
      DCHECK(pending_frames_.empty());
      DCHECK(!num_pending_decodes_);
      DCHECK_EQ(state_, FLUSHING);
      break;
    }

    auto it = available_shared_images_.begin();
    // Plugin's GLES2Interface and Renderer's RasterInterface are synchronized
    // by issued `ShallowFlushCHROMIUM` after each work. Synchronization with
    // SharedImageInterface happens during Creation/Destruction, so we don't
    // need SyncToken here and ignore one returned from
    // CopyVideoFrameToSharedImage.
    auto destination = gpu::MailboxHolder(*it, gpu::SyncToken(), GL_TEXTURE_2D);
    std::ignore = video_renderer_->CopyVideoFrameToSharedImage(
        shared_main_thread_context_provider_.get(), frame->video_frame,
        destination, /*use_visible_rect=*/false);
    available_shared_images_.erase(it);

    DCHECK(frame->decode_id.has_value());
    host_->SharedImageReady(
        base::checked_cast<int32_t>(frame->decode_id.value()),
        destination.mailbox, frame->video_frame->coded_size(),
        frame->video_frame->visible_rect());
    pending_frames_.pop();
  }

  // Flush our commands so they are executed before plugins command that use
  // this frame. This requires us to be on the same sequence.
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

  state_ = DECODING;
  host_->NotifyResetDone();
}

void VideoDecoderShim::NotifyCompletedDecodes() {
  while (!completed_decodes_.empty()) {
    host_->NotifyEndOfBitstreamBuffer(completed_decodes_.front());
    completed_decodes_.pop();
  }
}

void VideoDecoderShim::FlushCommandBuffer() {
  shared_main_thread_context_provider_->RasterInterface()->Flush();
}

}  // namespace content
