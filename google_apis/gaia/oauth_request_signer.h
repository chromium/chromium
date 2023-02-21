// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH_REQUEST_SIGNER_H_
#define GOOGLE_APIS_GAIA_OAUTH_REQUEST_SIGNER_H_

#include <map>
#include <string>

#include "base/component_export.h"

class GURL;

// Implements the OAuth request signing process as described here:
//   http://oauth.net/core/1.0/#signing_process
//
// NOTE: Currently the only supported SignatureMethod is HMAC_SHA1_SIGNATURE
class COMPONENT_EXPORT(GOOGLE_APIS) OAuthRequestSigner {
 public:
  enum SignatureMethod {
    HMAC_SHA1_SIGNATURE,
    RSA_SHA1_SIGNATURE,
    PLAINTEXT_SIGNATURE
  };

  enum HttpMethod {
    GET_METHOD,
    POST_METHOD
  };

  typedef std::map<std::string,std::string> Parameters;

  OAuthRequestSigner() = delete;
  OAuthRequestSigner(const OAuthRequestSigner&) = delete;
  OAuthRequestSigner& operator=(const OAuthRequestSigner&) = delete;

  // Percent encoding and decoding for OAuth.
  //
  // The form of percent encoding used for OAuth request signing is very
  // specific and strict.  See http://oauth.net/core/1.0/#encoding_parameters.
  // This definition is considered the current standard as of January 2005.
  // While as of July 2011 many systems to do not comply, any valid OAuth
  // implementation must comply.
  //
  // Any character which is in the "unreserved set" MUST NOT be encoded.
  // All other characters MUST be encoded.
  //
  // The unreserved set is comprised of the alphanumeric characters and these
  // others:
  //   - minus (-)
  //   - period (.)
  //   - underscore (_)
  //   - tilde (~)
  static bool Decode(const std::string& text, std::string* decoded_text);
  static std::string Encode(const std::string& text);

  // Signs a request specified as URL string, complete with parameters.
  //
  // If HttpMethod is GET_METHOD, the signed result is the full URL, otherwise
  // it is the request parameters, including the oauth_signature field.
  static bool ParseAndSign(const GURL& request_url_with_parameters,
                           SignatureMethod signature_method,
                           HttpMethod http_method,
                           const std::string& consumer_key,
                           const std::string& consumer_secret,
                           const std::string& token_key,
                           const std::string& token_secret,
                           std::string* signed_result);

  // Signs a request specified as the combination of a base URL string, with
  // parameters included in a separate map data structure.  NOTE: The base URL
  // string must not contain a question mark (?) character.  If it does,
  // you can use ParseAndSign() instead.
  //
  // If HttpMethod is GET_METHOD, the signed result is the full URL, otherwise
  // it is the request parameters, including the oauth_signature field.
  static bool SignURL(const GURL& request_base_url,
                      const Parameters& parameters,
                      SignatureMethod signature_method,
                      HttpMethod http_method,
                      const std::string& consumer_key,
                      const std::string& consumer_secret,
                      const std::string& token_key,
                      const std::string& token_secret,
                      std::string* signed_result);

  // Similar to SignURL(), but the returned string is not a URL, but the payload
  // to for an HTTP Authorization header.
  static bool SignAuthHeader(const GURL& request_base_url,
                             const Parameters& parameters,
                             SignatureMethod signature_method,
                             HttpMethod http_method,
                             const std::string& consumer_key,
                             const std::string& consumer_secret,
                             const std::string& token_key,
                             const std::string& token_secret,
                             std::string* signed_result);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_REQUEST_SIGNER_H_
