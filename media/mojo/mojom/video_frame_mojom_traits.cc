// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
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
#include "media/gpu/buffer_validation.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"  // nogncheck
#include "ui/ozone/public/ozone_platform.h"                      // nogncheck
#endif

namespace mojo {

namespace {

base::ReadOnlySharedMemoryRegion CreateRegion(const media::VideoFrame& frame,
                                              std::vector<uint32_t>& offsets,
                                              std::vector<int32_t>& strides) {
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
  size_t aggregate_size = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    strides[i] = frame.stride(i);
    offsets[i] = aggregate_size;
    aggregate_size += frame.data_span(i).size();
  }

  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
  if (!mapped_region.IsValid()) {
    DLOG(ERROR) << "Can't create new frame backing memory";
    return base::ReadOnlySharedMemoryRegion();
  }

  base::WritableSharedMemoryMapping& dst_mapping = mapped_region.mapping;
  auto dst_data = dst_mapping.GetMemoryAsSpan<uint8_t>();
  // The data from |frame| may not be consecutive between planes. Copy data into
  // a shared memory buffer which is tightly packed. Padding inside each planes
  // are preserved.
  for (size_t i = 0; i < num_planes; ++i) {
    dst_data.subspan(offsets[i]).copy_prefix_from(frame.data_span(i));
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

  bool is_mappable_si_enabled = input->is_mappable_si_enabled();
  if (input->HasMappableGpuBuffer()) {
    auto gpu_memory_buffer_handle = input->GetGpuMemoryBufferHandle();

    // STORAGE_GPU_MEMORY_BUFFER may carry meaningful or dummy shared_image.
    std::optional<gpu::ExportedSharedImage> shared_image;
    gpu::SyncToken sync_token;
#if BUILDFLAG(IS_CHROMEOS)
    if (input->HasSharedImage()) {
      shared_image = input->shared_image()->Export(
          /*with_buffer_handle=*/is_mappable_si_enabled);
      sync_token = input->acquire_sync_token();

      if (is_mappable_si_enabled) {
        return media::mojom::VideoFrameData::NewSharedImageData(
            media::mojom::SharedImageVideoFrameData::New(
                std::move(shared_image.value()), std::move(sync_token),
                /*is_mappable_si_enabled=*/true));
      }
    }

    return media::mojom::VideoFrameData::NewGpuMemoryBufferSharedImageData(
        media::mojom::GpuMemoryBufferSharedImageVideoFrameData::New(
            std::move(gpu_memory_buffer_handle), std::move(shared_image),
            std::move(sync_token)));
#else
    CHECK(input->HasSharedImage());
    CHECK(is_mappable_si_enabled);
    shared_image = input->shared_image()->Export(
        /*with_buffer_handle=*/true);
    sync_token = input->acquire_sync_token();
#if BUILDFLAG(IS_ANDROID)
    return media::mojom::VideoFrameData::NewSharedImageData(
        media::mojom::SharedImageVideoFrameData::New(
            std::move(shared_image.value()), std::move(sync_token),
            /*is_mappable_si_enabled=*/true, std::move(input->ycbcr_info())));
#else
    return media::mojom::VideoFrameData::NewSharedImageData(
        media::mojom::SharedImageVideoFrameData::New(
            std::move(shared_image.value()), std::move(sync_token),
            /*is_mappable_si_enabled=*/true));
#endif
#endif
  }

  if (input->HasSharedImage()) {
    // Mappable SI should only be used with `HasMappableGpuBuffer()`
    // VideoFrames.
    CHECK(!is_mappable_si_enabled);
    gpu::ExportedSharedImage shared_image = input->shared_image()->Export();
#if BUILDFLAG(IS_ANDROID)
    return media::mojom::VideoFrameData::NewSharedImageData(
        media::mojom::SharedImageVideoFrameData::New(
            std::move(shared_image), input->acquire_sync_token(),
            /*is_mappable_si_enabled=*/false, std::move(input->ycbcr_info())));
#else
    return media::mojom::VideoFrameData::NewSharedImageData(
        media::mojom::SharedImageVideoFrameData::New(
            std::move(shared_image), input->acquire_sync_token(),
            /*is_mappable_si_enabled=*/false));
#endif
  }

  if (input->storage_type() == media::VideoFrame::STORAGE_OPAQUE) {
    return media::mojom::VideoFrameData::NewOpaqueData(
        media::mojom::OpaqueVideoFrameData::New());
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (input->storage_type() == media::VideoFrame::STORAGE_DMABUFS) {
    // Duplicates the DMA buffer FDs to a new vector since this cannot take
    // ownership of the FDs in |input| due to constness.
    std::vector<mojo::PlatformHandle> duped_fds;
    const size_t num_fds = input->NumDmabufFds();
    duped_fds.reserve(num_fds);
    for (size_t i = 0; i < num_fds; ++i) {
      duped_fds.emplace_back(
          base::ScopedFD(HANDLE_EINTR(dup(input->GetDmabufFd(i)))));
    }

    std::vector<media::mojom::ColorPlaneLayoutPtr> planes;
    for (const auto& plane : input->layout().planes()) {
      planes.emplace_back(media::mojom::ColorPlaneLayout::New(
          plane.stride, plane.offset, plane.size));
    }

    return media::mojom::VideoFrameData::NewDmabufData(
        media::mojom::DmabufVideoFrameData::New(
            std::move(planes), input->layout().is_multi_planar(),
            input->layout().buffer_addr_align(), input->layout().modifier(),
            std::move(duped_fds)));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  NOTREACHED() << "Unsupported VideoFrame conversion";
}

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
bool StructTraits<
    media::mojom::ColorPlaneLayoutDataView,
    media::ColorPlaneLayout>::Read(media::mojom::ColorPlaneLayoutDataView data,
                                   media::ColorPlaneLayout* out) {
  if (!base::IsValueInRangeForNumericType<size_t>(data.stride())) {
    return false;
  }
  out->stride = data.stride();
  if (!base::IsValueInRangeForNumericType<size_t>(data.offset())) {
    return false;
  }
  out->offset = base::checked_cast<size_t>(data.offset());
  if (!base::IsValueInRangeForNumericType<size_t>(data.size())) {
    return false;
  }
  out->size = base::checked_cast<size_t>(data.size());
  return true;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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

    auto mapped_region = mapping.GetMemoryAsSpan<uint8_t>();
    std::array<base::span<const uint8_t>, 3> plane_data;
    std::vector<media::ColorPlaneLayout> planes(num_planes);
    for (size_t i = 0; i < num_planes; i++) {
      if (offsets[i] > mapped_region.size()) {
        DLOG(ERROR) << "Plane's offset is out of bounds. "
                    << " offset: " << offsets[i]
                    << " size: " << mapped_region.size();
        return false;
      }

      planes[i].stride = strides[i];
      planes[i].offset = base::strict_cast<size_t>(offsets[i]);
      const size_t space_till_mapping_end = mapping.size() - offsets[i];
      const size_t calculated_plane_size =
          media::VideoFrame::Rows(i, format, coded_size.height()) * strides[i];

      // TODO(crbug.com/378046071) For H.264 content Widevine outputs planes
      // in IMC4 pixel format. Since Y and V planes in IMC4 overlap,
      // the distance to the next plane can't be used to determent the size of
      // the current plane.
      planes[i].size = std::min(calculated_plane_size, space_till_mapping_end);
      plane_data[i] = mapped_region.subspan(offsets[i], planes[i].size);
    }

    auto layout = media::VideoFrameLayout::CreateWithPlanes(format, coded_size,
                                                            std::move(planes));
    if (!layout || !layout->FitsInContiguousBufferOfSize(mapping.size())) {
      DLOG(ERROR) << "Invalid layout";
      return false;
    }

    if (media::IsYuvPlanar(format) && media::IsOpaque(format)) {
      frame = media::VideoFrame::WrapExternalYuvDataWithLayout(
          *layout, visible_rect, natural_size, plane_data[0], plane_data[1],
          plane_data[2], timestamp);
    } else if (media::IsRGB(format)) {
      frame = media::VideoFrame::WrapExternalDataWithLayout(
          *layout, visible_rect, natural_size, plane_data[0], timestamp);
    } else {
      DLOG(ERROR) << "Format is not opaque YUV or RGB: "
                  << VideoPixelFormatToString(format);
      return false;
    }
    if (frame) {
      frame->BackWithOwnedSharedMemory(std::move(region), std::move(mapping));
    }
  } else if (data.is_gpu_memory_buffer_shared_image_data()) {
#if BUILDFLAG(IS_CHROMEOS)
    media::mojom::GpuMemoryBufferSharedImageVideoFrameDataDataView
        gpu_memory_buffer_data;
    data.GetGpuMemoryBufferSharedImageDataDataView(&gpu_memory_buffer_data);

    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    if (!gpu_memory_buffer_data.ReadGpuMemoryBufferHandle(
            &gpu_memory_buffer_handle)) {
      DLOG(ERROR) << "Failed to read GpuMemoryBufferHandle";
      return false;
    }

    std::optional<viz::SharedImageFormat> si_format =
        VideoPixelFormatToSharedImageFormat(format);
    if (!si_format || !viz::HasEquivalentBufferFormat(*si_format)) {
      return false;
    }

    if (gpu_memory_buffer_handle.type !=
        gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
      return false;
    }

    gfx::BufferUsage buffer_usage;
    if (metadata.protected_video) {
      buffer_usage = gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE;
    } else {
      buffer_usage = gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
    }

    auto client_native_pixmap_factory =
        ui::CreateClientNativePixmapFactoryOzone();
    frame = media::VideoFrame::WrapExternalGpuMemoryBufferHandle(
        visible_rect, natural_size, client_native_pixmap_factory.get(),
        std::move(gpu_memory_buffer_handle), coded_size, *si_format,
        buffer_usage, timestamp);
#else
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else if (data.is_shared_image_data()) {
    media::mojom::SharedImageVideoFrameDataDataView shared_image_data;
    data.GetSharedImageDataDataView(&shared_image_data);

    gpu::ExportedSharedImage exported_shared_image;
    if (!shared_image_data.ReadSharedImage(&exported_shared_image)) {
      return false;
    }
    scoped_refptr<gpu::ClientSharedImage> shared_image =
        gpu::ClientSharedImage::ImportUnowned(std::move(exported_shared_image));

    gpu::SyncToken sync_token;
    if (!shared_image_data.ReadSyncToken(&sync_token)) {
      return false;
    }

    bool is_mappable_si_enabled = shared_image_data.is_mappable_si_enabled();
    if (is_mappable_si_enabled) {
      // VideoFrame should have buffer usage if Mappable SharedImage is enabled.
      // NOTE: This isn't exactly correct for software SharedImages can be
      // mappable but do not have buffer usage. But since, such software
      // SharedImages are not used with VideoFrames this should work.
      if (!shared_image->buffer_usage().has_value()) {
        return false;
      }
      frame = media::VideoFrame::WrapMappableSharedImage(
          shared_image, sync_token, media::VideoFrame::ReleaseMailboxCB(),
          visible_rect, natural_size, timestamp);
    } else {
      frame = media::VideoFrame::WrapSharedImage(
          format, shared_image, sync_token,
          media::VideoFrame::ReleaseMailboxCB(), coded_size, visible_rect,
          natural_size, timestamp);
    }

#if BUILDFLAG(IS_ANDROID)
    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
    if (!shared_image_data.ReadYcbcrData(&ycbcr_info)) {
      return false;
    }
    frame->set_ycbcr_info(ycbcr_info);
#endif
  } else if (data.is_opaque_data()) {
    DCHECK(metadata.tracking_token.has_value());
    frame = media::VideoFrame::WrapTrackingToken(
        format, *metadata.tracking_token, coded_size, visible_rect,
        natural_size, timestamp);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  } else if (data.is_dmabuf_data()) {
    media::mojom::DmabufVideoFrameDataDataView dmabuf_data;
    data.GetDmabufDataDataView(&dmabuf_data);

    if (!media::VideoFrame::IsValidCodedSize(coded_size)) {
      DLOG(ERROR) << "Coded size is beyond allowed dimensions: "
                  << coded_size.ToString();
      return false;
    }

    // This format list suffices for supporting the ChromeOS OOPVideoDecoder. If
    // other formats are needed in the future, they may be added.
    if (format != media::PIXEL_FORMAT_I420 &&
        format != media::PIXEL_FORMAT_YV12 &&
        format != media::PIXEL_FORMAT_NV12 &&
        format != media::PIXEL_FORMAT_P010LE &&
        format != media::PIXEL_FORMAT_ARGB) {
      DLOG(ERROR) << "Unsupported: " << format;
      return false;
    }

    mojo::ArrayDataView<mojo::PlatformHandle> fds;
    dmabuf_data.GetFdsDataView(&fds);

    // Note that the number of FDs may be less than the number of color layout
    // planes. This happens when the data multiple planes are stored in a single
    // continuous DMA buffer.
    if (fds.size() == 0 || fds.size() > media::VideoFrame::NumPlanes(format)) {
      DLOG(ERROR) << "Frame has invalid number of FDs: " << fds.size();
      return false;
    }

    std::vector<base::ScopedFD> scoped_fds;
    scoped_fds.reserve(fds.size());
    for (size_t i = 0; i < fds.size(); ++i) {
      scoped_fds.emplace_back(fds.Take(i).TakeFD());
    }

    // In order to reconstruct the video frame, this needs to build a
    // VideoFrameLayout and a vector of base::ScopedFDs.
    std::vector<media::ColorPlaneLayout> planes;
    if (!dmabuf_data.ReadPlanes(&planes)) {
      DLOG(ERROR) << "Invalid planes";
      return false;
    }

    const size_t num_planes = planes.size();
    if (num_planes != media::VideoFrame::NumPlanes(format)) {
      DLOG(ERROR) << "Invalid number of planes (" << num_planes
                  << ") for format " << format;
      return false;
    }

    if (scoped_fds.size() > num_planes) {
      DLOG(ERROR) << "Unexpected number of FDs";
      return false;
    }

    // Checks that strides monotonically decrease.
    for (size_t i = 1; i < num_planes; i++) {
      if (planes[i - 1].stride < planes[i].stride) {
        DLOG(ERROR) << "Strides do not monotonically decrease";
        return false;
      }
    }

    for (size_t i = 0; i < num_planes; i++) {
      // Gets the size of the DMA buffer referenced by the FD. The DMA buffer's
      // size is invariant for its lifetime, so getting its size once suffices
      // for the lifetime of the ScopedFD. If there are more color planes than
      // FDs, this reuses the last FD for planes beyond the last FD index.
      const size_t scoped_fds_index = std::min(i, scoped_fds.size() - 1);
      size_t dmabuf_size = 0;
      // This checks the validity of the FD.
      if (!media::GetFileSize(scoped_fds[scoped_fds_index].get(),
                              &dmabuf_size)) {
        DLOG(ERROR) << "Failed to get the FD size";
        return false;
      }

      const size_t plane_height =
          media::VideoFrame::Rows(i, format, coded_size.height());
      base::CheckedNumeric<size_t> min_plane_size = base::CheckMul(
          base::strict_cast<size_t>(planes[i].stride), plane_height);
      if (!min_plane_size.IsValid<uint64_t>() ||
          min_plane_size.ValueOrDie<uint64_t>() > planes[i].size) {
        DLOG(ERROR) << "Invalid plane size at index " << i;
        return false;
      }

      size_t plane_pixel_width =
          media::VideoFrame::RowBytes(i, format, coded_size.width());
      // If this is a tiled, protected 10bpp MTK format, then
      // VideoFrame::RowBytes() produces the wrong stride. This fixes
      // |plane_pixel_width| for that picture type.
      if (metadata.protected_video && metadata.needs_detiling &&
          format == media::PIXEL_FORMAT_P010LE) {
        constexpr int kMT2TBppNumerator = 5;
        constexpr int kMT2TBppDenominator = 4;
        base::CheckedNumeric<size_t> stride = coded_size.width();
        stride *= kMT2TBppNumerator;
        stride /= kMT2TBppDenominator;
        if (!stride.IsValid()) {
          DLOG(ERROR) << "Failed to compute MT2T stride at index " << i;
          return false;
        }
        plane_pixel_width = stride.ValueOrDie<size_t>();
      }

      if (base::strict_cast<size_t>(planes[i].stride) < plane_pixel_width) {
        DLOG(ERROR) << "Invalid plane stride at index " << i;
        return false;
      }

      // Ensures the plane fits within the DMA buffer. |min_dmabuf_size| is the
      // computed minimum size needed to contain the plane.
      size_t min_dmabuf_size;
      if (!base::CheckAdd(planes[i].offset, planes[i].size)
               .AssignIfValid(&min_dmabuf_size)) {
        DLOG(ERROR) << "Invalid plane offset and size at index " << i;
        return false;
      }
      if (min_dmabuf_size > dmabuf_size) {
        DLOG(ERROR) << "Plane at index " << i
                    << " would reference out of bounds data in the DMA Buffer";
        return false;
      }
    }

    if (!base::IsValueInRangeForNumericType<size_t>(
            dmabuf_data.buffer_addr_align())) {
      DLOG(ERROR) << "Invalid buffer_addr_align";
      return false;
    }
    const size_t buffer_addr_align =
        base::checked_cast<size_t>(dmabuf_data.buffer_addr_align());

    std::optional<media::VideoFrameLayout> layout;
    if (dmabuf_data.is_multi_planar()) {
      layout = media::VideoFrameLayout::CreateMultiPlanar(
          format, coded_size, std::move(planes), buffer_addr_align,
          dmabuf_data.modifier());
    } else {
      layout = media::VideoFrameLayout::CreateWithPlanes(
          format, coded_size, std::move(planes), buffer_addr_align,
          dmabuf_data.modifier());
    }
    if (!layout) {
      DLOG(ERROR) << "Invalid layout";
      return false;
    }

    frame = media::VideoFrame::WrapExternalDmabufs(
        *layout, visible_rect, natural_size, std::move(scoped_fds), timestamp);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

  *output = std::move(frame);
  return true;
}

}  // namespace mojo
