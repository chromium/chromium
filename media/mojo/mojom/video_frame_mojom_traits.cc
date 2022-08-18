// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/mojom/video_frame_metadata_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/posix/eintr_wrapper.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace mojo {

namespace {

media::mojom::VideoFrameDataPtr MakeVideoFrameData(
    const media::VideoFrame* input) {
  if (input->metadata().end_of_stream) {
    return media::mojom::VideoFrameData::NewEosData(
        media::mojom::EosVideoFrameData::New());
  }

  if (input->storage_type() == media::VideoFrame::STORAGE_MOJO_SHARED_BUFFER) {
    const media::MojoSharedBufferVideoFrame* mojo_frame =
        static_cast<const media::MojoSharedBufferVideoFrame*>(input);

    base::ReadOnlySharedMemoryRegion region =
        mojo_frame->shmem_region().Duplicate();
    DCHECK(region.IsValid());
    size_t num_planes = media::VideoFrame::NumPlanes(mojo_frame->format());
    std::vector<uint32_t> offsets(num_planes);
    std::vector<int32_t> strides(num_planes);
    for (size_t i = 0; i < num_planes; ++i) {
      offsets[i] = mojo_frame->PlaneOffset(i);
      strides[i] = mojo_frame->stride(i);
    }

    return media::mojom::VideoFrameData::NewSharedBufferData(
        media::mojom::SharedBufferVideoFrameData::New(
            std::move(region), std::move(strides), std::move(offsets)));
  }

  std::vector<gpu::MailboxHolder> mailbox_holder(media::VideoFrame::kMaxPlanes);
  DCHECK_LE(input->NumTextures(), mailbox_holder.size());
  // STORAGE_GPU_MEMORY_BUFFER may carry meaningful or dummy mailboxes,
  // we should only access them when there are textures.
  for (size_t i = 0; i < input->NumTextures(); i++)
    mailbox_holder[i] = input->mailbox_holder(i);

  if (input->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    if (input->HasGpuMemoryBuffer())
      gpu_memory_buffer_handle = input->GetGpuMemoryBuffer()->CloneHandle();
    return media::mojom::VideoFrameData::NewGpuMemoryBufferData(
        media::mojom::GpuMemoryBufferVideoFrameData::New(
            std::move(gpu_memory_buffer_handle), std::move(mailbox_holder)));
  } else if (input->HasTextures()) {
    return media::mojom::VideoFrameData::NewMailboxData(
        media::mojom::MailboxVideoFrameData::New(
            std::move(mailbox_holder), std::move(input->ycbcr_info())));
  }

  NOTREACHED() << "Unsupported VideoFrame conversion";
  return nullptr;
}

}  // namespace

// static
media::mojom::VideoFrameDataPtr StructTraits<media::mojom::VideoFrameDataView,
                                             scoped_refptr<media::VideoFrame>>::
    data(const scoped_refptr<media::VideoFrame>& input) {
  return media::mojom::VideoFrameDataPtr(MakeVideoFrameData(input.get()));
}

// static
bool StructTraits<media::mojom::VideoFrameDataView,
                  scoped_refptr<media::VideoFrame>>::
    Read(media::mojom::VideoFrameDataView input,
         scoped_refptr<media::VideoFrame>* output) {
  // View of the |data| member of the input media::mojom::VideoFrame.
  media::mojom::VideoFrameDataDataView data;
  input.GetDataDataView(&data);

  if (data.is_eos_data()) {
    *output = media::VideoFrame::CreateEOSFrame();
    return !!*output;
  }

  media::VideoPixelFormat format;
  if (!input.ReadFormat(&format))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  if (!gfx::Rect(coded_size).Contains(visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  base::TimeDelta timestamp;
  if (!input.ReadTimestamp(&timestamp))
    return false;

  scoped_refptr<media::VideoFrame> frame;
  if (data.is_shared_buffer_data()) {
    media::mojom::SharedBufferVideoFrameDataDataView shared_buffer_data;
    data.GetSharedBufferDataDataView(&shared_buffer_data);

    base::ReadOnlySharedMemoryRegion region;
    if (!shared_buffer_data.ReadFrameData(&region))
      return false;

    mojo::ArrayDataView<uint32_t> offsets;
    shared_buffer_data.GetOffsetsDataView(&offsets);

    mojo::ArrayDataView<int32_t> strides;
    shared_buffer_data.GetStridesDataView(&strides);

    frame = media::MojoSharedBufferVideoFrame::Create(
        format, coded_size, visible_rect, natural_size, std::move(region),
        offsets, strides, timestamp);
  } else if (data.is_gpu_memory_buffer_data()) {
    media::mojom::GpuMemoryBufferVideoFrameDataDataView gpu_memory_buffer_data;
    data.GetGpuMemoryBufferDataDataView(&gpu_memory_buffer_data);

    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    if (!gpu_memory_buffer_data.ReadGpuMemoryBufferHandle(
            &gpu_memory_buffer_handle)) {
      return false;
    }

    std::vector<gpu::MailboxHolder> mailbox_holder;
    if (!gpu_memory_buffer_data.ReadMailboxHolder(&mailbox_holder)) {
      DLOG(WARNING) << "Failed to get mailbox holder";
    }
    if (mailbox_holder.size() > media::VideoFrame::kMaxPlanes) {
      DLOG(ERROR) << "The size of mailbox holder is too large: "
                  << mailbox_holder.size();
      return false;
    }

    gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
    for (size_t i = 0; i < mailbox_holder.size(); i++)
      mailbox_holder_array[i] = mailbox_holder[i];

    absl::optional<gfx::BufferFormat> buffer_format =
        VideoPixelFormatToGfxBufferFormat(format);
    if (!buffer_format)
      return false;

    // Shared memory GMBs do not support VEA/CAMERA usage.
    const gfx::BufferUsage buffer_usage =
        (gpu_memory_buffer_handle.type ==
         gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER)
            ? gfx::BufferUsage::SCANOUT_CPU_READ_WRITE
            : gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

    gpu::GpuMemoryBufferSupport support;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
        support.CreateGpuMemoryBufferImplFromHandle(
            std::move(gpu_memory_buffer_handle), coded_size, *buffer_format,
            buffer_usage, base::NullCallback());
    if (!gpu_memory_buffer)
      return false;

    frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect, natural_size, std::move(gpu_memory_buffer),
        mailbox_holder_array, base::NullCallback(), timestamp);
  } else if (data.is_mailbox_data()) {
    media::mojom::MailboxVideoFrameDataDataView mailbox_data;
    data.GetMailboxDataDataView(&mailbox_data);

    std::vector<gpu::MailboxHolder> mailbox_holder;
    if (!mailbox_data.ReadMailboxHolder(&mailbox_holder))
      return false;

    gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
    for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; i++)
      mailbox_holder_array[i] = mailbox_holder[i];

    absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
    if (!mailbox_data.ReadYcbcrData(&ycbcr_info))
      return false;

    frame = media::VideoFrame::WrapNativeTextures(
        format, mailbox_holder_array, media::VideoFrame::ReleaseMailboxCB(),
        coded_size, visible_rect, natural_size, timestamp);
    frame->set_ycbcr_info(ycbcr_info);
  } else {
    // TODO(sandersd): Switch on the union tag to avoid this ugliness?
    NOTREACHED();
    return false;
  }

  if (!frame)
    return false;

  media::VideoFrameMetadata metadata;
  if (!input.ReadMetadata(&metadata))
    return false;

  frame->set_metadata(metadata);

  gfx::ColorSpace color_space;
  if (!input.ReadColorSpace(&color_space))
    return false;
  frame->set_color_space(color_space);

  absl::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;
  frame->set_hdr_metadata(std::move(hdr_metadata));

  *output = std::move(frame);
  return true;
}

}  // namespace mojo
