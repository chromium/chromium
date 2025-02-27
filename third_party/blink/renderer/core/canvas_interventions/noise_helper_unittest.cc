// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/test/gtest_util.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {
namespace {

using NoiseHelperTest = testing::Test;

std::vector<uint8_t> GetRandomPixels(uint32_t width, uint32_t height) {
  std::vector<uint8_t> pixels;
  uint32_t num_bytes = width * height * 4;
  pixels.resize(num_bytes);
  auto out = base::span<uint8_t>(pixels);
  auto writer = base::SpanWriter(out);
  // Using FNV as a pseudo-random generator.
  uint64_t seed = 0xcbf29ce484222325;
  for (uint32_t i = 0u; i < num_bytes; i += 8u) {
    writer.WriteU64LittleEndian(seed);
    seed *= 0x00000100000001B3;
  }
  return pixels;
}

TEST_F(NoiseHelperTest, SeedHashFromSite) {
  net::SchemefulSite site1(GURL("https://a.com"));
  net::SchemefulSite site2(GURL("https://b.org"));
  const uint64_t token = 0x01234678901234567;
  auto noise_hash1 = std::make_unique<NoiseHash>(
      token, site1.registrable_domain_or_host_for_testing());
  auto noise_hash2 = std::make_unique<NoiseHash>(
      token, site2.registrable_domain_or_host_for_testing());
  // For the same token but different site, the seeds should differ.
  EXPECT_NE(noise_hash1->GetTokenHashForTesting(),
            noise_hash2->GetTokenHashForTesting());
  // The seed should be the same for the same site and token.
  auto noise_hash1_again = std::make_unique<NoiseHash>(
      token, site1.registrable_domain_or_host_for_testing());
  EXPECT_EQ(noise_hash1->GetTokenHashForTesting(),
            noise_hash1_again->GetTokenHashForTesting());
  const uint64_t token2 = token ^ 0xffffffffffffffff;
  // When the token differs, so should the seed even if the site remains the
  // same.
  auto noise_hash1_different_token = std::make_unique<NoiseHash>(
      token2, site1.registrable_domain_or_host_for_testing());
  EXPECT_NE(noise_hash1->GetTokenHashForTesting(),
            noise_hash1_different_token->GetTokenHashForTesting());
}

TEST_F(NoiseHelperTest, NoisePixels) {
  const uint32_t width = 50u;
  const uint32_t height = 150u;
  std::vector<uint8_t> image_data = GetRandomPixels(width, height);
  base::span pixels(image_data);
  std::vector<uint8_t> image_data_orig;
  image_data_orig.resize(image_data.size());
  base::span<uint8_t> pixels_orig(image_data_orig);
  pixels_orig.copy_from(pixels);
  EXPECT_EQ(pixels, pixels_orig);

  // When noised, the pixels should be perturbed by at most kMaxNoisePerChannel.
  const uint64_t token = 0x01234678901234567;
  const auto token_hash = NoiseHash(token, "https://a.com");
  NoisePixels(token_hash, pixels, width, height);
  EXPECT_NE(pixels, pixels_orig);
  double num_diff = 0;
  for (size_t i = 0; i < pixels.size(); ++i) {
    auto diff = std::max(pixels[i], pixels_orig[i]) -
                std::min(pixels[i], pixels_orig[i]);
    EXPECT_LE(diff, 3);
    if (diff != 0) {
      ++num_diff;
    }
  }
  // On average noise should be added to ~6/7 (=85.71%) channel values; for
  // convenience, ensuring it's higher than 50%.
  double pct_diff = num_diff / pixels.size();
  EXPECT_GT(pct_diff, 0.5);

  // Hashing again with the same token and site should result in the same noise.
  std::vector<uint8_t> image_data2;
  image_data2.resize(image_data_orig.size());
  base::span<uint8_t> pixels2(image_data2);
  pixels2.copy_from(pixels_orig);
  NoisePixels(token_hash, pixels2, width, height);
  EXPECT_EQ(pixels, pixels2);

  // Using a different token hash should result in different noise being added.
  const auto token_hash2 = NoiseHash(token, "https://b.com");
  pixels2.copy_from(pixels_orig);
  NoisePixels(token_hash2, pixels2, width, height);
  EXPECT_NE(pixels, pixels2);
}

}  // namespace
}  // namespace blink
