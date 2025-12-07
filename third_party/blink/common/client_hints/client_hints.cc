// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/permissions_policy/client_hints_permissions_policy_mapping.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace blink {

bool IsClientHintSentByDefault(network::mojom::WebClientHintsType type) {
  switch (type) {
    case network::mojom::WebClientHintsType::kSaveData:
    case network::mojom::WebClientHintsType::kUA:
    case network::mojom::WebClientHintsType::kUAMobile:
    case network::mojom::WebClientHintsType::kUAPlatform:
      return true;
    default:
      return false;
  }
}

// Add a list of Client Hints headers to be removed to the output vector, based
// on PermissionsPolicy and the url's origin.
void FindClientHintsToRemove(
    const network::PermissionsPolicy* permissions_policy,
    const GURL& url,
    std::vector<std::string>* removed_headers) {
  DCHECK(removed_headers);
  url::Origin origin = url::Origin::Create(url);
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    const auto& header = elem.second;
    // Remove the hint if any is true:
    // * Permissions policy is null (we're in a sync XHR case) and the hint is
    // not sent by default.
    // * Permissions policy exists and doesn't allow for the hint.
    if ((!permissions_policy && !IsClientHintSentByDefault(type)) ||
        (permissions_policy &&
         !permissions_policy->IsFeatureEnabledForOrigin(
             network::GetClientHintToPolicyFeatureMap().at(type), origin))) {
      removed_headers->push_back(header);
    }
  }
}

}  // namespace blink
