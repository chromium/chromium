// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/gaia/oauth_request_signer.h"

#include <stddef.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>

#include "base/base64.h"
#include "base/check.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/hmac.h"
#include "url/gurl.h"

namespace {

const int kHexBase = 16;
char kHexDigits[] = "0123456789ABCDEF";
const size_t kHmacDigestLength = 20;
const int kMaxNonceLength = 30;
const int kMinNonceLength = 15;

const char kOAuthConsumerKeyLabel[] = "oauth_consumer_key";
const char kOAuthNonceCharacters[] =
    "abcdefghijklmnopqrstuvwyz"
    "ABCDEFGHIJKLMNOPQRSTUVWYZ"
    "0123456789_";
const char kOAuthNonceLabel[] = "oauth_nonce";
const char kOAuthSignatureLabel[] = "oauth_signature";
const char kOAuthSignatureMethodLabel[] = "oauth_signature_method";
const char kOAuthTimestampLabel[] = "oauth_timestamp";
const char kOAuthTokenLabel[] = "oauth_token";
const char kOAuthVersion[] = "1.0";
const char kOAuthVersionLabel[] = "oauth_version";

enum ParseQueryState {
  START_STATE,
  KEYWORD_STATE,
  VALUE_STATE,
};

const std::string HttpMethodName(OAuthRequestSigner::HttpMethod method) {
  switch (method) {
    case OAuthRequestSigner::GET_METHOD:
      return "GET";
    case OAuthRequestSigner::POST_METHOD:
      return "POST";
  }
  NOTREACHED();
}

const std::string SignatureMethodName(
    OAuthRequestSigner::SignatureMethod method) {
  switch (method) {
    case OAuthRequestSigner::HMAC_SHA1_SIGNATURE:
      return "HMAC-SHA1";
    case OAuthRequestSigner::RSA_SHA1_SIGNATURE:
      return "RSA-SHA1";
    case OAuthRequestSigner::PLAINTEXT_SIGNATURE:
      return "PLAINTEXT";
  }
  NOTREACHED();
}

std::string BuildBaseString(const GURL& request_base_url,
                            OAuthRequestSigner::HttpMethod http_method,
                            const std::string& base_parameters) {
  return base::StringPrintf("%s&%s&%s",
                            HttpMethodName(http_method).c_str(),
                            OAuthRequestSigner::Encode(
                                request_base_url.spec()).c_str(),
                            OAuthRequestSigner::Encode(
                                base_parameters).c_str());
}

std::string BuildBaseStringParameters(
    const OAuthRequestSigner::Parameters& parameters) {
  std::string result;
  OAuthRequestSigner::Parameters::const_iterator cursor;
  OAuthRequestSigner::Parameters::const_iterator limit;
  bool first = true;
  for (cursor = parameters.begin(), limit = parameters.end();
       cursor != limit;
       ++cursor) {
    if (first)
      first = false;
    else
      result += '&';
    result += OAuthRequestSigner::Encode(cursor->first);
    result += '=';
    result += OAuthRequestSigner::Encode(cursor->second);
  }
  return result;
}

std::string GenerateNonce() {
  char result[kMaxNonceLength + 1];
  int length = base::RandUint64() % (kMaxNonceLength - kMinNonceLength + 1) +
      kMinNonceLength;
  result[length] = '\0';
  for (int index = 0; index < length; ++index)
    result[index] = kOAuthNonceCharacters[
        base::RandUint64() % (sizeof(kOAuthNonceCharacters) - 1)];
  return result;
}

std::string GenerateTimestamp() {
  return base::StringPrintf(
      "%" PRId64,
      (base::Time::NowFromSystemTime() - base::Time::UnixEpoch()).InSeconds());
}

// Creates a string-to-string, keyword-value map from a parameter/query string
// that uses ampersand (&) to seperate paris and equals (=) to seperate
// keyword from value.
bool ParseQuery(const std::string& query,
                OAuthRequestSigner::Parameters* parameters_result) {
  std::string::const_iterator cursor;
  std::string keyword;
  std::string::const_iterator limit;
  OAuthRequestSigner::Parameters parameters;
  ParseQueryState state;
  std::string value;

  state = START_STATE;
  for (cursor = query.begin(), limit = query.end();
       cursor != limit;
       ++cursor) {
    char character = *cursor;
    switch (state) {
      case KEYWORD_STATE:
        switch (character) {
          case '&':
            parameters[keyword] = value;
            keyword = "";
            value = "";
            state = START_STATE;
            break;
          case '=':
            state = VALUE_STATE;
            break;
          default:
            keyword += character;
        }
        break;
      case START_STATE:
        switch (character) {
          case '&':  // Intentionally falling through
          case '=':
            return false;
          default:
            keyword += character;
            state = KEYWORD_STATE;
        }
        break;
      case VALUE_STATE:
        switch (character) {
          case '=':
            return false;
          case '&':
            parameters[keyword] = value;
            keyword = "";
            value = "";
            state = START_STATE;
            break;
          default:
            value += character;
        }
        break;
    }
  }
  switch (state) {
    case START_STATE:
      break;
    case KEYWORD_STATE:  // Intentionally falling through
    case VALUE_STATE:
      parameters[keyword] = value;
      break;
    default:
      NOTREACHED();
  }
  *parameters_result = parameters;
  return true;
}

// Creates the value for the oauth_signature parameter when the
// oauth_signature_method is HMAC-SHA1.
bool SignHmacSha1(const std::string& text,
                  const std::string& key,
                  std::string* signature_return) {
  crypto::HMAC hmac(crypto::HMAC::SHA1);
  DCHECK(hmac.DigestLength() == kHmacDigestLength);
  unsigned char digest[kHmacDigestLength];
  bool result = hmac.Init(key) &&
      hmac.Sign(text, digest, kHmacDigestLength);
  if (result) {
    *signature_return = base::Base64Encode(digest);
  }
  return result;
}

// Creates the value for the oauth_signature parameter when the
// oauth_signature_method is PLAINTEXT.
//
// Not yet implemented, and might never be.
bool SignPlaintext(const std::string& text,
                   const std::string& key,
                   std::string* result) {
  NOTIMPLEMENTED();
  return false;
}

// Creates the value for the oauth_signature parameter when the
// oauth_signature_method is RSA-SHA1.
//
// Not yet implemented, and might never be.
bool SignRsaSha1(const std::string& text,
                 const std::string& key,
                 std::string* result) {
  NOTIMPLEMENTED();
  return false;
}

// Adds parameters that are required by OAuth added as needed to |parameters|.
void PrepareParameters(OAuthRequestSigner::Parameters* parameters,
                       OAuthRequestSigner::SignatureMethod signature_method,
                       OAuthRequestSigner::HttpMethod http_method,
                       const std::string& consumer_key,
                       const std::string& token_key) {
  if (parameters->find(kOAuthNonceLabel) == parameters->end())
    (*parameters)[kOAuthNonceLabel] = GenerateNonce();

  if (parameters->find(kOAuthTimestampLabel) == parameters->end())
    (*parameters)[kOAuthTimestampLabel] = GenerateTimestamp();

  (*parameters)[kOAuthConsumerKeyLabel] = consumer_key;
  (*parameters)[kOAuthSignatureMethodLabel] =
      SignatureMethodName(signature_method);
  (*parameters)[kOAuthTokenLabel] = token_key;
  (*parameters)[kOAuthVersionLabel] = kOAuthVersion;
}

// Implements shared signing logic, generating the signature and storing it in
// |parameters|. Returns true if the signature has been generated succesfully.
bool SignParameters(const GURL& request_base_url,
                    OAuthRequestSigner::SignatureMethod signature_method,
                    OAuthRequestSigner::HttpMethod http_method,
                    const std::string& consumer_key,
                    const std::string& consumer_secret,
                    const std::string& token_key,
                    const std::string& token_secret,
                    OAuthRequestSigner::Parameters* parameters) {
  DCHECK(request_base_url.is_valid());
  PrepareParameters(parameters, signature_method, http_method,
                    consumer_key, token_key);
  std::string base_parameters = BuildBaseStringParameters(*parameters);
  std::string base = BuildBaseString(request_base_url, http_method,
                                     base_parameters);
  std::string key = consumer_secret + '&' + token_secret;
  bool is_signed = false;
  std::string signature;
  switch (signature_method) {
    case OAuthRequestSigner::HMAC_SHA1_SIGNATURE:
      is_signed = SignHmacSha1(base, key, &signature);
      break;
    case OAuthRequestSigner::RSA_SHA1_SIGNATURE:
      is_signed = SignRsaSha1(base, key, &signature);
      break;
    case OAuthRequestSigner::PLAINTEXT_SIGNATURE:
      is_signed = SignPlaintext(base, key, &signature);
      break;
    default:
      NOTREACHED();
  }
  if (is_signed)
    (*parameters)[kOAuthSignatureLabel] = signature;
  return is_signed;
}


}  // namespace

// static
bool OAuthRequestSigner::Decode(const std::string& text,
                                std::string* decoded_text) {
  std::string accumulator;
  std::string::const_iterator cursor;
  std::string::const_iterator limit;
  for (limit = text.end(), cursor = text.begin(); cursor != limit; ++cursor) {
    char character = *cursor;
    if (character == '%') {
      ++cursor;
      if (cursor == limit)
        return false;
      char* first = strchr(kHexDigits, *cursor);
      if (!first)
        return false;
      int high = first - kHexDigits;
      DCHECK(high >= 0 && high < kHexBase);

      ++cursor;
      if (cursor == limit)
        return false;
      char* second = strchr(kHexDigits, *cursor);
      if (!second)
        return false;
      int low = second - kHexDigits;
      DCHECK(low >= 0 || low < kHexBase);

      char decoded = static_cast<char>(high * kHexBase + low);
      DCHECK(!(base::IsAsciiAlpha(decoded) || base::IsAsciiDigit(decoded)));
      DCHECK(!(decoded && strchr("-._~", decoded)));
      accumulator += decoded;
    } else {
      accumulator += character;
    }
  }
  *decoded_text = accumulator;
  return true;
}

// static
std::string OAuthRequestSigner::Encode(const std::string& text) {
  std::string result;
  std::string::const_iterator cursor;
  std::string::const_iterator limit;
  for (limit = text.end(), cursor = text.begin(); cursor != limit; ++cursor) {
    char character = *cursor;
    if (base::IsAsciiAlpha(character) || base::IsAsciiDigit(character)) {
      result += character;
    } else {
      switch (character) {
        case '-':
        case '.':
        case '_':
        case '~':
          result += character;
          break;
        default:
          unsigned char byte = static_cast<unsigned char>(character);
          result = result + '%' + kHexDigits[byte / kHexBase] +
              kHexDigits[byte % kHexBase];
      }
    }
  }
  return result;
}

// static
bool OAuthRequestSigner::ParseAndSign(const GURL& request_url_with_parameters,
                                      SignatureMethod signature_method,
                                      HttpMethod http_method,
                                      const std::string& consumer_key,
                                      const std::string& consumer_secret,
                                      const std::string& token_key,
                                      const std::string& token_secret,
                                      std::string* result) {
  DCHECK(request_url_with_parameters.is_valid());
  Parameters parameters;
  if (request_url_with_parameters.has_query()) {
    const std::string& query = request_url_with_parameters.query();
    if (!query.empty()) {
      if (!ParseQuery(query, &parameters))
        return false;
    }
  }
  std::string spec = request_url_with_parameters.spec();
  std::string url_without_parameters = spec;
  std::string::size_type question = spec.find("?");
  if (question != std::string::npos)
    url_without_parameters = spec.substr(0,question);
  return SignURL(GURL(url_without_parameters), parameters, signature_method,
                 http_method, consumer_key, consumer_secret, token_key,
                 token_secret, result);
}

// static
bool OAuthRequestSigner::SignURL(
    const GURL& request_base_url,
    const Parameters& request_parameters,
    SignatureMethod signature_method,
    HttpMethod http_method,
    const std::string& consumer_key,
    const std::string& consumer_secret,
    const std::string& token_key,
    const std::string& token_secret,
    std::string* signed_text_return) {
  DCHECK(request_base_url.is_valid());
  Parameters parameters(request_parameters);
  bool is_signed = SignParameters(request_base_url, signature_method,
                                  http_method, consumer_key, consumer_secret,
                                  token_key, token_secret, &parameters);
  if (is_signed) {
    std::string signed_text;
    switch (http_method) {
      case GET_METHOD:
        signed_text = request_base_url.spec() + '?';
        [[fallthrough]];
      case POST_METHOD:
        signed_text += BuildBaseStringParameters(parameters);
        break;
      default:
        NOTREACHED();
    }
    *signed_text_return = signed_text;
  }
  return is_signed;
}

// static
bool OAuthRequestSigner::SignAuthHeader(
    const GURL& request_base_url,
    const Parameters& request_parameters,
    SignatureMethod signature_method,
    HttpMethod http_method,
    const std::string& consumer_key,
    const std::string& consumer_secret,
    const std::string& token_key,
    const std::string& token_secret,
    std::string* signed_text_return) {
  DCHECK(request_base_url.is_valid());
  Parameters parameters(request_parameters);
  bool is_signed = SignParameters(request_base_url, signature_method,
                                  http_method, consumer_key, consumer_secret,
                                  token_key, token_secret, &parameters);
  if (is_signed) {
    std::string signed_text = "OAuth ";
    bool first = true;
    for (Parameters::const_iterator param = parameters.begin();
         param != parameters.end();
         ++param) {
      if (first)
        first = false;
      else
        signed_text += ", ";
      signed_text +=
          base::StringPrintf(
              "%s=\"%s\"",
              OAuthRequestSigner::Encode(param->first).c_str(),
              OAuthRequestSigner::Encode(param->second).c_str());
    }
    *signed_text_return = signed_text;
  }
  return is_signed;
}
