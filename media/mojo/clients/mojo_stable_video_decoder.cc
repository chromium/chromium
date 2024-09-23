// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_stable_video_decoder.h"

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/oop_video_decoder.h"
#include "media/gpu/macros.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

const char kMojoStableVideoDecoderDecodeLatencyHistogram[] =
    "Media.MojoStableVideoDecoder.Decode";

namespace {

std::optional<viz::SharedImageFormat> GetSharedImageFormat(
    VideoPixelFormat format) {
  viz::SharedImageFormat si_format;
  switch (format) {
    case PIXEL_FORMAT_ARGB:
      si_format = viz::SinglePlaneFormat::kBGRA_8888;
      break;
    case PIXEL_FORMAT_NV12:
      si_format = viz::MultiPlaneFormat::kNV12;
      break;
    case PIXEL_FORMAT_P010LE:
      si_format = viz::MultiPlaneFormat::kP010;
      break;
    case PIXEL_FORMAT_YV12:
      si_format = viz::MultiPlaneFormat::kYV12;
      break;
    default:
      return std::nullopt;
  }
  if (si_format.is_multi_plane()) {
    si_format.SetPrefersExternalSampler();
  }
  return si_format;
}

}  // namespace

// A SharedImageHolder allows us to manage the lifetime of a SharedImage with
// reference counting.
//
// The reason we don't use the gpu::ClientSharedImage directly is that we want
// to make sure the gpu::SharedImageInterface that was used to create the
// SharedImage outlives the SharedImage and can be used to destroy it.
//
// Thread safety: the underlying gpu::ClientSharedImage is not documented to be
// thread-safe. Therefore, concurrent access to SharedImageHolder instances must
// be synchronized externally if needed.
class MojoStableVideoDecoder::SharedImageHolder
    : public base::RefCountedThreadSafe<SharedImageHolder> {
 public:
  static scoped_refptr<SharedImageHolder> CreateFromFrameResource(
      const scoped_refptr<FrameResource>& frame_resource,
      scoped_refptr<gpu::SharedImageInterface> sii) {
    if (!sii) {
      DVLOGF(1) << "No gpu::SharedImageInterface available";
      return nullptr;
    }

    gpu::SharedImageUsageSet shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    if (frame_resource->metadata().is_webgpu_compatible &&
        !sii->GetCapabilities().disable_webgpu_shared_images) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
    }

    gfx::GpuMemoryBufferHandle gmb_handle =
        frame_resource->CreateGpuMemoryBufferHandle();
    if (gmb_handle.type != gfx::NATIVE_PIXMAP ||
        gmb_handle.native_pixmap_handle.planes.empty()) {
      DVLOGF(1)
          << "Could not obtain a GpuMemoryBufferHandle for the FrameResource";
      return nullptr;
    }

    std::optional<viz::SharedImageFormat> shared_image_format =
        GetSharedImageFormat(frame_resource->format());
    if (!shared_image_format.has_value()) {
      DVLOGF(1) << "Unsupported VideoPixelFormat " << frame_resource->format();
      return nullptr;
    }

    // The SharedImage size ultimately must correspond to the size used to
    // import the decoded frame into a graphics API (e.g., the EGL image size
    // when using OpenGL). For most videos, this is simply
    // |frame_resource|->visible_rect().size(). However, some H.264 videos
    // specify a visible rectangle that doesn't start at (0, 0). Since users of
    // the decoded frames are expected to calculate UV coordinates to handle
    // these exotic visible rectangles, we must include the area on the left and
    // on the top of the frames when computing the SharedImage size. Hence the
    // use of GetRectSizeFromOrigin().
    const gpu::SharedImageInfo shared_image_info(
        *shared_image_format,
        GetRectSizeFromOrigin(frame_resource->visible_rect()),
        frame_resource->ColorSpace(), kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, shared_image_usage,
        /*debug_label=*/"MojoStableVideoDecoder");
    scoped_refptr<gpu::ClientSharedImage> new_client_shared_image =
        sii->CreateSharedImage(shared_image_info, std::move(gmb_handle));
    if (!new_client_shared_image) {
      DVLOGF(1) << "Could not create a SharedImage for the FrameResource";
      return nullptr;
    }

    return base::WrapRefCounted(new SharedImageHolder(
        frame_resource->GetSharedMemoryId(), std::move(new_client_shared_image),
        frame_resource->ColorSpace(), std::move(sii)));
  }

  SharedImageHolder(const SharedImageHolder&) = delete;
  SharedImageHolder& operator=(const SharedImageHolder&) = delete;

  gfx::GenericSharedMemoryId id() const { return id_; }

  const scoped_refptr<gpu::ClientSharedImage> client_shared_image() const {
    return client_shared_image_;
  }

  uint32_t texture_target() const {
    return client_shared_image_->GetTextureTarget();
  }

  bool IsCompatibleWith(
      const scoped_refptr<FrameResource>& frame_resource) const {
    return client_shared_image_->size() ==
               GetRectSizeFromOrigin(frame_resource->visible_rect()) &&
           color_space_ == frame_resource->ColorSpace();
  }

  gpu::SyncToken GenUnverifiedSyncToken() {
    return sii_->GenUnverifiedSyncToken();
  }

  void Update() {
    sii_->UpdateSharedImage(gpu::SyncToken(), client_shared_image()->mailbox());
  }

 private:
  friend class base::RefCountedThreadSafe<SharedImageHolder>;

  SharedImageHolder(gfx::GenericSharedMemoryId id,
                    scoped_refptr<gpu::ClientSharedImage> client_shared_image,
                    const gfx::ColorSpace& color_space,
                    scoped_refptr<gpu::SharedImageInterface> sii)
      : id_(id),
        color_space_(color_space),
        sii_(std::move(sii)),
        client_shared_image_(std::move(client_shared_image)) {}

  ~SharedImageHolder() { CHECK(client_shared_image_->HasOneRef()); }

  const gfx::GenericSharedMemoryId id_;
  const gfx::ColorSpace color_space_;
  const scoped_refptr<gpu::SharedImageInterface> sii_;

  // |client_shared_image_| is declared after |sii_| to ensure the
  // gpu::ClientSharedImage can use the gpu::SharedImageInterface for the
  // destruction of the SharedImage.
  //
  // TODO(b/327268445): make gpu::ClientSharedImage::mailbox() and
  // GetTextureTarget() const so that we can make |client_shared_image_| a
  // const scoped_refptr<const gpu::ClientSharedImage>.
  const scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
};

MojoStableVideoDecoder::MojoStableVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder)
    : timestamps_(128),
      media_task_runner_(std::move(media_task_runner)),
      gpu_factories_(gpu_factories),
      media_log_(media_log),
      pending_remote_decoder_(std::move(pending_remote_decoder)),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MojoStableVideoDecoder::~MojoStableVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MojoStableVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoStableVideoDecoder::SupportsDecryption() const {
  // TODO(b/327268445): implement decoding of protected content for GTFO OOP-VD.
  return false;
}

void MojoStableVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): consider not constructing a MojoStableVideoDecoder to
  // begin with if there isn't a non-null GpuVideoAcceleratorFactories*
  // available (and then this if can be turned into a CHECK()).
  if (!gpu_factories_) {
    std::move(init_cb).Run(DecoderStatus::Codes::kInvalidArgument);
    return;
  }
  OOPVideoDecoder::NotifySupportKnown(
      std::move(pending_remote_decoder_),
      base::BindOnce(
          &MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown,
          weak_this_factory_.GetWeakPtr(), config, low_delay, cdm_context,
          std::move(init_cb), output_cb, waiting_cb));
}

void MojoStableVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  if (!buffer->end_of_stream()) {
    timestamps_.Put(buffer->timestamp().InMilliseconds(),
                    base::TimeTicks::Now());
  }
  oop_video_decoder_->Decode(std::move(buffer), std::move(decode_cb));
}

void MojoStableVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  oop_video_decoder_->Reset(std::move(closure));
}

bool MojoStableVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  return oop_video_decoder_->NeedsBitstreamConversion();
}

bool MojoStableVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  return oop_video_decoder_->CanReadWithoutStalling();
}

int MojoStableVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  return oop_video_decoder_->GetMaxDecodeRequests();
}

VideoDecoderType MojoStableVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing GetDecoderType().
  NOTIMPLEMENTED();
  return VideoDecoderType::kOutOfProcess;
}

void MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The OOPVideoDecoder initialization path assumes that a higher layer checks
  // that the VideoDecoderConfig is supported. That higher layer is the
  // MojoStableVideoDecoder in this case.
  std::optional<SupportedVideoDecoderConfigs> supported_configs =
      OOPVideoDecoder::GetSupportedConfigs();
  // InitializeOnceSupportedConfigsAreKnown() gets called only once the
  // supported configurations are known.
  CHECK(supported_configs.has_value());
  if (!IsVideoDecoderConfigSupported(supported_configs.value(), config)) {
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  if (!oop_video_decoder_) {
    // This should correspond to the first MojoStableVideoDecoder::Initialize()
    // call with a supported configuration, so |pending_remote_decoder| and
    // |media_log_| must be valid.
    CHECK(pending_remote_decoder);
    CHECK(media_log_);

    // |media_task_runner_| is expected to correspond to |sequence_checker_| and
    // is the sequence on which |oop_video_decoder_| will be used.
    CHECK(media_task_runner_->RunsTasksInCurrentSequence());

    oop_video_decoder_ = OOPVideoDecoder::Create(
        std::move(pending_remote_decoder), media_log_->Clone(),
        /*decoder_task_runner=*/media_task_runner_,
        /*client=*/nullptr);
    CHECK(oop_video_decoder_);

    media_log_ = nullptr;
  }

  CHECK(output_cb);
  output_cb_ = output_cb;

  oop_video_decoder()->Initialize(
      config, low_delay, cdm_context, std::move(init_cb),
      base::BindRepeating(&MojoStableVideoDecoder::OnFrameResourceDecoded,
                          weak_this_factory_.GetWeakPtr()),
      waiting_cb);
}

scoped_refptr<MojoStableVideoDecoder::SharedImageHolder>
MojoStableVideoDecoder::CreateOrUpdateSharedImageForFrame(
    const scoped_refptr<FrameResource>& frame_resource) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const gfx::GenericSharedMemoryId frame_id =
      frame_resource->GetSharedMemoryId();
  CHECK(frame_id.is_valid());

  // First, let's see if the buffer for this frame already has a SharedImage
  // that can be re-used.
  SharedImageHolder* shared_image_holder = shared_images_.Lookup(frame_id);
  if (shared_image_holder &&
      shared_image_holder->IsCompatibleWith(frame_resource)) {
    shared_image_holder->Update();
    return base::WrapRefCounted(shared_image_holder);
  }

  // Either we don't have an existing SharedImage or we can't re-use the
  // existing one. Let's create a new one.
  auto new_shared_image = SharedImageHolder::CreateFromFrameResource(
      frame_resource,
      base::WrapRefCounted(gpu_factories_->SharedImageInterface()));
  if (!new_shared_image) {
    return nullptr;
  }

  if (shared_image_holder) {
    // In this case, the buffer already has a SharedImage associated with it,
    // but it couldn't be re-used. We replace that SharedImage with
    // |new_shared_image|. Note that there may still be references to the older
    // SharedImage if the user of the decoded frames still hasn't released all
    // frames that use that SharedImage.
    shared_image_holder = nullptr;
    shared_images_.Replace(frame_id, new_shared_image);
  } else {
    // In this case, the buffer does not have a SharedImage associated with it.
    // Therefore, we need to ask the containing FrameResource to notify us when
    // it's about to be destroyed so that we can release the reference to
    // whatever SharedImage is associated with it.
    FrameResource* original_frame_resource =
        oop_video_decoder()->GetOriginalFrame(frame_id);
    CHECK(original_frame_resource);
    shared_images_.AddWithID(new_shared_image, frame_id);
    original_frame_resource->AddDestructionObserver(
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&MojoStableVideoDecoder::UnregisterSharedImage,
                           weak_this_factory_.GetWeakPtr(), frame_id)));
  }

  return new_shared_image;
}

void MojoStableVideoDecoder::UnregisterSharedImage(
    gfx::GenericSharedMemoryId frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(frame_id.is_valid());
  CHECK(shared_images_.Lookup(frame_id));
  shared_images_.Remove(frame_id);
}

void MojoStableVideoDecoder::OnFrameResourceDecoded(
    scoped_refptr<FrameResource> frame_resource) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The incoming |frame_resource| is backed by dma-buf FDs, but the
  // MojoStableVideoDecoder needs to output SharedImage-backed VideoFrames. We
  // do that here, but we have to be careful about the lifetime of the resulting
  // SharedImage. In particular, a SharedImage should live for at least as long
  // as the later of the following events:
  //
  // a) Once the OOPVideoDecoder forgets about the corresponding buffer (e.g. on
  //    detection of a typical resolution change event).
  //
  // b) Once the SharedImage cannot longer be associated with a particular
  //    buffer (e.g., when only the color space has changed).
  //
  // c) Once all VideoFrames output by the MojoStableVideoDecoder that share the
  //    same SharedImage have been released.
  //
  // To guarantee this, we maintain multiple references to the SharedImage using
  // a SharedImageHolder.
  scoped_refptr<SharedImageHolder> shared_image =
      CreateOrUpdateSharedImageForFrame(frame_resource);
  if (!shared_image) {
    return;
  }

  // Note that |mailbox_frame| will maintain a reference to |shared_image| and
  // to |frame_resource|. The former is to ensure that the SharedImage lives for
  // at least as long as the user of the decoded frame needs it. The latter is
  // to ensure the service gets notified that it may re-use the underlying
  // buffer once the decoded frame is no longer needed.
  auto client_shared_image = shared_image->client_shared_image();
  auto sync_token = shared_image->GenUnverifiedSyncToken();

  scoped_refptr<VideoFrame> mailbox_frame = VideoFrame::WrapSharedImage(
      frame_resource->format(), std::move(client_shared_image), sync_token,
      /*mailbox_holders_release_cb=*/
      base::DoNothingWithBoundArgs(std::move(shared_image), frame_resource),
      /*coded_size=*/GetRectSizeFromOrigin(frame_resource->visible_rect()),
      frame_resource->visible_rect(), frame_resource->natural_size(),
      frame_resource->timestamp());
  if (!mailbox_frame) {
    DVLOGF(1) << "Could not create a gpu::Mailbox-backed VideoFrame for the "
                 "decoded frame";
    return;
  }
  mailbox_frame->set_color_space(frame_resource->ColorSpace());
  mailbox_frame->set_hdr_metadata(frame_resource->hdr_metadata());
  mailbox_frame->set_metadata(frame_resource->metadata());
  mailbox_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormatExternalSampler);
  mailbox_frame->metadata().read_lock_fences_enabled = true;
  mailbox_frame->metadata().is_webgpu_compatible =
      frame_resource->metadata().is_webgpu_compatible;

  const int64_t timestamp = frame_resource->timestamp().InMilliseconds();
  const auto timestamp_it = timestamps_.Peek(timestamp);
  // The OOPVideoDecoder has an internal cache that ensures incoming frames have
  // a timestamp that corresponds to an earlier Decode() call. The cache in the
  // OOPVideoDecoder is of the same size as |timestamps_|. Therefore, we should
  // always be able to find the incoming frame in |timestamps_|, hence the
  // CHECK().
  CHECK(timestamp_it != timestamps_.end());
  const auto decode_start_time = timestamp_it->second;
  const auto decode_end_time = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(kMojoStableVideoDecoderDecodeLatencyHistogram,
                      decode_end_time - decode_start_time);

  output_cb_.Run(std::move(mailbox_frame));
}

OOPVideoDecoder* MojoStableVideoDecoder::oop_video_decoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<OOPVideoDecoder*>(oop_video_decoder_.get());
}

const OOPVideoDecoder* MojoStableVideoDecoder::oop_video_decoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<const OOPVideoDecoder*>(oop_video_decoder_.get());
}

}  // namespace media
