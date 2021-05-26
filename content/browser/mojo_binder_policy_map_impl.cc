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
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"

namespace content {

namespace {

// Register mojo binder policies for same-origin prerendering for content/
// interfaces.
void RegisterContentBinderPoliciesForSameOriginPrerendering(
    MojoBinderPolicyMap& map) {
  // For Prerendering, kCancel is usually used for those interfaces that cannot
  // be granted because they can cause undesirable side-effects (e.g., playing
  // audio, showing notification) and are non-deferrable.
  // Please update `PrerenderCancelledInterface` and
  // `GetCancelledInterfaceType()` in
  // content/browser/prerender/prerender_metrics.h once you add a new kCancel
  // interface.
  // NotificationService has a sync message and is requested in
  // Notification constructor, so it should be kCancel.
  map.SetPolicy<blink::mojom::NotificationService>(MojoBinderPolicy::kCancel);
  map.SetPolicy<device::mojom::GamepadHapticsManager>(
      MojoBinderPolicy::kCancel);
  map.SetPolicy<device::mojom::GamepadMonitor>(MojoBinderPolicy::kCancel);

  // ClipboardHost has sync messages, so it cannot be kDefer. However, the
  // renderer is not expected to request the interface; prerendering documents
  // do not have system focus nor user activation, which is required before
  // sending the request.
  map.SetPolicy<blink::mojom::ClipboardHost>(MojoBinderPolicy::kUnexpected);

  map.SetPolicy<blink::mojom::CacheStorage>(MojoBinderPolicy::kGrant);
  map.SetPolicy<blink::mojom::IDBFactory>(MojoBinderPolicy::kGrant);
  map.SetPolicy<blink::mojom::NativeIOHost>(MojoBinderPolicy::kGrant);
  map.SetPolicy<network::mojom::RestrictedCookieManager>(
      MojoBinderPolicy::kGrant);
  // Set policy to Grant for CodeCacheHost. Without this loads won't progress
  // since we wait for a response from code cache when loading resources.
  map.SetPolicy<blink::mojom::CodeCacheHost>(MojoBinderPolicy::kGrant);
}

// A singleton class that stores the `MojoBinderPolicyMap` of interfaces which
// are obtained via `BrowserInterfaceBrowser` for frames.
// content/ initializes the policy map with predefined policies, then allows
// embedders to update the map.
class BrowserInterfaceBrokerMojoBinderPolicyMapHolder {
 public:
  BrowserInterfaceBrokerMojoBinderPolicyMapHolder() {
    RegisterContentBinderPoliciesForSameOriginPrerendering(same_origin_map_);
    GetContentClient()
        ->browser()
        ->RegisterMojoBinderPoliciesForSameOriginPrerendering(same_origin_map_);
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

  const MojoBinderPolicyMapImpl* GetSameOriginPolicyMap() const {
    return &same_origin_map_;
  }

 private:
  // TODO(https://crbug.com/1145976): Set default policy map for content/.
  // Changes to `same_origin_map_` require security review.
  MojoBinderPolicyMapImpl same_origin_map_;
};

}  // namespace

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl() = default;

MojoBinderPolicyMapImpl::MojoBinderPolicyMapImpl(
    const base::flat_map<std::string, MojoBinderPolicy>& init_map)
    : policy_map_(init_map) {}

MojoBinderPolicyMapImpl::~MojoBinderPolicyMapImpl() = default;

const MojoBinderPolicyMapImpl*
MojoBinderPolicyMapImpl::GetInstanceForSameOriginPrerendering() {
  static const base::NoDestructor<
      BrowserInterfaceBrokerMojoBinderPolicyMapHolder>
      map;

  return map->GetSameOriginPolicyMap();
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
