// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"

#import <list>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/mojom/fetch_api.mojom.h"

using security_interstitials::UnsafeResource;

#pragma mark - UnsafeSubresourceContainer

namespace {
// Helper object storing unsafe subresources for a NavigationItem.
class UnsafeSubresourceContainer : public base::SupportsUserData::Data {
 public:
  ~UnsafeSubresourceContainer() override = default;

  // Lazily instantiates and returns the UnsafeSubresourceContainer for `item`.
  static UnsafeSubresourceContainer* FromNavigationItem(
      web::NavigationItem* item) {
    DCHECK(item);
    static void* kUserDataKey = &kUserDataKey;
    UnsafeSubresourceContainer* stack =
        static_cast<UnsafeSubresourceContainer*>(
            item->GetUserData(kUserDataKey));
    if (!stack) {
      auto new_stack = base::WrapUnique(new UnsafeSubresourceContainer());
      stack = new_stack.get();
      item->SetUserData(kUserDataKey, std::move(new_stack));
    }
    return stack;
  }

  // Returns a pointer to the first pending unsafe resource, if any.  Clears any
  // stored resources that are no longer pending.
  const UnsafeResource* GetUnsafeResource() {
    auto it = unsafe_resources_.begin();
    while (it != unsafe_resources_.end()) {
      const UnsafeResource* resource = it->resource();
      if (resource)
        return resource;
      it = unsafe_resources_.erase(it);
    }
    return nullptr;
  }

  // Stores `resource` in the container.
  void StoreUnsafeResource(const UnsafeResource& resource) {
    unsafe_resources_.push_back(PendingUnsafeResourceStorage(resource));
  }

 private:
  UnsafeSubresourceContainer() = default;

  std::list<PendingUnsafeResourceStorage> unsafe_resources_;
};
}  // namespace

#pragma mark - SafeBrowsingUnsafeResourceContainer

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingUnsafeResourceContainer)

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    web::WebState* web_state)
    : web_state_(web_state) {}

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer&
SafeBrowsingUnsafeResourceContainer::operator=(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer::~SafeBrowsingUnsafeResourceContainer() =
    default;

void SafeBrowsingUnsafeResourceContainer::StoreMainFrameUnsafeResource(
    const security_interstitials::UnsafeResource& resource) {
  DCHECK_EQ(resource.weak_web_state.get(), web_state_);
  DCHECK_EQ(network::mojom::RequestDestination::kDocument,
            resource.request_destination);

  // For main frame navigations, the copy is stored in
  // `main_frame_unsafe_resource_`.  It corresponds with the pending
  // NavigationItem, which may have not been created yet and will be discarded
  // after navigation failures.
  main_frame_unsafe_resource_ = PendingUnsafeResourceStorage(resource);
}

void SafeBrowsingUnsafeResourceContainer::StoreSubFrameUnsafeResource(
    const security_interstitials::UnsafeResource& resource,
    web::NavigationItem* main_frame_item) {
  DCHECK_EQ(resource.weak_web_state.get(), web_state_);
  DCHECK_EQ(network::mojom::RequestDestination::kIframe,
            resource.request_destination);
  DCHECK(main_frame_item);

  // Unsafe sub frame resources are caused by loads triggered by a committed
  // main frame navigation.  These are associated with the NavigationItem so
  // that they persist past reloads.
  UnsafeSubresourceContainer::FromNavigationItem(main_frame_item)
      ->StoreUnsafeResource(resource);
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetMainFrameUnsafeResource() const {
  return main_frame_unsafe_resource_.resource();
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetSubFrameUnsafeResource(
    web::NavigationItem* item) const {
  return UnsafeSubresourceContainer::FromNavigationItem(item)
      ->GetUnsafeResource();
}
