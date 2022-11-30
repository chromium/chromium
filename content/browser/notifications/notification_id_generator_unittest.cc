// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "content/browser/notifications/notification_id_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const int64_t kPersistentNotificationId = 430;
const char kExampleTag[] = "example";

class NotificationIdGeneratorTest : public ::testing::Test {
 public:
  NotificationIdGeneratorTest()
      : origin_(url::Origin::Create(GURL("https://example.com"))) {}

 protected:
  url::Origin origin_;
  NotificationIdGenerator generator_;
};

// -----------------------------------------------------------------------------
// Persistent notifications

// Two calls to the generator with exactly the same information should result
// in exactly the same notification ids being generated.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_IsDetermenistic) {
  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, false /* is_shown_by_browser */,
                kPersistentNotificationId));

  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
                kPersistentNotificationId));
}

// The origin of the notification will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_DifferentOrigins) {
  url::Origin different_origin(
      url::Origin::Create(GURL("https://example2.com")));

  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                different_origin.GetURL(), kExampleTag,
                false /* is_shown_by_browser */, kPersistentNotificationId));
}

// The tag, when non-empty, will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_DifferentTags) {
  const std::string& different_tag = std::string(kExampleTag) + "2";

  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), different_tag,
                false /* is_shown_by_browser */, kPersistentNotificationId));
}

// The persistent or non-persistent notification id will impact the generated
// notification id when the tag is empty.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_DifferentIds) {
  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
                kPersistentNotificationId + 1));
}

// Using a numeric tag that could resemble a persistent notification id should
// not be equal to a notification without a tag, but with that id.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_NumericTagAmbiguity) {
  EXPECT_NE(
      generator_.GenerateForPersistentNotification(
          origin_.GetURL(), base::NumberToString(kPersistentNotificationId),
          false /* is_shown_by_browser */, kPersistentNotificationId),
      generator_.GenerateForPersistentNotification(
          origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
          kPersistentNotificationId));
}

// Using port numbers and a tag which, when concatenated, could end up being
// equal to each other if origins stop ending with slashes.
TEST_F(NotificationIdGeneratorTest, GenerateForPersistent_OriginPortAmbiguity) {
  GURL origin_805("https://example.com:805");
  GURL origin_8051("https://example.com:8051");

  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_805, "17", false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_8051, "7", false /* is_shown_by_browser */,
                kPersistentNotificationId));
}

// -----------------------------------------------------------------------------
// Non-persistent notifications

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_IsDeterministic) {
  EXPECT_EQ(
      generator_.GenerateForNonPersistentNotification(origin_, "example-token"),
      generator_.GenerateForNonPersistentNotification(origin_,
                                                      "example-token"));
}

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_DifferentOrigins) {
  url::Origin different_origin(
      url::Origin::Create(GURL("https://example2.com")));

  EXPECT_NE(
      generator_.GenerateForNonPersistentNotification(origin_, "example-token"),
      generator_.GenerateForNonPersistentNotification(different_origin,
                                                      "example-token"));
}

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_DifferentTokens) {
  EXPECT_NE(
      generator_.GenerateForNonPersistentNotification(origin_, "example-token"),
      generator_.GenerateForNonPersistentNotification(origin_, "other-token"));
}

// Use port numbers and a token which, when concatenated, could end up being
// equal to each other if origins stop ending with slashes.
TEST_F(NotificationIdGeneratorTest,
       GenerateForNonPersistent_OriginPortAmbiguity) {
  auto origin_805(url::Origin::Create(GURL("https://example.com:805")));
  auto origin_8051(url::Origin::Create(GURL("https://example.com:8051")));

  EXPECT_NE(generator_.GenerateForNonPersistentNotification(origin_805, "17"),
            generator_.GenerateForNonPersistentNotification(origin_8051, "7"));
}

// -----------------------------------------------------------------------------
// Both persistent and non-persistent notifications.

// Verifies that the static Is(Non)PersistentNotification helpers can correctly
// identify persistent and non-persistent notification IDs.
TEST_F(NotificationIdGeneratorTest, DetectIdFormat) {
  std::string persistent_id = generator_.GenerateForPersistentNotification(
      origin_.GetURL(), "" /* tag */, false /* is_shown_by_browser */,
      kPersistentNotificationId);

  std::string non_persistent_id =
      generator_.GenerateForNonPersistentNotification(origin_, "token");

  EXPECT_TRUE(NotificationIdGenerator::IsPersistentNotification(persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsNonPersistentNotification(persistent_id));

  EXPECT_TRUE(
      NotificationIdGenerator::IsNonPersistentNotification(non_persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsPersistentNotification(non_persistent_id));
}

TEST_F(NotificationIdGeneratorTest, ShownByBrowser) {
  std::string id = generator_.GenerateForPersistentNotification(
      origin_.GetURL(), "" /* tag */, true /* is_shown_by_browser */,
      kPersistentNotificationId);

  // We should still be able to detect browser notifications as persistent.
  EXPECT_TRUE(NotificationIdGenerator::IsPersistentNotification(id));
  EXPECT_FALSE(NotificationIdGenerator::IsNonPersistentNotification(id));

  // Browser vs non-browser ids should be different.
  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, false /* is_shown_by_browser */,
                kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, true /* is_shown_by_browser */,
                kPersistentNotificationId));
}

}  // namespace
}  // namespace content
