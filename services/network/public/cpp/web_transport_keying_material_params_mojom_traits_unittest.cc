// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_keying_material_params_mojom_traits.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

network::mojom::WebTransportKeyingMaterialParamsPtr
MakeInput(size_t label_length, size_t context_length, uint32_t output_length) {
  return network::mojom::WebTransportKeyingMaterialParams::New(
      std::vector<uint8_t>(label_length, 1),
      std::vector<uint8_t>(context_length, 2), output_length);
}

bool RoundTrip(network::mojom::WebTransportKeyingMaterialParamsPtr input,
               network::WebTransportKeyingMaterialParams& output) {
  return mojo::test::SerializeAndDeserialize<
      network::mojom::WebTransportKeyingMaterialParams>(input, output);
}

TEST(WebTransportKeyingMaterialParamsTraitsTest, ValidParamsRoundTrip) {
  network::WebTransportKeyingMaterialParams output;
  ASSERT_TRUE(RoundTrip(MakeInput(3, 2, 32), output));
  EXPECT_EQ(output.label, std::vector<uint8_t>({1, 1, 1}));
  EXPECT_EQ(output.context, std::vector<uint8_t>({2, 2}));
  EXPECT_EQ(output.output_length, 32u);
}

TEST(WebTransportKeyingMaterialParamsTraitsTest, RejectsOversizedLabel) {
  network::WebTransportKeyingMaterialParams output;
  EXPECT_FALSE(RoundTrip(
      MakeInput(network::mojom::kWebTransportExporterMaxInputLength + 1, 0, 1),
      output));
}

TEST(WebTransportKeyingMaterialParamsTraitsTest, RejectsOversizedContext) {
  network::WebTransportKeyingMaterialParams output;
  EXPECT_FALSE(RoundTrip(
      MakeInput(0, network::mojom::kWebTransportExporterMaxInputLength + 1, 1),
      output));
}

TEST(WebTransportKeyingMaterialParamsTraitsTest, RejectsZeroOutputLength) {
  network::WebTransportKeyingMaterialParams output;
  EXPECT_FALSE(RoundTrip(MakeInput(0, 0, 0), output));
}

TEST(WebTransportKeyingMaterialParamsTraitsTest, RejectsOversizedOutputLength) {
  network::WebTransportKeyingMaterialParams output;
  EXPECT_FALSE(RoundTrip(
      MakeInput(0, 0, network::mojom::kWebTransportExporterMaxOutputLength + 1),
      output));
}

}  // namespace
}  // namespace mojo
