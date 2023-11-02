// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/session_storage_builder.h"

#import <memory>

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_builder.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/session/session_certificate_policy_cache_storage_builder.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
CRWSessionStorage* SessionStorageBuilder::BuildStorage(
    const WebStateImpl& web_state,
    const NavigationManagerImpl& navigation_manager,
    const SessionCertificatePolicyCacheImpl& session_certificate_policy_cache) {
  DCHECK_EQ(&web_state, navigation_manager.GetWebState());

  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.lastActiveTime = web_state.GetLastActiveTime();
  session_storage.creationTime = web_state.GetCreationTime();
  session_storage.stableIdentifier = web_state.GetStableIdentifier();
  session_storage.hasOpener = web_state.HasOpener();
  session_storage.lastCommittedItemIndex =
      navigation_manager.GetLastCommittedItemIndex();
  if (session_storage.lastCommittedItemIndex == -1) {
    // This can happen when a session is saved during restoration. Instead,
    // default to GetItemCount() - 1.
    session_storage.lastCommittedItemIndex =
        navigation_manager.GetItemCount() - 1;
  }

  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [[NSMutableArray alloc] init];
  const size_t original_index = session_storage.lastCommittedItemIndex;
  const size_t navigation_items =
      static_cast<size_t>(navigation_manager.GetItemCount());

  // Drop URLs larger than a certain threshold.
  for (size_t index = 0; index < navigation_items; ++index) {
    const NavigationItemImpl* item =
        navigation_manager.GetNavigationItemImplAtIndex(index);
    if (item->ShouldSkipSerialization() ||
        item->GetURL().spec().size() > url::kMaxURLChars) {
      if (index <= original_index) {
        session_storage.lastCommittedItemIndex--;
      }
      continue;
    }

    [item_storages addObject:NavigationItemStorageBuilder::BuildStorage(*item)];
  }

  int loc = 0;
  int len = 0;
  session_storage.lastCommittedItemIndex = wk_navigation_util::GetSafeItemRange(
      session_storage.lastCommittedItemIndex, item_storages.count, &loc, &len);

  DCHECK_LT(session_storage.lastCommittedItemIndex,
            static_cast<NSInteger>(len));
  session_storage.itemStorages =
      [item_storages subarrayWithRange:NSMakeRange(loc, len)];
  session_storage.certPolicyCacheStorage =
      SessionCertificatePolicyCacheStorageBuilder::BuildStorage(
          session_certificate_policy_cache);
  const SerializableUserDataManager* user_data_manager =
      SerializableUserDataManager::FromWebState(&web_state);
  if (user_data_manager) {
    session_storage.userData = user_data_manager->GetUserDataForSession();
  }
  session_storage.userAgentType = web_state.GetUserAgentForSessionRestoration();

  return session_storage;
}

// static
void SessionStorageBuilder::ExtractSessionState(
    WebStateImpl& web_state,
    NavigationManagerImpl& navigation_manager,
    CRWSessionStorage* session_storage) {
  DCHECK(session_storage);
  DCHECK_EQ(&web_state, navigation_manager.GetWebState());

  web_state.SetHasOpener(session_storage.hasOpener);
  NSArray<CRWNavigationItemStorage*>* item_storages =
      session_storage.itemStorages;

  std::vector<std::unique_ptr<NavigationItem>> items(item_storages.count);
  for (size_t index = 0; index < item_storages.count; ++index) {
    std::unique_ptr<NavigationItemImpl> item_impl =
        NavigationItemStorageBuilder::BuildNavigationItemImpl(
            item_storages[index]);

    navigation_manager.RewriteItemURLIfNecessary(item_impl.get());
    items[index] = std::move(item_impl);
  }
  navigation_manager.Restore(session_storage.lastCommittedItemIndex,
                             std::move(items));

  std::unique_ptr<SessionCertificatePolicyCacheImpl> cert_policy_cache =
      SessionCertificatePolicyCacheStorageBuilder::
          BuildSessionCertificatePolicyCache(
              session_storage.certPolicyCacheStorage,
              web_state.GetBrowserState());
  if (!cert_policy_cache) {
    cert_policy_cache = std::make_unique<SessionCertificatePolicyCacheImpl>(
        web_state.GetBrowserState());
  }
  web_state.SetSessionCertificatePolicyCacheImpl(std::move(cert_policy_cache));

  SerializableUserDataManager::FromWebState(&web_state)
      ->SetUserDataFromSession(session_storage.userData);
  UserAgentType user_agent_type = session_storage.userAgentType;
  web_state.SetUserAgent(user_agent_type);
}

}  // namespace web
