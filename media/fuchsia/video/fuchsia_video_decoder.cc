// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/fuchsia/video/fuchsia_video_decoder.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/zx/eventpair.h>
#include <vulkan/vulkan.h>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "media/base/cdm_context.h"
#include "media/base/media_switches.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/cdm/fuchsia/fuchsia_cdm_context.h"
#include "media/cdm/fuchsia/fuchsia_stream_decryptor.h"
#include "media/fuchsia/common/decrypting_sysmem_buffer_stream.h"
#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"

namespace media {

namespace {

// Number of output buffers allocated "for camping". This value is passed to
// sysmem to ensure that we get one output buffer for the frame currently
// displayed on the screen.
constexpr uint32_t kOutputBuffersForCamping = 1;

// Maximum number of frames we expect to have queued up while playing video.
// Higher values require more memory for output buffers. Lower values make it
// more likely that renderer will stall because decoded frames are not available
// on time.
constexpr uint32_t kMaxUsedOutputBuffers = 5;

// Use 2 buffers for decoder input. Allocating more than one buffers ensures
// that when the decoder is done working on one packet it will have another one
// waiting in the queue. Limiting number of buffers to 2 allows to minimize
// required memory, without significant effect on performance.
constexpr size_t kNumInputBuffers = 2;

// Some codecs do not support splitting video frames across multiple input
// buffers, so the buffers need to be large enough to fit all video frames. The
// buffer size is calculated to fit 1080p I420 frame with MinCR=2 (per H264
// spec), plus 128KiB for SEI/SPS/PPS. (note that the same size is used for all
// codecs, not just H264).
constexpr size_t kInputBufferSize = 1920 * 1080 * 3 / 2 / 2 + 128 * 1024;

const fuchsia::images2::PixelFormat kSupportedPixelFormats[] = {
    fuchsia::images2::PixelFormat::NV12,
    fuchsia::images2::PixelFormat::I420,
    fuchsia::images2::PixelFormat::YV12,
};
const fuchsia::images2::ColorSpace kSupportedColorSpaces[] = {
    fuchsia::images2::ColorSpace::REC601_NTSC,
    fuchsia::images2::ColorSpace::REC601_NTSC_FULL_RANGE,
    fuchsia::images2::ColorSpace::REC601_PAL,
    fuchsia::images2::ColorSpace::REC601_PAL_FULL_RANGE,
    fuchsia::images2::ColorSpace::REC709,
};

std::optional<gfx::Size> ParseMinBufferSize() {
  std::string min_buffer_size_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMinVideoDecoderOutputBufferSize);
  if (min_buffer_size_arg.empty())
    return std::nullopt;
  size_t width;
  size_t height;
  if (sscanf(min_buffer_size_arg.c_str(), "%zux%zu" SCNu32, &width, &height) !=
      2) {
    LOG(WARNING) << "Invalid value for --"
                 << switches::kMinVideoDecoderOutputBufferSize << ": '"
                 << min_buffer_size_arg << "'";
    return std::nullopt;
  }
  return gfx::Size(width, height);
}

std::optional<gfx::Size> GetMinBufferSize() {
  static std::optional<gfx::Size> value = ParseMinBufferSize();
  return value;
}

}  // namespace

// Helper used to hold mailboxes for the output textures. OutputMailbox may
// outlive FuchsiaVideoDecoder if is referenced by a VideoFrame.
class FuchsiaVideoDecoder::OutputMailbox {
 public:
  OutputMailbox(
      scoped_refptr<viz::RasterContextProvider> raster_context_provider,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size& size,
      viz::SharedImageFormat& format,
      gfx::ClientNativePixmapFactory* pixmap_factory,
      const gfx::ColorSpace& color_space)
      : raster_context_provider_(raster_context_provider),
        size_(size),
        weak_factory_(this) {
    gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                     gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE;

    // Note that the shared image prefers external sampler.
    format.SetPrefersExternalSampler();
    shared_image_ =
        raster_context_provider_->SharedImageInterface()->CreateSharedImage(
            {format, size, color_space, usage, "FuchsiaVideoDecoder"},
            std::move(gmb_handle));

    create_sync_token_ = raster_context_provider_->SharedImageInterface()
                             ->GenVerifiedSyncToken();
  }

  OutputMailbox(const OutputMailbox&) = delete;
  OutputMailbox& operator=(const OutputMailbox&) = delete;

  ~OutputMailbox() {
    raster_context_provider_->SharedImageInterface()->DestroySharedImage(
        release_sync_token_, std::move(shared_image_));
  }

  const gpu::Mailbox& mailbox() { return shared_image_->mailbox(); }

  const gfx::Size& size() { return size_; }

  // Create a new video frame that wraps the mailbox. |reuse_callback| will be
  // called when the mailbox can be reused.
  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat pixel_format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp,
                                        base::OnceClosure reuse_callback) {
    DCHECK(!is_used_);
    is_used_ = true;
    reuse_callback_ = std::move(reuse_callback);

    auto frame = VideoFrame::WrapSharedImage(
        pixel_format, shared_image_, create_sync_token_,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &OutputMailbox::OnFrameDestroyed, base::Unretained(this))),
        coded_size, visible_rect, natural_size, timestamp);
    create_sync_token_.Clear();

    frame->set_shared_image_format_type(
        media::SharedImageFormatType::kSharedImageFormatExternalSampler);

    // Request a fence we'll wait on before reusing the buffer.
    frame->metadata().read_lock_fences_enabled = true;

    return frame;
  }

  // Called by FuchsiaVideoDecoder when it no longer needs this mailbox.
  void Release() {
    if (is_used_) {
      // The mailbox is referenced by a VideoFrame. It will be deleted  as soon
      // as the frame is destroyed.
      DCHECK(reuse_callback_);
      reuse_callback_ = base::OnceClosure();
    } else {
      delete this;
    }
  }

 private:
  void OnFrameDestroyed(const gpu::SyncToken& sync_token) {
    DCHECK(is_used_);
    is_used_ = false;
    release_sync_token_ = sync_token;

    if (!reuse_callback_) {
      // If the mailbox cannot be reused then we can just delete it.
      delete this;
      return;
    }

    raster_context_provider_->ContextSupport()->SignalSyncToken(
        release_sync_token_,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &OutputMailbox::OnSyncTokenSignaled, weak_factory_.GetWeakPtr())));
  }

  void OnSyncTokenSignaled() {
    release_sync_token_.Clear();
    std::move(reuse_callback_).Run();
  }

  const scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  gfx::Size size_;

  scoped_refptr<gpu::ClientSharedImage> shared_image_;

  gpu::SyncToken create_sync_token_;
  gpu::SyncToken release_sync_token_;

  // Set to true when the mailbox is referenced by a video frame.
  bool is_used_ = false;

  base::OnceClosure reuse_callback_;

  base::WeakPtrFactory<OutputMailbox> weak_factory_;
};

FuchsiaVideoDecoder::FuchsiaVideoDecoder(
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    const mojo::SharedRemote<media::mojom::FuchsiaMediaCodecProvider>&
        media_codec_provider,
    bool allow_overlays)
    : raster_context_provider_(raster_context_provider),
      media_codec_provider_(media_codec_provider),
      use_overlays_for_video_(allow_overlays),
      sysmem_allocator_("CrFuchsiaVideoDecoder"),
      client_native_pixmap_factory_(
          ui::CreateClientNativePixmapFactoryOzone()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(raster_context_provider_);
}

FuchsiaVideoDecoder::~FuchsiaVideoDecoder() {
  // Reset SysmemBufferStream to ensure it doesn't try to send new packets when
  // the |decoder_| is destroyed.
  sysmem_buffer_stream_.reset();
  decoder_.reset();

  // Release mailboxes used for output frames.
  ReleaseOutputBuffers();
}

bool FuchsiaVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool FuchsiaVideoDecoder::SupportsDecryption() const {
  return true;
}

VideoDecoderType FuchsiaVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kFuchsia;
}

void FuchsiaVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                     bool low_delay,
                                     CdmContext* cdm_context,
                                     InitCB init_cb,
                                     const OutputCB& output_cb,
                                     const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb);
  DCHECK(decode_callbacks_.empty());

  auto done_callback = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  // There should be no pending decode request, so DropInputQueue() is not
  // expected to fail.
  bool result = DropInputQueue(DecoderStatus::Codes::kAborted);
  DCHECK(result);

  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  // Keep decoder and decryptor if the configuration hasn't changed.
  if (decoder_ && current_config_.codec() == config.codec() &&
      current_config_.is_encrypted() == config.is_encrypted()) {
    std::move(done_callback).Run(DecoderStatus::Codes::kOk);
    return;
  }

  sysmem_buffer_stream_.reset();
  decoder_.reset();

  // Initialize the stream.
  bool secure_input = false;
  DecoderStatus status = InitializeSysmemBufferStream(
      config.is_encrypted(), cdm_context, &secure_input);

  if (!status.is_ok()) {
    std::move(done_callback).Run(status);
    return;
  }

  media::mojom::VideoDecoderSecureMemoryMode secure_mode =
      media::mojom::VideoDecoderSecureMemoryMode::CLEAR;
  if (secure_input) {
    if (!use_overlays_for_video_) {
      DLOG(ERROR) << "Protected content can be rendered only using overlays.";
      std::move(done_callback)
          .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig,
                             FROM_HERE));
      return;
    }
    secure_mode = media::mojom::VideoDecoderSecureMemoryMode::SECURE;
  } else if (use_overlays_for_video_ &&
             base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kForceProtectedVideoOutputBuffers)) {
    secure_mode = media::mojom::VideoDecoderSecureMemoryMode::SECURE_OUTPUT;
  }
  protected_output_ =
      secure_mode != media::mojom::VideoDecoderSecureMemoryMode::CLEAR;

  // Reset output buffers since we won't be able to re-use them.
  ReleaseOutputBuffers();

  fuchsia::media::StreamProcessorPtr decoder;
  media_codec_provider_->CreateVideoDecoder(config.codec(), secure_mode,
                                            decoder.NewRequest());
  decoder_ = std::make_unique<StreamProcessorHelper>(std::move(decoder), this);

  current_config_ = config;

  // Default to REC601 when the colorspace is not specified in the container.
  // TODO(crbug.com/42050522): HW decoders currently don't provide accurate
  // color space information to sysmem. Once that issue is resolved, we'll
  // need to update this logic accordingly.
  if (!current_config_.color_space_info().IsSpecified())
    current_config_.set_color_space_info(VideoColorSpace::REC601());

  std::move(done_callback).Run(DecoderStatus::Codes::kOk);
}

void FuchsiaVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                 DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decoder_) {
    // Post the callback to the current sequence as DecoderStream doesn't expect
    // Decode() to complete synchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  decode_callbacks_.push_back(std::move(decode_cb));

  sysmem_buffer_stream_->EnqueueBuffer(std::move(buffer));
}

void FuchsiaVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DropInputQueue(DecoderStatus::Codes::kAborted);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

bool FuchsiaVideoDecoder::NeedsBitstreamConversion() const {
  return true;
}

bool FuchsiaVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_used_output_buffers_ < kMaxUsedOutputBuffers;
}

int FuchsiaVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_decoder_requests_;
}

void FuchsiaVideoDecoder::SetClientNativePixmapFactoryForTests(
    std::unique_ptr<gfx::ClientNativePixmapFactory> factory) {
  client_native_pixmap_factory_ = std::move(factory);
}

DecoderStatus FuchsiaVideoDecoder::InitializeSysmemBufferStream(
    bool is_encrypted,
    CdmContext* cdm_context,
    bool* out_secure_mode) {
  DCHECK(!sysmem_buffer_stream_);

  *out_secure_mode = false;

  // By default queue as many decode requests as the input buffers available
  // with one extra request to be able to send a new InputBuffer immediately.
  max_decoder_requests_ = kNumInputBuffers + 1;

  if (is_encrypted) {
    // `waiting_cb_` is required for encrypted streams.
    DCHECK(waiting_cb_);

    // Caller makes sure |cdm_context| is available if the stream is encrypted.
    if (!cdm_context) {
      DLOG(ERROR) << "No cdm context for encrypted stream.";
      return DecoderStatus::Codes::kUnsupportedEncryptionMode;
    }

    // Use FuchsiaStreamDecryptor with FuchsiaCdm (it doesn't support
    // media::Decryptor interface). Otherwise (e.g. for ClearKey CDM) use
    // DecryptingSysmemBufferStream.
    FuchsiaCdmContext* fuchsia_cdm = cdm_context->GetFuchsiaCdmContext();
    if (fuchsia_cdm) {
      *out_secure_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableProtectedVideoBuffers);
      sysmem_buffer_stream_ =
          fuchsia_cdm->CreateStreamDecryptor(*out_secure_mode);

      // For optimal performance allow more requests to fill the decryptor
      // queue.
      max_decoder_requests_ += FuchsiaStreamDecryptor::kInputBufferCount;
    } else {
      sysmem_buffer_stream_ = std::make_unique<DecryptingSysmemBufferStream>(
          &sysmem_allocator_, cdm_context, Decryptor::kVideo);
    }
  } else {
    sysmem_buffer_stream_ =
        std::make_unique<PassthroughSysmemBufferStream>(&sysmem_allocator_);
  }

  sysmem_buffer_stream_->Initialize(this, kInputBufferSize, kNumInputBuffers);

  return DecoderStatus::Codes::kOk;
}

void FuchsiaVideoDecoder::OnSysmemBufferStreamBufferCollectionToken(
    fuchsia::sysmem2::BufferCollectionTokenPtr token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_);
  decoder_->SetInputBufferCollectionToken(std::move(token));
}

void FuchsiaVideoDecoder::OnSysmemBufferStreamOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  packet.AddOnDestroyClosure(
      base::BindOnce(&FuchsiaVideoDecoder::CallNextDecodeCallback,
                     decode_callbacks_weak_factory_.GetWeakPtr()));
  decoder_->Process(std::move(packet));
}

void FuchsiaVideoDecoder::OnSysmemBufferStreamEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decoder_->ProcessEos();
}

void FuchsiaVideoDecoder::OnSysmemBufferStreamError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnError();
}

void FuchsiaVideoDecoder::OnSysmemBufferStreamNoKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
}

void FuchsiaVideoDecoder::OnStreamProcessorAllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& output_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReleaseOutputBuffers();

  output_buffer_collection_ = sysmem_allocator_.AllocateNewCollection();

  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::CompleteOutputBuffersAllocation,
                     base::Unretained(decoder_.get())),
      "codec");
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&FuchsiaVideoDecoder::SetBufferCollectionTokenForGpu,
                     base::Unretained(this)),
      "gpu");

  fuchsia::sysmem2::BufferCollectionConstraints constraints;
  constraints.mutable_usage()->set_none(fuchsia::sysmem2::NONE_USAGE);
  constraints.set_min_buffer_count_for_camping(kOutputBuffersForCamping);
  constraints.set_min_buffer_count_for_shared_slack(kMaxUsedOutputBuffers -
                                                    kOutputBuffersForCamping);

  for (size_t pixel_format_index = 0;
       pixel_format_index < std::size(kSupportedPixelFormats);
       ++pixel_format_index) {
    auto& image_constraints =
        constraints.mutable_image_format_constraints()->emplace_back();
    image_constraints.set_pixel_format(
        kSupportedPixelFormats[pixel_format_index]);
    image_constraints.set_pixel_format_modifier(
        fuchsia::images2::PixelFormatModifier::LINEAR);

    for (size_t i = 0; i < std::size(kSupportedColorSpaces); ++i) {
      image_constraints.mutable_color_spaces()->emplace_back(kSupportedColorSpaces[i]);
    }
  }

  auto min_buffer_size = GetMinBufferSize();
  if (min_buffer_size) {
    for (size_t pixel_format_index = 0;
         pixel_format_index < std::size(kSupportedPixelFormats);
         ++pixel_format_index) {
      auto& image_constraints = constraints.mutable_image_format_constraints()->at(
          pixel_format_index);
      image_constraints.set_required_max_size(fuchsia::math::SizeU{
          static_cast<uint32_t>(min_buffer_size->width()),
          static_cast<uint32_t>(min_buffer_size->height())});
    }
  }

  output_buffer_collection_->Initialize(std::move(constraints),
                                        "ChromiumVideoDecoderOutput");
}

void FuchsiaVideoDecoder::OnStreamProcessorEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Decode() is not supposed to be called again after EOF.
  DCHECK_EQ(decode_callbacks_.size(), 1U);
  CallNextDecodeCallback();
}

void FuchsiaVideoDecoder::OnStreamProcessorOutputFormat(
    fuchsia::media::StreamOutputFormat output_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* format = output_format.mutable_format_details();
  if (!format->has_domain() || !format->domain().is_video() ||
      !format->domain().video().is_uncompressed()) {
    DLOG(ERROR) << "Received OnOutputFormat() with invalid format.";
    OnError();
    return;
  }

  output_format_ = std::move(format->mutable_domain()->video().uncompressed());
}

void FuchsiaVideoDecoder::OnStreamProcessorOutputPacket(
    StreamProcessorHelper::IoPacket output_packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We can safely cast from fuchsia.sysmem.PixelFormatType to
  // fuchsia.images2.PixelFormat.
  auto sysmem_pixel_format = static_cast<fuchsia::images2::PixelFormat>(
      fidl::ToUnderlying(output_format_.image_format.pixel_format.type));

  VideoPixelFormat pixel_format;
  // The GMB is either kNV12 or kYV12.
  viz::SharedImageFormat si_format;
  VkFormat vk_format;
  switch (sysmem_pixel_format) {
    case fuchsia::images2::PixelFormat::NV12:
      pixel_format = PIXEL_FORMAT_NV12;
      si_format = viz::MultiPlaneFormat::kNV12;
      vk_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
      break;

    case fuchsia::images2::PixelFormat::I420:
    case fuchsia::images2::PixelFormat::YV12:
      pixel_format = PIXEL_FORMAT_I420;
      si_format = viz::MultiPlaneFormat::kYV12;
      vk_format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
      break;

    default:
      DLOG(ERROR) << "Unsupported pixel format: "
                  << static_cast<int>(sysmem_pixel_format);
      OnError();
      return;
  }

  size_t buffer_index = output_packet.buffer_index();

  if (buffer_index >= output_mailboxes_.size())
    output_mailboxes_.resize(buffer_index + 1, nullptr);

  auto coded_size = gfx::Size(output_format_.primary_width_pixels,
                              output_format_.primary_height_pixels);

  if (output_mailboxes_[buffer_index] &&
      output_mailboxes_[buffer_index]->size() != coded_size) {
    output_mailboxes_[buffer_index]->Release();
    output_mailboxes_[buffer_index] = nullptr;
  }

  if (!output_mailboxes_[buffer_index]) {
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::NATIVE_PIXMAP;
    auto status = output_buffer_collection_handle_.duplicate(
        ZX_RIGHT_SAME_RIGHTS,
        &gmb_handle.native_pixmap_handle.buffer_collection_handle);
    ZX_DCHECK(status == ZX_OK, status);
    gmb_handle.native_pixmap_handle.buffer_index = buffer_index;

    output_mailboxes_[buffer_index] = new OutputMailbox(
        raster_context_provider_, std::move(gmb_handle), coded_size, si_format,
        client_native_pixmap_factory_.get(),
        current_config_.color_space_info().ToGfxColorSpace());
  } else {
    raster_context_provider_->SharedImageInterface()->UpdateSharedImage(
        gpu::SyncToken(), output_mailboxes_[buffer_index]->mailbox());
  }

  auto display_rect = gfx::Rect(output_format_.primary_display_width_pixels,
                                output_format_.primary_display_height_pixels);

  VideoAspectRatio aspect_ratio = current_config_.aspect_ratio();
  if (!aspect_ratio.IsValid() && output_format_.has_pixel_aspect_ratio) {
    aspect_ratio =
        VideoAspectRatio::PAR(output_format_.pixel_aspect_ratio_width,
                              output_format_.pixel_aspect_ratio_height);
  }

  auto timestamp = output_packet.timestamp();

  // SendInputPacket() sets timestamp for all packets sent to the decoder, so we
  // expect to receive timestamp for all decoded frames. Missing timestamp
  // indicates a bug in the decoder implementation.
  if (timestamp == kNoTimestamp) {
    LOG(ERROR) << "Received frame without timestamp.";
    OnError();
    return;
  }

  num_used_output_buffers_++;

  auto frame = output_mailboxes_[buffer_index]->CreateFrame(
      pixel_format, coded_size, display_rect,
      aspect_ratio.GetNaturalSize(display_rect), timestamp,
      base::BindOnce(&FuchsiaVideoDecoder::ReleaseOutputPacket,
                     base::Unretained(this), std::move(output_packet)));

  VkSamplerYcbcrModelConversion ycbcr_conversion =
      (current_config_.color_space_info().matrix ==
       VideoColorSpace::MatrixID::BT709)
          ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
          : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;

  // Currently sysmem doesn't specify location of chroma samples relative to
  // luma (see fxbug.dev/13677). Assume they are cosited with luma. YCbCr info
  // here must match the values passed for the same buffer in
  // ui::SysmemBufferCollection::CreateVkImage() (see
  // ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.cc).
  // |format_features| are resolved later in the GPU process before this info is
  // passed to Skia.
  frame->set_ycbcr_info(gpu::VulkanYCbCrInfo(
      vk_format, /*external_format=*/0, ycbcr_conversion,
      VK_SAMPLER_YCBCR_RANGE_ITU_NARROW, VK_CHROMA_LOCATION_COSITED_EVEN,
      VK_CHROMA_LOCATION_COSITED_EVEN, /*format_features=*/0));

  // Mark the frame as power-efficient since (software decoders are used only in
  // tests).
  frame->metadata().power_efficient = true;

  // Allow this video frame to be promoted as an overlay, because it was
  // registered with an ImagePipe.
  frame->metadata().allow_overlay = use_overlays_for_video_;

  if (protected_output_) {
    frame->metadata().protected_video = true;
    frame->metadata().hw_protected = true;
  }

  output_cb_.Run(std::move(frame));
}

void FuchsiaVideoDecoder::OnStreamProcessorNoKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Decoder is not expected to produce NoKey() error.
  DLOG(ERROR) << "Video decoder failed with DECRYPTOR_NO_KEY expectedly";
  OnError();
}

void FuchsiaVideoDecoder::OnStreamProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnError();
}

void FuchsiaVideoDecoder::CallNextDecodeCallback() {
  DCHECK(!decode_callbacks_.empty());
  auto cb = std::move(decode_callbacks_.front());
  decode_callbacks_.pop_front();

  std::move(cb).Run(DecoderStatus::Codes::kOk);
}

bool FuchsiaVideoDecoder::DropInputQueue(DecoderStatus status) {
  // Invalidate callbacks for CallNextDecodeCallback(), so the callbacks are not
  // called when the |decoder_| is dropped below. The callbacks are called
  // explicitly later.
  decode_callbacks_weak_factory_.InvalidateWeakPtrs();

  if (decoder_) {
    decoder_->Reset();
  }

  if (sysmem_buffer_stream_) {
    sysmem_buffer_stream_->Reset();
  }

  // Get a fresh WeakPtr, to use to check whether any DecodeCB deletes |this|.
  auto weak_this = decode_callbacks_weak_factory_.GetWeakPtr();

  for (auto& cb : decode_callbacks_) {
    std::move(cb).Run(status);
    if (!weak_this)
      return false;
  }
  decode_callbacks_.clear();

  return true;
}

void FuchsiaVideoDecoder::OnError() {
  sysmem_buffer_stream_.reset();
  decoder_.reset();

  ReleaseOutputBuffers();

  DropInputQueue(DecoderStatus::Codes::kFailed);
}

void FuchsiaVideoDecoder::SetBufferCollectionTokenForGpu(
    fuchsia::sysmem2::BufferCollectionTokenPtr token) {
  // Register the new collection with the GPU process.
  DCHECK(!output_buffer_collection_handle_);

  zx::eventpair service_handle;
  auto status = zx::eventpair::create(0, &output_buffer_collection_handle_,
                                      &service_handle);
  ZX_DCHECK(status == ZX_OK, status);
  raster_context_provider_->SharedImageInterface()
      ->RegisterSysmemBufferCollection(
          std::move(service_handle), token.Unbind().TakeChannel(),
          viz::MultiPlaneFormat::kNV12, gfx::BufferUsage::GPU_READ,
          use_overlays_for_video_ /*register_with_image_pipe*/);

  // Exact number of buffers sysmem will allocate is unknown here.
  // |output_mailboxes_| is resized when we start receiving output frames.
  DCHECK(output_mailboxes_.empty());
}

void FuchsiaVideoDecoder::ReleaseOutputBuffers() {
  // Release the buffer collection.
  num_used_output_buffers_ = 0;
  if (output_buffer_collection_) {
    output_buffer_collection_.reset();
  }

  // Release all output mailboxes.
  for (OutputMailbox* mailbox : output_mailboxes_) {
    if (mailbox)
      mailbox->Release();
  }
  output_mailboxes_.clear();

  // Tell the GPU process to drop the buffer collection.
  output_buffer_collection_handle_.reset();
}

void FuchsiaVideoDecoder::ReleaseOutputPacket(
    StreamProcessorHelper::IoPacket output_packet) {
  DCHECK_GT(num_used_output_buffers_, 0U);
  num_used_output_buffers_--;
}

}  // namespace media
