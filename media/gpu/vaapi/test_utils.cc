// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test_utils.h"

#include <sys/mman.h>

#include <array>
#include <memory>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/base/video_types.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "media/gpu/test/test_gbm_buffer_manager.h"
#endif

namespace media {
namespace vaapi_test_utils {

namespace {

// Credit to the Mesa project for writing extensive documentation on the Tile4
// format. https://docs.mesa3d.org/isl/tiling.html#tile4
//
// Tile4 has 2 levels of tiling. The "main" tiles are 8x8 squares of subtiles.
// Subtiles are 16x4 rectangles of pixels. Pixels within subtiles are laid out
// in raster order. Subtiles within tiles are laid out in a repeating "Z"
// pattern. The "Z"s are 4 subtiles wide and 2 subtiles tall. Main tiles within
// a plane are laid out in raster order.
//
// Subtiles are conveniently the size of one cache line, and tiles are
// conveniently the size of one (4K) page.
//
// This detiling algorithm prioritizes linear writes at the expense of very
// non-linear reads so that we take advantage of the write combiner. One low
// hanging fruit optimization might be to experiment with prefetching to help
// with the unusual memory read pattern.

constexpr size_t kTile4TileWidth = 8;
constexpr size_t kTile4TileHeight = 8;
constexpr size_t kTile4SubtileWidth = 16;
constexpr size_t kTile4SubtileHeight = 4;
constexpr size_t kTile4SubtileSizeBytes =
    kTile4SubtileWidth * kTile4SubtileHeight;
constexpr size_t kTile4TileWidthBytes = kTile4TileWidth * kTile4SubtileWidth;
constexpr size_t kTile4TileHeightBytes = kTile4TileHeight * kTile4SubtileHeight;

}  // namespace

void Detile4(base::span<uint8_t> linear_dest,
             base::span<const uint8_t> tiled_src,
             size_t width,
             size_t height) {
  constexpr size_t kTile4TileSizeBytes =
      kTile4TileWidthBytes * kTile4TileHeightBytes;
  constexpr size_t kTile4ZWidth = 4;

  width = base::bits::AlignDown(width, kTile4TileWidthBytes);
  height = base::bits::AlignDown(height, kTile4TileHeightBytes);

  base::SpanWriter<uint8_t> dest(linear_dest);
  base::SpanReader<const uint8_t> src(tiled_src);

  for (size_t y = 0; y < height; y += kTile4TileHeight * kTile4SubtileHeight) {
    for (size_t tile_y = 0; tile_y < kTile4TileHeight; tile_y++) {
      for (size_t subtile_y = 0; subtile_y < kTile4SubtileHeight; subtile_y++) {
        base::SpanReader<const uint8_t> row = src;
        for (size_t x = 0; x < width;
             x += kTile4TileWidth * kTile4SubtileWidth) {
          size_t tile_x = 0;
          // Copy 1 row from 4 subtiles.
          for (; tile_x < kTile4ZWidth; tile_x++) {
            dest.Write(*row.Read<kTile4SubtileWidth>());
            row.Skip(kTile4SubtileSizeBytes - kTile4SubtileWidth);
          }
          row.Skip(kTile4ZWidth * kTile4SubtileSizeBytes);
          // Copy 1 row from another 4 subtiles.
          for (; tile_x < kTile4TileWidth; tile_x++) {
            dest.Write(*row.Read<kTile4SubtileWidth>());
            row.Skip(kTile4SubtileSizeBytes - kTile4SubtileWidth);
          }

          // Advance to the tile to the right.
          row.Skip(kTile4TileSizeBytes -
                   (3 * kTile4ZWidth * kTile4SubtileSizeBytes));
        }

        // Advance to the next row in the subtile.
        src.Skip(kTile4SubtileWidth);
      }

      // Advance to the next row in the tile.
      if (tile_y % 2 == 0) {
        src.Skip(kTile4ZWidth * kTile4SubtileSizeBytes -
                 kTile4SubtileSizeBytes);
      } else {
        src.Skip(3 * kTile4ZWidth * kTile4SubtileSizeBytes -
                 kTile4SubtileSizeBytes);
      }
    }

    // Advance to the tile below.
    src.Skip(width * kTile4TileHeight * kTile4SubtileHeight -
             kTile4TileSizeBytes);
  }
}

DecodedImage::DecodedImage() = default;
DecodedImage::DecodedImage(const DecodedImage&) = default;
DecodedImage& DecodedImage::operator=(const DecodedImage&) = default;
DecodedImage::~DecodedImage() = default;

std::string TestParamToString(
    const testing::TestParamInfo<TestParam>& param_info) {
  return param_info.param.test_name;
}

DecodedImage ScopedVAImageToDecodedImage(const ScopedVAImage* scoped_va_image) {
  DecodedImage decoded_image{};

  decoded_image.fourcc = scoped_va_image->image()->format.fourcc;
  decoded_image.number_of_planes = scoped_va_image->image()->num_planes;
  decoded_image.size =
      gfx::Size(base::strict_cast<int>(scoped_va_image->image()->width),
                base::strict_cast<int>(scoped_va_image->image()->height));

  DCHECK_LE(base::strict_cast<size_t>(decoded_image.number_of_planes),
            kMaxNumberPlanes);

  // This is safe because |number_of_planes| is retrieved from the VA-API and it
  // can not be greater than 3, which is also the size of the |planes| array.
  const auto offsets = base::span(scoped_va_image->image()->offsets);
  const auto pitches = base::span(scoped_va_image->image()->pitches);
  for (uint32_t i = 0u; i < decoded_image.number_of_planes; ++i) {
    // SAFETY: `va_buffer()->data()` points to the mapped VAImage buffer, and
    // `offsets[i]` is the byte offset to plane `i` provided by VA-API.
    decoded_image.planes[i].data = UNSAFE_BUFFERS(
        static_cast<uint8_t*>(scoped_va_image->va_buffer()->data()) +
        offsets[i]);
    decoded_image.planes[i].stride = base::checked_cast<int>(pitches[i]);
  }

  return decoded_image;
}

#if BUILDFLAG(IS_CHROMEOS)

class NativePixmapMapping {
 public:
  virtual ~NativePixmapMapping() = default;
  virtual raw_ptr<uint8_t> GetData(size_t plane_idx) const = 0;
  virtual int GetStride(size_t plane_idx) const = 0;
  virtual gfx::Size GetSize() const = 0;
};

class GbmBufferMapping : public NativePixmapMapping {
 public:
  static std::unique_ptr<GbmBufferMapping> CreateGbmBufferMapping(
      gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format) {
    std::unique_ptr<TestGbmBufferManager> gbm_buffer_manager =
        std::make_unique<TestGbmBufferManager>();
    std::unique_ptr<TestGbmBuffer> gbm_buffer =
        gbm_buffer_manager->ImportDmaBuf(handle, size, format);

    if (!gbm_buffer->Map()) {
      LOG(ERROR) << "Failed to map GBM buffer!";
      return nullptr;
    }

    return std::make_unique<GbmBufferMapping>(std::move(gbm_buffer_manager),
                                              std::move(gbm_buffer));
  }

  GbmBufferMapping(std::unique_ptr<TestGbmBufferManager> gbm_buffer_manager,
                   std::unique_ptr<TestGbmBuffer> gbm_buffer)
      : gbm_buffer_manager_(std::move(gbm_buffer_manager)),
        gbm_buffer_(std::move(gbm_buffer)) {}

  ~GbmBufferMapping() override { gbm_buffer_->Unmap(); }

  raw_ptr<uint8_t> GetData(size_t plane_idx) const override {
    return static_cast<uint8_t*>(gbm_buffer_->memory(plane_idx));
  }

  int GetStride(size_t plane_idx) const override {
    return gbm_buffer_->stride(plane_idx);
  }

  gfx::Size GetSize() const override { return gbm_buffer_->GetSize(); }

 private:
  // It's very important these two objects are initialized in this order,
  // because C++ guarantees they will be destroyed in the reverse order.
  // Unfortunately, the destructor for TestGbmBuffer calls the GBM
  // device that gets destroyed by the TestGbmBufferManager destructor,
  // so there is an order we need to do this in to prevent a segfault.
  const std::unique_ptr<TestGbmBufferManager> gbm_buffer_manager_;
  const std::unique_ptr<TestGbmBuffer> gbm_buffer_;
};

class Tile4Mapping : public NativePixmapMapping {
 public:
  static std::unique_ptr<Tile4Mapping> CreateTile4Mapping(
      gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format) {
    std::array<size_t, 2> plane_strides;
    std::array<size_t, 2> plane_sizes;
    std::array<raw_ptr<uint8_t>, 2> plane_addrs;

    int aligned_width = base::bits::AlignUp(
        handle.planes[0].stride, static_cast<uint32_t>(kTile4TileWidthBytes));
    int aligned_height =
        base::bits::AlignUp(base::checked_cast<uint32_t>(size.height()),
                            uint32_t{kTile4TileHeightBytes});
    plane_strides[0] = aligned_width;
    plane_sizes[0] = aligned_height * aligned_width;

    aligned_width = base::bits::AlignUp(
        handle.planes[1].stride, static_cast<uint32_t>(kTile4TileWidthBytes));
    aligned_height =
        base::bits::AlignUp(base::checked_cast<uint32_t>(size.height() / 2),
                            uint32_t{kTile4TileHeightBytes});
    plane_strides[1] = aligned_width;
    plane_sizes[1] = aligned_height * aligned_width;

    // minigbm doesn't support Tile4 mappings, so we tell it to perform the
    // mapping as if the buffer were linear to work around this limitation.
    CHECK_EQ(handle.modifier, I915_FORMAT_MOD_4_TILED);
    handle.modifier = gfx::NativePixmapHandle::kNoModifier;

    TestGbmBufferManager gbm_buffer_manager;
    std::unique_ptr<TestGbmBuffer> gbm_buffer =
        gbm_buffer_manager.ImportDmaBuf(handle, size, format);

    if (!gbm_buffer->Map()) {
      LOG(ERROR) << "Failed to map GBM buffer!";
      return nullptr;
    }

    CHECK_EQ(handle.planes.size(), 2u);
    for (size_t plane_idx = 0; plane_idx < handle.planes.size(); plane_idx++) {
      size_t width = plane_strides[plane_idx];
      size_t height = plane_sizes[plane_idx] / width;
      size_t plane_size = plane_sizes[plane_idx];
      const uint8_t* src = static_cast<uint8_t*>(gbm_buffer->memory(plane_idx));
      uint8_t* dest = static_cast<uint8_t*>(
          mmap(nullptr, plane_size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
      if (dest == MAP_FAILED) {
        PLOG(ERROR) << "Failed to create detiled mapping!";
        return nullptr;
      }

      // SAFETY: `dest` is mapped via `mmap` with `plane_size` bytes, and `src`
      // is mapped by `gbm_buffer` for plane `plane_idx` with at least
      // `plane_size` bytes.
      Detile4(UNSAFE_BUFFERS(base::span(dest, plane_size)),
              UNSAFE_BUFFERS(base::span(src, plane_size)), width, height);

      plane_addrs[plane_idx] = dest;

      // We don't want to give the user the impression that this mapping is
      // bidirectional. We are performing a one-off detile operation to allow
      // this Tile4 buffer to be read, but we have no way of propagating writes
      // from our temporary linear buffer to the underlying Tile4 buffer. So, we
      // mark these pages as read only.
      if (mprotect(dest, plane_sizes[plane_idx], PROT_READ)) {
        PLOG(ERROR) << "Failed to mark detiled mapping read only!";
        return nullptr;
      }
    }

    gbm_buffer->Unmap();

    handle.modifier = I915_FORMAT_MOD_4_TILED;

    return std::make_unique<Tile4Mapping>(size, plane_strides, plane_sizes,
                                          plane_addrs);
  }

  Tile4Mapping(gfx::Size size,
               std::array<size_t, 2> plane_strides,
               std::array<size_t, 2> plane_sizes,
               std::array<raw_ptr<uint8_t>, 2> plane_addrs)
      : size_(size),
        plane_strides_(plane_strides),
        plane_sizes_(plane_sizes),
        plane_addrs_(plane_addrs) {}

  ~Tile4Mapping() override {
    for (size_t plane_idx = 0; plane_idx < std::size(plane_addrs_);
         plane_idx++) {
      munmap(static_cast<void*>(plane_addrs_[plane_idx]),
             plane_sizes_[plane_idx]);
    }
  }

  raw_ptr<uint8_t> GetData(size_t plane_idx) const override {
    if (plane_idx >= std::size(plane_addrs_)) {
      return nullptr;
    }

    return plane_addrs_[plane_idx];
  }

  int GetStride(size_t plane_idx) const override {
    if (plane_idx >= std::size(plane_strides_)) {
      return -1;
    }

    return plane_strides_[plane_idx];
  }

  gfx::Size GetSize() const override { return size_; }

 private:
  const gfx::Size size_;
  const std::array<size_t, 2> plane_strides_;
  const std::array<size_t, 2> plane_sizes_;
  const std::array<raw_ptr<uint8_t>, 2> plane_addrs_;
};

std::unique_ptr<NativePixmapMapping> CreateNativePixmapMapping(
    gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  if (handle.modifier == I915_FORMAT_MOD_4_TILED) {
    return Tile4Mapping::CreateTile4Mapping(handle, size, format);
  }

  return GbmBufferMapping::CreateGbmBufferMapping(handle, size, format);
}

struct NativePixmapDecodedImage : public DecodedImage {
 public:
  NativePixmapDecodedImage(const uint32_t fourcc,
                           const uint32_t number_of_planes,
                           const gfx::Size& size,
                           std::unique_ptr<NativePixmapMapping> mapping)
      : mapping_(std::move(mapping)) {
    this->fourcc = fourcc;
    this->number_of_planes = number_of_planes;
    this->size = size;

    for (size_t plane_idx = 0; plane_idx < number_of_planes; plane_idx++) {
      planes[plane_idx].data = mapping_->GetData(plane_idx);
      planes[plane_idx].stride = mapping_->GetStride(plane_idx);
    }
  }

 private:
  const std::unique_ptr<NativePixmapMapping> mapping_;
};

std::unique_ptr<DecodedImage> NativePixmapToDecodedImage(
    gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    const viz::SharedImageFormat& format) {
  uint32_t fourcc;
  uint32_t number_of_planes;
  if (format == viz::MultiPlaneFormat::kYV12) {
    fourcc = VA_FOURCC_I420;
    number_of_planes = 3;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    fourcc = VA_FOURCC_NV12;
    number_of_planes = 2;
  } else {
    LOG(ERROR) << "Unsupported format " << format.ToString();
    return nullptr;
  }

  std::unique_ptr<NativePixmapMapping> mapping =
      CreateNativePixmapMapping(handle, size, format);

  if (!mapping) {
    LOG(ERROR) << "Failed to create NativePixmapMapping";
    return nullptr;
  }

  return std::make_unique<NativePixmapDecodedImage>(fourcc, number_of_planes,
                                                    size, std::move(mapping));
}

#endif

bool CompareImages(const DecodedImage& reference_image,
                   const DecodedImage& hw_decoded_image,
                   double min_ssim) {
  if (reference_image.fourcc != VA_FOURCC_I420)
    return false;

  // Uses the reference image's size as the ground truth.
  const gfx::Size image_size = reference_image.size;
  if (image_size != hw_decoded_image.size) {
    LOG(ERROR) << "Wrong expected software decoded image size, "
               << image_size.ToString() << " versus VaAPI provided "
               << hw_decoded_image.size.ToString();
    return false;
  }

  double ssim = 0;
  const uint32_t hw_fourcc = hw_decoded_image.fourcc;
  if (hw_fourcc == VA_FOURCC_I420) {
    ssim = libyuv::I420Ssim(
        reference_image.planes[0].data, reference_image.planes[0].stride,
        reference_image.planes[1].data, reference_image.planes[1].stride,
        reference_image.planes[2].data, reference_image.planes[2].stride,
        hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
        hw_decoded_image.planes[1].data, hw_decoded_image.planes[1].stride,
        hw_decoded_image.planes[2].data, hw_decoded_image.planes[2].stride,
        image_size.width(), image_size.height());
  } else if (hw_fourcc == VA_FOURCC_NV12 || hw_fourcc == VA_FOURCC_YUY2 ||
             hw_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
    // Calculate the stride for the chroma planes.
    const gfx::Size half_image_size((image_size.width() + 1) / 2,
                                    (image_size.height() + 1) / 2);
    // Temporary planes to hold intermediate conversions to I420 (i.e. NV12 to
    // I420 or YUYV/2 to I420).
    auto temp_y = std::make_unique<uint8_t[]>(image_size.GetArea());
    auto temp_u = std::make_unique<uint8_t[]>(half_image_size.GetArea());
    auto temp_v = std::make_unique<uint8_t[]>(half_image_size.GetArea());
    int conversion_result = -1;

    if (hw_fourcc == VA_FOURCC_NV12) {
      conversion_result = libyuv::NV12ToI420(
          hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
          hw_decoded_image.planes[1].data, hw_decoded_image.planes[1].stride,
          temp_y.get(), image_size.width(), temp_u.get(),
          half_image_size.width(), temp_v.get(), half_image_size.width(),
          image_size.width(), image_size.height());
    } else {
      // |hw_fourcc| is YUY2 or YUYV, which are handled the same.
      // TODO(crbug.com/40586948): support other formats/planarities/pitches.
      conversion_result = libyuv::YUY2ToI420(
          hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
          temp_y.get(), image_size.width(), temp_u.get(),
          half_image_size.width(), temp_v.get(), half_image_size.width(),
          image_size.width(), image_size.height());
    }
    if (conversion_result != 0) {
      LOG(ERROR) << "libyuv conversion error";
      return false;
    }

    ssim = libyuv::I420Ssim(
        reference_image.planes[0].data, reference_image.planes[0].stride,
        reference_image.planes[1].data, reference_image.planes[1].stride,
        reference_image.planes[2].data, reference_image.planes[2].stride,
        temp_y.get(), image_size.width(), temp_u.get(), half_image_size.width(),
        temp_v.get(), half_image_size.width(), image_size.width(),
        image_size.height());
  } else {
    LOG(ERROR) << "HW FourCC not supported: " << FourccToString(hw_fourcc);
    return false;
  }

  if (ssim < min_ssim) {
    LOG(ERROR) << "SSIM too low: " << ssim << " < " << min_ssim;
    return false;
  }

  return true;
}

}  // namespace vaapi_test_utils
}  // namespace media
