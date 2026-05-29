// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/public/bnpl_issuer_data.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using BnplIssuerDataTest = PlatformTest;

// Tests property mappings in BnplIssuerData initialized with a linked issuer.
TEST_F(BnplIssuerDataTest, InitializationLinkedIssuer) {
  // Pass an instrument_id to create a linked issuer.
  autofill::BnplIssuer issuer(
      /*instrument_id=*/123456, autofill::BnplIssuer::IssuerId::kBnplKlarna,
      /*eligible_price_ranges=*/{});

  UIImage* mockIcon = [[UIImage alloc] init];
  NSString* optionText = @"Pay in 4 installments with Klarna";

  BnplIssuerData* data = [[BnplIssuerData alloc] initWithBnplIssuer:issuer
                                                selectionOptionText:optionText
                                                               icon:mockIcon];

  EXPECT_EQ(data.issuerId, autofill::BnplIssuer::IssuerId::kBnplKlarna);
  EXPECT_NSEQ(data.issuerName, @"Klarna");
  EXPECT_NSEQ(data.selectionOptionText, optionText);
  EXPECT_EQ(data.icon, mockIcon);
  EXPECT_TRUE(data.isLinked);
}

// Tests property mappings in BnplIssuerData initialized with an unlinked
// issuer.
TEST_F(BnplIssuerDataTest, InitializationUnlinkedIssuer) {
  // Pass std::nullopt to create an unlinked issuer.
  autofill::BnplIssuer issuer(
      /*instrument_id=*/std::nullopt,
      autofill::BnplIssuer::IssuerId::kBnplKlarna,
      /*eligible_price_ranges=*/{});

  UIImage* mockIcon = [[UIImage alloc] init];
  NSString* optionText = @"Pay in 4 installments with Klarna";

  BnplIssuerData* data = [[BnplIssuerData alloc] initWithBnplIssuer:issuer
                                                selectionOptionText:optionText
                                                               icon:mockIcon];

  EXPECT_EQ(data.issuerId, autofill::BnplIssuer::IssuerId::kBnplKlarna);
  EXPECT_NSEQ(data.issuerName, @"Klarna");
  EXPECT_NSEQ(data.selectionOptionText, optionText);
  EXPECT_EQ(data.icon, mockIcon);
  EXPECT_FALSE(data.isLinked);
}
