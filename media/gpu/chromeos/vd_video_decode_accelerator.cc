// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vd_video_decode_accelerator.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/format_utils.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "media/base/waiting.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check does not account for BUILDFLAG(), so including these headers will
// make gn check fail for builds other than ash-chrome. See gn help nogncheck
// for more information.
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"  // nogncheck
#include "media/gpu/chromeos/secure_buffer.pb.h"                  // nogncheck
#include "third_party/cros_system_api/constants/cdm_oemcrypto.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {
namespace {

// VideoDecoder copies the timestamp from DecodeBuffer to its corresponding
// FrameResource. However, VideoDecodeAccelerator uses bitstream ID to find the
// corresponding output picture. Therefore, we store bitstream ID at the
// timestamp field. These two functions are used for converting between
// bitstream ID and fake timestamp.
base::TimeDelta BitstreamIdToFakeTimestamp(int32_t bitstream_id) {
  return base::Milliseconds(bitstream_id);
}

int32_t FakeTimestampToBitstreamId(base::TimeDelta timestamp) {
  return static_cast<int32_t>(timestamp.InMilliseconds());
}

std::vector<ColorPlaneLayout> ExtractColorPlaneLayout(
    const gfx::GpuMemoryBufferHandle& gmb_handle) {
  std::vector<ColorPlaneLayout> planes;
  for (const auto& plane : gmb_handle.native_pixmap_handle.planes)
    planes.emplace_back(plane.stride, plane.offset, plane.size);
  return planes;
}

// TODO(akahuang): Move this function to a utility file.
template <class T>
std::string VectorToString(const std::vector<T>& vec) {
  std::ostringstream result;
  std::string delim;
  result << "[";
  for (auto& v : vec) {
    result << delim << v;
    if (delim.size() == 0)
      delim = ", ";
  }
  result << "]";
  return result.str();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<DecoderBuffer> DecryptBitstreamBuffer(
    BitstreamBuffer bitstream_buffer) {
  // Check to see if we have our secure buffer tag and then extract the
  // decrypt parameters.
  auto mem_region = bitstream_buffer.DuplicateRegion();
  if (!mem_region.IsValid()) {
    DVLOG(2) << "Invalid shared memory region";
    return nullptr;
  }
  const size_t available_size =
      mem_region.GetSize() -
      base::checked_cast<size_t>(bitstream_buffer.offset());
  auto mapping = mem_region.Map();
  if (!mapping.IsValid()) {
    DVLOG(2) << "Failed mapping shared memory";
    return nullptr;
  }
  // Checks if this buffer contains the details needed for HW protected video
  // decoding.
  // The header is 1KB in size (cdm_oemcrypto::kSecureBufferHeaderSize).
  // It consists of 3 components.
  // 1. Marker tag - cdm_oemcrypto::kSecureBufferTag
  // 2. unsigned 32-bit size of #3
  // 3. Serialized ArcSecureBufferForChrome proto
  uint8_t* data = mapping.GetMemoryAs<uint8_t>();
  if (!data) {
    DVLOG(2) << "Failed accessing shared memory";
    return nullptr;
  }
  // Apply the offset here so we don't need to worry about page alignment in the
  // mapping.
  data += bitstream_buffer.offset();
  if (available_size <= cdm_oemcrypto::kSecureBufferHeaderSize ||
      memcmp(data, cdm_oemcrypto::kSecureBufferTag,
             cdm_oemcrypto::kSecureBufferTagLen)) {
    // This occurs in Intel implementations when we are in a clear portion.
    return bitstream_buffer.ToDecoderBuffer();
  }
  VLOG(2) << "Detected secure buffer format in VDVDA";
  // Read the protobuf size.
  uint32_t proto_size = 0;
  memcpy(&proto_size, data + cdm_oemcrypto::kSecureBufferTagLen,
         sizeof(uint32_t));
  if (proto_size > cdm_oemcrypto::kSecureBufferHeaderSize -
                       cdm_oemcrypto::kSecureBufferProtoOffset) {
    DVLOG(2) << "Proto size goes beyond header size";
    return nullptr;
  }
  // Read the serialized proto.
  std::string serialized_proto(
      data + cdm_oemcrypto::kSecureBufferProtoOffset,
      data + cdm_oemcrypto::kSecureBufferProtoOffset + proto_size);
  chromeos::cdm::ArcSecureBufferForChrome buffer_proto;
  if (!buffer_proto.ParseFromString(serialized_proto)) {
    DVLOG(2) << "Failed deserializing secure buffer proto";
    return nullptr;
  }

  // Now extract the DecryptConfig info from the protobuf.
  std::vector<media::SubsampleEntry> subsamples;
  size_t buffer_size = 0;
  for (const auto& subsample : buffer_proto.subsample()) {
    buffer_size += subsample.clear_bytes() + subsample.cypher_bytes();
    subsamples.emplace_back(subsample.clear_bytes(), subsample.cypher_bytes());
  }
  std::optional<EncryptionPattern> pattern = std::nullopt;
  if (buffer_proto.has_pattern()) {
    pattern.emplace(buffer_proto.pattern().cypher_bytes(),
                    buffer_proto.pattern().clear_bytes());
  }
  // Now create the DecryptConfig and set it in the decoder buffer.
  scoped_refptr<DecoderBuffer> buffer = bitstream_buffer.ToDecoderBuffer(
      cdm_oemcrypto::kSecureBufferHeaderSize, buffer_size);
  if (!buffer) {
    DVLOG(2) << "Secure buffer data goes beyond shared memory size";
    return nullptr;
  }
  if (buffer_proto.encryption_scheme() !=
      chromeos::cdm::ArcSecureBufferForChrome::NONE) {
    buffer->set_decrypt_config(std::make_unique<DecryptConfig>(
        buffer_proto.encryption_scheme() ==
                chromeos::cdm::ArcSecureBufferForChrome::CBCS
            ? EncryptionScheme::kCbcs
            : EncryptionScheme::kCenc,
        buffer_proto.key_id(), buffer_proto.iv(), std::move(subsamples),
        std::move(pattern)));
  }
  return buffer;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// static
std::unique_ptr<VideoDecodeAccelerator> VdVideoDecodeAccelerator::Create(
    CreateVideoDecoderCb create_vd_cb,
    Client* client,
    const Config& config,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::unique_ptr<VdVideoDecodeAccelerator,
                  std::default_delete<VideoDecodeAccelerator>>
      vda(new VdVideoDecodeAccelerator(std::move(create_vd_cb),
                                       std::move(task_runner)));
  if (!vda->Initialize(config, client, /*low_delay=*/true))
    return nullptr;
  return vda;
}

VdVideoDecodeAccelerator::VdVideoDecodeAccelerator(
    CreateVideoDecoderCb create_vd_cb,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : create_vd_cb_(std::move(create_vd_cb)),
      client_task_runner_(std::move(client_task_runner)) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

void VdVideoDecodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();

  // Because VdaVideoFramePool is blocked for this callback, we must call the
  // callback before destroying.
  if (notify_layout_changed_cb_)
    std::move(notify_layout_changed_cb_)
        .Run(CroStatus::Codes::kFailedToGetFrameLayout);
  client_ = nullptr;
  vd_.reset();

  delete this;
}

VdVideoDecodeAccelerator::~VdVideoDecodeAccelerator() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
}

bool VdVideoDecodeAccelerator::Initialize(const Config& config,
                                          Client* client) {
  // |low_delay_| came from the most recent initialization, or false if it has
  // never been explicitly set.
  return Initialize(config, client, low_delay_);
}

bool VdVideoDecodeAccelerator::Initialize(const Config& config,
                                          Client* client,
                                          bool low_delay) {
  VLOGF(2) << "config: " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

#if !BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  if (config.is_encrypted()) {
    VLOGF(1) << "Encrypted streams are not supported";
    return false;
  }
#endif  //  !BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  if (config.output_mode != Config::OutputMode::kImport) {
    VLOGF(1) << "Only IMPORT OutputMode is supported.";
    return false;
  }
  if (!config.is_deferred_initialization_allowed) {
    VLOGF(1) << "Only is_deferred_initialization_allowed is supported.";
    return false;
  }

  // In case we are re-initializing for encrypted content.
  if (!vd_) {
    std::unique_ptr<VdaVideoFramePool> frame_pool =
        std::make_unique<VdaVideoFramePool>(weak_this_, client_task_runner_);
    // TODO(b/238684141): Wire a meaningful GpuDriverBugWorkarounds or remove
    // its use.
    vd_ = create_vd_cb_.Run(
        gpu::GpuDriverBugWorkarounds(), client_task_runner_,
        std::move(frame_pool),
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs());
    if (!vd_)
      return false;

    client_ = client;
  }
  media::CdmContext* cdm_context = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_encrypted_ = config.is_encrypted();
  if (is_encrypted_)
    cdm_context = chromeos::ChromeOsCdmFactory::GetArcCdmContext();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  VideoDecoderConfig vd_config(
      VideoCodecProfileToVideoCodec(config.profile), config.profile,
      VideoDecoderConfig::AlphaMode::kIsOpaque, config.container_color_space,
      VideoTransformation(), config.initial_expected_coded_size,
      gfx::Rect(config.initial_expected_coded_size),
      config.initial_expected_coded_size, std::vector<uint8_t>(),
      config.encryption_scheme);
  auto init_cb =
      base::BindOnce(&VdVideoDecodeAccelerator::OnInitializeDone, weak_this_);
  auto output_cb =
      base::BindRepeating(&VdVideoDecodeAccelerator::OnFrameReady, weak_this_);
  vd_->Initialize(std::move(vd_config), low_delay, cdm_context,
                  std::move(init_cb), std::move(output_cb), base::DoNothing());
  // Save the value for possible future re-initialization.
  low_delay_ = low_delay;
  return true;
}

void VdVideoDecodeAccelerator::OnInitializeDone(DecoderStatus status) {
  DVLOGF(3) << "success: " << status.is_ok();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  client_->NotifyInitializationComplete(status);
}

void VdVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer) {
  const int32_t bitstream_id = bitstream_buffer.id();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_encrypted_) {
    scoped_refptr<DecoderBuffer> buffer =
        DecryptBitstreamBuffer(std::move(bitstream_buffer));
    // This happens in the error case.
    if (!buffer) {
      OnError(FROM_HERE, PLATFORM_FAILURE);
      return;
    }
    Decode(std::move(buffer), bitstream_id);
    return;
  }
#endif  // BUILFLAG(IS_CHROMEOS_ASH)
  Decode(bitstream_buffer.ToDecoderBuffer(), bitstream_id);
}

void VdVideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      int32_t bitstream_id) {
  DVLOGF(4) << "bitstream_id:" << bitstream_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(vd_);

  // Set timestamp field as bitstream buffer id, because we can only use
  // timestamp field to find the corresponding output frames. Also, VDA doesn't
  // care about timestamp.
  buffer->set_timestamp(BitstreamIdToFakeTimestamp(bitstream_id));

  vd_->Decode(std::move(buffer),
              base::BindOnce(&VdVideoDecodeAccelerator::OnDecodeDone,
                             weak_this_, bitstream_id));
}

void VdVideoDecodeAccelerator::OnDecodeDone(int32_t bitstream_buffer_id,
                                            DecoderStatus status) {
  DVLOGF(4) << "status: " << status.group() << ":"
            << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  if (!status.is_ok() && status.code() != DecoderStatus::Codes::kAborted) {
    OnError(FROM_HERE, PLATFORM_FAILURE);
    return;
  }

  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void VdVideoDecodeAccelerator::OnFrameReady(scoped_refptr<VideoFrame> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(frame);
  DCHECK(client_);

  std::optional<Picture> picture = GetPicture(*frame);
  if (!picture) {
    VLOGF(1) << "Failed to get picture.";
    OnError(FROM_HERE, PLATFORM_FAILURE);
    return;
  }

  // Record that the picture is sent to the client.
  auto it = picture_at_client_.find(picture->picture_buffer_id());
  if (it == picture_at_client_.end()) {
    // We haven't sent the buffer to the client. Set |num_sent| = 1;
    picture_at_client_.emplace(picture->picture_buffer_id(),
                               std::make_pair(std::move(frame), 1));
  } else {
    // We already sent the buffer to the client (only happen when using VP9
    // show_existing_frame feature). Increase |num_sent|;
    ++(it->second.second);
  }

  client_->PictureReady(*picture);
}

void VdVideoDecodeAccelerator::Flush() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(vd_);

  vd_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&VdVideoDecodeAccelerator::OnFlushDone, weak_this_));
}

void VdVideoDecodeAccelerator::OnFlushDone(DecoderStatus status) {
  DVLOGF(3) << "status: " << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  switch (status.code()) {
    case DecoderStatus::Codes::kOk:
      client_->NotifyFlushDone();
      break;
    case DecoderStatus::Codes::kAborted:
      // Do nothing.
      break;
    default:
      OnError(FROM_HERE, PLATFORM_FAILURE);
      break;
  }
}

void VdVideoDecodeAccelerator::Reset() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(vd_);

  if (is_resetting_) {
    VLOGF(1) << "The previous Reset() has not finished yet, aborted.";
    return;
  }

  is_resetting_ = true;
  if (notify_layout_changed_cb_) {
    std::move(notify_layout_changed_cb_).Run(CroStatus::Codes::kResetRequired);
    import_frame_cb_.Reset();
  }

  vd_->Reset(
      base::BindOnce(&VdVideoDecodeAccelerator::OnResetDone, weak_this_));
}

void VdVideoDecodeAccelerator::OnResetDone() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);
  DCHECK(is_resetting_);

  is_resetting_ = false;
  client_->NotifyResetDone();
}

void VdVideoDecodeAccelerator::RequestFrames(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    size_t max_num_frames,
    NotifyLayoutChangedCb notify_layout_changed_cb,
    ImportFrameCb import_frame_cb) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);
  DCHECK(!notify_layout_changed_cb_);

  // Stop tracking currently-allocated pictures, otherwise the count will be
  // corrupted as we import new frames with the same IDs as the old ones.
  // The client should still have its own reference to the frame data, which
  // will keep it valid for as long as it needs it.
  picture_at_client_.clear();

  notify_layout_changed_cb_ = std::move(notify_layout_changed_cb);
  import_frame_cb_ = std::move(import_frame_cb);
  // We need to check if Reset() was received before RequestFrames() so that we
  // can unblock the frame pool in that case.
  if (is_resetting_) {
    std::move(notify_layout_changed_cb_).Run(CroStatus::Codes::kResetRequired);
    import_frame_cb_.Reset();
    return;
  }

  // After calling ProvidePictureBuffersWithVisibleRect(), the client might
  // still send buffers with old coded size. We temporarily store at
  // |pending_coded_size_|.
  pending_coded_size_ = coded_size;
  client_->ProvidePictureBuffersWithVisibleRect(
      max_num_frames, fourcc.ToVideoPixelFormat(), coded_size, visible_rect);
}

VideoFrame::StorageType VdVideoDecodeAccelerator::GetFrameStorageType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  return VideoFrame::STORAGE_DMABUFS;
}

void VdVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // After AssignPictureBuffers() is called, the buffers sent from
  // ImportBufferForPicture() should be with new coded size. Now we can update
  // |coded_size_|.
  coded_size_ = pending_coded_size_;
}

void VdVideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gmb_handle) {
  DVLOGF(4) << "picture_buffer_id: " << picture_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!import_frame_cb_)
    return;

  // The first imported picture after requesting buffers.
  // |notify_layout_changed_cb_| must be called in this clause because it blocks
  // VdaVideoFramePool.
  if (notify_layout_changed_cb_) {
    auto fourcc = Fourcc::FromVideoPixelFormat(pixel_format);
    if (!fourcc) {
      VLOGF(1) << "Failed to convert to Fourcc.";
      import_frame_cb_.Reset();
      std::move(notify_layout_changed_cb_)
          .Run(CroStatus::Codes::kFailedToChangeResolution);
      return;
    }

    CHECK(media::VerifyGpuMemoryBufferHandle(pixel_format, coded_size_,
                                             gmb_handle));
    const uint64_t modifier = gmb_handle.type == gfx::NATIVE_PIXMAP
                                  ? gmb_handle.native_pixmap_handle.modifier
                                  : gfx::NativePixmapHandle::kNoModifier;

    std::vector<ColorPlaneLayout> planes = ExtractColorPlaneLayout(gmb_handle);
    layout_ = VideoFrameLayout::CreateWithPlanes(
        pixel_format, coded_size_, planes,
        VideoFrameLayout::kBufferAddressAlignment, modifier);
    if (!layout_) {
      VLOGF(1) << "Failed to create VideoFrameLayout. format: "
               << VideoPixelFormatToString(pixel_format)
               << ", coded_size: " << coded_size_.ToString()
               << ", planes: " << VectorToString(planes)
               << ", modifier: " << std::hex << modifier;
      import_frame_cb_.Reset();
      std::move(notify_layout_changed_cb_)
          .Run(CroStatus::Codes::kFailedToChangeResolution);
      return;
    }

    auto gb_layout =
        GpuBufferLayout::Create(*fourcc, coded_size_, planes, modifier);
    if (!gb_layout) {
      VLOGF(1) << "Failed to create GpuBufferLayout. fourcc: "
               << fourcc->ToString()
               << ", coded_size: " << coded_size_.ToString()
               << ", planes: " << VectorToString(planes)
               << ", modifier: " << std::hex << modifier;
      layout_ = std::nullopt;
      import_frame_cb_.Reset();
      std::move(notify_layout_changed_cb_)
          .Run(CroStatus::Codes::kFailedToChangeResolution);
      return;
    }
    std::move(notify_layout_changed_cb_).Run(*gb_layout);
  }

  if (!layout_)
    return;

  CHECK(media::VerifyGpuMemoryBufferHandle(pixel_format, layout_->coded_size(),
                                           gmb_handle));
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  CHECK(buffer_format);
  // Usage is SCANOUT_CPU_READ_WRITE because we may need to map the buffer in
  // order to use the LibYUVImageProcessorBackend.
  // TODO(b/349610963): investigate whether there is a better buffer usage.
  // FrameResource::CreateWrappingFrame() will check whether the updated
  // visible_rect is sub rect of the original visible_rect. Therefore we set
  // visible_rect as large as coded_size to guarantee this condition.
  scoped_refptr<media::FrameResource> origin_frame =
      NativePixmapFrameResource::Create(
          gfx::Rect(layout_->coded_size()), layout_->coded_size(),
          base::TimeDelta(), gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
          base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
              layout_->coded_size(), *buffer_format,
              std::move(gmb_handle.native_pixmap_handle)));

  // Makes sure that GetFrameStorageType() agrees with the usage of the previous
  // call to NativePixmapFrameResource::Create().
  CHECK_EQ(origin_frame->storage_type(), GetFrameStorageType());

  auto res = frame_id_to_picture_id_.emplace(origin_frame->GetSharedMemoryId(),
                                             picture_buffer_id);
  // The frame ID should not be inside the map before insertion.
  DCHECK(res.second);

  // |wrapped_frame| is used to keep |origin_frame| alive until everyone
  // released |wrapped_frame|. Then GpuMemoryBufferId will be available at
  // OnFrameReleased().
  scoped_refptr<FrameResource> wrapped_frame =
      origin_frame->CreateWrappingFrame();
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&VdVideoDecodeAccelerator::OnFrameReleasedThunk,
                     weak_this_, client_task_runner_, std::move(origin_frame)));

  // This should not happen - picture_at_client_ should either be initially
  // empty, or be cleared as RequestFrames() is called. However for extra safety
  // let's make sure the slot for the picture buffer ID is free, otherwise we
  // might lose track of the reference count and keep frames out of the pool
  // forever.
  if (picture_at_client_.erase(picture_buffer_id) > 0) {
    VLOGF(1) << "Picture " << picture_buffer_id
             << " still referenced, dropping it.";
  }

  import_frame_cb_.Run(std::move(wrapped_frame));
}

std::optional<Picture> VdVideoDecodeAccelerator::GetPicture(
    const VideoFrame& frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = frame_id_to_picture_id_.find(GetSharedMemoryId(frame));
  if (it == frame_id_to_picture_id_.end()) {
    VLOGF(1) << "Failed to find the picture buffer id.";
    return std::nullopt;
  }
  int32_t picture_buffer_id = it->second;
  int32_t bitstream_id = FakeTimestampToBitstreamId(frame.timestamp());
  return std::make_optional(
      Picture(picture_buffer_id, bitstream_id, frame.visible_rect()));
}

// static
void VdVideoDecodeAccelerator::OnFrameReleasedThunk(
    std::optional<base::WeakPtr<VdVideoDecodeAccelerator>> weak_this,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<FrameResource> origin_frame) {
  DVLOGF(4);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&VdVideoDecodeAccelerator::OnFrameReleased,
                                *weak_this, std::move(origin_frame)));
}

void VdVideoDecodeAccelerator::OnFrameReleased(
    scoped_refptr<FrameResource> origin_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = frame_id_to_picture_id_.find(origin_frame->GetSharedMemoryId());
  CHECK(it != frame_id_to_picture_id_.end(), base::NotFatalUntil::M130);
  int32_t picture_buffer_id = it->second;
  frame_id_to_picture_id_.erase(it);

  client_->DismissPictureBuffer(picture_buffer_id);
}

void VdVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOGF(4) << "picture_buffer_id: " << picture_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = picture_at_client_.find(picture_buffer_id);
  if (it == picture_at_client_.end()) {
    DVLOGF(3) << picture_buffer_id << " has already been dismissed, ignore.";
    return;
  }

  size_t& num_sent = it->second.second;
  DCHECK_NE(num_sent, 0u);
  --num_sent;

  // The count of calling VDA::ReusePictureBuffer() is the same as calling
  // Client::PictureReady(). Now we could really reuse the buffer.
  if (num_sent == 0)
    picture_at_client_.erase(it);
}

void VdVideoDecodeAccelerator::OnError(base::Location location, Error error) {
  LOG(ERROR) << "Failed at " << location.ToString()
             << ", error code: " << static_cast<int>(error);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  client_->NotifyError(error);
}

}  // namespace media
