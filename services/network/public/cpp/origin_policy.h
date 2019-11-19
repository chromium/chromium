// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_POLICY_H_

#include <memory>
#include <string>
#include <vector>

#include "url/gurl.h"

namespace network {

// The state of a returned origin policy.
enum class OriginPolicyState {
  // The origin policy was successfully retrieved and its contents is available.
  kLoaded,
  // There has been an error when attempting to retrieve the policy. For example
  // this could mean the server has returned a 404 when attempting to retrieve
  // the origin policy.
  kCannotLoadPolicy,
  // An invalid redirect has been encountered. The only valid redirect is if
  // we requested the default "/.well-known/origin-policy", to which the
  // server MUST respond with a redirect to the latest origin policy. Any
  // other redirect (or more than 1 redirect) is invalid.
  // https://wicg.github.io/origin-policy/#origin-policy-well-known
  kInvalidRedirect,
  // There is no need to apply an origin policy. This could be (for example) if
  // an exception has been added for the requested origin.
  kNoPolicyApplies,
  // Other origin policy state.
  kOther,

  // kMaxValue needs to always be set to the last value of the enum.
  kMaxValue = kOther,
};

struct COMPONENT_EXPORT(NETWORK_CPP_BASE) OriginPolicyContents;

using OriginPolicyContentsPtr = std::unique_ptr<OriginPolicyContents>;

COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool operator==(const OriginPolicyContentsPtr& a,
                const OriginPolicyContentsPtr& b);

// Contains the parsed result of an origin policy file as a struct of the
// relevant fields.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) OriginPolicyContents {
  OriginPolicyContents();
  ~OriginPolicyContents();
  OriginPolicyContents(
      const std::vector<std::string>& features,
      const std::vector<std::string>& content_security_policies,
      const std::vector<std::string>& content_security_policies_report_only);

  OriginPolicyContents(const OriginPolicyContents& other);
  OriginPolicyContents& operator=(const OriginPolicyContents& other);
  bool operator==(const OriginPolicyContents& other) const;

  OriginPolicyContentsPtr ClonePtr();

  // The feature policy that is dictated by the origin policy. Each feature
  // is one member of the array.
  // https://w3c.github.io/webappsec-feature-policy/
  std::vector<std::string> features;

  // These two fields together represent the CSP that should be applied to the
  // origin, based on the origin policy.
  // https://w3c.github.io/webappsec-csp/

  // The "enforced" portion of the CSP. This CSP is to be treated as having
  // an "enforced" disposition.
  // https://w3c.github.io/webappsec-csp/#policy-disposition
  std::vector<std::string> content_security_policies;

  // The "report-only" portion of the CSP. This CSP is to be treated as having
  // a "report" disposition.
  // https://w3c.github.io/webappsec-csp/#policy-disposition
  std::vector<std::string> content_security_policies_report_only;
};

// Native implementation of mojom::OriginPolicy. This is done so we can pass
// the OriginPolicy through IPC.
// Represents the result of retrieving an origin policy.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) OriginPolicy {
  OriginPolicy();
  ~OriginPolicy();
  OriginPolicy(const OriginPolicy& other);
  OriginPolicy& operator=(const OriginPolicy& other);

  // The state of the origin policy. Possible values are explained in the
  // OriginPolicyState struct comments.
  OriginPolicyState state;

  // The final URL from which the policy has been retrieved. If unsuccessful,
  // it will be the URL from which the policy was attempted to be retrieved.
  // This is useful (for example) for displaying this information to the user
  // as part of an interstitial page.
  GURL policy_url;

  // The origin policy contents. If `state` is `kLoaded` this will be populated,
  // otherwise it will be `null`.
  OriginPolicyContentsPtr contents;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_POLICY_H_
