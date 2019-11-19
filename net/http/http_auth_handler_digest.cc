// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_digest.h"

#include <string>

#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_string_util.h"
#include "net/base/url_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_request_info.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {

// Digest authentication is specified in RFC 2617.
// The expanded derivations are listed in the tables below.

//==========+==========+==========================================+
//    qop   |algorithm |               response                   |
//==========+==========+==========================================+
//    ?     |  ?, md5, | MD5(MD5(A1):nonce:MD5(A2))               |
//          | md5-sess |                                          |
//--------- +----------+------------------------------------------+
//   auth,  |  ?, md5, | MD5(MD5(A1):nonce:nc:cnonce:qop:MD5(A2)) |
// auth-int | md5-sess |                                          |
//==========+==========+==========================================+
//    qop   |algorithm |                  A1                      |
//==========+==========+==========================================+
//          | ?, md5   | user:realm:password                      |
//----------+----------+------------------------------------------+
//          | md5-sess | MD5(user:realm:password):nonce:cnonce    |
//==========+==========+==========================================+
//    qop   |algorithm |                  A2                      |
//==========+==========+==========================================+
//  ?, auth |          | req-method:req-uri                       |
//----------+----------+------------------------------------------+
// auth-int |          | req-method:req-uri:MD5(req-entity-body)  |
//=====================+==========================================+

HttpAuthHandlerDigest::NonceGenerator::NonceGenerator() = default;

HttpAuthHandlerDigest::NonceGenerator::~NonceGenerator() = default;

HttpAuthHandlerDigest::DynamicNonceGenerator::DynamicNonceGenerator() = default;

std::string HttpAuthHandlerDigest::DynamicNonceGenerator::GenerateNonce()
    const {
  // This is how mozilla generates their cnonce -- a 16 digit hex string.
  static const char domain[] = "0123456789abcdef";
  std::string cnonce;
  cnonce.reserve(16);
  for (int i = 0; i < 16; ++i)
    cnonce.push_back(domain[base::RandInt(0, 15)]);
  return cnonce;
}

HttpAuthHandlerDigest::FixedNonceGenerator::FixedNonceGenerator(
    const std::string& nonce)
    : nonce_(nonce) {
}

std::string HttpAuthHandlerDigest::FixedNonceGenerator::GenerateNonce() const {
  return nonce_;
}

HttpAuthHandlerDigest::Factory::Factory()
    : nonce_generator_(new DynamicNonceGenerator()) {
}

HttpAuthHandlerDigest::Factory::~Factory() = default;

void HttpAuthHandlerDigest::Factory::set_nonce_generator(
    const NonceGenerator* nonce_generator) {
  nonce_generator_.reset(nonce_generator);
}

int HttpAuthHandlerDigest::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerDigest(digest_nonce_count, nonce_generator_.get()));
  if (!tmp_handler->InitFromChallenge(challenge, target, ssl_info, origin,
                                      net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

bool HttpAuthHandlerDigest::Init(HttpAuthChallengeTokenizer* challenge,
                                 const SSLInfo& ssl_info) {
  return ParseChallenge(challenge);
}

int HttpAuthHandlerDigest::GenerateAuthTokenImpl(
    const AuthCredentials* credentials,
    const HttpRequestInfo* request,
    CompletionOnceCallback callback,
    std::string* auth_token) {
  // Generate a random client nonce.
  std::string cnonce = nonce_generator_->GenerateNonce();

  // Extract the request method and path -- the meaning of 'path' is overloaded
  // in certain cases, to be a hostname.
  std::string method;
  std::string path;
  GetRequestMethodAndPath(request, &method, &path);

  *auth_token =
      AssembleCredentials(method, path, *credentials, cnonce, nonce_count_);
  return OK;
}

HttpAuth::AuthorizationResult HttpAuthHandlerDigest::HandleAnotherChallengeImpl(
    HttpAuthChallengeTokenizer* challenge) {
  // Even though Digest is not connection based, a "second round" is parsed
  // to differentiate between stale and rejected responses.
  // Note that the state of the current handler is not mutated - this way if
  // there is a rejection the realm hasn't changed.
  if (challenge->auth_scheme() != kDigestAuthScheme)
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;

  HttpUtil::NameValuePairsIterator parameters = challenge->param_pairs();

  // Try to find the "stale" value, and also keep track of the realm
  // for the new challenge.
  std::string original_realm;
  while (parameters.GetNext()) {
    if (base::LowerCaseEqualsASCII(parameters.name_piece(), "stale")) {
      if (base::LowerCaseEqualsASCII(parameters.value_piece(), "true"))
        return HttpAuth::AUTHORIZATION_RESULT_STALE;
    } else if (base::LowerCaseEqualsASCII(parameters.name_piece(), "realm")) {
      original_realm = parameters.value();
    }
  }
  return (original_realm_ != original_realm) ?
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM :
      HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

HttpAuthHandlerDigest::HttpAuthHandlerDigest(
    int nonce_count, const NonceGenerator* nonce_generator)
    : stale_(false),
      algorithm_(ALGORITHM_UNSPECIFIED),
      qop_(QOP_UNSPECIFIED),
      nonce_count_(nonce_count),
      nonce_generator_(nonce_generator) {
  DCHECK(nonce_generator_);
}

HttpAuthHandlerDigest::~HttpAuthHandlerDigest() = default;

// The digest challenge header looks like:
//   WWW-Authenticate: Digest
//     [realm="<realm-value>"]
//     nonce="<nonce-value>"
//     [domain="<list-of-URIs>"]
//     [opaque="<opaque-token-value>"]
//     [stale="<true-or-false>"]
//     [algorithm="<digest-algorithm>"]
//     [qop="<list-of-qop-values>"]
//     [<extension-directive>]
//
// Note that according to RFC 2617 (section 1.2) the realm is required.
// However we allow it to be omitted, in which case it will default to the
// empty string.
//
// This allowance is for better compatibility with webservers that fail to
// send the realm (See http://crbug.com/20984 for an instance where a
// webserver was not sending the realm with a BASIC challenge).
bool HttpAuthHandlerDigest::ParseChallenge(
    HttpAuthChallengeTokenizer* challenge) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_DIGEST;
  score_ = 2;
  properties_ = ENCRYPTS_IDENTITY;

  // Initialize to defaults.
  stale_ = false;
  algorithm_ = ALGORITHM_UNSPECIFIED;
  qop_ = QOP_UNSPECIFIED;
  realm_ = original_realm_ = nonce_ = domain_ = opaque_ = std::string();

  // FAIL -- Couldn't match auth-scheme.
  if (challenge->auth_scheme() != kDigestAuthScheme)
    return false;

  HttpUtil::NameValuePairsIterator parameters = challenge->param_pairs();

  // Loop through all the properties.
  while (parameters.GetNext()) {
    // FAIL -- couldn't parse a property.
    if (!ParseChallengeProperty(parameters.name_piece(),
                                parameters.value_piece()))
      return false;
  }

  // Check if tokenizer failed.
  if (!parameters.valid())
    return false;

  // Check that a minimum set of properties were provided.
  if (nonce_.empty())
    return false;

  return true;
}

bool HttpAuthHandlerDigest::ParseChallengeProperty(base::StringPiece name,
                                                   base::StringPiece value) {
  if (base::LowerCaseEqualsASCII(name, "realm")) {
    std::string realm;
    if (!ConvertToUtf8AndNormalize(value, kCharsetLatin1, &realm))
      return false;
    realm_ = realm;
    original_realm_ = value.as_string();
  } else if (base::LowerCaseEqualsASCII(name, "nonce")) {
    nonce_ = value.as_string();
  } else if (base::LowerCaseEqualsASCII(name, "domain")) {
    domain_ = value.as_string();
  } else if (base::LowerCaseEqualsASCII(name, "opaque")) {
    opaque_ = value.as_string();
  } else if (base::LowerCaseEqualsASCII(name, "stale")) {
    // Parse the stale boolean.
    stale_ = base::LowerCaseEqualsASCII(value, "true");
  } else if (base::LowerCaseEqualsASCII(name, "algorithm")) {
    // Parse the algorithm.
    if (base::LowerCaseEqualsASCII(value, "md5")) {
      algorithm_ = ALGORITHM_MD5;
    } else if (base::LowerCaseEqualsASCII(value, "md5-sess")) {
      algorithm_ = ALGORITHM_MD5_SESS;
    } else {
      DVLOG(1) << "Unknown value of algorithm";
      return false;  // FAIL -- unsupported value of algorithm.
    }
  } else if (base::LowerCaseEqualsASCII(name, "qop")) {
    // Parse the comma separated list of qops.
    // auth is the only supported qop, and all other values are ignored.
    //
    // TODO(https://crbug.com/820198): Remove this copy when
    // HttpUtil::ValuesIterator can take a StringPiece.
    std::string value_str = value.as_string();
    HttpUtil::ValuesIterator qop_values(value_str.begin(), value_str.end(),
                                        ',');
    qop_ = QOP_UNSPECIFIED;
    while (qop_values.GetNext()) {
      if (base::LowerCaseEqualsASCII(qop_values.value_piece(), "auth")) {
        qop_ = QOP_AUTH;
        break;
      }
    }
  } else {
    DVLOG(1) << "Skipping unrecognized digest property";
    // TODO(eroman): perhaps we should fail instead of silently skipping?
  }
  return true;
}

// static
std::string HttpAuthHandlerDigest::QopToString(QualityOfProtection qop) {
  switch (qop) {
    case QOP_UNSPECIFIED:
      return std::string();
    case QOP_AUTH:
      return "auth";
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
std::string HttpAuthHandlerDigest::AlgorithmToString(
    DigestAlgorithm algorithm) {
  switch (algorithm) {
    case ALGORITHM_UNSPECIFIED:
      return std::string();
    case ALGORITHM_MD5:
      return "MD5";
    case ALGORITHM_MD5_SESS:
      return "MD5-sess";
    default:
      NOTREACHED();
      return std::string();
  }
}

void HttpAuthHandlerDigest::GetRequestMethodAndPath(
    const HttpRequestInfo* request,
    std::string* method,
    std::string* path) const {
  DCHECK(request);

  const GURL& url = request->url;

  if (target_ == HttpAuth::AUTH_PROXY &&
      (url.SchemeIs("https") || url.SchemeIsWSOrWSS())) {
    *method = "CONNECT";
    *path = GetHostAndPort(url);
  } else {
    *method = request->method;
    *path = url.PathForRequest();
  }
}

std::string HttpAuthHandlerDigest::AssembleResponseDigest(
    const std::string& method,
    const std::string& path,
    const AuthCredentials& credentials,
    const std::string& cnonce,
    const std::string& nc) const {
  // ha1 = MD5(A1)
  // TODO(eroman): is this the right encoding?
  std::string ha1 = base::MD5String(base::UTF16ToUTF8(credentials.username()) +
                                    ":" + original_realm_ + ":" +
                                    base::UTF16ToUTF8(credentials.password()));
  if (algorithm_ == HttpAuthHandlerDigest::ALGORITHM_MD5_SESS)
    ha1 = base::MD5String(ha1 + ":" + nonce_ + ":" + cnonce);

  // ha2 = MD5(A2)
  // TODO(eroman): need to add MD5(req-entity-body) for qop=auth-int.
  std::string ha2 = base::MD5String(method + ":" + path);

  std::string nc_part;
  if (qop_ != HttpAuthHandlerDigest::QOP_UNSPECIFIED) {
    nc_part = nc + ":" + cnonce + ":" + QopToString(qop_) + ":";
  }

  return base::MD5String(ha1 + ":" + nonce_ + ":" + nc_part + ha2);
}

std::string HttpAuthHandlerDigest::AssembleCredentials(
    const std::string& method,
    const std::string& path,
    const AuthCredentials& credentials,
    const std::string& cnonce,
    int nonce_count) const {
  // the nonce-count is an 8 digit hex string.
  std::string nc = base::StringPrintf("%08x", nonce_count);

  // TODO(eroman): is this the right encoding?
  std::string authorization = (std::string("Digest username=") +
                               HttpUtil::Quote(
                                   base::UTF16ToUTF8(credentials.username())));
  authorization += ", realm=" + HttpUtil::Quote(original_realm_);
  authorization += ", nonce=" + HttpUtil::Quote(nonce_);
  authorization += ", uri=" + HttpUtil::Quote(path);

  if (algorithm_ != ALGORITHM_UNSPECIFIED) {
    authorization += ", algorithm=" + AlgorithmToString(algorithm_);
  }
  std::string response = AssembleResponseDigest(method, path, credentials,
                                                cnonce, nc);
  // No need to call HttpUtil::Quote() as the response digest cannot contain
  // any characters needing to be escaped.
  authorization += ", response=\"" + response + "\"";

  if (!opaque_.empty()) {
    authorization += ", opaque=" + HttpUtil::Quote(opaque_);
  }
  if (qop_ != QOP_UNSPECIFIED) {
    // TODO(eroman): Supposedly IIS server requires quotes surrounding qop.
    authorization += ", qop=" + QopToString(qop_);
    authorization += ", nc=" + nc;
    authorization += ", cnonce=" + HttpUtil::Quote(cnonce);
  }

  return authorization;
}

}  // namespace net
