// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace content {
namespace {

std::vector<uint8_t> ToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> JsonToCbor(const std::string& json) {
  std::vector<uint8_t> cbor;
  crdtp::Status status =
      crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(json), &cbor);
  EXPECT_TRUE(status.ok()) << status.ToASCIIString();
  return cbor;
}

TEST(DevToolsSessionValidateMessageTest, MalformedJson) {
  std::vector<uint8_t> message = ToVector("{invalid cbor/json");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message));
}

TEST(DevToolsSessionValidateMessageTest, EmptyMessage) {
  std::vector<uint8_t> message;
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ValidJson_RootSession_NoIdExpected_Valid) {
  std::vector<uint8_t> message =
      ToVector("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ValidCbor_RootSession_NoIdExpected_Valid) {
  std::vector<uint8_t> message =
      JsonToCbor("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message));
}

TEST(DevToolsSessionValidateMessageTest,
     RootSession_NoIdExpected_HasId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"method\": \"Page.loadEventFired\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_IdExpected_HasId_Valid) {
  std::vector<uint8_t> message = ToVector("{\"id\": 1, \"result\": {}}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", true, message));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_IdExpected_NoId_Valid) {
  // ValidateMessage does not enforce 'id' presence when expected_has_id is
  // true.
  std::vector<uint8_t> message = ToVector("{\"result\": {}}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", true, message));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_HasSessionId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"sessionId\": \"123\", \"result\": {}}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", true, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_ValidMatchingSessionId_NoIdExpected) {
  std::vector<uint8_t> message = ToVector(
      "{\"method\": \"Page.loadEventFired\", \"sessionId\": \"session1\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("session1", false, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_ValidMatchingSessionId_IdExpected) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("session1", true, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_MissingSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector("{\"id\": 1, \"result\": {}}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_MismatchingSessionId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"result\": {}, \"sessionId\": \"session2\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_NoIdExpected_HasId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"method\": \"Page.loadEventFired\", \"sessionId\": "
      "\"session1\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", false, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_DuplicateSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\", \"sessionId\": "
      "\"session1\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_DuplicateMismatchingSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\", \"sessionId\": "
      "\"session2\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message));
}

}  // namespace
}  // namespace content
