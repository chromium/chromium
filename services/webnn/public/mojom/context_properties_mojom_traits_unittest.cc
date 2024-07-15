// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/context_properties_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ContextPropertiesMojomTraitsTest, Basic) {
  auto input = webnn::ContextProperties(
      webnn::InputOperandLayout::kNchw,
      {webnn::SupportedDataTypes::All(),
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       {webnn::OperandDataType::kFloat16},
       {webnn::OperandDataType::kInt32, webnn::OperandDataType::kInt64},
       {webnn::OperandDataType::kUint8},
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32},
       {webnn::OperandDataType::kFloat16, webnn::OperandDataType::kFloat32}});

  webnn::ContextProperties output =
      webnn::ContextProperties(webnn::InputOperandLayout::kNhwc,
                               {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}});
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::ContextProperties>(
          input, output));
  EXPECT_EQ(input, output);
}
