// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
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

base::ReadOnlySharedMemoryRegion CreateRegion(const media::VideoFrame& frame,
                                              std::vector<uint32_t>& offsets,
                                              std::vector<int32_t>& strides) {
  if (!media::IsYuvPlanar(frame.format()) || !media::IsOpaque(frame.format())) {
    DLOG(ERROR) << "format is not opaque YUV: "
                << VideoPixelFormatToString(frame.format());
    return base::ReadOnlySharedMemoryRegion();
  }

  size_t num_planes = media::VideoFrame::NumPlanes(frame.format());
  DCHECK_LE(num_planes, 3u);
  offsets.resize(num_planes);
  strides.resize(num_planes);
  if (frame.storage_type() == media::VideoFrame::STORAGE_SHMEM) {
    for (size_t i = 0; i < num_planes; ++i) {
      // This offset computation is safe because the planes are in the single
      // buffer, a single SharedMemoryBuffer. The first plane data must lie
      // in the beginning of the buffer.
      base::CheckedNumeric<intptr_t> offset =
          reinterpret_cast<intptr_t>(frame.data(i));
      offset -= reinterpret_cast<intptr_t>(frame.data(0));
      if (!offset.AssignIfValid(&offsets[i])) {
        DLOG(ERROR) << "Invalid offset: "
                    << static_cast<intptr_t>(frame.data(i) - frame.data(0));
        return base::ReadOnlySharedMemoryRegion();
      }

      strides[i] = frame.stride(i);
    }
    return frame.shm_region()->Duplicate();
  }

  // |frame| is on-memory based VideoFrame. Creates a ReadOnlySharedMemoryRegion
  // and copy the frame data to the region. This DCHECK is safe because of the
  // the conditional in a calling function.
  DCHECK(frame.storage_type() == media::VideoFrame::STORAGE_UNOWNED_MEMORY ||
         frame.storage_type() == media::VideoFrame::STORAGE_OWNED_MEMORY);
  std::vector<size_t> sizes(num_planes);
  size_t aggregate_size = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    strides[i] = frame.stride(i);
    offsets[i] = aggregate_size;
    sizes[i] = media::VideoFrame::Rows(i, frame.format(),
                                       frame.coded_size().height()) *
               strides[i];
    aggregate_size += sizes[i];
  }

  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
  if (!mapped_region.IsValid()) {
    DLOG(ERROR) << "Can't create new frame backing memory";
    return base::ReadOnlySharedMemoryRegion();
  }

  base::WritableSharedMemoryMapping& dst_mapping = mapped_region.mapping;
  uint8_t* dst_data = dst_mapping.GetMemoryAs<uint8_t>();
  // The data from |frame| may not be consecutive between planes. Copy data into
  // a shared memory buffer which is tightly packed. Padding inside each planes
  // are preserved.
  for (size_t i = 0; i < num_planes; ++i) {
    memcpy(dst_data + offsets[i], static_cast<const void*>(frame.data(i)),
           sizes[i]);
  }

  return std::move(mapped_region.region);
}

media::mojom::VideoFrameDataPtr MakeVideoFrameData(
    const media::VideoFrame* input) {
  if (input->metadata().end_of_stream) {
    return media::mojom::VideoFrameData::NewEosData(
        media::mojom::EosVideoFrameData::New());
  }

  if (input->storage_type() == media::VideoFrame::STORAGE_SHMEM ||
      input->storage_type() == media::VideoFrame::STORAGE_UNOWNED_MEMORY ||
      input->storage_type() == media::VideoFrame::STORAGE_OWNED_MEMORY) {
    std::vector<uint32_t> offsets;
    std::vector<int32_t> strides;
    auto region = CreateRegion(*input, offsets, strides);
    if (!region.IsValid()) {
      DLOG(ERROR) << "Failed to create region from VideoFrame";
      return nullptr;
    }

    return media::mojom::VideoFrameData::NewSharedMemoryData(
        media::mojom::SharedMemoryVideoFrameData::New(
            std::move(region), std::move(strides), std::move(offsets)));
  }

  if (input->HasMappableGpuBuffer()) {
    auto gpu_memory_buffer_handle = input->GetGpuMemoryBufferHandle();

    // STORAGE_GPU_MEMORY_BUFFER may carry meaningful or dummy mailbox,
    // we should only access it when there is shared_image.
    gpu::MailboxHolder mailbox_holder;
    std::optional<gpu::ExportedSharedImage> shared_image;
    if (input->HasSharedImage()) {
      shared_image = input->shared_image()->Export();
      mailbox_holder = input->mailbox_holder(/*texture_index=*/0);
    }

    CHECK(input->HasSharedImage() || mailbox_holder.mailbox.IsZero());
    return media::mojom::VideoFrameData::NewGpuMemoryBufferSharedImageData(
        media::mojom::GpuMemoryBufferSharedImageVideoFrameData::New(
            std::move(gpu_memory_buffer_handle), std::move(shared_image),
            std::move(mailbox_holder.sync_token),
            mailbox_holder.texture_target));
  } else if (input->HasSharedImage()) {
    gpu::ExportedSharedImage shared_image = input->shared_image()->Export();
    gpu::MailboxHolder mailbox_holder =
        input->mailbox_holder(/*texture_index=*/0);
    return media::mojom::VideoFrameData::NewSharedImageData(
        media::mojom::SharedImageVideoFrameData::New(
            std::move(shared_image), std::move(mailbox_holder.sync_token),
            std::move(mailbox_holder.texture_target),
            std::move(input->ycbcr_info())));
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  } else if (input->HasOOPVDMailbox()) {
    return media::mojom::VideoFrameData::NewMailboxData(
        media::mojom::MailboxVideoFrameData::New(input->oopvd_mailbox()));
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  }

  NOTREACHED_IN_MIGRATION() << "Unsupported VideoFrame conversion";
  return nullptr;
}

}  // namespace

// static
media::mojom::SharedImageFormatType EnumTraits<
    media::mojom::SharedImageFormatType,
    media::SharedImageFormatType>::ToMojom(media::SharedImageFormatType type) {
  switch (type) {
    case media::SharedImageFormatType::kSharedImageFormat:
      return media::mojom::SharedImageFormatType::kSharedImageFormat;
    case media::SharedImageFormatType::kSharedImageFormatExternalSampler:
      return media::mojom::SharedImageFormatType::
          kSharedImageFormatExternalSampler;
  }
}

// static
bool EnumTraits<media::mojom::SharedImageFormatType,
                media::SharedImageFormatType>::
    FromMojom(media::mojom::SharedImageFormatType input,
              media::SharedImageFormatType* out) {
  switch (input) {
    case media::mojom::SharedImageFormatType::kSharedImageFormat:
      *out = media::SharedImageFormatType::kSharedImageFormat;
      return true;
    case media::mojom::SharedImageFormatType::kSharedImageFormatExternalSampler:
      *out = media::SharedImageFormatType::kSharedImageFormatExternalSampler;
      return true;
  }
  return false;
}

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

  media::VideoFrameMetadata metadata;
  if (!input.ReadMetadata(&metadata)) {
    return false;
  }

  scoped_refptr<media::VideoFrame> frame;
  if (data.is_shared_memory_data()) {
    media::mojom::SharedMemoryVideoFrameDataDataView shared_memory_data;
    data.GetSharedMemoryDataDataView(&shared_memory_data);

    base::ReadOnlySharedMemoryRegion region;
    if (!shared_memory_data.ReadFrameData(&region))
      return false;

    mojo::ArrayDataView<uint32_t> offsets;
    shared_memory_data.GetOffsetsDataView(&offsets);

    mojo::ArrayDataView<int32_t> strides;
    shared_memory_data.GetStridesDataView(&strides);

    base::ReadOnlySharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      DLOG(ERROR) << "Failed to map ReadOnlySharedMemoryRegion";
      return false;
    }

    const size_t num_planes = offsets.size();
    if (num_planes == 0 || num_planes > 3) {
      DLOG(ERROR) << "Invalid number of planes: " << num_planes;
      return false;
    }

    uint8_t* addr[3] = {};
    std::vector<media::ColorPlaneLayout> planes(num_planes);
    for (size_t i = 0; i < num_planes; i++) {
      addr[i] =
          const_cast<uint8_t*>(mapping.GetMemoryAs<uint8_t>()) + offsets[i];
      planes[i].stride = strides[i];
      planes[i].offset = base::strict_cast<size_t>(offsets[i]);
      planes[i].size = i + 1 < num_planes
                           ? offsets[i + 1] - offsets[i]
                           : mapping.size() - offsets[num_planes - 1];
    }

    auto layout = media::VideoFrameLayout::CreateWithPlanes(format, coded_size,
                                                            std::move(planes));
    if (!layout || !layout->FitsInContiguousBufferOfSize(mapping.size())) {
      DLOG(ERROR) << "Invalid layout";
      return false;
    }

    frame = media::VideoFrame::WrapExternalYuvDataWithLayout(
        *layout, visible_rect, natural_size, addr[0], addr[1], addr[2],
        timestamp);
    if (frame) {
      frame->BackWithOwnedSharedMemory(std::move(region), std::move(mapping));
    }
  } else if (data.is_gpu_memory_buffer_shared_image_data()) {
    media::mojom::GpuMemoryBufferSharedImageVideoFrameDataDataView
        gpu_memory_buffer_data;
    data.GetGpuMemoryBufferSharedImageDataDataView(&gpu_memory_buffer_data);

    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    if (!gpu_memory_buffer_data.ReadGpuMemoryBufferHandle(
            &gpu_memory_buffer_handle)) {
      DLOG(ERROR) << "Failed to read GpuMemoryBufferHandle";
      return false;
    }

    std::optional<gpu::ExportedSharedImage> exported_shared_image;
    if (!gpu_memory_buffer_data.ReadSharedImage(&exported_shared_image)) {
      DLOG(ERROR) << "Failed to get shared image";
      return false;
    }

    gpu::SyncToken sync_token;
    if (!gpu_memory_buffer_data.ReadSyncToken(&sync_token)) {
      return false;
    }

    std::optional<gfx::BufferFormat> buffer_format =
        VideoPixelFormatToGfxBufferFormat(format);
    if (!buffer_format) {
      return false;
    }

    // Shared memory GMBs do not support VEA/CAMERA usage.
    gfx::BufferUsage buffer_usage;
    if (metadata.protected_video) {
      buffer_usage = gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE;
    } else if (gpu_memory_buffer_handle.type ==
               gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER) {
      buffer_usage = gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
    } else {
      buffer_usage = gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
    }

    gpu::GpuMemoryBufferSupport support;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
        support.CreateGpuMemoryBufferImplFromHandle(
            std::move(gpu_memory_buffer_handle), coded_size, *buffer_format,
            buffer_usage, base::NullCallback());
    if (!gpu_memory_buffer) {
      return false;
    }

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    if (exported_shared_image) {
      shared_image =
          gpu::ClientSharedImage::ImportUnowned(*exported_shared_image);
    }

    frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect, natural_size, std::move(gpu_memory_buffer), shared_image,
        sync_token, base::NullCallback(), timestamp);
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  } else if (data.is_mailbox_data()) {
    media::mojom::MailboxVideoFrameDataDataView mailbox_data;
    data.GetMailboxDataDataView(&mailbox_data);

    gpu::Mailbox mailbox;
    if (!mailbox_data.ReadMailbox(&mailbox)) {
      return false;
    }

    frame = media::VideoFrame::WrapOOPVDMailbox(
        format, mailbox, media::VideoFrame::ReleaseMailboxCB(), coded_size,
        visible_rect, natural_size, timestamp);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  } else if (data.is_shared_image_data()) {
    media::mojom::SharedImageVideoFrameDataDataView shared_image_data;
    data.GetSharedImageDataDataView(&shared_image_data);

    gpu::ExportedSharedImage exported_shared_image;
    if (!shared_image_data.ReadSharedImage(&exported_shared_image)) {
      return false;
    }
    scoped_refptr<gpu::ClientSharedImage> shared_image =
        gpu::ClientSharedImage::ImportUnowned(exported_shared_image);

    gpu::SyncToken sync_token;
    if (!shared_image_data.ReadSyncToken(&sync_token)) {
      return false;
    }
    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
    if (!shared_image_data.ReadYcbcrData(&ycbcr_info)) {
      return false;
    }

    frame = media::VideoFrame::WrapSharedImage(
        format, shared_image, sync_token, media::VideoFrame::ReleaseMailboxCB(),
        coded_size, visible_rect, natural_size, timestamp);

    frame->set_ycbcr_info(ycbcr_info);
  } else {
    // TODO(sandersd): Switch on the union tag to avoid this ugliness?
    NOTREACHED();
  }

  if (!frame) {
    return false;
  }

  frame->set_metadata(metadata);

  gfx::ColorSpace color_space;
  if (!input.ReadColorSpace(&color_space))
    return false;
  frame->set_color_space(color_space);

  std::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;
  frame->set_hdr_metadata(std::move(hdr_metadata));

  media::SharedImageFormatType shared_image_format_type;
  if (!input.ReadSharedImageFormatType(&shared_image_format_type)) {
    return false;
  }
  frame->set_shared_image_format_type(shared_image_format_type);

  *output = std::move(frame);
  return true;
}

}  // namespace mojo
