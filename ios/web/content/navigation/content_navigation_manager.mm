// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/navigation/content_navigation_manager.h"

#import <Foundation/Foundation.h>
#import <sstream>
#import "base/strings/sys_string_conversions.h"
#import "content/public/browser/navigation_controller.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/navigation_handle.h"
#import "ios/web/content/navigation/content_navigation_item.h"
#import "ios/web/content/web_state/content_web_state.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/web_state_observer.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

network::mojom::ReferrerPolicy ToContentReferrerPolicy(ReferrerPolicy policy) {
  switch (policy) {
    case ReferrerPolicyAlways:
      return network::mojom::ReferrerPolicy::kAlways;
    case ReferrerPolicyDefault:
      return network::mojom::ReferrerPolicy::kDefault;
    case ReferrerPolicyNoReferrerWhenDowngrade:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case ReferrerPolicyNever:
      return network::mojom::ReferrerPolicy::kNever;
    case ReferrerPolicyOrigin:
      return network::mojom::ReferrerPolicy::kOrigin;
    case ReferrerPolicyOriginWhenCrossOrigin:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case ReferrerPolicySameOrigin:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case ReferrerPolicyStrictOrigin:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    default:
      NOTREACHED();
  }
  return network::mojom::ReferrerPolicy::kDefault;
}

content::ReloadType ToContentReloadType(ReloadType reload_type) {
  switch (reload_type) {
    case ReloadType::NORMAL:
      return content::ReloadType::NORMAL;
    case ReloadType::ORIGINAL_REQUEST_URL:
      return content::ReloadType::ORIGINAL_REQUEST_URL;
    default:
      NOTREACHED();
  }
  return content::ReloadType::NORMAL;
}

}  // namespace

ContentNavigationManager::ContentNavigationManager(
    ContentWebState* web_state,
    BrowserState* browser_state,
    content::NavigationController* controller)
    : web_state_(web_state),
      browser_state_(browser_state),
      controller_(controller) {}

ContentNavigationManager::~ContentNavigationManager() = default;

BrowserState* ContentNavigationManager::GetBrowserState() const {
  return browser_state_;
}

WebState* ContentNavigationManager::GetWebState() const {
  return web_state_;
}

NavigationItem* ContentNavigationManager::GetVisibleItem() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return nullptr;
  }
  return ContentNavigationItem::GetOrCreate(controller_->GetVisibleEntry());
}

NavigationItem* ContentNavigationManager::GetLastCommittedItem() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return nullptr;
  }
  return ContentNavigationItem::GetOrCreate(
      controller_->GetLastCommittedEntry());
}

NavigationItem* ContentNavigationManager::GetPendingItem() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return nullptr;
  }
  return ContentNavigationItem::GetOrCreate(controller_->GetPendingEntry());
}

void ContentNavigationManager::DiscardNonCommittedItems() {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  controller_->DiscardNonCommittedEntries();
}

void ContentNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams& web_params) {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  content::NavigationController::LoadURLParams params(web_params.url);
  params.referrer =
      content::Referrer(web_params.referrer.url,
                        ToContentReferrerPolicy(web_params.referrer.policy));
  params.is_renderer_initiated = web_params.is_renderer_initiated;
  params.transition_type = web_params.transition_type;

  std::ostringstream ss;
  for (id key in web_params.extra_headers) {
    auto key_utf8 = base::SysNSStringToUTF8(key);
    auto value_utf8 =
        base::SysNSStringToUTF8([web_params.extra_headers objectForKey:key]);
    if (!net::HttpUtil::IsValidHeaderName(key_utf8) ||
        !net::HttpUtil::IsValidHeaderValue(value_utf8)) {
      LOG(ERROR) << "Invalid header";
      return;
    }
    ss << key_utf8 << ": " << value_utf8 << "\n";
  }
  params.extra_headers = ss.str();

  if (web_params.post_data) {
    params.post_data = new network::ResourceRequestBody();
    params.post_data->AppendBytes(
        static_cast<const char*>([web_params.post_data bytes]),
        [web_params.post_data length]);
  }

  // We are not setting the virtual URL for data URL here.
  controller_->LoadURLWithParams(params);
}

void ContentNavigationManager::LoadIfNecessary() {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (controller_) {
    controller_->LoadIfNecessary();
  }
}

void ContentNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  // TODO(crbug.com/1419001)
  NOTIMPLEMENTED();
}

int ContentNavigationManager::GetItemCount() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return 0;
  }
  return controller_->GetEntryCount();
}

NavigationItem* ContentNavigationManager::GetItemAtIndex(size_t index) const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return nullptr;
  }
  return ContentNavigationItem::GetOrCreate(
      controller_->GetEntryAtIndex(index));
}

int ContentNavigationManager::GetIndexOfItem(const NavigationItem* item) const {
  for (int i = 0; i < GetItemCount(); ++i) {
    if (GetItemAtIndex(i) == item) {
      return i;
    }
  }
  return 0;
}

int ContentNavigationManager::GetPendingItemIndex() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return 0;
  }
  return controller_->GetPendingEntryIndex();
}

int ContentNavigationManager::GetLastCommittedItemIndex() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return 0;
  }
  return controller_->GetLastCommittedEntryIndex();
}

bool ContentNavigationManager::CanGoBack() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return false;
  }
  return controller_->CanGoBack();
}

bool ContentNavigationManager::CanGoForward() const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return false;
  }
  return controller_->CanGoForward();
}

bool ContentNavigationManager::CanGoToOffset(int offset) const {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return false;
  }
  return controller_->CanGoToOffset(offset);
}

void ContentNavigationManager::GoBack() {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  controller_->GoBack();
}

void ContentNavigationManager::GoForward() {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  controller_->GoForward();
}

void ContentNavigationManager::GoToIndex(int index) {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  controller_->GoToIndex(index);
}

void ContentNavigationManager::Reload(ReloadType reload_type,
                                      bool check_for_reposts) {
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return;
  }
  controller_->Reload(ToContentReloadType(reload_type), check_for_reposts);
}

void ContentNavigationManager::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  // TODO(crbug.com/1419001)
  NOTIMPLEMENTED();
}

std::vector<NavigationItem*> ContentNavigationManager::GetBackwardItems()
    const {
  std::vector<NavigationItem*> items;
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return items;
  }
  int last_committed_index = GetLastCommittedItemIndex();
  for (int i = last_committed_index - 1; i >= 0; --i) {
    items.push_back(GetItemAtIndex(i));
  }
  return items;
}

std::vector<NavigationItem*> ContentNavigationManager::GetForwardItems() const {
  std::vector<NavigationItem*> items;
  // TODO(crbug.com/1419001): controller_ should not be null.
  if (!controller_) {
    return items;
  }
  int last_committed_index = GetLastCommittedItemIndex();
  int item_count = GetItemCount();
  for (int i = last_committed_index + 1; i < item_count; ++i) {
    items.push_back(GetItemAtIndex(i));
  }
  return items;
}

void ContentNavigationManager::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  // TODO(crbug.com/1419001)
  NOTIMPLEMENTED();
}

bool ContentNavigationManager::IsRestoreSessionInProgress() const {
  return false;
}

void ContentNavigationManager::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  // TODO(crbug.com/1419001)
  NOTIMPLEMENTED();
}

}  // namespace web
