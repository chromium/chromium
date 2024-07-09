// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"

#include <optional>
#include <set>

#include "base/memory/singleton.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/darkmode/darkmode_classifier.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

// Decision tree lower and upper thresholds for grayscale and color images.
const float kLowColorCountThreshold[2] = {0.8125, 0.015137};
const float kHighColorCountThreshold[2] = {1, 0.025635};

bool IsColorGray(const SkColor& color) {
  return abs(static_cast<int>(SkColorGetR(color)) -
             static_cast<int>(SkColorGetG(color))) +
             abs(static_cast<int>(SkColorGetG(color)) -
                 static_cast<int>(SkColorGetB(color))) <=
         8;
}

bool IsColorTransparent(const SkColor& color) {
  return (SkColorGetA(color) < 128);
}

const int kMaxSampledPixels = 1000;
const int kMaxBlocks = 10;
const float kMinOpaquePixelPercentageForForeground = 0.2;

}  // namespace

DarkModeImageClassifier::DarkModeImageClassifier(
    DarkModeImageClassifierPolicy image_classifier_policy)
    : image_classifier_policy_(image_classifier_policy) {}

DarkModeImageClassifier::~DarkModeImageClassifier() = default;

DarkModeResult DarkModeImageClassifier::Classify(const SkPixmap& pixmap,
                                                 const SkIRect& src) const {
  // Empty pixmap or |src| out of bounds cannot be classified.
  SkIRect bounds = pixmap.bounds();
  if (src.isEmpty() || bounds.isEmpty() || !bounds.contains(src) ||
      !pixmap.addr())
    return DarkModeResult::kDoNotApplyFilter;

  auto features_or_null = GetFeatures(pixmap, src);
  if (!features_or_null)
    return DarkModeResult::kDoNotApplyFilter;

  return ClassifyWithFeatures(features_or_null.value());
}

std::optional<DarkModeImageClassifier::Features>
DarkModeImageClassifier::GetFeatures(const SkPixmap& pixmap,
                                     const SkIRect& src) const {
  DCHECK(!pixmap.bounds().isEmpty());
  float transparency_ratio;
  float background_ratio;
  std::vector<SkColor> sampled_pixels;
  GetSamples(pixmap, src, &sampled_pixels, &transparency_ratio,
             &background_ratio);
  // TODO(https://crbug.com/945434): Investigate why an incorrect resource is
  // loaded and how we can fetch the correct resource. This condition will
  // prevent going further with the rest of the classification logic.
  if (sampled_pixels.size() == 0)
    return std::nullopt;

  return ComputeFeatures(sampled_pixels, transparency_ratio, background_ratio);
}

// Extracts sample pixels from the image. The image is separated into uniformly
// distributed blocks through its width and height, each block is sampled, and
// checked to see if it seems to be background or foreground.
void DarkModeImageClassifier::GetSamples(const SkPixmap& pixmap,
                                         const SkIRect& src,
                                         std::vector<SkColor>* sampled_pixels,
                                         float* transparency_ratio,
                                         float* background_ratio) const {
  DCHECK(!src.isEmpty());

  int num_sampled_pixels =
      std::min(kMaxSampledPixels, src.width() * src.height());
  int num_blocks_x = std::min(kMaxBlocks, src.width());
  int num_blocks_y = std::min(kMaxBlocks, src.height());
  int pixels_per_block = num_sampled_pixels / (num_blocks_x * num_blocks_y);
  int transparent_pixels = 0;
  int opaque_pixels = 0;
  int blocks_count = 0;

  std::vector<int> horizontal_grid(num_blocks_x + 1);
  std::vector<int> vertical_grid(num_blocks_y + 1);

  float block_width = static_cast<float>(src.width()) / num_blocks_x;
  float block_height = static_cast<float>(src.height()) / num_blocks_y;

  for (int block = 0; block <= num_blocks_x; block++) {
    horizontal_grid[block] =
        src.x() + static_cast<int>(round(block_width * block));
  }
  for (int block = 0; block <= num_blocks_y; block++) {
    vertical_grid[block] =
        src.y() + static_cast<int>(round(block_height * block));
  }

  sampled_pixels->clear();
  std::vector<SkIRect> foreground_blocks;

  for (int y = 0; y < num_blocks_y; y++) {
    for (int x = 0; x < num_blocks_x; x++) {
      SkIRect block =
          SkIRect::MakeXYWH(horizontal_grid[x], vertical_grid[y],
                            horizontal_grid[x + 1] - horizontal_grid[x],
                            vertical_grid[y + 1] - vertical_grid[y]);

      std::vector<SkColor> block_samples;
      int block_transparent_pixels;
      GetBlockSamples(pixmap, block, pixels_per_block, &block_samples,
                      &block_transparent_pixels);
      opaque_pixels += static_cast<int>(block_samples.size());
      transparent_pixels += block_transparent_pixels;
      sampled_pixels->insert(sampled_pixels->end(), block_samples.begin(),
                             block_samples.end());
      if (opaque_pixels >
          kMinOpaquePixelPercentageForForeground * pixels_per_block) {
        foreground_blocks.push_back(block);
      }
      blocks_count++;
    }
  }

  *transparency_ratio = static_cast<float>(transparent_pixels) /
                        (transparent_pixels + opaque_pixels);
  *background_ratio =
      1.0 - static_cast<float>(foreground_blocks.size()) / blocks_count;
}

// Selects samples at regular intervals from a block of the image.
// Returns the opaque sampled pixels, and the number of transparent
// sampled pixels.
void DarkModeImageClassifier::GetBlockSamples(
    const SkPixmap& pixmap,
    const SkIRect& block,
    const int required_samples_count,
    std::vector<SkColor>* sampled_pixels,
    int* transparent_pixels_count) const {
  *transparent_pixels_count = 0;

  DCHECK(pixmap.bounds().contains(block));

  sampled_pixels->clear();

  int cx = static_cast<int>(
      ceil(static_cast<float>(block.width()) / sqrt(required_samples_count)));
  int cy = static_cast<int>(
      ceil(static_cast<float>(block.height()) / sqrt(required_samples_count)));

  for (int y = block.y(); y < block.bottom(); y += cy) {
    for (int x = block.x(); x < block.right(); x += cx) {
      SkColor new_sample = pixmap.getColor(x, y);
      if (IsColorTransparent(new_sample))
        (*transparent_pixels_count)++;
      else
        sampled_pixels->push_back(new_sample);
    }
  }
}

DarkModeImageClassifier::Features DarkModeImageClassifier::ComputeFeatures(
    const std::vector<SkColor>& sampled_pixels,
    const float transparency_ratio,
    const float background_ratio) const {
  int samples_count = static_cast<int>(sampled_pixels.size());

  // Is image grayscale.
  int color_pixels = 0;
  for (const SkColor& sample : sampled_pixels) {
    if (!IsColorGray(sample))
      color_pixels++;
  }
  ColorMode color_mode = (color_pixels > samples_count / 100)
                             ? ColorMode::kColor
                             : ColorMode::kGrayscale;

  DarkModeImageClassifier::Features features;
  features.is_colorful = color_mode == ColorMode::kColor;
  features.color_buckets_ratio =
      ComputeColorBucketsRatio(sampled_pixels, color_mode);
  features.transparency_ratio = transparency_ratio;
  features.background_ratio = background_ratio;

  return features;
}

float DarkModeImageClassifier::ComputeColorBucketsRatio(
    const std::vector<SkColor>& sampled_pixels,
    const ColorMode color_mode) const {
  std::set<uint16_t> buckets;

  // If image is in color, use 4 bits per color channel, otherwise 4 bits for
  // illumination.
  if (color_mode == ColorMode::kColor) {
    for (const SkColor& sample : sampled_pixels) {
      uint16_t bucket = ((SkColorGetR(sample) >> 4) << 8) +
                        ((SkColorGetG(sample) >> 4) << 4) +
                        ((SkColorGetB(sample) >> 4));
      buckets.insert(bucket);
    }
  } else {
    for (const SkColor& sample : sampled_pixels) {
      uint16_t illumination =
          (SkColorGetR(sample) * 5 + SkColorGetG(sample) * 3 +
           SkColorGetB(sample) * 2) /
          10;
      buckets.insert(illumination / 16);
    }
  }

  // Using 4 bit per channel representation of each color bucket, there would be
  // 2^4 buckets for grayscale images and 2^12 for color images.
  const float max_buckets[] = {16, 4096};
  return static_cast<float>(buckets.size()) /
         max_buckets[color_mode == ColorMode::kColor];
}

DarkModeResult DarkModeImageClassifier::ClassifyWithFeatures(
    const Features& features) const {
  if (image_classifier_policy_ ==
      DarkModeImageClassifierPolicy::kTransparencyAndNumColors) {
    return (features.transparency_ratio > 0 &&
            features.color_buckets_ratio < static_cast<float>(0.5))
               ? DarkModeResult::kApplyFilter
               : DarkModeResult::kDoNotApplyFilter;
  }

  DCHECK(image_classifier_policy_ ==
         DarkModeImageClassifierPolicy::kNumColorsWithMlFallback);

  DarkModeResult result = ClassifyUsingDecisionTree(features);

  // If decision tree cannot decide, we use a neural network to decide whether
  // to filter or not based on all the features.
  if (result == DarkModeResult::kNotClassified) {
    darkmode_tfnative_model::FixedAllocations nn_temp;
    float nn_out;

    // The neural network expects these features to be in a specific order
    // within float array. Do not change the order here without also changing
    // the neural network code!
    float feature_list[]{
        features.is_colorful ? 1.0f : 0.0f, features.color_buckets_ratio,
        features.transparency_ratio, features.background_ratio};

    darkmode_tfnative_model::Inference(feature_list, &nn_out, &nn_temp);
    result = nn_out > 0 ? DarkModeResult::kApplyFilter
                        : DarkModeResult::kDoNotApplyFilter;
  }

  return result;
}

DarkModeResult DarkModeImageClassifier::ClassifyUsingDecisionTree(
    const DarkModeImageClassifier::Features& features) const {
  float low_color_count_threshold =
      kLowColorCountThreshold[features.is_colorful];
  float high_color_count_threshold =
      kHighColorCountThreshold[features.is_colorful];

  // Very few colors means it's not a photo, apply the filter.
  if (features.color_buckets_ratio < low_color_count_threshold)
    return DarkModeResult::kApplyFilter;

  // Too many colors means it's probably photorealistic, do not apply it.
  if (features.color_buckets_ratio > high_color_count_threshold)
    return DarkModeResult::kDoNotApplyFilter;

  // In-between, decision tree cannot give a precise result.
  return DarkModeResult::kNotClassified;
}

}  // namespace blink
