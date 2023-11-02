// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl_serialized_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
namespace {
// The key under which the WebState's stable identifier was saved in the user
// serializable data before the M98 release.
NSString* const kTabIdKey = @"TabId";
}

WebStateImpl::SerializedData::SerializedData(WebStateImpl* owner,
                                             const CreateParams& create_params,
                                             CRWSessionStorage* session_storage)
    : owner_(owner),
      create_params_(create_params),
      session_storage_(session_storage) {
  DCHECK(owner_);
  DCHECK(session_storage_);

  // Restore the serializable user data as user code may depend on accessing
  // on those values even for an unrealized WebState.
  if (session_storage_.userData) {
    SerializableUserDataManager::FromWebState(owner_)->SetUserDataFromSession(
        session_storage_.userData);
  }
}

WebStateImpl::SerializedData::~SerializedData() = default;

void WebStateImpl::SerializedData::TearDown() {
  for (auto& observer : observers())
    observer.WebStateDestroyed(owner_);
  for (auto& observer : policy_deciders())
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders())
    observer.ResetWebState();
}

WebState::CreateParams WebStateImpl::SerializedData::GetCreateParams() const {
  return create_params_;
}

CRWSessionStorage* WebStateImpl::SerializedData::GetSessionStorage() const {
  // If a SerializableUserDataManager is attached to the WebState, the user
  // may have changed its content. Thus, update the serializable user data
  // if needed. Use a const pointer to the WebState to avoid creating the
  // manager if it does not exists yet.
  const SerializableUserDataManager* user_data_manager =
      SerializableUserDataManager::FromWebState(
          const_cast<const WebStateImpl*>(owner_));

  if (user_data_manager) {
    session_storage_.userData = user_data_manager->GetUserDataForSession();
  }

  return session_storage_;
}

base::Time WebStateImpl::SerializedData::GetLastActiveTime() const {
  if (!create_params_.last_active_time.is_null())
    return create_params_.last_active_time;

  return session_storage_.lastActiveTime;
}

base::Time WebStateImpl::SerializedData::GetCreationTime() const {
  return session_storage_.creationTime;
}

BrowserState* WebStateImpl::SerializedData::GetBrowserState() const {
  return create_params_.browser_state;
}

NSString* WebStateImpl::SerializedData::GetStableIdentifier() const {
  DCHECK(session_storage_.stableIdentifier.length);
  return [session_storage_.stableIdentifier copy];
}

const std::u16string& WebStateImpl::SerializedData::GetTitle() const {
  static const std::u16string kEmptyString16;
  CRWNavigationItemStorage* item = GetLastCommittedItem();
  return item ? item.title : kEmptyString16;
}

const FaviconStatus& WebStateImpl::SerializedData::GetFaviconStatus() const {
  return favicon_status_;
}

void WebStateImpl::SerializedData::SetFaviconStatus(
    const FaviconStatus& favicon_status) {
  favicon_status_ = favicon_status;
}

int WebStateImpl::SerializedData::GetNavigationItemCount() const {
  return session_storage_.itemStorages.count;
}

const GURL& WebStateImpl::SerializedData::GetVisibleURL() const {
  // A restored WebState has no pending item. Thus the visible item is the
  // last committed item. This means that GetVisibleURL() must return the
  // same URL as GetLastCommittedURL().
  return GetLastCommittedURL();
}

const GURL& WebStateImpl::SerializedData::GetLastCommittedURL() const {
  CRWNavigationItemStorage* item = GetLastCommittedItem();
  return item ? item.virtualURL : GURL::EmptyGURL();
}

// TODO(crbug.com/1264451): this private method allow to implement `GetTitle()`
// and `GetLastCommittedURL()` without duplicating code. As of today, the title
// and URL for the WebState are not saved directly, so this method access them
// via the serialized NavigationManager state. This will be removed once the
// format of the WebState serialization is changed to directly saved the title
// and URL. This implementation allow to test unrealized WebState before the
// new format is used. This slightly break encapsulation, but this is a private
// method of a private class and the file format is quite stable, so this seem
// reasonable as a temporary solution.
CRWNavigationItemStorage* WebStateImpl::SerializedData::GetLastCommittedItem()
    const {
  const NSInteger index = session_storage_.lastCommittedItemIndex;
  if (index < 0)
    return nil;

  const NSUInteger uindex = static_cast<NSUInteger>(index);
  if (session_storage_.itemStorages.count <= uindex) {
    return nil;
  }

  return session_storage_.itemStorages[uindex];
}

}  // namespace web
