// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vd_video_decode_accelerator.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "media/base/media_util.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "media/base/waiting.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/macros.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

// VideoDecoder copies the timestamp from DecodeBuffer to its corresponding
// VideoFrame. However, VideoDecodeAccelerator uses bitstream ID to find the
// corresponding output picture. Therefore, we store bitstream ID at the
// timestamp field. These two functions are used for converting between
// bitstream ID and fake timestamp.
base::TimeDelta BitstreamIdToFakeTimestamp(int32_t bitstream_id) {
  return base::TimeDelta::FromMilliseconds(bitstream_id);
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

std::vector<base::ScopedFD> ExtractFds(gfx::GpuMemoryBufferHandle gmb_handle) {
  std::vector<base::ScopedFD> fds;
  for (auto& plane : gmb_handle.native_pixmap_handle.planes)
    fds.push_back(std::move(plane.fd));
  return fds;
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

}  // namespace

// static
std::unique_ptr<VideoDecodeAccelerator> VdVideoDecodeAccelerator::Create(
    CreateVideoDecoderCb create_vd_cb,
    Client* client,
    const Config& config,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::unique_ptr<VideoDecodeAccelerator> vda(new VdVideoDecodeAccelerator(
      std::move(create_vd_cb), std::move(task_runner)));
  if (!vda->Initialize(config, client))
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
    std::move(notify_layout_changed_cb_).Run(base::nullopt);
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
  VLOGF(2) << "config: " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(!client_);

  if (config.is_encrypted()) {
    VLOGF(1) << "Encrypted streams are not supported";
    return false;
  }
  if (config.output_mode != Config::OutputMode::IMPORT) {
    VLOGF(1) << "Only IMPORT OutputMode is supported.";
    return false;
  }
  if (!config.is_deferred_initialization_allowed) {
    VLOGF(1) << "Only is_deferred_initialization_allowed is supported.";
    return false;
  }

  std::unique_ptr<VdaVideoFramePool> frame_pool =
      std::make_unique<VdaVideoFramePool>(weak_this_, client_task_runner_);
  vd_ = create_vd_cb_.Run(client_task_runner_, std::move(frame_pool),
                          std::make_unique<VideoFrameConverter>(),
                          std::make_unique<NullMediaLog>());
  if (!vd_)
    return false;

  client_ = client;

  VideoDecoderConfig vd_config(
      VideoCodecProfileToVideoCodec(config.profile), config.profile,
      VideoDecoderConfig::AlphaMode::kIsOpaque, config.container_color_space,
      VideoTransformation(), config.initial_expected_coded_size,
      gfx::Rect(config.initial_expected_coded_size),
      config.initial_expected_coded_size, std::vector<uint8_t>(),
      EncryptionScheme::kUnencrypted);
  auto init_cb =
      base::BindOnce(&VdVideoDecodeAccelerator::OnInitializeDone, weak_this_);
  auto output_cb =
      base::BindRepeating(&VdVideoDecodeAccelerator::OnFrameReady, weak_this_);
  vd_->Initialize(std::move(vd_config), false /* low_delay */,
                  nullptr /* cdm_context */, std::move(init_cb),
                  std::move(output_cb), WaitingCB());
  return true;
}

void VdVideoDecodeAccelerator::OnInitializeDone(Status status) {
  DVLOGF(3) << "success: " << status.is_ok();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  client_->NotifyInitializationComplete(status);
}

void VdVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer) {
  Decode(bitstream_buffer.ToDecoderBuffer(), bitstream_buffer.id());
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
                                            Status status) {
  DVLOGF(4) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  if (!status.is_ok() && status.code() != StatusCode::kAborted) {
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

  base::Optional<Picture> picture = GetPicture(*frame);
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

void VdVideoDecodeAccelerator::OnFlushDone(Status status) {
  DVLOGF(3) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

  switch (status.code()) {
    case StatusCode::kOk:
      client_->NotifyFlushDone();
      break;
    case StatusCode::kAborted:
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

  vd_->Reset(
      base::BindOnce(&VdVideoDecodeAccelerator::OnResetDone, weak_this_));
}

void VdVideoDecodeAccelerator::OnResetDone() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client_);

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

  notify_layout_changed_cb_ = std::move(notify_layout_changed_cb);
  import_frame_cb_ = std::move(import_frame_cb);

  // After calling ProvidePictureBuffersWithVisibleRect(), the client might
  // still send buffers with old coded size. We temporarily store at
  // |pending_coded_size_|.
  pending_coded_size_ = coded_size;
  client_->ProvidePictureBuffersWithVisibleRect(
      max_num_frames, fourcc.ToVideoPixelFormat(), 1 /* textures_per_buffer */,
      coded_size, visible_rect, GL_TEXTURE_EXTERNAL_OES);
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

  // The first imported picture after requesting buffers.
  // |notify_layout_changed_cb_| must be called in this clause because it blocks
  // VdaVideoFramePool.
  if (notify_layout_changed_cb_) {
    auto fourcc = Fourcc::FromVideoPixelFormat(pixel_format);
    if (!fourcc) {
      VLOGF(1) << "Failed to convert to Fourcc.";
      std::move(notify_layout_changed_cb_).Run(base::nullopt);
      return;
    }

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
      std::move(notify_layout_changed_cb_).Run(base::nullopt);
      return;
    }

    std::move(notify_layout_changed_cb_)
        .Run(GpuBufferLayout::Create(*fourcc, coded_size_, planes, modifier));
  }

  if (!layout_)
    return;

  // VideoFrame::WrapVideoFrame() will check whether the updated visible_rect
  // is sub rect of the original visible_rect. Therefore we set visible_rect
  // as large as coded_size to guarantee this condition.
  scoped_refptr<VideoFrame> origin_frame = VideoFrame::WrapExternalDmabufs(
      *layout_, gfx::Rect(coded_size_), coded_size_,
      ExtractFds(std::move(gmb_handle)), base::TimeDelta());
  DmabufId dmabuf_id = DmabufVideoFramePool::GetDmabufId(*origin_frame);
  auto res = frame_id_to_picture_id_.emplace(dmabuf_id, picture_buffer_id);
  // |dmabuf_id| should not be inside the map before insertion.
  DCHECK(res.second);

  // |wrapped_frame| is used to keep |origin_frame| alive until everyone
  // released |wrapped_frame|. Then DmabufId will be available at
  // OnFrameReleased().
  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      origin_frame, origin_frame->format(), origin_frame->visible_rect(),
      origin_frame->natural_size());
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&VdVideoDecodeAccelerator::OnFrameReleasedThunk,
                     weak_this_, client_task_runner_, std::move(origin_frame)));

  DCHECK(import_frame_cb_);
  import_frame_cb_.Run(std::move(wrapped_frame));
}

base::Optional<Picture> VdVideoDecodeAccelerator::GetPicture(
    const VideoFrame& frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it =
      frame_id_to_picture_id_.find(DmabufVideoFramePool::GetDmabufId(frame));
  if (it == frame_id_to_picture_id_.end()) {
    VLOGF(1) << "Failed to find the picture buffer id.";
    return base::nullopt;
  }
  int32_t picture_buffer_id = it->second;
  int32_t bitstream_id = FakeTimestampToBitstreamId(frame.timestamp());
  bool allow_overlay = frame.metadata()->allow_overlay;
  return base::make_optional(Picture(picture_buffer_id, bitstream_id,
                                     frame.visible_rect(), frame.ColorSpace(),
                                     allow_overlay));
}

// static
void VdVideoDecodeAccelerator::OnFrameReleasedThunk(
    base::Optional<base::WeakPtr<VdVideoDecodeAccelerator>> weak_this,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<VideoFrame> origin_frame) {
  DVLOGF(4);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&VdVideoDecodeAccelerator::OnFrameReleased,
                                *weak_this, std::move(origin_frame)));
}

void VdVideoDecodeAccelerator::OnFrameReleased(
    scoped_refptr<VideoFrame> origin_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = frame_id_to_picture_id_.find(
      DmabufVideoFramePool::GetDmabufId(*origin_frame));
  DCHECK(it != frame_id_to_picture_id_.end());
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
