// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_deflate_parameters.h"

#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "net/websockets/websocket_extension_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CheckExtension(const WebSocketDeflateParameters& params,
                    const std::string& name,
                    const std::string& value) {
  WebSocketExtension e = params.AsExtension();
  EXPECT_EQ("permessage-deflate", e.name());
  if (e.parameters().size() != 1)
    FAIL() << "parameters must have one element.";
  EXPECT_EQ(name, e.parameters()[0].name());
  EXPECT_EQ(value, e.parameters()[0].value());
}

TEST(WebSocketDeflateParametersTest, Empty) {
  WebSocketDeflateParameters r;

  EXPECT_EQ(WebSocketDeflater::TAKE_OVER_CONTEXT,
            r.server_context_take_over_mode());
  EXPECT_EQ(WebSocketDeflater::TAKE_OVER_CONTEXT,
            r.client_context_take_over_mode());
  EXPECT_FALSE(r.is_server_max_window_bits_specified());
  EXPECT_FALSE(r.is_client_max_window_bits_specified());
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_TRUE(r.IsValidAsResponse());
  WebSocketExtension e = r.AsExtension();
  EXPECT_EQ("permessage-deflate", e.name());
  EXPECT_TRUE(e.parameters().empty());
}

TEST(WebSocketDeflateParametersTest, ServerContextTakeover) {
  WebSocketDeflateParameters r;

  r.SetServerNoContextTakeOver();
  CheckExtension(r, "server_no_context_takeover", "");
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_TRUE(r.IsValidAsResponse());
}

TEST(WebSocketDeflateParametersTest, ClientContextTakeover) {
  WebSocketDeflateParameters r;

  r.SetClientNoContextTakeOver();
  CheckExtension(r, "client_no_context_takeover", "");
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_TRUE(r.IsValidAsResponse());
}

TEST(WebSocketDeflateParametersTest, ServerMaxWindowBits) {
  WebSocketDeflateParameters r;

  r.SetServerMaxWindowBits(13);
  CheckExtension(r, "server_max_window_bits", "13");
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_TRUE(r.IsValidAsResponse());
}

TEST(WebSocketDeflateParametersTest, ClientMaxWindowBitsWithoutValue) {
  WebSocketDeflateParameters r;
  std::string failure_message;

  r.SetClientMaxWindowBits();
  CheckExtension(r, "client_max_window_bits", "");
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_FALSE(r.IsValidAsResponse(&failure_message));
  EXPECT_EQ("client_max_window_bits must have value", failure_message);
}

TEST(WebSocketDeflateParametersTest, ClientMaxWindowBitsWithValue) {
  WebSocketDeflateParameters r;

  r.SetClientMaxWindowBits(12);
  CheckExtension(r, "client_max_window_bits", "12");
  EXPECT_TRUE(r.IsValidAsRequest());
  EXPECT_TRUE(r.IsValidAsResponse());
}

struct InitializeTestParameter {
  const std::string query;
  struct Expectation {
    bool result;
    std::string failure_message;
  } const expected;
};

void PrintTo(const InitializeTestParameter& p, std::ostream* o) {
  *o << p.query;
}

class WebSocketDeflateParametersInitializeTest
    : public ::testing::TestWithParam<InitializeTestParameter> {};

TEST_P(WebSocketDeflateParametersInitializeTest, Initialize) {
  const std::string query = GetParam().query;
  const bool expected = GetParam().expected.result;
  const std::string expected_failure_message =
      GetParam().expected.failure_message;

  WebSocketExtensionParser parser;
  ASSERT_TRUE(parser.Parse("permessage-deflate" + query));
  ASSERT_EQ(1u, parser.extensions().size());
  WebSocketExtension extension = parser.extensions()[0];

  WebSocketDeflateParameters parameters;
  std::string failure_message;
  bool actual = parameters.Initialize(extension, &failure_message);

  if (expected) {
    EXPECT_TRUE(actual);
    EXPECT_TRUE(extension.Equivalent(parameters.AsExtension()));
  } else {
    EXPECT_FALSE(actual);
  }
  EXPECT_EQ(expected_failure_message, failure_message);
}

struct CompatibilityTestParameter {
  const char* request_query;
  const char* response_query;
  const bool expected;
};

void PrintTo(const CompatibilityTestParameter& p, std::ostream* o) {
  *o << "req = \"" << p.request_query << "\", res = \"" << p.response_query
     << "\"";
}

class WebSocketDeflateParametersCompatibilityTest
    : public ::testing::TestWithParam<CompatibilityTestParameter> {};

TEST_P(WebSocketDeflateParametersCompatibilityTest, CheckCompatiblity) {
  const std::string request_query = GetParam().request_query;
  const std::string response_query = GetParam().response_query;
  const bool expected = GetParam().expected;

  std::string message;
  WebSocketDeflateParameters request, response;

  WebSocketExtensionParser request_parser;
  ASSERT_TRUE(request_parser.Parse("permessage-deflate" + request_query));
  ASSERT_EQ(1u, request_parser.extensions().size());
  ASSERT_TRUE(request.Initialize(request_parser.extensions()[0], &message));
  ASSERT_TRUE(request.IsValidAsRequest(&message));

  WebSocketExtensionParser response_parser;
  ASSERT_TRUE(response_parser.Parse("permessage-deflate" + response_query));
  ASSERT_EQ(1u, response_parser.extensions().size());
  ASSERT_TRUE(response.Initialize(response_parser.extensions()[0], &message));
  ASSERT_TRUE(response.IsValidAsResponse(&message));

  EXPECT_EQ(expected, request.IsCompatibleWith(response));
}

InitializeTestParameter::Expectation Duplicate(const std::string& name) {
  return {false,
          "Received duplicate permessage-deflate extension parameter " + name};
}

InitializeTestParameter::Expectation Invalid(const std::string& name) {
  return {false, "Received invalid " + name + " parameter"};
}

// We need this function in order to avoid global non-pod variables.
std::vector<InitializeTestParameter> InitializeTestParameters() {
  const InitializeTestParameter::Expectation kInitialized = {true, ""};
  const InitializeTestParameter::Expectation kUnknownParameter = {
      false, "Received an unexpected permessage-deflate extension parameter"};

  const InitializeTestParameter parameters[] = {
      {"", kInitialized},
      {"; server_no_context_takeover", kInitialized},
      {"; server_no_context_takeover=0", Invalid("server_no_context_takeover")},
      {"; server_no_context_takeover; server_no_context_takeover",
       Duplicate("server_no_context_takeover")},
      {"; client_no_context_takeover", kInitialized},
      {"; client_no_context_takeover=0", Invalid("client_no_context_takeover")},
      {"; client_no_context_takeover; client_no_context_takeover",
       Duplicate("client_no_context_takeover")},
      {"; server_max_window_bits=8", kInitialized},
      {"; server_max_window_bits=15", kInitialized},
      {"; server_max_window_bits=15; server_max_window_bits=15",
       Duplicate("server_max_window_bits")},
      {"; server_max_window_bits=a", Invalid("server_max_window_bits")},
      {"; server_max_window_bits=09", Invalid("server_max_window_bits")},
      {"; server_max_window_bits=+9", Invalid("server_max_window_bits")},
      {"; server_max_window_bits=9a", Invalid("server_max_window_bits")},
      {"; server_max_window_bits", Invalid("server_max_window_bits")},
      {"; server_max_window_bits=7", Invalid("server_max_window_bits")},
      {"; server_max_window_bits=16", Invalid("server_max_window_bits")},
      {"; client_max_window_bits=8", kInitialized},
      {"; client_max_window_bits=15", kInitialized},
      {"; client_max_window_bits=15; client_max_window_bits=15",
       Duplicate("client_max_window_bits")},
      {"; client_max_window_bits=a", Invalid("client_max_window_bits")},
      {"; client_max_window_bits=09", Invalid("client_max_window_bits")},
      {"; client_max_window_bits=+9", Invalid("client_max_window_bits")},
      {"; client_max_window_bits=9a", Invalid("client_max_window_bits")},
      {"; client_max_window_bits", kInitialized},
      {"; client_max_window_bits=7", Invalid("client_max_window_bits")},
      {"; client_max_window_bits=16", Invalid("client_max_window_bits")},
      {"; server_no_context_takeover; client_no_context_takeover"
       "; server_max_window_bits=12; client_max_window_bits=13",
       kInitialized},
      {"; hogefuga", kUnknownParameter},
  };
  return std::vector<InitializeTestParameter>(
      parameters, parameters + std::size(parameters));
}

constexpr CompatibilityTestParameter kCompatibilityTestParameters[] = {
    {"", "", true},
    // server_no_context_takeover
    {"", "; server_no_context_takeover", true},
    {"; server_no_context_takeover", "", false},
    {"; server_no_context_takeover", "; server_no_context_takeover", true},
    // client_no_context_takeover
    {"", "; client_no_context_takeover", true},
    {"; client_no_context_takeover", "", true},
    {"; client_no_context_takeover", "; client_no_context_takeover", true},
    // server_max_window_bits
    {"", "; server_max_window_bits=14", true},
    {"; server_max_window_bits=12", "", false},
    {"; server_max_window_bits=12", "; server_max_window_bits=12", true},
    {"; server_max_window_bits=12", "; server_max_window_bits=11", true},
    {"; server_max_window_bits=12", "; server_max_window_bits=13", false},
    // client_max_window_bits
    {"", "; client_max_window_bits=14", false},
    {"; client_max_window_bits", "", true},
    {"; client_max_window_bits", "; client_max_window_bits=15", true},
    {"; client_max_window_bits=12", "", true},
    {"; client_max_window_bits=12", "; client_max_window_bits=12", true},
    {"; client_max_window_bits=12", "; client_max_window_bits=11", true},
    {"; client_max_window_bits=12", "; client_max_window_bits=13", true},
};

INSTANTIATE_TEST_SUITE_P(WebSocketDeflateParametersInitializeTest,
                         WebSocketDeflateParametersInitializeTest,
                         ::testing::ValuesIn(InitializeTestParameters()));

INSTANTIATE_TEST_SUITE_P(WebSocketDeflateParametersCompatibilityTest,
                         WebSocketDeflateParametersCompatibilityTest,
                         ::testing::ValuesIn(kCompatibilityTestParameters));

}  // namespace

}  // namespace net
