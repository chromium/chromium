// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/packed_yuv_utils.h"

#include <limits>
#include <vector>

#include "base/check.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 2;

scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format, int width) {
  const gfx::Size size(width, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, size, gfx::Rect(size), size, base::TimeDelta());
  CHECK(frame);
  return frame;
}

base::span<uint8_t> WritableRow(VideoFrame& frame, size_t plane, int row) {
  const size_t stride = base::checked_cast<size_t>(frame.stride(plane));
  return frame.GetWritableVisiblePlaneData(plane).subspan(
      base::checked_cast<size_t>(row) * stride);
}

void SetSample8(VideoFrame& frame,
                size_t plane,
                int index,
                int row,
                uint8_t value) {
  WritableRow(frame, plane, row)[base::checked_cast<size_t>(index)] = value;
}

// Writes the 10-bit |value| at 16-bit |index| of |row|, placing the data in the
// most significant bits as P010/P210/P410 do.
void SetSample10(VideoFrame& frame,
                 size_t plane,
                 int index,
                 int row,
                 uint16_t value) {
  WritableRow(frame, plane, row)
      .subspan(base::checked_cast<size_t>(index) * 2u)
      .first<2u>()
      .copy_from(base::U16ToLittleEndian(static_cast<uint16_t>(value << 6)));
}

uint16_t ReadU16(base::span<const uint8_t> data, int index) {
  return base::U16FromLittleEndian(
      data.subspan(base::checked_cast<size_t>(index) * 2u).first<2u>());
}

uint32_t ReadU32(base::span<const uint8_t> data, int index) {
  return base::U32FromLittleEndian(
      data.subspan(base::checked_cast<size_t>(index) * 4u).first<4u>());
}

void WriteU16(base::span<uint8_t> data, int index, uint16_t value) {
  data.subspan(base::checked_cast<size_t>(index) * 2u)
      .first<2u>()
      .copy_from(base::U16ToLittleEndian(value));
}

void WriteU32(base::span<uint8_t> data, int index, uint32_t value) {
  data.subspan(base::checked_cast<size_t>(index) * 4u)
      .first<4u>()
      .copy_from(base::U32ToLittleEndian(value));
}

// One packer instance shared by the tests, which also exercises the scratch
// reuse across frames of different widths.
DXGIFramePacker& GetPacker() {
  static DXGIFramePacker packer;
  return packer;
}

base::span<const uint8_t> PlaneRow(const VideoFrame& frame,
                                   size_t plane,
                                   int row) {
  const size_t stride = base::checked_cast<size_t>(frame.stride(plane));
  const size_t row_bytes =
      base::checked_cast<size_t>(frame.GetVisibleRowBytes(plane));
  return frame.GetVisiblePlaneData(plane).subspan(
      base::checked_cast<size_t>(row) * stride, row_bytes);
}

// Independent scalar implementation of the three layouts, used to validate the
// libyuv-based packer over widths that reach its vector rows.
std::vector<uint8_t> PackReference(const VideoFrame& frame,
                                   DXGI_FORMAT format,
                                   size_t dst_stride) {
  const int width = frame.visible_rect().width();
  const int height = frame.visible_rect().height();
  std::vector<uint8_t> dst(dst_stride * base::checked_cast<size_t>(height), 0);

  for (int row = 0; row < height; ++row) {
    auto dst_row =
        base::span(dst).subspan(base::checked_cast<size_t>(row) * dst_stride);
    if (format == DXGI_FORMAT_AYUV) {
      auto y = PlaneRow(frame, VideoFrame::Plane::kY, row);
      auto u = PlaneRow(frame, VideoFrame::Plane::kU, row);
      auto v = PlaneRow(frame, VideoFrame::Plane::kV, row);
      for (int x = 0; x < width; ++x) {
        const size_t offset = base::checked_cast<size_t>(x) * 4u;
        dst_row[offset + 0u] = v[base::checked_cast<size_t>(x)];
        dst_row[offset + 1u] = u[base::checked_cast<size_t>(x)];
        dst_row[offset + 2u] = y[base::checked_cast<size_t>(x)];
        dst_row[offset + 3u] = 0xFF;
      }
      continue;
    }

    auto y = PlaneRow(frame, VideoFrame::Plane::kY, row);
    auto uv = PlaneRow(frame, VideoFrame::Plane::kUV, row);
    for (int x = 0; x < width; ++x) {
      if (format == DXGI_FORMAT_Y210) {
        WriteU16(dst_row, x * 2, ReadU16(y, x));
        WriteU16(dst_row, x * 2 + 1, ReadU16(uv, x));
      } else {
        const uint32_t u = ReadU16(uv, x * 2) >> 6;
        const uint32_t luma = ReadU16(y, x) >> 6;
        const uint32_t v = ReadU16(uv, x * 2 + 1) >> 6;
        WriteU32(dst_row, x, u | (luma << 10) | (v << 20) | (0x3u << 30));
      }
    }
  }
  return dst;
}

// Builds a frame over externally owned, tightly packed memory, the way the
// D3D12 upload path wraps a mapped buffer.
scoped_refptr<VideoFrame> WrapExternalFrame(VideoPixelFormat format,
                                            int width,
                                            std::vector<uint8_t>& storage) {
  const gfx::Size size(width, kHeight);
  const gfx::Size y_size =
      VideoFrame::PlaneSize(format, VideoFrame::Plane::kY, size);
  if (VideoFrame::NumPlanes(format) == 2) {
    const gfx::Size uv_size =
        VideoFrame::PlaneSize(format, VideoFrame::Plane::kUV, size);
    storage.assign(y_size.GetArea() + uv_size.GetArea(), 0);
    auto data = base::span(storage);
    return VideoFrame::WrapExternalYuvData(
        format, size, gfx::Rect(size), size, y_size.width(), uv_size.width(),
        data.first(base::checked_cast<size_t>(y_size.GetArea())),
        data.subspan(base::checked_cast<size_t>(y_size.GetArea())),
        base::TimeDelta());
  }
  const gfx::Size u_size =
      VideoFrame::PlaneSize(format, VideoFrame::Plane::kU, size);
  const gfx::Size v_size =
      VideoFrame::PlaneSize(format, VideoFrame::Plane::kV, size);
  const size_t y_bytes = base::checked_cast<size_t>(y_size.GetArea());
  const size_t u_bytes = base::checked_cast<size_t>(u_size.GetArea());
  const size_t v_bytes = base::checked_cast<size_t>(v_size.GetArea());
  storage.assign(y_bytes + u_bytes + v_bytes, 0);
  auto data = base::span(storage);
  return VideoFrame::WrapExternalYuvData(
      format, size, gfx::Rect(size), size, y_size.width(), u_size.width(),
      v_size.width(), data.first(y_bytes), data.subspan(y_bytes, u_bytes),
      data.subspan(y_bytes + u_bytes, v_bytes), base::TimeDelta());
}

void FillDeterministic(VideoFrame& frame) {
  for (size_t plane = 0; plane < VideoFrame::NumPlanes(frame.format());
       ++plane) {
    const size_t stride = base::checked_cast<size_t>(frame.stride(plane));
    const size_t row_bytes =
        base::checked_cast<size_t>(frame.GetVisibleRowBytes(plane));
    const size_t rows = base::checked_cast<size_t>(frame.GetVisibleRows(plane));
    auto data = frame.GetWritableVisiblePlaneData(plane);
    for (size_t row = 0; row < rows; ++row) {
      auto row_span = data.subspan(row * stride, row_bytes);
      for (size_t i = 0; i < row_bytes; ++i) {
        row_span[i] =
            static_cast<uint8_t>(plane * 37u + row * 13u + i * 7u + 1u);
      }
    }
  }
}

}  // namespace

TEST(PackedYuvUtilsTest, SourceFormats) {
  EXPECT_EQ(GetPackedDxgiSourceFormat(DXGI_FORMAT_AYUV), PIXEL_FORMAT_I444);
  EXPECT_EQ(GetPackedDxgiSourceFormat(DXGI_FORMAT_Y210), PIXEL_FORMAT_P210LE);
  EXPECT_EQ(GetPackedDxgiSourceFormat(DXGI_FORMAT_Y410), PIXEL_FORMAT_P410LE);
  EXPECT_EQ(GetPackedDxgiSourceFormat(DXGI_FORMAT_NV12), PIXEL_FORMAT_UNKNOWN);
  EXPECT_EQ(GetPackedDxgiSourceFormat(DXGI_FORMAT_P010), PIXEL_FORMAT_UNKNOWN);
}

TEST(PackedYuvUtilsTest, RowBytes) {
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, 16), 64u);
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_Y210, 16), 64u);
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_Y410, 16), 64u);
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_NV12, 16), 0u);
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, 0), 0u);
  EXPECT_EQ(GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, -1), 0u);
}

// AYUV orders the components V, U, Y, A in ascending byte order.
TEST(PackedYuvUtilsTest, PackAYUV) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  for (int row = 0; row < kHeight; ++row) {
    for (int x = 0; x < kWidth; ++x) {
      const uint8_t sample = static_cast<uint8_t>(row * kWidth + x);
      SetSample8(*frame, VideoFrame::Plane::kY, x, row, sample);
      SetSample8(*frame, VideoFrame::Plane::kU, x, row, sample + 0x40);
      SetSample8(*frame, VideoFrame::Plane::kV, x, row, sample + 0x80);
    }
  }

  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, kWidth);
  std::vector<uint8_t> dst(stride * kHeight);
  ASSERT_TRUE(GetPacker().Pack(*frame, DXGI_FORMAT_AYUV, dst, stride));

  for (int row = 0; row < kHeight; ++row) {
    for (int x = 0; x < kWidth; ++x) {
      SCOPED_TRACE(testing::Message() << "pixel (" << x << ", " << row << ")");
      const uint8_t sample = static_cast<uint8_t>(row * kWidth + x);
      const size_t offset = row * stride + x * 4u;
      EXPECT_EQ(dst[offset + 0u], sample + 0x80);  // V
      EXPECT_EQ(dst[offset + 1u], sample + 0x40);  // U
      EXPECT_EQ(dst[offset + 2u], sample);         // Y
      EXPECT_EQ(dst[offset + 3u], 0xFF);           // A
    }
  }
}

// Y210 stores Y0, U, Y1, V as 16-bit words with the data in the high bits.
TEST(PackedYuvUtilsTest, PackY210) {
  auto frame = CreateFrame(PIXEL_FORMAT_P210LE, kWidth);
  for (int row = 0; row < kHeight; ++row) {
    for (int x = 0; x < kWidth; ++x) {
      SetSample10(*frame, VideoFrame::Plane::kY, x, row,
                  static_cast<uint16_t>(0x100 + x));
    }
    // The UV plane holds one U and one V sample per pixel pair.
    for (int i = 0; i < kWidth / 2; ++i) {
      SetSample10(*frame, VideoFrame::Plane::kUV, i * 2, row,
                  static_cast<uint16_t>(0x200 + i));
      SetSample10(*frame, VideoFrame::Plane::kUV, i * 2 + 1, row,
                  static_cast<uint16_t>(0x300 + i));
    }
  }

  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y210, kWidth);
  std::vector<uint8_t> dst(stride * kHeight);
  ASSERT_TRUE(GetPacker().Pack(*frame, DXGI_FORMAT_Y210, dst, stride));

  for (int row = 0; row < kHeight; ++row) {
    base::span<const uint8_t> dst_row =
        base::span(dst).subspan(row * stride, stride);
    for (int i = 0; i < kWidth / 2; ++i) {
      SCOPED_TRACE(testing::Message() << "macro-pixel " << i << " row " << row);
      EXPECT_EQ(ReadU16(dst_row, i * 4 + 0),
                static_cast<uint16_t>((0x100 + i * 2) << 6));  // Y0
      EXPECT_EQ(ReadU16(dst_row, i * 4 + 1),
                static_cast<uint16_t>((0x200 + i) << 6));  // U
      EXPECT_EQ(ReadU16(dst_row, i * 4 + 2),
                static_cast<uint16_t>((0x100 + i * 2 + 1) << 6));  // Y1
      EXPECT_EQ(ReadU16(dst_row, i * 4 + 3),
                static_cast<uint16_t>((0x300 + i) << 6));  // V
    }
  }
}

// Y410 packs U into bits 0-9, Y into 10-19, V into 20-29 and A into 30-31.
TEST(PackedYuvUtilsTest, PackY410) {
  auto frame = CreateFrame(PIXEL_FORMAT_P410LE, kWidth);
  for (int row = 0; row < kHeight; ++row) {
    for (int x = 0; x < kWidth; ++x) {
      SetSample10(*frame, VideoFrame::Plane::kY, x, row,
                  static_cast<uint16_t>(0x100 + x));
      SetSample10(*frame, VideoFrame::Plane::kUV, x * 2, row,
                  static_cast<uint16_t>(0x200 + x));
      SetSample10(*frame, VideoFrame::Plane::kUV, x * 2 + 1, row,
                  static_cast<uint16_t>(0x300 + x));
    }
  }

  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y410, kWidth);
  std::vector<uint8_t> dst(stride * kHeight);
  ASSERT_TRUE(GetPacker().Pack(*frame, DXGI_FORMAT_Y410, dst, stride));

  for (int row = 0; row < kHeight; ++row) {
    base::span<const uint8_t> dst_row =
        base::span(dst).subspan(row * stride, stride);
    for (int x = 0; x < kWidth; ++x) {
      SCOPED_TRACE(testing::Message() << "pixel (" << x << ", " << row << ")");
      const uint32_t packed = ReadU32(dst_row, x);
      EXPECT_EQ(packed & 0x3FFu, static_cast<uint32_t>(0x200 + x));
      EXPECT_EQ((packed >> 10) & 0x3FFu, static_cast<uint32_t>(0x100 + x));
      EXPECT_EQ((packed >> 20) & 0x3FFu, static_cast<uint32_t>(0x300 + x));
      EXPECT_EQ((packed >> 30) & 0x3u, 0x3u);
    }
  }
}

// The packer owns the P410->Y410 scratch, so packing a wider frame after a
// narrow one grows it and must still work with the same instance.
TEST(PackedYuvUtilsTest, ReusesScratchAcrossY410Packs) {
  auto narrow = CreateFrame(PIXEL_FORMAT_P410LE, kWidth);
  const size_t narrow_stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y410, kWidth);
  std::vector<uint8_t> narrow_dst(narrow_stride * kHeight);
  ASSERT_TRUE(
      GetPacker().Pack(*narrow, DXGI_FORMAT_Y410, narrow_dst, narrow_stride));

  auto wide = CreateFrame(PIXEL_FORMAT_P410LE, 32);
  const size_t wide_stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y410, 32);
  std::vector<uint8_t> wide_dst(wide_stride * kHeight);
  ASSERT_TRUE(GetPacker().Pack(*wide, DXGI_FORMAT_Y410, wide_dst, wide_stride));
}

// Rows are written at |dst_stride| intervals and the padding is left alone.
TEST(PackedYuvUtilsTest, HonorsDestinationStride) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  const size_t row_bytes = GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, kWidth);
  const size_t stride = row_bytes + 8u;
  std::vector<uint8_t> dst(stride * kHeight, 0xAB);
  ASSERT_TRUE(GetPacker().Pack(*frame, DXGI_FORMAT_AYUV, dst, stride));

  for (int row = 0; row < kHeight; ++row) {
    for (size_t i = row_bytes; i < stride; ++i) {
      EXPECT_EQ(dst[row * stride + i], 0xAB) << "row " << row << " byte " << i;
    }
  }
}

TEST(PackedYuvUtilsTest, RejectsMismatchedSourceFormat) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y410, kWidth);
  std::vector<uint8_t> dst(stride * kHeight);
  EXPECT_FALSE(GetPacker().Pack(*frame, DXGI_FORMAT_Y410, dst, stride));
}

TEST(PackedYuvUtilsTest, RejectsUnsupportedDxgiFormat) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  std::vector<uint8_t> dst(kWidth * kHeight * 4u);
  EXPECT_FALSE(GetPacker().Pack(*frame, DXGI_FORMAT_NV12, dst, kWidth * 4u));
}

TEST(PackedYuvUtilsTest, RejectsOddWidthForY210) {
  auto frame = CreateFrame(PIXEL_FORMAT_P210LE, 3);
  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y210, 3);
  std::vector<uint8_t> dst(stride * kHeight);
  EXPECT_FALSE(GetPacker().Pack(*frame, DXGI_FORMAT_Y210, dst, stride));
}

TEST(PackedYuvUtilsTest, RejectsTooSmallDestination) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, kWidth);
  std::vector<uint8_t> dst(stride * kHeight - 1u);
  EXPECT_FALSE(GetPacker().Pack(*frame, DXGI_FORMAT_AYUV, dst, stride));
}

// The row stride cannot be smaller than a row. Encoder-sized frames guarantee
// it, so a violation CHECKs rather than returning false.
TEST(PackedYuvUtilsTest, ChecksStrideSmallerThanRow) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  const size_t row_bytes = GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, kWidth);
  std::vector<uint8_t> dst(row_bytes * kHeight);
  EXPECT_DEATH_IF_SUPPORTED(
      GetPacker().Pack(*frame, DXGI_FORMAT_AYUV, dst, row_bytes - 1u), "");
}

TEST(PackedYuvUtilsTest, PacksFrameWrappingExternalMemory) {
  const struct {
    VideoPixelFormat source;
    DXGI_FORMAT dxgi;
  } kCases[] = {
      {PIXEL_FORMAT_I444, DXGI_FORMAT_AYUV},
      {PIXEL_FORMAT_P210LE, DXGI_FORMAT_Y210},
      {PIXEL_FORMAT_P410LE, DXGI_FORMAT_Y410},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(VideoPixelFormatToString(test_case.source));

    auto owned = CreateFrame(test_case.source, kWidth);
    FillDeterministic(*owned);

    std::vector<uint8_t> storage;
    auto wrapped = WrapExternalFrame(test_case.source, kWidth, storage);
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(wrapped->storage_type(), VideoFrame::STORAGE_UNOWNED_MEMORY);
    EXPECT_TRUE(wrapped->HasDirectCpuAccess());
    FillDeterministic(*wrapped);

    const size_t stride = GetPackedDxgiRowBytes(test_case.dxgi, kWidth);
    std::vector<uint8_t> from_owned(stride * kHeight);
    std::vector<uint8_t> from_wrapped(stride * kHeight);
    ASSERT_TRUE(GetPacker().Pack(*owned, test_case.dxgi, from_owned, stride));
    ASSERT_TRUE(
        GetPacker().Pack(*wrapped, test_case.dxgi, from_wrapped, stride));
    EXPECT_EQ(from_owned, from_wrapped);
  }
}

// The visible rect need not start at the coded origin, so the pack must read
// from the visible offset rather than the start of each plane.
TEST(PackedYuvUtilsTest, PacksCroppedFrame) {
  const struct {
    VideoPixelFormat source;
    DXGI_FORMAT dxgi;
  } kCases[] = {
      {PIXEL_FORMAT_I444, DXGI_FORMAT_AYUV},
      {PIXEL_FORMAT_P210LE, DXGI_FORMAT_Y210},
      {PIXEL_FORMAT_P410LE, DXGI_FORMAT_Y410},
  };

  // An even x origin keeps the crop valid for the 4:2:2 source too.
  const gfx::Size coded_size(kWidth * 4, kHeight * 4);
  const gfx::Size visible_size(kWidth, kHeight);
  const gfx::Rect visible_rect(kWidth, kHeight, kWidth, kHeight);

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(VideoPixelFormatToString(test_case.source));

    auto cropped = VideoFrame::CreateZeroInitializedFrame(
        test_case.source, coded_size, visible_rect, visible_size,
        base::TimeDelta());
    ASSERT_TRUE(cropped);
    FillDeterministic(*cropped);

    auto uncropped = CreateFrame(test_case.source, kWidth);
    FillDeterministic(*uncropped);

    const size_t stride = GetPackedDxgiRowBytes(test_case.dxgi, kWidth);
    std::vector<uint8_t> from_cropped(stride * kHeight);
    std::vector<uint8_t> from_uncropped(stride * kHeight);
    ASSERT_TRUE(
        GetPacker().Pack(*cropped, test_case.dxgi, from_cropped, stride));
    ASSERT_TRUE(
        GetPacker().Pack(*uncropped, test_case.dxgi, from_uncropped, stride));
    EXPECT_EQ(from_cropped, from_uncropped);
  }
}

// The destination row is reinterpreted as uint16_t, so a misaligned row start
// CHECKs in reinterpret_span().
TEST(PackedYuvUtilsTest, ChecksMisalignedDestinationForY210) {
  auto frame = CreateFrame(PIXEL_FORMAT_P210LE, kWidth);
  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_Y210, kWidth);
  std::vector<uint8_t> dst(stride * kHeight + 1u);
  auto misaligned = base::span(dst).subspan(1u);
  ASSERT_NE(reinterpret_cast<uintptr_t>(misaligned.data()) % alignof(uint16_t),
            0u);
  EXPECT_DEATH_IF_SUPPORTED(
      GetPacker().Pack(*frame, DXGI_FORMAT_Y210, misaligned, stride), "");
}

// libyuv takes strides as int, and encoder-sized frames keep the stride well
// within range, so an oversized stride CHECKs rather than returning false.
TEST(PackedYuvUtilsTest, ChecksStrideLargerThanInt) {
  auto frame = CreateFrame(PIXEL_FORMAT_I444, kWidth);
  const size_t stride = GetPackedDxgiRowBytes(DXGI_FORMAT_AYUV, kWidth);
  std::vector<uint8_t> dst(stride * kHeight);
  EXPECT_DEATH_IF_SUPPORTED(
      GetPacker().Pack(
          *frame, DXGI_FORMAT_AYUV, dst,
          static_cast<size_t>(std::numeric_limits<int>::max()) + 1u),
      "");
}

// The other tests use a width below libyuv's block size, which only reaches the
// scratch-padded remainder path. 32 is a multiple of every block size used here
// so the plain vector row runs, and 34 leaves a remainder so the Any_ wrapper
// runs both the vector body and the remainder.
TEST(PackedYuvUtilsTest, MatchesReferenceOverVectorWidths) {
  const struct {
    VideoPixelFormat source;
    DXGI_FORMAT dxgi;
  } kCases[] = {
      {PIXEL_FORMAT_I444, DXGI_FORMAT_AYUV},
      {PIXEL_FORMAT_P210LE, DXGI_FORMAT_Y210},
      {PIXEL_FORMAT_P410LE, DXGI_FORMAT_Y410},
  };

  for (const int width : {32, 34}) {
    for (const auto& test_case : kCases) {
      SCOPED_TRACE(testing::Message()
                   << VideoPixelFormatToString(test_case.source) << " width "
                   << width);

      auto frame = CreateFrame(test_case.source, width);
      FillDeterministic(*frame);

      const size_t stride = GetPackedDxgiRowBytes(test_case.dxgi, width);
      std::vector<uint8_t> actual(stride * kHeight);
      ASSERT_TRUE(GetPacker().Pack(*frame, test_case.dxgi, actual, stride));
      EXPECT_EQ(actual, PackReference(*frame, test_case.dxgi, stride));
    }
  }
}

}  // namespace media
