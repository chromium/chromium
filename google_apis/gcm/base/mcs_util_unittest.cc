// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/gcm/base/mcs_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

const uint64_t kAuthId = 4421448356646222460;
const uint64_t kAuthToken = 12345;

// Build a login request protobuf.
TEST(MCSUtilTest, BuildLoginRequest) {
  std::unique_ptr<mcs_proto::LoginRequest> login_request =
      BuildLoginRequest(kAuthId, kAuthToken, "1.0");
  ASSERT_EQ("chrome-1.0", login_request->id());
  ASSERT_EQ(base::NumberToString(kAuthToken), login_request->auth_token());
  ASSERT_EQ(base::NumberToString(kAuthId), login_request->user());
  ASSERT_EQ("android-3d5c23dac2a1fa7c", login_request->device_id());
  ASSERT_EQ("new_vc", login_request->setting(0).name());
  ASSERT_EQ("1", login_request->setting(0).value());
  // TODO(zea): test the other fields once they have valid values.
}

// Test building a protobuf and extracting the tag from a protobuf.
TEST(MCSUtilTest, ProtobufToTag) {
  for (uint8_t i = 0; i < kNumProtoTypes; ++i) {
    std::unique_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(i);
    if (!protobuf.get())  // Not all tags have protobuf definitions.
      continue;
    ASSERT_EQ(i, GetMCSProtoTag(*protobuf)) << "Type " << i;
  }
}

// Test getting and setting persistent ids.
TEST(MCSUtilTest, PersistentIds) {
  static_assert(kNumProtoTypes == 16U, "Update Persistent Ids");
  const int kTagsWithPersistentIds[] = {
    kIqStanzaTag,
    kDataMessageStanzaTag
  };
  for (size_t i = 0; i < std::size(kTagsWithPersistentIds); ++i) {
    int tag = kTagsWithPersistentIds[i];
    std::unique_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(tag);
    ASSERT_TRUE(protobuf.get());
    SetPersistentId(base::NumberToString(tag), protobuf.get());
    int get_val = 0;
    base::StringToInt(GetPersistentId(*protobuf), &get_val);
    ASSERT_EQ(tag, get_val);
  }
}

// Test getting and setting stream ids.
TEST(MCSUtilTest, StreamIds) {
  static_assert(kNumProtoTypes == 16U, "Update Stream Ids");
  const int kTagsWithStreamIds[] = {
    kIqStanzaTag,
    kDataMessageStanzaTag,
    kHeartbeatPingTag,
    kHeartbeatAckTag,
    kLoginResponseTag,
  };
  for (size_t i = 0; i < std::size(kTagsWithStreamIds); ++i) {
    int tag = kTagsWithStreamIds[i];
    std::unique_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(tag);
    ASSERT_TRUE(protobuf.get());
    SetLastStreamIdReceived(tag, protobuf.get());
    int get_id = GetLastStreamIdReceived(*protobuf);
    ASSERT_EQ(tag, get_id);
  }
}

}  // namespace
}  // namespace gcm
