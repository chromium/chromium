// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_name_fixer_internal.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVCreditCardNameFixerTest : public TestWithLocaleAndResources {
 public:
  void AcceptNameCallback(const std::u16string& name) {
    accepted_name_ = base::SysUTF16ToNSString(name);
  }
  NSString* accepted_name_;
};

// Tests CWVCreditCardNameFixer properties.
TEST_F(CWVCreditCardNameFixerTest, Properties) {
  NSString* name = @"John Doe";
  CWVCreditCardNameFixer* fixer =
      [[CWVCreditCardNameFixer alloc] initWithName:name
                                          callback:base::DoNothing()];
  EXPECT_NSEQ(name, fixer.inferredCardHolderName);
  EXPECT_TRUE(fixer.inferredCardHolderName);
  EXPECT_TRUE(fixer.cancelButtonLabel);
  EXPECT_TRUE(fixer.inferredNameTooltipText);
  EXPECT_TRUE(fixer.inputLabel);
  EXPECT_TRUE(fixer.inputPlaceholderText);
  EXPECT_TRUE(fixer.saveButtonLabel);
  EXPECT_TRUE(fixer.titleText);
}

// Tests CWVCreditCardNameFixer properly accepts the chosen name.
TEST_F(CWVCreditCardNameFixerTest, AcceptName) {
  NSString* inferred_name = @"John Doe";
  NSString* accepted_name = @"Jane Doe";
  base::OnceCallback<void(const std::u16string&)> callback = base::BindOnce(
      &CWVCreditCardNameFixerTest::AcceptNameCallback, base::Unretained(this));
  CWVCreditCardNameFixer* fixer =
      [[CWVCreditCardNameFixer alloc] initWithName:inferred_name
                                          callback:std::move(callback)];
  [fixer acceptWithName:accepted_name];
  EXPECT_NSEQ(accepted_name, accepted_name_);
}

}  // namespace ios_web_view
