// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_encode_accelerator.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

// File-static mojom::VideoEncodeAcceleratorClient implementation to trampoline
// method calls to its |client_|. Note that this class is thread hostile when
// bound.
class VideoEncodeAcceleratorClient
    : public mojom::VideoEncodeAcceleratorClient {
 public:
  VideoEncodeAcceleratorClient(
      VideoEncodeAccelerator::Client* client,
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorClient> receiver);
  ~VideoEncodeAcceleratorClient() override = default;

  // mojom::VideoEncodeAcceleratorClient impl.
  void RequireBitstreamBuffers(uint32_t input_count,
                               const gfx::Size& input_coded_size,
                               uint32_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(VideoEncodeAccelerator::Error error) override;

 private:
  VideoEncodeAccelerator::Client* client_;
  mojo::Receiver<mojom::VideoEncodeAcceleratorClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncodeAcceleratorClient);
};

VideoEncodeAcceleratorClient::VideoEncodeAcceleratorClient(
    VideoEncodeAccelerator::Client* client,
    mojo::PendingReceiver<mojom::VideoEncodeAcceleratorClient> receiver)
    : client_(client), receiver_(this, std::move(receiver)) {
  DCHECK(client_);
}

void VideoEncodeAcceleratorClient::RequireBitstreamBuffers(
    uint32_t input_count,
    const gfx::Size& input_coded_size,
    uint32_t output_buffer_size) {
  DVLOG(2) << __func__ << " input_count= " << input_count
           << " input_coded_size= " << input_coded_size.ToString()
           << " output_buffer_size=" << output_buffer_size;
  client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                   output_buffer_size);
}

void VideoEncodeAcceleratorClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << "B,  key_frame=" << metadata.key_frame;
  client_->BitstreamBufferReady(bitstream_buffer_id, metadata);
}

void VideoEncodeAcceleratorClient::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DVLOG(2) << __func__;
  client_->NotifyError(error);
}

}  // anonymous namespace

MojoVideoEncodeAccelerator::MojoVideoEncodeAccelerator(
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> vea,
    const gpu::VideoEncodeAcceleratorSupportedProfiles& supported_profiles)
    : vea_(std::move(vea)), supported_profiles_(supported_profiles) {
  DVLOG(1) << __func__;
  DCHECK(vea_);
}

VideoEncodeAccelerator::SupportedProfiles
MojoVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
      supported_profiles_);
}

bool MojoVideoEncodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  DVLOG(2) << __func__ << " " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client)
    return false;

  // Get a mojom::VideoEncodeAcceleratorClient bound to a local implementation
  // (VideoEncodeAcceleratorClient) and send the remote.
  mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> vea_client_remote;
  vea_client_ = std::make_unique<VideoEncodeAcceleratorClient>(
      client, vea_client_remote.InitWithNewPipeAndPassReceiver());

  bool result = false;
  vea_->Initialize(config, std::move(vea_client_remote), &result);
  return result;
}

void MojoVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DVLOG(2) << __func__ << " tstamp=" << frame->timestamp();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(VideoFrame::NumPlanes(frame->format()),
            frame->layout().num_planes());
  DCHECK(vea_.is_bound());

#if defined(OS_LINUX)
  // TODO(crbug.com/1003197): Remove this once we stop supporting STORAGE_DMABUF
  // in VideoEncodeAccelerator.
  if (frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    DCHECK(frame->HasDmaBufs());
    vea_->Encode(
        frame, force_keyframe,
        base::BindOnce(base::DoNothing::Once<scoped_refptr<VideoFrame>>(),
                       frame));
    return;
  }
#endif
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    vea_->Encode(
        frame, force_keyframe,
        base::BindOnce(base::DoNothing::Once<scoped_refptr<VideoFrame>>(),
                       frame));
    return;
  }

  if (frame->format() != PIXEL_FORMAT_I420 ||
      VideoFrame::STORAGE_SHMEM != frame->storage_type() ||
      !frame->shm_region()->IsValid()) {
    DLOG(ERROR) << "Unexpected video frame buffer";
    return;
  }

  // Oftentimes |frame|'s underlying planes will be aligned and not tightly
  // packed, so don't use VideoFrame::AllocationSize().
  const size_t allocation_size = frame->shm_region()->GetSize();

  // A MojoSharedBufferVideoFrame is created with an owned writable handle. As
  // the handle in |frame| is not owned, a new region must be created and
  // |frame| copied into it.
  mojo::ScopedSharedBufferHandle dst_handle =
      mojo::SharedBufferHandle::Create(allocation_size);
  if (!dst_handle->is_valid()) {
    DLOG(ERROR) << "Can't create new frame backing memory";
    return;
  }
  mojo::ScopedSharedBufferMapping dst_mapping =
      dst_handle->Map(allocation_size);
  if (!dst_mapping) {
    DLOG(ERROR) << "Can't map new frame backing memory";
    return;
  }
  DCHECK(frame->shm_region());
  base::WritableSharedMemoryMapping src_mapping = frame->shm_region()->Map();
  if (!src_mapping.IsValid()) {
    DLOG(ERROR) << "Can't map src frame backing memory";
    return;
  }
  memcpy(dst_mapping.get(), src_mapping.memory(), allocation_size);

  const size_t y_offset = frame->shared_memory_offset();
  const size_t u_offset = y_offset + frame->data(VideoFrame::kUPlane) -
                          frame->data(VideoFrame::kYPlane);
  const size_t v_offset = y_offset + frame->data(VideoFrame::kVPlane) -
                          frame->data(VideoFrame::kYPlane);
  // Temporary Mojo VideoFrame to allow for marshalling.
  scoped_refptr<MojoSharedBufferVideoFrame> mojo_frame =
      MojoSharedBufferVideoFrame::Create(
          frame->format(), frame->coded_size(), frame->visible_rect(),
          frame->natural_size(), std::move(dst_handle), allocation_size,
          y_offset, u_offset, v_offset, frame->stride(VideoFrame::kYPlane),
          frame->stride(VideoFrame::kUPlane),
          frame->stride(VideoFrame::kVPlane), frame->timestamp());

  // Encode() is synchronous: clients will assume full ownership of |frame| when
  // this gets destroyed and probably recycle its shared_memory_handle(): keep
  // the former alive until the remote end is actually finished.
  vea_->Encode(
      std::move(mojo_frame), force_keyframe,
      base::BindOnce(base::DoNothing::Once<scoped_refptr<VideoFrame>>(),
                     std::move(frame)));
}

void MojoVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(2) << __func__ << " buffer.id()= " << buffer.id()
           << " buffer.size()= " << buffer.size() << "B";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(buffer.region().IsValid());

  auto buffer_handle =
      mojo::WrapPlatformSharedMemoryRegion(buffer.TakeRegion());

  vea_->UseOutputBitstreamBuffer(buffer.id(), std::move(buffer_handle));
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  media::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, bitrate);
  vea_->RequestEncodingParametersChange(bitrate_allocation, framerate);
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->RequestEncodingParametersChange(bitrate, framerate);
}

void MojoVideoEncodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vea_client_.reset();
  vea_.reset();
  // See media::VideoEncodeAccelerator for more info on this peculiar pattern.
  delete this;
}

MojoVideoEncodeAccelerator::~MojoVideoEncodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace media
