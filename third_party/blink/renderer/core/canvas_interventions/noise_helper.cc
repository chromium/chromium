// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"

namespace blink {

namespace {

constexpr uint8_t kMaxClosePixelDelta = 10u;
constexpr uint8_t kMaxNoisePerChannel = 3u;
constexpr uint8_t kChannelsPerPixel = 4u;
constexpr std::array<uint8_t, 4u> kEmptyPixel({0, 0, 0, 0});

struct PixelLocation {
  int x;
  int y;
};

void ClampPixelLocation(PixelLocation* location,
                        const int width,
                        const int height) {
  CHECK(width >= 1 && height >= 1);
  location->x = std::min(std::max(0, location->x), width - 1);
  location->y = std::min(std::max(0, location->y), height - 1);
}

// Returns two random pixel locations; one close to the offset and one
// randomly selected from the entire canvas.
const std::pair<PixelLocation, PixelLocation> GetRandomPixelLocations(
    NoiseHash& token_hash,
    const PixelLocation offset,
    const int width,
    const int height) {
  std::pair<PixelLocation, PixelLocation> pixel_locations;
  // Uses 2 * log2(kMaxClosePixelDelta*2+1) = 8 bits from hash.
  pixel_locations.first = {
      offset.x + token_hash.GetValueBelow(kMaxClosePixelDelta * 2) -
          kMaxClosePixelDelta + 1,
      offset.y + token_hash.GetValueBelow(kMaxClosePixelDelta * 2) -
          kMaxClosePixelDelta + 1};
  // x and y might be negative or go beyond the width or height here, so we need
  // to clamp them.
  ClampPixelLocation(&pixel_locations.first, width, height);
  // Uses max 2 * log2(kMaximumCanvasSize) = 40 bits from hash.
  pixel_locations.second = {token_hash.GetValueBelow(width),
                            token_hash.GetValueBelow(height)};
  // Used at most 48 bits from hash
  return pixel_locations;
}

void NoisePixel(base::span<uint8_t> pixel, NoiseHash& token_hash) {
  base::SpanWriter writer(base::as_writable_bytes(pixel));
  for (int i = 0; i < kChannelsPerPixel; ++i) {
    int channel_value = pixel[i];
    // Clamp min- and maxNoisedVal to [0, 255] and [1, 255] for the alpha
    // channel if it was non-zero before.
    int lowerLimit = (i == kChannelsPerPixel - 1 && channel_value > 0) ? 1 : 0;
    int minNoisedVal = channel_value <= kMaxNoisePerChannel
                           ? lowerLimit
                           : channel_value - kMaxNoisePerChannel;
    int maxNoisedVal = channel_value >= 255 - kMaxNoisePerChannel
                           ? 255
                           : channel_value + kMaxNoisePerChannel;
    int noise = token_hash.GetValueBelow(
        std::min(kMaxNoisePerChannel * 2 + 1, maxNoisedVal - minNoisedVal + 1));
    writer.WriteU8LittleEndian(
        base::checked_cast<uint8_t>(minNoisedVal + noise));
  }
}

base::span<uint8_t, 4u> GetPixelAt(const int x,
                                   const int y,
                                   const int width,
                                   const base::span<uint8_t> pixels) {
  return pixels
      .subspan(static_cast<size_t>((x + y * width) * kChannelsPerPixel))
      .first<4u>();
}

uint64_t GetValueFromPixelLocations(
    const std::pair<PixelLocation, PixelLocation> locations,
    const base::span<uint8_t> pixels,
    int width) {
  uint64_t result = 0u;
  result |= base::U32FromLittleEndian(
      GetPixelAt(locations.first.x, locations.first.y, width, pixels));
  result <<= 32u;
  result |= base::U32FromLittleEndian(
      GetPixelAt(locations.second.x, locations.second.y, width, pixels));
  return result;
}

void CopyPixelValue(const base::span<uint8_t> from_pixel,
                    base::span<uint8_t> to_pixel) {
  base::SpanWriter writer(base::as_writable_bytes(to_pixel));
  writer.Write(from_pixel.first<kChannelsPerPixel>());
}

}  // namespace

void NoisePixels(const NoiseHash& token_hash,
                 base::span<uint8_t> pixels,
                 const int width,
                 const int height) {
  CHECK_EQ(pixels.size(),
           static_cast<size_t>(width * height * kChannelsPerPixel));

  const size_t row_size = width * kChannelsPerPixel;
  std::vector<uint8_t> row1(row_size);
  std::vector<uint8_t> row2(row_size);
  base::span<uint8_t> unnoised_previous_row(row1);
  base::span<uint8_t> unnoised_current_row(row2);

  for (int y = 0; y < height; ++y) {
    unnoised_current_row.copy_from(pixels.subspan(y * row_size, row_size));
    for (int x = 0; x < width; ++x) {
      auto pixel = GetPixelAt(x, y, width, pixels);
      if (pixel == kEmptyPixel) {
        continue;
      }
      if (y > 0 && x > 0 &&
          GetPixelAt(x - 1, 0, width, unnoised_previous_row) == pixel) {
        // same top-left pixel, copy.
        CopyPixelValue(GetPixelAt(x - 1, y - 1, width, pixels), pixel);
      } else if (y > 0 &&
                 GetPixelAt(x, 0, width, unnoised_previous_row) == pixel) {
        // same top pixel, copy.
        CopyPixelValue(GetPixelAt(x, y - 1, width, pixels), pixel);
      } else if (y > 0 && x < width - 1 &&
                 GetPixelAt(x + 1, 0, width, unnoised_previous_row) == pixel) {
        // same top-right pixel, copy.
        CopyPixelValue(GetPixelAt(x + 1, y - 1, width, pixels), pixel);
      } else if (x > 0 &&
                 GetPixelAt(x - 1, 0, width, unnoised_current_row) == pixel) {
        // same left pixel, copy.
        CopyPixelValue(GetPixelAt(x - 1, y, width, pixels), pixel);
      } else {
        // otherwise, noise the pixel.
        NoiseHash hash_copy = token_hash;
        hash_copy.Update(base::U32FromLittleEndian(pixel));
        // GetRandomPixelLocations consumes at most 46 bits from hash.
        const std::pair<PixelLocation, PixelLocation> other_pixels =
            GetRandomPixelLocations(hash_copy, {x, y}, width, height);
        hash_copy.Update(
            GetValueFromPixelLocations(other_pixels, pixels, width));
        // NoisePixel consumes 12 bits from hash
        NoisePixel(pixel, hash_copy);
      }
    }
    // Previous can be overwritten (new current). Current is the now previous.
    std::swap(unnoised_previous_row, unnoised_current_row);
  }
}

}  // namespace blink
