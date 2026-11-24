// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_mechanism.h"
#include "net/http/mock_gssapi_library_posix.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

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
  // `gss_buffer_desc` is also `#pragma pack(2)` on x86 macOS; keep any lowered
  // `__DATA,__const` initializer 8-byte aligned so `value` cannot straddle a
  // page boundary.
  alignas(8) gss_buffer_desc out_buffer = {
      std::size(kInitialAuthResponse), const_cast<char*>(kInitialAuthResponse)};
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
  auto gssapi = base::MakeRefCounted<GSSAPISharedLibrary>(std::string());
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
      base::MakeRefCounted<GSSAPISharedLibrary>("/this/library/does/not/exist");
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
  auto gssapi = base::MakeRefCounted<GSSAPISharedLibrary>(module.value());
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
  auto gssapi = base::MakeRefCounted<GSSAPISharedLibrary>(module.value());

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
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
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
    test::CopyGssBuffer(&output_token, &input_token);
    test::ClearGssBuffer(&output_token);
  }
  test::ClearGssBuffer(&input_token);
  major_status = mock_library->delete_sec_context(&minor_status,
                                                  &context_handle,
                                                  GSS_C_NO_BUFFER);
  EXPECT_EQ(static_cast<OM_uint32>(GSS_S_COMPLETE), major_status);
}

TEST(HttpAuthGSSAPITest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&challenge));
}

TEST(HttpAuthGSSAPITest, ParseChallenge_TwoRounds) {
  RecordingNetLogObserver net_log_observer;
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  // Generate an auth token and create another thing.
  EstablishInitialContext(mock_library.get());
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
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_gssapi.ParseChallenge(&challenge));
}

TEST(HttpAuthGSSAPITest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  EstablishInitialContext(mock_library.get());
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
  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_gssapi.ParseChallenge(&first_challenge));

  EstablishInitialContext(mock_library.get());
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
  auto expected = base::JSONReader::Read(R"({ "oid": "<Empty OID>" })",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, OidToValue_Known) {
  PaddedOidDesc known = {
      .desc = {6, const_cast<char*>("\x2b\x06\01\x05\x06\x03")}};

  auto actual = OidToValue(const_cast<const gss_OID>(&known.desc));
  auto expected = base::JSONReader::Read(R"(
      {
        "oid"   : "GSS_C_NT_ANONYMOUS",
        "length": 6,
        "bytes" : "KwYBBQYD"
      }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, OidToValue_Unknown) {
  PaddedOidDesc unknown = {
      .desc = {6, const_cast<char*>("\x2b\x06\01\x05\x06\x05")}};
  auto actual = OidToValue(const_cast<const gss_OID>(&unknown.desc));
  auto expected = base::JSONReader::Read(R"(
      {
        "length": 6,
        "bytes" : "KwYBBQYF"
      }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_WithLibrary) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual =
      GetGssStatusValue(library.get(), "my_method", GSS_S_BAD_NAME, 1);
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Multiline) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_InfiniteLines) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Failure) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_EmptyMessage) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_Misbehave) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetGssStatusValue_NotUtf8) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetGssStatusValue(
      library.get(), "my_method",
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetContextStateAsValue(
      library.get(), reinterpret_cast<const gss_ctx_id_t>(&context));
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
          "length": 6,
          "bytes": "KwYBBQUC"
        },
        "flags": {
          "value": "0x00000000",
          "delegated": false,
          "mutual": false
        },
        "open": false
      }
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPITest, GetContextStateAsValue_NoContext) {
  auto library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  auto actual = GetContextStateAsValue(library.get(), GSS_C_NO_CONTEXT);
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
  )",
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(expected.has_value());
  EXPECT_EQ(actual, expected);
}

TEST(HttpAuthGSSAPIPOSIXTest, GetTargetPrincipalHost) {
  using Error = HttpAuthMechanism::TargetPrincipalError;
  using Result = base::expected<std::string, Error>;
  const Result kNotReported = base::unexpected(Error::kNotReported);
  const Result kQueryFailed = base::unexpected(Error::kQueryFailed);
  const Result kNonHttpServicePrincipal =
      base::unexpected(Error::kNonHttpServicePrincipal);
  const Result kUnparsableHost = base::unexpected(Error::kUnparsableHost);

  auto mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
  HttpAuthGSSAPI auth_gssapi(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);

  // Before establishing a context, no principal is reported.
  EXPECT_EQ(auth_gssapi.GetTargetPrincipalHost(), kNotReported);

  PaddedOidDesc hostbased_service_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")}};
  PaddedOidDesc hostbased_service_x_oid = {
      .desc = {6, const_cast<char*>("\x2b\x06\x01\x05\x06\x02")}};
  PaddedOidDesc user_name_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01")}};
  PaddedOidDesc krb5_principal_oid = {
      .desc = {10,
               const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01")}};
  PaddedOidDesc unknown_name_oid = {
      .desc = {6, const_cast<char*>("\x2b\x06\x01\x05\x06\x05")}};
  PaddedOidDesc krb5_mech_oid = {
      .desc = {9, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02")}};
  PaddedOidDesc ntlmssp_mech_oid = {
      .desc = {10,
               const_cast<char*>("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a")}};
  gss_OID_desc empty_oid = {0, nullptr};

  auto test_context = [&](test::GssContextMockImpl context_info,
                          const Result& expected) {
    mock_library = base::MakeRefCounted<test::MockGSSAPILibrary>();
    mock_library->Init(NetLogWithSource());
    HttpAuthGSSAPI auth_sys(mock_library, CHROME_GSS_SPNEGO_MECH_OID_DESC);

    gss_buffer_desc in_buffer = {0, nullptr};
    char out_buf[] = "Mary had a little lamb";
    gss_buffer_desc out_buffer = {std::size(out_buf), out_buf};
    mock_library->ExpectSecurityContext("Negotiate", GSS_S_CONTINUE_NEEDED, 0,
                                        context_info, in_buffer, out_buffer);

    std::string auth_token;
    EXPECT_EQ(
        OK, auth_sys.GenerateAuthToken(
                nullptr, "HTTP@requested.host.com", std::string(), &auth_token,
                NetLogWithSource::Make(NetLogSourceType::NONE),
                base::BindOnce(&UnexpectedCallback)));

    EXPECT_EQ(auth_sys.GetTargetPrincipalHost(), expected);
  };

  // `mech_type` defaults to SPNEGO with `open == 0`, which is what MIT krb5
  // reports until SPNEGO negotiation completes.
  auto test_target = [&](std::string_view mock_spn, const Result& expected,
                         const gss_OID_desc& name_type = {0, nullptr},
                         const gss_OID_desc& mech_type =
                             *CHROME_GSS_SPNEGO_MECH_OID_DESC,
                         int open = 0) {
    SCOPED_TRACE(mock_spn);
    test::GssContextMockImpl context_info(
        "localhost",  // Source name
        "",           // Target name (assigned below to preserve embedded NULs)
        23,           // Lifetime
        mech_type,    // Mechanism
        0,            // Context flags
        1,            // Locally initiated
        open);        // Open
    context_info.targ_name.assign(mock_spn.data(), mock_spn.size());
    context_info.targ_name_type = name_type;
    test_context(context_info, expected);
  };

  // Incomplete round-1 SPNEGO or Heimdal NTLM context with `GSS_C_NO_NAME`.
  {
    test::GssContextMockImpl no_name_context(
        "localhost", "", 23, *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
    no_name_context.has_targ_name = false;
    test_context(no_name_context, kNotReported);
  }

  // NTLMSSP fallback under SPNEGO (`1.3.6.1.4.1.311.2.2.10`) skips Kerberos
  // principal validation whatever name `display_name()` reports.
  test_target("HTTP/canonical.example.com", kNotReported,
              hostbased_service_oid.desc, ntlmssp_mech_oid.desc);
  test_target("HTTP@canonical.example.com", kNotReported, user_name_oid.desc,
              ntlmssp_mech_oid.desc);
  test_target("HTTP@canonical.example.com", kNotReported, empty_oid,
              ntlmssp_mech_oid.desc);

  // `inquire_context()`, `display_name()`, or empty-buffer failures on an
  // established non-NTLM context.
  {
    test::GssContextMockImpl inquire_fail_context(
        "localhost", "HTTP/canonical.example.com@REALM", 23,
        *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
    inquire_fail_context.inquire_context_status = GSS_S_FAILURE;
    test_context(inquire_fail_context, kQueryFailed);
  }
  // Heimdal SPNEGO returns `GSS_S_NO_CONTEXT` for an incomplete round-1 context
  // that only proposed a mechanism list (no optimistic mechanism token).
  {
    test::GssContextMockImpl mech_list_only_context(
        "localhost", "HTTP/canonical.example.com@REALM", 23,
        *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, /*open=*/0);
    mech_list_only_context.inquire_context_status = GSS_S_NO_CONTEXT;
    test_context(mech_list_only_context, kNotReported);
  }
  // `GSS_S_NO_CONTEXT` on an established context still fails closed.
  {
    test::GssContextMockImpl open_no_context(
        "localhost", "HTTP/canonical.example.com@REALM", 23,
        *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, /*open=*/1);
    open_no_context.inquire_context_status = GSS_S_NO_CONTEXT;
    test_context(open_no_context, kQueryFailed);
  }
  {
    test::GssContextMockImpl display_fail_context(
        "localhost", "HTTP/canonical.example.com@REALM", 23,
        *CHROME_GSS_SPNEGO_MECH_OID_DESC, 0, 1, 0);
    display_fail_context.display_targ_name_status = GSS_S_BAD_NAME;
    test_context(display_fail_context, kQueryFailed);
  }
  test_target("", kQueryFailed);
  test_target(std::string_view("\0", 1), kQueryFailed);

  // Standard `HTTP/<host>[:<port>][@<REALM>]` and KDC-aliased `host/<host>`
  // service principals, including trailing dots and trailing NUL terminators.
  test_target("HTTP/canonical.example.com@REALM", "canonical.example.com");
  test_target("http/canonical.EXAMPLE.COM@REALM", "canonical.example.com");
  test_target("HTTP/canonical.example.com", "canonical.example.com");
  test_target("HTTP/canonical.example.com:8443@REALM", "canonical.example.com");
  test_target("host/canonical.example.com@REALM", "canonical.example.com");
  test_target("HOST/canonical.EXAMPLE.COM:8443@REALM", "canonical.example.com");
  test_target("HTTP/canonical.example.com.@REALM", "canonical.example.com");
  test_target(std::string_view("HTTP/canonical.example.com@REALM\0", 33),
              "canonical.example.com");
  test_target("HTTP/canonical.example.com@REALM", "canonical.example.com",
              empty_oid, krb5_mech_oid.desc);

  // `GSS_C_NT_HOSTBASED_SERVICE` (`HTTP@<host>` and `host@<host>`) names.
  test_target("HTTP@canonical.EXAMPLE.COM", "canonical.example.com",
              hostbased_service_oid.desc);
  test_target("http@canonical.example.com:8080", "canonical.example.com",
              hostbased_service_x_oid.desc);
  test_target("host@canonical.EXAMPLE.COM", "canonical.example.com",
              hostbased_service_oid.desc);
  test_target("HOST@canonical.example.com.", "canonical.example.com",
              hostbased_service_x_oid.desc);
  test_target("HTTP@canonical.example.com", "canonical.example.com",
              hostbased_service_oid.desc, *CHROME_GSS_SPNEGO_MECH_OID_DESC,
              /*open=*/1);
  // While SPNEGO hasn't completed (`!open` and `mech_type` is SPNEGO),
  // `gss-ntlmssp` displays `HTTP/<host>` (with a trailing NUL) under
  // `GSS_C_NT_HOSTBASED_SERVICE`, and `HTTP@<host>` under an empty or
  // unrecognized name type is also returned for validation.
  test_target(std::string_view("HTTP/canonical.example.com\0", 27),
              "canonical.example.com", hostbased_service_oid.desc);
  test_target("HTTP@canonical.example.com", "canonical.example.com", empty_oid);
  test_target("host@canonical.example.com", "canonical.example.com", empty_oid);
  test_target("HTTP@canonical.example.com", "canonical.example.com",
              unknown_name_oid.desc);
  test_target("host@canonical.example.com", "canonical.example.com",
              unknown_name_oid.desc);
  // Once the context is open, or under `GSS_C_NT_USER_NAME` /
  // `GSS_KRB5_NT_PRINCIPAL_NAME` (even on SPNEGO Round 1), or under a
  // non-SPNEGO mechanism without a hostbased OID, `HTTP@REALM` and `host@REALM`
  // are rejected as `kNonHttpServicePrincipal`.
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal, empty_oid,
              *CHROME_GSS_SPNEGO_MECH_OID_DESC, /*open=*/1);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal, empty_oid,
              *CHROME_GSS_SPNEGO_MECH_OID_DESC, /*open=*/1);
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal,
              user_name_oid.desc);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal,
              user_name_oid.desc);
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal,
              krb5_principal_oid.desc);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal,
              krb5_principal_oid.desc);
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal, empty_oid,
              krb5_mech_oid.desc);
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal,
              user_name_oid.desc, krb5_mech_oid.desc);
  test_target("HTTP@canonical.example.com", kNonHttpServicePrincipal,
              krb5_principal_oid.desc, krb5_mech_oid.desc);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal, empty_oid,
              krb5_mech_oid.desc);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal,
              user_name_oid.desc, krb5_mech_oid.desc);
  test_target("host@canonical.example.com", kNonHttpServicePrincipal,
              krb5_principal_oid.desc, krb5_mech_oid.desc);

  // Canonical IPv4, IPv6 (bracketed, bare, or differing only in ASCII case),
  // and punycode hosts.
  test_target("HTTP/10.0.0.2@REALM", "10.0.0.2");
  test_target("HTTP/[2001:db8::1]:80@REALM", "[2001:db8::1]");
  test_target("HTTP/2001:db8::1@REALM", "[2001:db8::1]");
  test_target("HTTP/2001:DB8::1@REALM", "[2001:db8::1]");
  test_target("HTTP@2001:db8::1", "[2001:db8::1]", hostbased_service_oid.desc);
  test_target("HTTP/xn--bcher-kva.example.com@REALM",
              "xn--bcher-kva.example.com");

  // Hosts that only compare equal to another host after canonicalization:
  // IPv4 shorthand, non-RFC-5952 IPv6, percent-encoding, and UTS-46 mapping of
  // IDN U-labels, fullwidth letters, and ignored code points (U+00AD).
  test_target("HTTP/0x0a.0.0.2@REALM", kUnparsableHost);
  test_target("HTTP/167772162@REALM", kUnparsableHost);
  test_target("HTTP/010.0.0.2@REALM", kUnparsableHost);
  test_target("HTTP/10.2@REALM", kUnparsableHost);
  test_target("HTTP/[2001:0DB8:0000:0000:0000:0000:0000:0001]:80@REALM",
              kUnparsableHost);
  test_target("HTTP/2001:0DB8:0:0:0:0:0:1@REALM", kUnparsableHost);
  test_target("HTTP/app%2edev.example.com@REALM", kUnparsableHost);
  test_target("HTTP@app%2edev.example.com", kUnparsableHost,
              hostbased_service_oid.desc);
  test_target(
      "HTTP/b\xc3\xbc"
      "cher.example.com@REALM",
      kUnparsableHost);
  test_target(
      "HTTP@b\xc3\xbc"
      "cher.example.com",
      kUnparsableHost, hostbased_service_oid.desc);
  test_target(
      "HTTP/\xef\xbd\x83"
      "anonical.example.com@REALM",
      kUnparsableHost);
  test_target(
      "HTTP/canon\xc2\xad"
      "ical.example.com@REALM",
      kUnparsableHost);

  // Non-HTTP services and UPNs.
  test_target("ldap/canonical.example.com@REALM", kNonHttpServicePrincipal);
  test_target("ldap@canonical.example.com", kNonHttpServicePrincipal,
              hostbased_service_oid.desc);
  test_target("user@REALM", kNonHttpServicePrincipal);
  test_target("canonical.example.com", kNonHttpServicePrincipal);

  // Escaped delimiters, NULs, extra components, and malformed hosts/ports.
  test_target(R"(HTTP/foo\/bar@REALM)", kUnparsableHost);
  test_target(R"(user\/HTTP/canonical.example.com@REALM)", kUnparsableHost);
  test_target(R"(HTTP/canonical.example.com\@evil.com@REALM)", kUnparsableHost);
  test_target("HTTP/canonical.example.com/instance@REALM", kUnparsableHost);
  test_target("HTTP/canonical.example.com@REALM@OTHER", kUnparsableHost);
  test_target(std::string_view("HTTP/canonical.example.com\0@REALM", 33),
              kUnparsableHost);
  test_target(std::string_view("HTTP/canonical.example.com@REALM\0\0", 34),
              kUnparsableHost);
  test_target("HTTP/.@REALM", kUnparsableHost);
  test_target("HTTP/canonical.example.com..@REALM", kUnparsableHost);
  test_target("HTTP/@REALM", kUnparsableHost);
  test_target("HTTP@", kUnparsableHost, hostbased_service_oid.desc);
  test_target("HTTP/canonical.example.com:99999@REALM", kUnparsableHost);
  test_target("HTTP/[not-ipv6]@REALM", kUnparsableHost);
  test_target("HTTP/not:an:ipv6@REALM", kUnparsableHost);
}

}  // namespace net
