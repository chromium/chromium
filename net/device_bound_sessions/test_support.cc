// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/test_support.h"

#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "crypto/evp.h"
#include "crypto/signature_verifier.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net::device_bound_sessions {

namespace {

// Copied from //tools/origin_trials/eftest.key
constexpr std::array<uint8_t, 64> kTestOriginTrialPrivateKey = {
    0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0xe,  0x4,  0xd,  0x43, 0x13,
    0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x2,  0xe2, 0xba,
    0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
    0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
    0xd0, 0xb,  0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
    0x90, 0x8,  0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x4,  0xd0};

std::string GetOriginTrialToken(const GURL& base_url) {
  base::Value::Dict token_data;
  token_data.Set("origin", url::Origin::Create(base_url).Serialize());
  token_data.Set("feature", "DeviceBoundSessionCredentials");
  base::Time expiry = base::Time::Now() + base::Days(1);
  token_data.Set("expiry", static_cast<int>(expiry.InSecondsFSinceUnixEpoch()));

  std::string payload = base::WriteJson(token_data).value_or(std::string());
  std::array<uint8_t, 4> payload_size = base::U32ToBigEndian(payload.size());
  // Version 3
  std::string data_to_sign =
      "\x03" + std::string(payload_size.begin(), payload_size.end()) + payload;

  std::array<uint8_t, ED25519_SIGNATURE_LEN> signature;

  if (!ED25519_sign(signature.data(),
                    reinterpret_cast<const uint8_t*>(data_to_sign.data()),
                    data_to_sign.size(), kTestOriginTrialPrivateKey.data())) {
    return "";
  }

  std::string token = "\x03" + std::string(signature.begin(), signature.end()) +
                      std::string(payload_size.begin(), payload_size.end()) +
                      payload;

  return base::Base64Encode(token);
}

std::optional<std::string> GetQueryParameter(GURL url, const std::string& key) {
  std::string result;
  bool found = net::GetValueForKeyInQuery(url, key, &result);
  if (!found) {
    return std::nullopt;
  }
  return result;
}

std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
    const GURL& base_url,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  if (request.relative_url == "/dbsc_login_page") {
    response->AddCustomHeader("Origin-Trial", GetOriginTrialToken(base_url));
    response->set_content_type("text/html");
    return response;
  } else if (request.relative_url == "/dbsc_required") {
    response->AddCustomHeader(
        "Secure-Session-Registration",
        "(RS256 "
        "ES256);challenge=\"challenge_value\";path=\"dbsc_register_session\"");
    response->set_content_type("text/html");
    return response;
  } else if (request.relative_url == "/dbsc_register_session" ||
             request.relative_url == "/dbsc_refresh_session") {
    response->AddCustomHeader("Set-Cookie",
                              "auth_cookie=abcdef0123;SameSite=None;Secure");

    const auto registration_response =
        base::Value::Dict()
            .Set("session_identifier", "session_id")
            .Set("refresh_url",
                 base_url.Resolve("/dbsc_refresh_session").spec())
            .Set("scope", base::Value::Dict()
                              .Set("include_site", false)
                              .Set("scope_specification",
                                   base::Value::List().Append(
                                       base::Value::Dict()
                                           .Set("type", "exclude")
                                           .Set("domain", base_url.GetHost())
                                           .Set("path", "/favicon.ico"))))
            .Set("credentials",
                 base::Value::List().Append(
                     base::Value::Dict()
                         .Set("type", "cookie")
                         .Set("name", "auth_cookie")
                         .Set("attributes", "SameSite=None; Secure")));

    std::optional<std::string> json = base::WriteJson(registration_response);
    EXPECT_TRUE(json.has_value());
    response->set_content(*json);
    return response;
  } else if (request.relative_url == "/resource_triggered_dbsc_registration") {
    response->AddCustomHeader("Origin-Trial", GetOriginTrialToken(base_url));
    response->set_content_type("text/html");
    response->set_content(base::StringPrintf(
        R"*(<html><body onload="fetch('%s')"></body></html>)*",
        base_url.Resolve("/dbsc_required").spec()));
    return response;
  } else if (request.relative_url.starts_with("/set_early_challenge")) {
    std::string challenge = request.GetURL().GetQuery();
    CHECK(!challenge.empty());
    response->AddCustomHeader("Secure-Session-Challenge",
                              "\"" + challenge + "\";id=\"session_id\"");
    response->set_content_type("text/html");
    return response;
  } else if (request.relative_url.starts_with("/ensure_authenticated")) {
    std::optional<std::string> expected_debug_header =
        GetQueryParameter(request.GetURL(), "debug_header");
    auto debug_header_it = request.headers.find("Secure-Session-Skipped");
    std::string actual_debug_header = debug_header_it == request.headers.end()
                                          ? std::string()
                                          : debug_header_it->second;
    if (expected_debug_header.has_value()) {
      EXPECT_EQ(actual_debug_header, expected_debug_header);
    } else {
      EXPECT_EQ(actual_debug_header, "");
    }
    // We do a very coarse-grained cookie check here rather than parsing
    // cookies.
    auto it = request.headers.find("Cookie");
    if (it == request.headers.end()) {
      response->set_code(net::HTTP_UNAUTHORIZED);
    } else if (it->second.find("auth_cookie") == std::string::npos) {
      response->set_code(net::HTTP_UNAUTHORIZED);
    }
    response->set_content_type("text/html");
    return response;
  }

  return nullptr;
}

std::optional<std::vector<uint8_t>> Es256JwkToSpki(
    const base::Value::Dict& jwk) {
  const std::string* x = jwk.FindString("x");
  const std::string* y = jwk.FindString("y");
  if (!x || !y) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> x_bytes =
      base::Base64UrlDecode(*x, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  std::optional<std::vector<uint8_t>> y_bytes =
      base::Base64UrlDecode(*y, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!x_bytes || !y_bytes) {
    return std::nullopt;
  }

  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!ec_key) {
    return std::nullopt;
  }

  bssl::UniquePtr<BIGNUM> x_bn(
      BN_bin2bn(x_bytes->data(), x_bytes->size(), nullptr));
  bssl::UniquePtr<BIGNUM> y_bn(
      BN_bin2bn(y_bytes->data(), y_bytes->size(), nullptr));
  if (!x_bn || !y_bn) {
    return std::nullopt;
  }

  if (!EC_KEY_set_public_key_affine_coordinates(ec_key.get(), x_bn.get(),
                                                y_bn.get())) {
    return std::nullopt;
  }

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get())) {
    return std::nullopt;
  }

  return crypto::evp::PublicKeyToBytes(pkey.get());
}

std::optional<std::vector<uint8_t>> RawSigToDerSig(
    base::span<const uint8_t> raw_sig) {
  base::span<const uint8_t> r_bytes = raw_sig.first(32u);
  base::span<const uint8_t> s_bytes = raw_sig.subspan(32u);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig) {
    return std::nullopt;
  }

  BN_bin2bn(r_bytes.data(), r_bytes.size(), ecdsa_sig->r);
  BN_bin2bn(s_bytes.data(), s_bytes.size(), ecdsa_sig->s);
  if (!ecdsa_sig->r || !ecdsa_sig->s) {
    return std::nullopt;
  }

  uint8_t* der;
  size_t der_len;
  if (!ECDSA_SIG_to_bytes(&der, &der_len, ecdsa_sig.get())) {
    return std::nullopt;
  }

  bssl::UniquePtr<uint8_t> delete_der(der);
  // SAFETY: `ECDSA_SIG_to_bytes` uses a C-style API.
  auto der_span = UNSAFE_BUFFERS(base::span<const uint8_t>(der, der_len));
  return base::ToVector(der_span);
}

}  // namespace

std::pair<base::span<const uint8_t>, std::string>
GetRS256SpkiAndJwkForTesting() {
  static constexpr uint8_t kSpki[] = {
      0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
      0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
      0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xB8, 0x72, 0x09,
      0xEA, 0xD7, 0x1D, 0x84, 0xD4, 0x9B, 0x22, 0xA1, 0xE8, 0x6A, 0x5F, 0xB1,
      0x6C, 0x03, 0x8B, 0x45, 0xDA, 0xF7, 0xE5, 0xF9, 0x0E, 0x95, 0xF2, 0x43,
      0xE6, 0x38, 0x19, 0x2B, 0x23, 0x29, 0x22, 0xA7, 0xE6, 0xF6, 0xEC, 0xB6,
      0x43, 0x61, 0xFB, 0x5F, 0x4C, 0xEA, 0xB8, 0x77, 0x9E, 0x43, 0x18, 0x76,
      0x2D, 0x16, 0x84, 0x44, 0xA1, 0x29, 0xA6, 0x93, 0xC3, 0x02, 0x1A, 0x11,
      0x1F, 0x2A, 0x3D, 0xDC, 0xE9, 0x44, 0xAE, 0x61, 0x9F, 0xC1, 0xDE, 0xDB,
      0xEA, 0x04, 0x01, 0xE5, 0x2A, 0xAB, 0x55, 0x67, 0xA6, 0x3D, 0xB3, 0x97,
      0xA7, 0x15, 0x02, 0x7B, 0xCA, 0x4C, 0x44, 0xA1, 0x4D, 0x2B, 0xB9, 0xBE,
      0xE3, 0x96, 0xC3, 0x17, 0x42, 0x4D, 0xCA, 0x60, 0xA8, 0x30, 0xC5, 0xD0,
      0xC9, 0x64, 0xD8, 0x39, 0xB0, 0x91, 0xA8, 0x22, 0x94, 0xA0, 0x61, 0x6B,
      0xE6, 0xF4, 0xD9, 0x64, 0x82, 0x17, 0xB3, 0x27, 0xF6, 0xDA, 0x3D, 0xEF,
      0xD8, 0x05, 0x87, 0x90, 0x1C, 0xE5, 0xB5, 0xB3, 0xB5, 0x41, 0x0E, 0xFC,
      0x45, 0xAD, 0x64, 0xCA, 0xB1, 0x39, 0x10, 0x63, 0x32, 0x67, 0x7E, 0x88,
      0x95, 0x0F, 0xFD, 0x8E, 0xCE, 0x5A, 0xF7, 0x5B, 0x60, 0x85, 0xA3, 0xB0,
      0x48, 0x26, 0x10, 0x19, 0xDA, 0x0A, 0xC5, 0xD3, 0x78, 0x6E, 0x0B, 0x86,
      0x78, 0x55, 0xB4, 0xA8, 0xFD, 0x1C, 0x81, 0x8A, 0x33, 0x18, 0x40, 0x1A,
      0x5F, 0x75, 0x87, 0xD1, 0x05, 0x2B, 0x2B, 0x53, 0x1F, 0xAD, 0x8E, 0x22,
      0xB3, 0xEE, 0x1C, 0xA1, 0x03, 0x97, 0xF1, 0xE0, 0x88, 0x0F, 0x98, 0xAF,
      0x05, 0x37, 0xB3, 0xC3, 0x95, 0x1C, 0x34, 0xDE, 0x39, 0xEB, 0x85, 0x12,
      0xEC, 0x3D, 0x77, 0x27, 0xA7, 0x5C, 0xEA, 0x39, 0x24, 0xD5, 0xE9, 0x49,
      0xCF, 0x97, 0x88, 0x4A, 0xF4, 0x01, 0x4F, 0xA4, 0x7E, 0x77, 0x57, 0x7F,
      0x73, 0x02, 0x03, 0x01, 0x00, 0x01};

  static constexpr char kJwkTemplate[] = R"json({
      "kty": "RSA",
      "n": "<n>",
      "e": "AQAB"})json";

  static constexpr char kRsaN[] =
      "uHIJ6tcdhNSbIqHoal-xbAOLRdr35fkOlfJD5jgZKyMpIqfm9uy2Q2H7X0zquHeeQxh2LRaE"
      "RKEpppPDAhoRHyo93OlErmGfwd7b6gQB5SqrVWemPbOXpxUCe8pMRKFNK7m-45bDF0JNymCo"
      "MMXQyWTYObCRqCKUoGFr5vTZZIIXsyf22j3v2AWHkBzltbO1QQ78Ra1kyrE5EGMyZ36IlQ_9"
      "js5a91tghaOwSCYQGdoKxdN4bguGeFW0qP0cgYozGEAaX3WH0QUrK1MfrY4is-4coQOX8eCI"
      "D5ivBTezw5UcNN4564US7D13J6dc6jkk1elJz5eISvQBT6R-d1d_cw";

  std::string jwk = kJwkTemplate;
  base::ReplaceFirstSubstringAfterOffset(&jwk, 0, "<n>", kRsaN);

  return {kSpki, jwk};
}

// Copied from //docs/origin_trials_integration.md
constexpr char kTestOriginTrialPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

EmbeddedTestServer::HandleRequestCallback GetTestRequestHandler(
    const GURL& base_url) {
  return base::BindRepeating(&RequestHandler, base_url);
}

bool VerifyEs256Jwt(std::string_view jwt) {
  // Parse JWT.
  std::vector<std::string> jwt_sections =
      base::SplitString(jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3u) {
    return false;
  }

  const std::string& header64 = jwt_sections[0];
  const std::string& payload64 = jwt_sections[1];
  const std::string& signature64 = jwt_sections[2];

  std::string header, payload, signature;
  if (!base::Base64UrlDecode(
          header64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &header) ||
      !base::Base64UrlDecode(
          payload64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &payload) ||
      !base::Base64UrlDecode(signature64,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &signature)) {
    return false;
  }

  const std::optional<base::Value::Dict> header_json =
      base::JSONReader::ReadDict(header, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!header_json) {
    return false;
  }
  const std::optional<base::Value::Dict> payload_json =
      base::JSONReader::ReadDict(payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!payload_json) {
    return false;
  }

  // Extract the JWK.
  const base::Value::Dict* jwk = header_json->FindDict("jwk");
  if (!jwk) {
    return false;
  }

  // `crypto::SignatureVerifier` expects the public key in the
  // SubjectPublicKeyInfo format and the signature in the DER format, so convert
  // accordingly.
  std::optional<std::vector<uint8_t>> spki = Es256JwkToSpki(*jwk);
  if (!spki) {
    return false;
  }

  std::optional<std::vector<uint8_t>> der_sig =
      RawSigToDerSig(base::as_byte_span((signature)));
  if (!der_sig) {
    return false;
  }

  crypto::SignatureVerifier verifier;
  verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256, der_sig.value(),
                      spki.value());
  verifier.VerifyUpdate(
      base::as_byte_span(base::StrCat({header64, ".", payload64})));
  return verifier.VerifyFinal();
}

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
// static
ScopedTestRegistrationFetcher ScopedTestRegistrationFetcher::CreateWithSuccess(
    std::string_view session_id,
    std::string_view refresh_url_string,
    std::string_view origin_string) {
  return ScopedTestRegistrationFetcher(base::BindRepeating(
      [](const std::string& session_id, const std::string& refresh_url_string,
         const std::string& origin_string,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        std::vector<SessionParams::Credential> cookie_credentials;
        cookie_credentials.push_back(
            SessionParams::Credential{"test_cookie", "secure"});
        SessionParams::Scope scope;
        scope.include_site = true;
        scope.origin = origin_string;
        std::move(callback).Run(
            nullptr,
            RegistrationResult(Session::CreateIfValid(SessionParams(
                session_id, GURL(refresh_url_string), refresh_url_string,
                std::move(scope), std::move(cookie_credentials),
                unexportable_keys::UnexportableKeyId(),
                /*allowed_refresh_initiators=*/{}))));
      },
      std::string(session_id), std::string(refresh_url_string),
      std::string(origin_string)));
}

// static
ScopedTestRegistrationFetcher ScopedTestRegistrationFetcher::CreateWithFailure(
    SessionError::ErrorType error_type,
    std::string_view refresh_url_string) {
  return ScopedTestRegistrationFetcher(base::BindRepeating(
      [](SessionError::ErrorType error_type, const GURL& refresh_url,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        return std::move(callback).Run(
            nullptr, RegistrationResult(SessionError{error_type}));
      },
      error_type, GURL(refresh_url_string)));
}

// static
ScopedTestRegistrationFetcher
ScopedTestRegistrationFetcher::CreateWithTermination(
    std::string_view session_id,
    std::string_view refresh_url_string) {
  return ScopedTestRegistrationFetcher(base::BindRepeating(
      [](const std::string& session_id, const std::string& refresh_url_string,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        return std::move(callback).Run(
            nullptr, RegistrationResult(SessionError{
                         SessionError::kServerRequestedTermination}));
      },
      std::string(session_id), std::string(refresh_url_string)));
}

ScopedTestRegistrationFetcher::ScopedTestRegistrationFetcher(
    RegistrationFetcher::FetcherType fetcher)
    : fetcher_(fetcher) {
  RegistrationFetcher::SetFetcherForTesting(&fetcher_);
}
ScopedTestRegistrationFetcher::~ScopedTestRegistrationFetcher() {
  RegistrationFetcher::SetFetcherForTesting(nullptr);
}
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

}  // namespace net::device_bound_sessions
