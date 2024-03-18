// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/authentication/card_unmask_challenge_option_ios.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ChallengeOptionId =
    autofill::CardUnmaskChallengeOption::ChallengeOptionId;

typedef PlatformTest CardUnmaskChallengeOptionIOSTest;

TEST_F(CardUnmaskChallengeOptionIOSTest, ConvertFrom_SetsProperties) {
  autofill::CardUnmaskChallengeOption autofill_option(
      ChallengeOptionId("option_id"),
      autofill::CardUnmaskChallengeOptionType::kEmailOtp,
      /*challenge_info=*/u"with your email, somebody@example.test",
      /*challenge_input_length=*/0);

  CardUnmaskChallengeOptionIOS* ios_option =
      [CardUnmaskChallengeOptionIOS convertFrom:autofill_option
                                      modeLabel:u"mode label"];

  EXPECT_NSEQ([[CardUnmaskChallengeOptionIOS alloc]
                     initWithId:ChallengeOptionId("option_id")
                      modeLabel:@"mode label"
                  challengeInfo:@"with your email, somebody@example.test"],
              ios_option);
}

TEST_F(CardUnmaskChallengeOptionIOSTest, IsEqual_FalseWithNil) {
  EXPECT_FALSE([[[CardUnmaskChallengeOptionIOS alloc]
         initWithId:ChallengeOptionId("option_id")
          modeLabel:@"mode_label"
      challengeInfo:@"challenge_info"] isEqual:nil]);
}

TEST_F(CardUnmaskChallengeOptionIOSTest, IsEqual_True) {
  CardUnmaskChallengeOptionIOS* left = [[CardUnmaskChallengeOptionIOS alloc]
         initWithId:ChallengeOptionId("option_id")
          modeLabel:@"mode_label"
      challengeInfo:@"challenge_info"];
  CardUnmaskChallengeOptionIOS* right = [[CardUnmaskChallengeOptionIOS alloc]
         initWithId:ChallengeOptionId("option_id")
          modeLabel:@"mode_label"
      challengeInfo:@"challenge_info"];
  EXPECT_NSEQ(left, right);
  EXPECT_EQ([left hash], [right hash]);
}

TEST_F(CardUnmaskChallengeOptionIOSTest, IsEqual_FalseOnOptionId) {
  CardUnmaskChallengeOptionIOS* left = [[CardUnmaskChallengeOptionIOS alloc]
         initWithId:ChallengeOptionId("option_id")
          modeLabel:@""
      challengeInfo:@""];
  CardUnmaskChallengeOptionIOS* right = [[CardUnmaskChallengeOptionIOS alloc]
         initWithId:ChallengeOptionId("option_id2")
          modeLabel:@""
      challengeInfo:@""];
  EXPECT_NSNE(left, right);
  EXPECT_NE([left hash], [right hash]);
}

TEST_F(CardUnmaskChallengeOptionIOSTest, IsEqual_FalseOnModeLabel) {
  CardUnmaskChallengeOptionIOS* left =
      [[CardUnmaskChallengeOptionIOS alloc] initWithId:ChallengeOptionId("")
                                             modeLabel:@"mode_label"
                                         challengeInfo:@""];
  CardUnmaskChallengeOptionIOS* right =
      [[CardUnmaskChallengeOptionIOS alloc] initWithId:ChallengeOptionId("")
                                             modeLabel:@"mode_label2"
                                         challengeInfo:@""];
  EXPECT_NSNE(left, right);
  EXPECT_NE([left hash], [right hash]);
}

TEST_F(CardUnmaskChallengeOptionIOSTest, IsEqual_FalseOnChallengeInfo) {
  CardUnmaskChallengeOptionIOS* left =
      [[CardUnmaskChallengeOptionIOS alloc] initWithId:ChallengeOptionId("")
                                             modeLabel:@""
                                         challengeInfo:@"challenge_info"];
  CardUnmaskChallengeOptionIOS* right =
      [[CardUnmaskChallengeOptionIOS alloc] initWithId:ChallengeOptionId("")
                                             modeLabel:@""
                                         challengeInfo:@"challenge_info2"];
  EXPECT_NSNE(left, right);
  EXPECT_NE([left hash], [right hash]);
}
