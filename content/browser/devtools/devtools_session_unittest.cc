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

// Builds the CBOR message {"method": |method|}. The function writes |method|
// as a STRING8 value and does not change its bytes.
std::vector<uint8_t> CborMessageWithString8Method(const std::string& method) {
  const std::string key = "method";
  std::vector<uint8_t> cbor;
  crdtp::cbor::EnvelopeEncoder envelope;
  envelope.EncodeStart(&cbor);
  cbor.push_back(crdtp::cbor::EncodeIndefiniteLengthMapStart());
  crdtp::cbor::EncodeString8(crdtp::SpanFrom(key), &cbor);
  crdtp::cbor::EncodeString8(crdtp::SpanFrom(method), &cbor);
  cbor.push_back(crdtp::cbor::EncodeStop());
  EXPECT_TRUE(envelope.EncodeStop(&cbor));
  return cbor;
}

TEST(DevToolsSessionValidateMessageTest, MalformedJson) {
  std::vector<uint8_t> message = ToVector("{invalid cbor/json");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, EmptyMessage) {
  std::vector<uint8_t> message;
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ValidJson_RootSession_NoIdExpected_Valid) {
  std::vector<uint8_t> message =
      ToVector("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message,
                                               /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ValidCbor_RootSession_NoIdExpected_Valid) {
  std::vector<uint8_t> message =
      JsonToCbor("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message,
                                               /*expect_cbor=*/true));
}

TEST(DevToolsSessionValidateMessageTest,
     RootSession_NoIdExpected_HasId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"method\": \"Page.loadEventFired\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_IdExpected_HasId_Valid) {
  std::vector<uint8_t> message = ToVector("{\"id\": 1, \"result\": {}}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", true, message,
                                               /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_IdExpected_NoId_Valid) {
  // ValidateMessage does not enforce 'id' presence when expected_has_id is
  // true.
  std::vector<uint8_t> message = ToVector("{\"result\": {}}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", true, message,
                                               /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, RootSession_HasSessionId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"sessionId\": \"123\", \"result\": {}}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_ValidMatchingSessionId_NoIdExpected) {
  std::vector<uint8_t> message = ToVector(
      "{\"method\": \"Page.loadEventFired\", \"sessionId\": \"session1\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("session1", false, message,
                                               /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_ValidMatchingSessionId_IdExpected) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("session1", true, message,
                                               /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_MissingSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector("{\"id\": 1, \"result\": {}}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_MismatchingSessionId_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"id\": 1, \"result\": {}, \"sessionId\": \"session2\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_NoIdExpected_HasId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"method\": \"Page.loadEventFired\", \"sessionId\": "
      "\"session1\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_DuplicateSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\", \"sessionId\": "
      "\"session1\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_DuplicateMismatchingSessionId_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"result\": {}, \"sessionId\": \"session1\", \"sessionId\": "
      "\"session2\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, CborMessage_ExpectCborFalse_Invalid) {
  std::vector<uint8_t> message =
      JsonToCbor("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, JsonMessage_ExpectCborTrue_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"method\": \"Page.loadEventFired\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/true));
}

TEST(DevToolsSessionValidateMessageTest, BrowserOnlyDomainEvent_Invalid) {
  for (const bool expect_cbor : {false, true}) {
    const std::string json =
        "{\"method\": \"Target.attachedToTarget\", \"params\": {}}";
    std::vector<uint8_t> message =
        expect_cbor ? JsonToCbor(json) : ToVector(json);
    EXPECT_FALSE(
        DevToolsSession::ValidateMessage("", false, message, expect_cbor));
  }
}

TEST(DevToolsSessionValidateMessageTest,
     ChildSession_BrowserOnlyDomainEvent_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"method\": \"Target.detachedFromTarget\", \"params\": {}, "
      "\"sessionId\": \"session1\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("session1", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest,
     BrowserOnlyDomainMethodInResponse_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"id\": 1, \"method\": \"Target.attachedToTarget\", \"params\": {}}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", true, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, EscapedBrowserOnlyDomain_Invalid) {
  std::vector<uint8_t> message =
      ToVector("{\"method\": \"\\u0054arget.attachedToTarget\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, DuplicateMethod_Invalid) {
  std::vector<uint8_t> message = ToVector(
      "{\"method\": \"Page.loadEventFired\", \"method\": "
      "\"Target.attachedToTarget\"}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, NonStringMethod_Invalid) {
  std::vector<uint8_t> message = ToVector("{\"method\": 1}");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/false));
}

TEST(DevToolsSessionValidateMessageTest, NonAsciiMethod_Invalid) {
  // ConvertJSONToCBOR encodes a string with a non-ASCII character as STRING16.
  for (const char* json : {"{\"method\": \"Page.loadEventFired\\u0100\"}",
                           "{\"method\": \"Page.loadEventFired\\u00e9\"}"}) {
    for (const bool expect_cbor : {false, true}) {
      std::vector<uint8_t> message =
          expect_cbor ? JsonToCbor(json) : ToVector(json);
      EXPECT_FALSE(
          DevToolsSession::ValidateMessage("", false, message, expect_cbor))
          << json << " expect_cbor=" << expect_cbor;
    }
  }
}

TEST(DevToolsSessionValidateMessageTest, CborString8AsciiMethod_Valid) {
  std::vector<uint8_t> message =
      CborMessageWithString8Method("Page.loadEventFired");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message,
                                               /*expect_cbor=*/true));
}

TEST(DevToolsSessionValidateMessageTest, CborString8NonAsciiMethod_Invalid) {
  // The method is a STRING8 value with well-formed multi-byte UTF-8 (U+0100).
  std::vector<uint8_t> message =
      CborMessageWithString8Method("Page.loadEventFired\xc4\x80");
  EXPECT_FALSE(DevToolsSession::ValidateMessage("", false, message,
                                                /*expect_cbor=*/true));
}

TEST(DevToolsSessionValidateMessageTest, SimilarDomainEvent_Valid) {
  std::vector<uint8_t> message =
      ToVector("{\"method\": \"TargetLike.eventFired\"}");
  EXPECT_TRUE(DevToolsSession::ValidateMessage("", false, message,
                                               /*expect_cbor=*/false));
}

}  // namespace
}  // namespace content
