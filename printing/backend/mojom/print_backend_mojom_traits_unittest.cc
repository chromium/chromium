// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

#if defined(OS_CHROMEOS)
const printing::AdvancedCapability kAdvancedCapability1(
    /*name=*/"advanced_cap_bool",
    /*display_name=*/"Advanced Capability #1 (bool)",
    /*type=*/printing::AdvancedCapability::Type::kBoolean,
    /*default_value=*/"true",
    /*values=*/std::vector<printing::AdvancedCapabilityValue>());
const printing::AdvancedCapability kAdvancedCapability2(
    /*name=*/"advanced_cap_double",
    /*display_name=*/"Advanced Capability #2 (double)",
    /*type=*/printing::AdvancedCapability::Type::kFloat,
    /*default_value=*/"3.14159",
    /*values=*/
    std::vector<printing::AdvancedCapabilityValue>{
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_1",
            /*display_name=*/"Advanced Capability #1"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_2",
            /*display_name=*/"Advanced Capability #2"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_3",
            /*display_name=*/"Advanced Capability #3"),
    });
const printing::AdvancedCapabilities kAdvancedCapabilities{
    kAdvancedCapability1, kAdvancedCapability2};

// Returns true if the advanced capabilities have the equivalent members.
bool AdvancedCapabilitiesEqual(const AdvancedCapability& lhs,
                               const AdvancedCapability& rhs) {
  return lhs.name == rhs.name && lhs.display_name == rhs.display_name &&
         lhs.type == rhs.type && lhs.default_value == rhs.default_value &&
         lhs.values == rhs.values;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePaper) {
  const printing::PrinterSemanticCapsAndDefaults::Papers kPapers{
      kPaperA3, kPaperA4, kPaperLetter, kPaperLedger};

  for (const auto& paper : kPapers) {
    printing::PrinterSemanticCapsAndDefaults::Paper input = paper;
    printing::PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<printing::mojom::Paper>(
        &input, &output));
    EXPECT_EQ(paper, output);
  }
}

#if defined(OS_CHROMEOS)
TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializeAdvancedCapability) {
  for (const auto& advanced_capability : kAdvancedCapabilities) {
    printing::AdvancedCapability input = advanced_capability;
    printing::AdvancedCapability output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                printing::mojom::AdvancedCapability>(&input, &output));
    EXPECT_TRUE(AdvancedCapabilitiesEqual(advanced_capability, output));
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace printing
