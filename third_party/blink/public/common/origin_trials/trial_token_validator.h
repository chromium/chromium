// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_

#include <memory>
#include <string>
#include <vector>
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace net {
class HttpResponseHeaders;
class URLRequest;
}  // namespace net

namespace blink {

class OriginTrialPolicy;
class TrialTokenResult;

// TrialTokenValidator checks that a page's OriginTrial token enables a certain
// feature.
//
// Given a policy, feature and token, this class determines if the feature
// should be enabled or not for a specific document.
class BLINK_COMMON_EXPORT TrialTokenValidator {
 public:
  TrialTokenValidator();
  virtual ~TrialTokenValidator();

  using FeatureToTokensMap =
      base::flat_map<std::string /* feature_name */,
                     std::vector<std::string /* token */>>;

  // If the token validates, status will be set to
  // OriginTrialTokenStatus::kSuccess, the rest will be populated with name of
  // the feature this token enables, the expiry time of the token and whether it
  // is a third-party token. Otherwise, only the status will be set.
  // This method is thread-safe.
  virtual TrialTokenResult ValidateToken(base::StringPiece token,
                                         const url::Origin& origin,
                                         base::Time current_time) const;
  // Validates a token for the given |origin|. If identified as a third-party
  // token, instead validate for the given |third_party_origin|. Validation of a
  // third-party token will fail if |third_party-origin| is not given. Returns
  // the same result as ValidateToken() above.
  // This method is thread-safe.
  virtual TrialTokenResult ValidateToken(base::StringPiece token,
                                         const url::Origin& origin,
                                         const url::Origin* third_party_origin,
                                         base::Time current_time) const;

  // |request| must not be nullptr.
  // NOTE: This is not currently used, but remains here for future trials.
  bool RequestEnablesFeature(const net::URLRequest* request,
                             base::StringPiece feature_name,
                             base::Time current_time) const;

  // Returns whether the given response for the given URL enable the named
  // Origin or Deprecation Trial at the given time.
  //
  // |response_headers| must not be nullptr.
  bool RequestEnablesFeature(const GURL& request_url,
                             const net::HttpResponseHeaders* response_headers,
                             base::StringPiece feature_name,
                             base::Time current_time) const;

  // Similar to |RequestEnablesFeature()|, but for Deprecation Trials that may
  // be enabled on insecure origins.
  //
  // For Origin Trials (as opposed to Deprecation Trials) or Deprecation Trials
  // that are enabled exclusively on secure origins, use
  // |RequestEnablesFeature()| instead.
  //
  // Functionally, the only difference is that this can return true even if
  // |request_url|'s origin is not secure.
  //
  // |response_headers| must not be nullptr.
  bool RequestEnablesDeprecatedFeature(
      const GURL& request_url,
      const net::HttpResponseHeaders* response_headers,
      base::StringPiece feature_name,
      base::Time current_time) const;

  // Returns all valid tokens in |headers|.
  std::unique_ptr<FeatureToTokensMap> GetValidTokensFromHeaders(
      const url::Origin& origin,
      const net::HttpResponseHeaders* headers,
      base::Time current_time) const;

  // Returns all valid tokens in |tokens|. This method is used to re-validate
  // previously stored tokens.
  std::unique_ptr<FeatureToTokensMap> GetValidTokens(
      const url::Origin& origin,
      const FeatureToTokensMap& tokens,
      base::Time current_time) const;

  static void SetOriginTrialPolicyGetter(
      base::RepeatingCallback<OriginTrialPolicy*()> policy);
  static void ResetOriginTrialPolicyGetter();

  static bool IsTrialPossibleOnOrigin(const GURL& url);

 private:
  // Helper for |RequestEnablesFeature()| and
  // |RequestEnablesDeprecatedFeature()|.
  bool ResponseBearsValidTokenForFeature(
      const GURL& request_url,
      const net::HttpResponseHeaders& response_headers,
      base::StringPiece feature_name,
      base::Time current_time) const;
};  // class TrialTokenValidator

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
