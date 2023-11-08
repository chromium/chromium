// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state_builder.h"

#import "base/strings/sys_string_conversions.h"
#import "content/public/browser/navigation_controller.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/restore_type.h"
#import "ios/web/content/content_browser_context.h"
#import "ios/web/content/web_state/content_web_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "net/http/http_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace web {

void ExtractContentSessionStorage(ContentWebState* web_state,
                                  content::NavigationController& controller,
                                  web::BrowserState* browser_state,
                                  CRWSessionStorage* session_storage) {
  web_state->SetHasOpener(session_storage.hasOpener);
  NSArray<CRWNavigationItemStorage*>* item_storages =
      session_storage.itemStorages;
  std::vector<std::unique_ptr<content::NavigationEntry>> items(
      item_storages.count);

  content::BrowserContext* browser_context =
      ContentBrowserContext::FromBrowserState(browser_state);

  for (size_t index = 0; index < item_storages.count; ++index) {
    CRWNavigationItemStorage* navigation_item_storage = item_storages[index];
    std::unique_ptr<content::NavigationEntry> new_entry =
        content::NavigationController::CreateNavigationEntry(
            navigation_item_storage.virtualURL, content::Referrer(),
            /* initiator_origin= */ std::nullopt,
            /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_RELOAD,
            /* is_renderer_initiated= */ false, std::string(), browser_context,
            /* blob_url_loader_factory= */ nullptr);
    new_entry->SetOriginalRequestURL(navigation_item_storage.URL);
    if (navigation_item_storage.URL.SchemeIsHTTPOrHTTPS()) {
      new_entry->SetURL(navigation_item_storage.URL);
      new_entry->SetVirtualURL(navigation_item_storage.virtualURL);
    } else {
      new_entry->SetURL(navigation_item_storage.virtualURL);
    }
    new_entry->SetTimestamp(navigation_item_storage.timestamp);
    new_entry->SetTitle(navigation_item_storage.title);
    new_entry->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    std::ostringstream ss;
    for (id key in navigation_item_storage.HTTPRequestHeaders) {
      auto key_utf8 = base::SysNSStringToUTF8(key);
      auto value_utf8 = base::SysNSStringToUTF8(
          [navigation_item_storage.HTTPRequestHeaders objectForKey:key]);
      if (!net::HttpUtil::IsValidHeaderName(key_utf8) ||
          !net::HttpUtil::IsValidHeaderValue(value_utf8)) {
        continue;
      }
      ss << key_utf8 << ": " << value_utf8 << "\n";
    }
    if (!ss.str().empty()) {
      new_entry->AddExtraHeaders(ss.str());
    }

    GURL url = new_entry->GetURL();
    if (web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
            &url, browser_state)) {
      GURL virtual_url = new_entry->GetURL();
      new_entry->SetURL(url);
      new_entry->SetVirtualURL(virtual_url);
    }

    // Content doesn't allow about://newtab
    if (url.possibly_invalid_spec() == "about://newtab/") {
      new_entry->SetURL(GURL("chrome://newtab"));
    }

    items[index] = std::move(new_entry);
  }
  controller.Restore(session_storage.lastCommittedItemIndex,
                     content::RestoreType::kRestored, &items);

  SerializableUserDataManager::FromWebState(web_state)->SetUserDataFromSession(
      session_storage.userData);
}

CRWSessionStorage* BuildContentSessionStorage(
    const ContentWebState* web_state,
    ContentNavigationManager* navigation_manager) {
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.lastActiveTime = web_state->GetLastActiveTime();
  session_storage.creationTime = web_state->GetCreationTime();
  session_storage.stableIdentifier = web_state->GetStableIdentifier();
  session_storage.hasOpener = web_state->HasOpener();
  session_storage.lastCommittedItemIndex =
      navigation_manager->GetLastCommittedItemIndex();
  if (session_storage.lastCommittedItemIndex == -1) {
    // This can happen when a session is saved during restoration. Instead,
    // default to GetItemCount() - 1.
    session_storage.lastCommittedItemIndex =
        navigation_manager->GetItemCount() - 1;
  }

  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [[NSMutableArray alloc] init];
  const size_t original_index = session_storage.lastCommittedItemIndex;
  const size_t navigation_items =
      static_cast<size_t>(navigation_manager->GetItemCount());

  // Drop URLs larger than a certain threshold.
  for (size_t index = 0; index < navigation_items; ++index) {
    const NavigationItem* item = navigation_manager->GetItemAtIndex(index);
    if (item->GetURL().spec().size() > url::kMaxURLChars) {
      if (index <= original_index) {
        session_storage.lastCommittedItemIndex--;
      }
      continue;
    }

    CRWNavigationItemStorage* storage = [[CRWNavigationItemStorage alloc] init];
    storage.virtualURL = item->GetVirtualURL();
    storage.URL = item->GetURL();
    // Use default referrer if URL is longer than allowed. Navigation items with
    // these long URLs will not be serialized, so there is no point in keeping
    // referrer URL.
    if (item->GetReferrer().url.spec().size() <= url::kMaxURLChars) {
      storage.referrer = item->GetReferrer();
    }
    storage.timestamp = item->GetTimestamp();
    storage.title = item->GetTitle();
    storage.userAgentType = item->GetUserAgentType();
    storage.HTTPRequestHeaders = item->GetHttpRequestHeaders();
    [item_storages addObject:storage];
  }
  session_storage.itemStorages = item_storages;
  session_storage.certPolicyCacheStorage =
      [[CRWSessionCertificatePolicyCacheStorage alloc] init];
  session_storage.certPolicyCacheStorage.certificateStorages =
      [NSMutableSet set];
  const SerializableUserDataManager* user_data_manager =
      SerializableUserDataManager::FromWebState(web_state);
  if (user_data_manager) {
    session_storage.userData = user_data_manager->GetUserDataForSession();
  }
  session_storage.userAgentType = UserAgentType::AUTOMATIC;

  return session_storage;
}

}  // namespace web
