// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/session_storage_builder.h"

#import <memory>

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "components/sessions/core/session_id.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/session.pb.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
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
  session_storage.uniqueIdentifier = web_state.GetUniqueIdentifier();
  session_storage.hasOpener = web_state.HasOpener();
  session_storage.userAgentType = web_state.GetUserAgentForSessionRestoration();

  const SerializableUserDataManager* user_data_manager =
      SerializableUserDataManager::FromWebState(&web_state);
  if (user_data_manager) {
    session_storage.userData = user_data_manager->GetUserDataForSession();
  }

  // Serialize the NavigationManagerImpl to proto representation then
  // fill CRWSessionStorage `lastCommittedItemIndex` and `itemStorages`
  // to allow serialization using NSCoding protocol.
  {
    proto::NavigationStorage storage;
    navigation_manager.SerializeToProto(storage);

    NSMutableArray<CRWNavigationItemStorage*>* items = [NSMutableArray array];
    for (const proto::NavigationItemStorage& item_storage : storage.items()) {
      [items addObject:[[CRWNavigationItemStorage alloc]
                           initWithProto:item_storage]];
    }

    session_storage.itemStorages = items;
    session_storage.lastCommittedItemIndex =
        storage.last_committed_item_index();
  }

  // Serialize the SessionCertificatePolicyCacheImpl to proto represention
  // then create a CRWSessionCertificatePolicyCacheStorage* wrapping it to
  // allow serialization using NSCoding protocol.
  {
    proto::CertificatesCacheStorage storage;
    session_certificate_policy_cache.SerializeToProto(storage);
    session_storage.certPolicyCacheStorage =
        [[CRWSessionCertificatePolicyCacheStorage alloc] initWithProto:storage];
  }

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
  web_state.SetUserAgent(session_storage.userAgentType);

  SerializableUserDataManager::FromWebState(&web_state)
      ->SetUserDataFromSession(session_storage.userData);

  // Restore the NavigationManagerImpl by extracting the `itemStorages` and
  // `lastCommittedItemIndex` creating a proto representation and using the
  // corresponding method of NavigationManagerImpl.
  {
    proto::NavigationStorage storage;
    storage.set_last_committed_item_index(
        session_storage.lastCommittedItemIndex);

    for (CRWNavigationItemStorage* item in session_storage.itemStorages) {
      [item serializeToProto:*storage.add_items()];
    }

    navigation_manager.RestoreFromProto(storage);
  }

  // Restore the SessionCertificatePolicyCacheImpl by converting the
  // CRWSessionCertificatePolicyCacheStorage* to a proto represention
  // and using the corresponding constructor.
  {
    proto::CertificatesCacheStorage storage;
    [session_storage.certPolicyCacheStorage serializeToProto:storage];
    auto cert_policy_cache =
        std::make_unique<SessionCertificatePolicyCacheImpl>(
            web_state.GetBrowserState(), storage);
    web_state.SetSessionCertificatePolicyCacheImpl(
        std::move(cert_policy_cache));
  }
}

}  // namespace web
