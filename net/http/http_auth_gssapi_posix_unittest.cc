// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include <memory>
#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/mock_gssapi_library_posix.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// gss_buffer_t helpers.
void ClearBuffer(gss_buffer_t dest) {
  if (!dest)
    return;
  dest->length = 0;
  delete [] reinterpret_cast<char*>(dest->value);
  dest->value = nullptr;
}

void SetBuffer(gss_buffer_t dest, const void* src, size_t length) {
  if (!dest)
    return;
  ClearBuffer(dest);
  if (!src)
    return;
  dest->length = length;
  if (length) {
    dest->value = new char[length];
    memcpy(dest->value, src, length);
  }
}

void CopyBuffer(gss_buffer_t dest, const gss_buffer_t src) {
  if (!dest)
    return;
  ClearBuffer(dest);
  if (!src)
    return;
  SetBuffer(dest, src->value, src->length);
}

const char kInitialAuthResponse[] = "Mary had a little lamb";

void EstablishInitialContext(test::MockGSSAPILibrary* library) {
  test::GssContextMockImpl context_info(
      "localhost",                         // Source name
      "example.com",                       // Target name
      23,                                  // Lifetime
      *CHROME_GSS_SPNEGO_MECH_OID_DESC,    // Mechanism
      0,                                   // Context flags
      1,                                   // Locally initiated
      0);                                  // Open
  gss_buffer_desc in_buffer = {0, nullptr};
  gss_buffer_desc out_buffer = {std::size(kInitialAuthResponse),
                                const_cast<char*>(kInitialAuthResponse)};
  library->ExpectSecurityContext(
      "Negotiate",
      GSS_S_CONTINUE_NEEDED,
      0,
      context_info,
      in_buffer,
      out_buffer);
}

void UnexpectedCallback(int result) {
  // At present getting tokens from gssapi is fully synchronous, so the callback
  // should never be called.
  ADD_FAILURE();
}

}  // namespace

TEST(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup) {
  RecordingNetLogObserver net_log_observer;
  // TODO(ahendrickson): Manipulate the libraries and paths to test each of the
  // libraries we expect, and also whether or not they have the interface
  // functions we want.
  auto gssapi = std::make_unique<GSSAPISharedLibrary>(std::string());
  DCHECK(gssapi.get());
  EXPECT_TRUE(
      gssapi.get()->Init(NetLogWithSource::Make(NetLogSourceType::NONE)));

  // Should've logged a AUTH_LIBRARY_LOAD event, but not
  // AUTH_LIBRARY_BIND_FAILED.
  auto entries = net_log_observer.GetEntries();
  auto offset = ExpectLogContainsSomewhere(
      entries, 0u, NetLogEventType::AUTH_LIBRARY_LOAD, NetLogEventPhase::BEGIN);
  offset = ExpectLogContainsSomewhereAfter(entries, offset,
                                           NetLogEventType::AUTH_LIBRARY_LOAD,
                                           NetLogEventPhase::END);
  ASSERT_LT(offset, entries.size());

  const auto& entry = entries[offset];
  EXPECT_NE("", GetStringValueFromParams(entry, "library_name"));

  // No load_result since it succeeded.
  EXPECT_FALSE(GetOptionalStringValueFromParams(entry, "load_result"));
}

TEST(HttpAuthGSSAPIPOSIXTest, CustomLibraryMissing) {
  RecordingNetLogObserver net_log_observer;

  auto gssapi =
      std::make_unique<GSSAPISharedLibrary>("/this/library/does/not/exist");
  EXPECT_FALSE(
      gssapi.get()->Init(NetLogWithSource::Make(NetLogSourceType::NONE)));

  auto entries = net_log_observer.GetEntries();
  auto offset = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::AUTH_LIBRARY_LOAD, NetLogEventPhase::END);
  ASSERT_LT(offset, entries.size());

  const auto& entry = entries[offset];
  EXPECT_NE("", GetStringValueFromParams(entry, "load_result"));
}

TEST(HttpAuthGSSAPIPOSIXTest, CustomLibraryExists) {
  RecordingNetLogObserver net_log_observer;
  base::FilePath module;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &module));
  auto basename = base::GetNativeLibraryName("test_gssapi");
  module = module.AppendASCII(basename);
  auto gssapi = std::make_unique<GSSAPISharedLibrary>(module.value());
  EXPECT_TRUE(
      gssapi.get()->Init(NetLogWithSource::Make(NetLogSourceType::NONE)));

  auto entries = net_log_observer.GetEntries();
  auto offset = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::AUTH_LIBRARY_LOAD, NetLogEventPhase::END);
  ASSERT_LT(offset, entries.size());

  const auto& entry = entries[offset];
  EXPECT_FALSE(GetOptionalStringValueFromParams(entry, "load_result"));
  EXPECT_EQ(module.AsUTF8Unsafe(),
            GetStringValueFromParams(entry, "library_name"));
}

TEST(HttpAuthGSSAPIPOSIXTest, CustomLibraryMethodsMissing) {
  RecordingNetLogObserver net_log_observer;
  base::FilePath module;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &module));
  auto basename = base::GetNativeLibraryName("test_badgssapi");
  module = module.AppendASCII(basename);
  auto gssapi = std::make_unique<GSSAPISharedLibrary>(module.value());

  // Are you here because this test mysteriously passed even though the library
  // doesn't actually have all the methods we need? This could be because the
  // test library (//net:test_badgssapi) inadvertently depends on a valid GSSAPI
  // library. On macOS this can happen because it's pretty easy to end up
  // depending on GSS.framework.
  //
  // To resolve this issue, make sure that //net:test_badgssapi target in
  // //net/BUILD.gn should have an empty `deps` and an empty `libs`.
  EXPECT_FALSE(
      gssapi.get()->Init(NetLogWithSource::Make(NetLogSourceType::NONE)));

  auto entries = net_log_observer.GetEntries();
  auto offset = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::AUTH_LIBRARY_BIND_FAILED,
      NetLogEventPhase::NONE);
  ASSERT_LT(offset, entries.size());

  const auto& entry = entries[offset];
  EXPECT_EQ("gss_import_name", GetStringValueFromParams(entry, "method"));
}

TEST(HttpAuthGSSAPIPOSIXTest, GSSAPICycle) {
  auto mock_library = std::make_unique<test::MockGSSAPILibrary>();
  DCHECK(mock_library.get());
  mock_library->Init(NetLogWithSource());
  const char kAuthResponse[] = "Mary had a little lamb";
  test::GssContextMockImpl context1(
      "localhost",                         // Source name
      "example.com",                       // Target name
      23,                                  // Lifetime
      *CHROME_GSS_SPNEGO_MECH_OID_DESC,    // Mechanism
      0,                                   // Context flags
      1,                                   // Locally initiated
      0);                                  // Open
  test::GssContextMockImpl context2(
      "localhost",                         // Source name
      "example.com",                       // Target name
      23,                                  // Lifetime
      *CHROME_GSS_SPNEGO_MECH_OID_DESC,    // Mechanism
      0,                                   // Context flags
      1,                                   // Locally initiated
      1);                                  // Open
  test::MockGSSAPILibrary::SecurityContextQuery queries[] = {
      test::MockGSSAPILibrary::SecurityContextQuery(
          "Negotiate",            // Package name
          GSS_S_CONTINUE_NEEDED,  // Major response code
          0,                      // Minor response code
          context1,               // Context
          nullptr,                // Expected input token
          kAuthResponse),         // Output token
      test::MockGSSAPILibrary::SecurityContextQuery(
          "Negotiate",     // Package name
          GSS_S_COMPLETE,  // Major response code
          0,               // Minor response code
          context2,        // Context
          kAuthResponse,   // Expected input token
          kAuthResponse)   // Output token
  };

  for (const auto& query : queries) {
    mock_library->ExpectSecurityContext(
        query.expected_package, query.response_code, query.minor_response_code,
        query.context_info, query.expected_input_token, query.output_token);
  }

  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_cred_id_t initiator_cred_handle = nullptr;
  gss_ctx_id_t context_handle = nullptr;
  gss_name_t target_name = nullptr;
  gss_OID mech_type = nullptr;
  OM_uint32 req_flags = 0;
  OM_uint32 time_req = 25;
  gss_channel_bindings_t input_chan_bindings = nullptr;
  gss_buffer_desc input_token = {0, nullptr};
  gss_OID actual_mech_type = nullptr;
  gss_buffer_desc output_token = {0, nullptr};
  OM_uint32 ret_flags = 0;
  OM_uint32 time_rec = 0;
  for (const auto& query : queries) {
    major_status = mock_library->init_sec_context(&minor_status,
                                                  initiator_cred_handle,
                                                  &context_handle,
                                                  target_name,
                                                  mech_type,
                                                  req_flags,
                                                  time_req,
                                                  input_chan_bindings,
                                                  &input_token,
                                                  &actual_mech_type,
                                                  &output_token,
                                                  &ret_flags,
                                                  &time_rec);
    EXPECT_EQ(query.response_code, major_status);
    CopyBuffer(&input_token, &output_token);
    ClearBuffer(&output_token);
  }
  ClearBuffer(&input_token);
  major_status = mock_library->delete_sec_context(&minor_status,
                                                  &context_handle,
                                                  GSS_C_NO_BUFFER);
  EXPECT_EQ(static_cast<OM_uint32>(GSS_S_COMPLETE), major_status);
}

TEST(HttpAuthGSSAPITest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  test::MockGSSAPILibrary mock_library;
  HttpAuthGSSAPI auth_gssapi(&mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&challenge));
}

TEST(HttpAuthGSSAPITest, ParseChallenge_TwoRounds) {
  RecordingNetLogObserver net_log_observer;
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  test::MockGSSAPILibrary mock_library;
  HttpAuthGSSAPI auth_gssapi(&mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  // Generate an auth token and create another thing.
  EstablishInitialContext(&mock_library);
  std::string auth_token;
  EXPECT_EQ(OK, auth_gssapi.GenerateAuthToken(
                    nullptr, "HTTP/intranet.google.com", std::string(),
                    &auth_token, NetLogWithSource::Make(NetLogSourceType::NONE),
                    base::BindOnce(&UnexpectedCallback)));

  HttpAuthChallengeTokenizer second_challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&second_challenge));

  auto entries = net_log_observer.GetEntries();
  auto offset = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX,
      NetLogEventPhase::END);
  // There should be two of these.
  offset = ExpectLogContainsSomewhere(
      entries, offset, NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX,
      NetLogEventPhase::END);
  ASSERT_LT(offset, entries.size());
  const std::string* source =
      entries[offset].params.FindStringByDottedPath("context.source.name");
  ASSERT_TRUE(source);
  EXPECT_EQ("localhost", *source);
}

TEST(HttpAuthGSSAPITest, ParseChallenge_UnexpectedTokenFirstRound) {
  // If the first round challenge has an additional authentication token, it
  // should be treated as an invalid challenge from the server.
  test::MockGSSAPILibrary mock_library;
  HttpAuthGSSAPI auth_gssapi(&mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_gssapi.ParseChallenge(&challenge));
}

TEST(HttpAuthGSSAPITest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  test::MockGSSAPILibrary mock_library;
  HttpAuthGSSAPI auth_gssapi(&mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  EstablishInitialContext(&mock_library);
  std::string auth_token;
  EXPECT_EQ(OK,
            auth_gssapi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  HttpAuthChallengeTokenizer second_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            auth_gssapi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthGSSAPITest, ParseChallenge_NonBase64EncodedToken) {
  // If a later-round challenge has an invalid base64 encoded token, it should
  // be treated as an invalid challenge.
  test::MockGSSAPILibrary mock_library;
  HttpAuthGSSAPI auth_gssapi(&mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  EstablishInitialContext(&mock_library);
  std::string auth_token;
  EXPECT_EQ(OK,
            auth_gssapi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  HttpAuthChallengeTokenizer second_challenge("Negotiate =happyjoy=");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_gssapi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthGSSAPITest, OidToValue_NIL) {
  auto actual = OidToValue(GSS_C_NO_OID);
  auto expected = base::JSONReader::Read(R"({ "oid": "<Empty OID>" })");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, OidToValue_Known) {
  gss_OID_desc known = {6, const_cast<char*>("\x2b\x06\01\x05\x06\x03")};

  auto actual = OidToValue(const_cast<const gss_OID>(&known));
  auto expected = base::JSONReader::Read(R"(
      {
        "oid"   : "GSS_C_NT_ANONYMOUS",
        "length": 6,
        "bytes" : "KwYBBQYD"
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, OidToValue_Unknown) {
  gss_OID_desc unknown = {6, const_cast<char*>("\x2b\x06\01\x05\x06\x05")};
  auto actual = OidToValue(const_cast<const gss_OID>(&unknown));
  auto expected = base::JSONReader::Read(R"(
      {
        "length": 6,
        "bytes" : "KwYBBQYF"
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_NoLibrary) {
  auto actual = GetGssStatusValue(nullptr, "my_method", GSS_S_BAD_NAME, 1);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 131072
        },
        "minor_status": {
          "status": 1
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_WithLibrary) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(&library, "my_method", GSS_S_BAD_NAME, 1);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 131072,
          "message": [ "Value: 131072, Type 1" ]
        },
        "minor_status": {
          "status": 1,
          "message": [ "Value: 1, Type 2" ]
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Multiline) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::MultiLine),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 128,
          "message": [
            "Line 1 for status 128",
            "Line 2 for status 128",
            "Line 3 for status 128",
            "Line 4 for status 128",
            "Line 5 for status 128"
          ]
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_InfiniteLines) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::InfiniteLines),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 129,
          "message": [
            "Line 1 for status 129",
            "Line 2 for status 129",
            "Line 3 for status 129",
            "Line 4 for status 129",
            "Line 5 for status 129",
            "Line 6 for status 129",
            "Line 7 for status 129",
            "Line 8 for status 129"
          ]
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Failure) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::Fail),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 130
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_EmptyMessage) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::EmptyMessage),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 131
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Misbehave) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::UninitalizedBuffer),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 132
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_NotUtf8) {
  test::MockGSSAPILibrary library;
  auto actual = GetGssStatusValue(
      &library, "my_method",
      static_cast<OM_uint32>(
          test::MockGSSAPILibrary::DisplayStatusSpecials::InvalidUtf8),
      0);
  auto expected = base::JSONReader::Read(R"(
      {
        "function": "my_method",
        "major_status": {
          "status": 133
        },
        "minor_status": {
          "status": 0
        }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetContextStateAsValue_ValidContext) {
  test::GssContextMockImpl context{"source_spn@somewhere",
                                   "target_spn@somewhere.else",
                                   /* lifetime_rec= */ 100,
                                   *CHROME_GSS_SPNEGO_MECH_OID_DESC,
                                   /* ctx_flags= */ 0,
                                   /* locally_initiated= */ 1,
                                   /* open= */ 0};
  test::MockGSSAPILibrary library;
  auto actual = GetContextStateAsValue(
      &library, reinterpret_cast<const gss_ctx_id_t>(&context));
  auto expected = base::JSONReader::Read(R"(
      {
        "source": {
          "name": "source_spn@somewhere",
          "type": {
            "oid" : "<Empty OID>"
          }
        },
        "target": {
          "name": "target_spn@somewhere.else",
          "type": {
            "oid" : "<Empty OID>"
          }
        },
        "lifetime": "100",
        "mechanism": {
          "oid": "<Empty OID>"
        },
        "flags": {
          "value": "0x00000000",
          "delegated": false,
          "mutual": false
        },
        "open": false
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetContextStateAsValue_NoContext) {
  test::MockGSSAPILibrary library;
  auto actual = GetContextStateAsValue(&library, GSS_C_NO_CONTEXT);
  auto expected = base::JSONReader::Read(R"(
      {
         "error": {
            "function": "<none>",
            "major_status": {
               "status": 524288
            },
            "minor_status": {
               "status": 0
            }
         }
      }
  )");
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

}  // namespace net
