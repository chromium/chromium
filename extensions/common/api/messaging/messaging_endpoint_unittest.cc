// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/messaging_endpoint.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kExtId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kExtId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

TEST(MessagingEndpointTest, GetRelationshipTest_ExtensionEndpoint) {
  MessagingEndpoint endpoint = MessagingEndpoint::ForExtension(kExtId1);
  EXPECT_EQ(MessagingEndpoint::Relationship::kInternal,
            MessagingEndpoint::GetRelationship(endpoint, kExtId1));
  EXPECT_EQ(MessagingEndpoint::Relationship::kExternalExtension,
            MessagingEndpoint::GetRelationship(endpoint, kExtId2));
}

TEST(MessagingEndpointTest, GetRelationshipTest_ContentScriptEndpoint) {
  MessagingEndpoint endpoint = MessagingEndpoint::ForContentScript(kExtId1);
  EXPECT_EQ(MessagingEndpoint::Relationship::kInternal,
            MessagingEndpoint::GetRelationship(endpoint, kExtId1));
  EXPECT_EQ(MessagingEndpoint::Relationship::kExternalExtension,
            MessagingEndpoint::GetRelationship(endpoint, kExtId2));
}

TEST(MessagingEndpointTest, GetRelationshipTest_WebPageEndpoint) {
  MessagingEndpoint endpoint = MessagingEndpoint::ForWebPage();
  EXPECT_EQ(MessagingEndpoint::Relationship::kExternalWebPage,
            MessagingEndpoint::GetRelationship(endpoint, kExtId1));
}

TEST(MessagingEndpointTest, GetRelationshipTest_NativeAppEndpoint) {
  MessagingEndpoint endpoint = MessagingEndpoint::ForNativeApp("app");
  EXPECT_EQ(MessagingEndpoint::Relationship::kExternalNativeApp,
            MessagingEndpoint::GetRelationship(endpoint, kExtId1));
}

}  // namespace extensions
