// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/origin_trials.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";
const char kFrobulatePersistentTrialName[] = "FrobulatePersistent";

}  // namespace

TEST(OriginTrialTest, TrialsValid) {
  EXPECT_TRUE(origin_trials::IsTrialValid(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialValid(kFrobulateThirdPartyTrialName));
}

TEST(OriginTrialTest, TrialEnabledForInsecureContext) {
  EXPECT_FALSE(
      origin_trials::IsTrialEnabledForInsecureContext(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialEnabledForInsecureContext(
      kFrobulateDeprecationTrialName));
  EXPECT_FALSE(origin_trials::IsTrialEnabledForInsecureContext(
      kFrobulateThirdPartyTrialName));
}

TEST(OriginTrialTest, TrialsEnabledForThirdPartyOrigins) {
  EXPECT_FALSE(
      origin_trials::IsTrialEnabledForThirdPartyOrigins(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialEnabledForThirdPartyOrigins(
      kFrobulateThirdPartyTrialName));
}

TEST(OriginTrialTest, TrialIsPersistent) {
  EXPECT_FALSE(
      origin_trials::IsTrialPersistentToNextResponse(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialPersistentToNextResponse(
      kFrobulatePersistentTrialName));
}

}  // namespace blink
