// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl_serialized_data.h"

#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/web_state_observer.h"

namespace web {

WebStateImpl::SerializedData::SerializedData(
    WebStateImpl* owner,
    BrowserState* browser_state,
    NSString* stable_identifier,
    WebStateID unique_identifier,
    proto::WebStateMetadataStorage metadata,
    WebStateStorageLoader storage_loader,
    NativeSessionFetcher session_fetcher)
    : owner_(owner),
      browser_state_(browser_state),
      stable_identifier_(stable_identifier),
      unique_identifier_(unique_identifier),
      creation_time_(TimeFromProto(metadata.creation_time())),
      last_active_time_(TimeFromProto(metadata.last_active_time())),
      page_title_(base::UTF8ToUTF16(metadata.active_page().page_title())),
      page_visible_url_(metadata.active_page().page_url()),
      navigation_item_count_(metadata.navigation_item_count()),
      storage_loader_(std::move(storage_loader)),
      session_fetcher_(std::move(session_fetcher)) {
  DCHECK(owner_);
  DCHECK(browser_state_);
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

CRWSessionStorage* WebStateImpl::SerializedData::GetSessionStorage() const {
  DCHECK(session_storage_);
  return session_storage_;
}

void WebStateImpl::SerializedData::SetSessionStorage(
    CRWSessionStorage* storage) {
  session_storage_ = storage;
  DCHECK(session_storage_);
}

void WebStateImpl::SerializedData::SerializeMetadataToProto(
    proto::WebStateMetadataStorage& storage) const {
  storage.set_navigation_item_count(navigation_item_count_);
  SerializeTimeToProto(creation_time_, *storage.mutable_creation_time());
  SerializeTimeToProto(last_active_time_, *storage.mutable_last_active_time());

  if (page_visible_url_.is_valid() || !page_title_.empty()) {
    proto::PageMetadataStorage& page_storage = *storage.mutable_active_page();
    if (page_visible_url_.is_valid()) {
      page_storage.set_page_url(page_visible_url_.spec());
    }
    if (!page_title_.empty()) {
      page_storage.set_page_title(base::UTF16ToUTF8(page_title_));
    }
  }
}

WebState::WebStateStorageLoader
WebStateImpl::SerializedData::TakeStorageLoader() {
  return std::move(storage_loader_);
}

WebState::NativeSessionFetcher
WebStateImpl::SerializedData::TakeNativeSessionFetcher() {
  return std::move(session_fetcher_);
}

base::Time WebStateImpl::SerializedData::GetLastActiveTime() const {
  return last_active_time_;
}

base::Time WebStateImpl::SerializedData::GetCreationTime() const {
  return creation_time_;
}

BrowserState* WebStateImpl::SerializedData::GetBrowserState() const {
  return browser_state_;
}

NSString* WebStateImpl::SerializedData::GetStableIdentifier() const {
  return stable_identifier_;
}

WebStateID WebStateImpl::SerializedData::GetUniqueIdentifier() const {
  return unique_identifier_;
}

const std::u16string& WebStateImpl::SerializedData::GetTitle() const {
  return page_title_;
}

const FaviconStatus& WebStateImpl::SerializedData::GetFaviconStatus() const {
  return favicon_status_;
}

void WebStateImpl::SerializedData::SetFaviconStatus(
    const FaviconStatus& favicon_status) {
  favicon_status_ = favicon_status;
}

int WebStateImpl::SerializedData::GetNavigationItemCount() const {
  return navigation_item_count_;
}

const GURL& WebStateImpl::SerializedData::GetVisibleURL() const {
  // A restored WebState has no pending item. Thus the visible item is the
  // last committed item. This means that GetVisibleURL() must return the
  // same URL as GetLastCommittedURL().
  return page_visible_url_;
}

const GURL& WebStateImpl::SerializedData::GetLastCommittedURL() const {
  return page_visible_url_;
}

}  // namespace web
