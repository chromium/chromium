// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_map_impl.h"

#include "base/no_destructor.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/mojo_binder_policy_map.h"
#include "content/public/common/content_client.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

namespace {

// Register mojo binder policies for prerendering for content/ interfaces.
// TODO(https://crbug.com/1145976): Set same-origin policies and cross-origin
// policies separately. The polices set in this function are for same-origin
// prerendering.
void RegisterContentBinderPoliciesForPrerendering(MojoBinderPolicyMap& map) {
  map.SetPolicy<device::mojom::GamepadHapticsManager>(
      MojoBinderPolicy::kCancel);
  map.SetPolicy<device::mojom::GamepadMonitor>(MojoBinderPolicy::kCancel);

  map.SetPolicy<blink::mojom::IDBFactory>(MojoBinderPolicy::kGrant);
  map.SetPolicy<network::mojom::RestrictedCookieManager>(
      MojoBinderPolicy::kGrant);
}

// A singleton class that stores the `MojoBinderPolicyMap` of interfaces which
// are obtained via `BrowserInterfaceBrowser` for frames.
// content/ initializes the policy map with predefined policies, then allows
// embedders to update the map.
class BrowserInterfaceBrokerMojoBinderPolicyMapHolder {
 public:
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder() {
    RegisterContentBinderPoliciesForPrerendering(map_);
    GetContentClient()->browser()->RegisterMojoBinderPoliciesForPrerendering(
        map_);
  }

  ~BrowserInterfaceBrokerMojoBinderPolicyMapHolder() = default;

  // Remove copy and move operations.
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder(
      const BrowserInterfaceBrokerMojoBinderPolicyMapHolder& other) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder& operator=(
      const BrowserInterfaceBrokerMojoBinderPolicyMapHolder& other) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder(
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder&&) = delete;
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder& operator=(
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder&&) = delete;

  const MojoBinderPolicyMapImpl* GetPolicyMap() const { return &map_; }

 private:
  // TODO(https://crbug.com/1145976): Set default policy map for content/.
  // Changes to `map_` require security review.
  MojoBinderPolicyMapImpl map_;
};

}  // namespace

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl() = default;

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl(
    const base::flat_map<std::string, MojoBinderPolicy>& init_map)
    : policy_map_(init_map) {}

MojoBinderPolicyMapImpl::~MojoBinderPolicyMapImpl() = default;

const MojoBinderPolicyMapImpl*
MojoBinderPolicyMapImpl::GetInstanceForPrerendering() {
  static const base::NoDestructor<
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder>
      map;
  return map->GetPolicyMap();
}

MojoBinderPolicy MojoBinderPolicyMapImpl::GetMojoBinderPolicy(
    const std::string& interface_name,
    const MojoBinderPolicy default_policy) const {
  const auto& found = policy_map_.find(interface_name);
  if (found != policy_map_.end())
    return found->second;
  return default_policy;
}

MojoBinderPolicy MojoBinderPolicyMapImpl::GetMojoBinderPolicyOrDieForTesting(
    const std::string& interface_name) const {
  const auto& found = policy_map_.find(interface_name);
  DCHECK(found != policy_map_.end());
  return found->second;
}

void MojoBinderPolicyMapImpl::SetPolicyByName(const base::StringPiece& name,
                                              MojoBinderPolicy policy) {
  policy_map_.emplace(name, policy);
}

}  // namespace content
