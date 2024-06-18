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

  // For main frame navigations, the copy is stored in
  // `main_frame_unsafe_resource_`.  It corresponds with the pending
  // NavigationItem, which may have not been created yet and will be discarded
  // after navigation failures.
  main_frame_unsafe_resource_ = PendingUnsafeResourceStorage(resource);
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetMainFrameUnsafeResource() const {
  return main_frame_unsafe_resource_.resource();
}
