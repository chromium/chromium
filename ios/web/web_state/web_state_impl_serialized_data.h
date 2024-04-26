// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_

#import "base/memory/raw_ptr.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {
namespace proto {
class WebStateMetadataStorage;
}  // namespace proto

// Object storing the information needed to realize a WebState.
//
// This object is mostly read-only storage, but some information may
// be modified during the lifetime of the object (mostly the favicon
// information).
//
// Like RealizedWebState, the WebStateImpl may forward method to this
// class. Those methods will not be documented, but instead they will
// be annotated with the name of the class that declares them.
class WebStateImpl::SerializedData {
 public:
  // Creates a SerializedData with a non-null pointer to the owning
  // WebStateImpl, `browser_state`, unique and stable identifiers,
  // and data loaded from disk via `metadata`. The `storage_loader`
  // is used when the WebState will transition to "realized" state.
  SerializedData(WebStateImpl* owner,
                 BrowserState* browser_state,
                 NSString* stable_identifier,
                 WebStateID unique_identifier,
                 proto::WebStateMetadataStorage metadata,
                 WebStateStorageLoader storage_loader,
                 NativeSessionFetcher session_fetcher);

  SerializedData(const SerializedData&) = delete;
  SerializedData& operator=(const SerializedData) = delete;

  ~SerializedData();

  // Tears down the SerializedData. The tear down *must* be called before
  // the object is destroyed because the WebStateObserver may call methods on
  // the WebState being destroyed, which will have to be forwarded via `saved_`
  // pointer (thus it must be non-null).
  void TearDown();

  // Getter and setter for the CRWSessionStorage; only available when the
  // session serialization optimisation feature is disabled.
  // TODO(crbug.com/40245950): remove once the feature is fully launched.
  CRWSessionStorage* GetSessionStorage() const;
  void SetSessionStorage(CRWSessionStorage* storage);

  // Serializes the metadata to `storage`.
  void SerializeMetadataToProto(proto::WebStateMetadataStorage& storage) const;

  // Returns the callback used to load the complete data from disk.
  WebStateStorageLoader TakeStorageLoader();

  // Returns the callback used to fetch the native session data blob.
  NativeSessionFetcher TakeNativeSessionFetcher();

  // WebState:
  base::Time GetLastActiveTime() const;
  base::Time GetCreationTime() const;
  BrowserState* GetBrowserState() const;
  NSString* GetStableIdentifier() const;
  WebStateID GetUniqueIdentifier() const;
  const std::u16string& GetTitle() const;
  const FaviconStatus& GetFaviconStatus() const;
  void SetFaviconStatus(const FaviconStatus& favicon_status);
  int GetNavigationItemCount() const;
  const GURL& GetVisibleURL() const;
  const GURL& GetLastCommittedURL() const;

 private:
  // Returns a reference to the owning WebState WebStateObserverList.
  WebStateObserverList& observers() { return owner_->observers_; }

  // Returns a reference to the owning WebState WebStatePolicyDeciderList.
  WebStatePolicyDeciderList& policy_deciders() {
    return owner_->policy_deciders_;
  }

  // Owner. Never null. Owns this object.
  const raw_ptr<WebStateImpl> owner_;

  // The owning BrowserState. Indirectly owns this object.
  const raw_ptr<BrowserState> browser_state_;

  // The stable and unique identifiers.
  NSString* const stable_identifier_;
  const WebStateID unique_identifier_;

  // Information about this WebState available when the object is not
  // yet realized. This is limited to the information accessible in
  // the `storage` instance passed in the constructor.
  const base::Time creation_time_;
  const base::Time last_active_time_;
  const std::u16string page_title_;
  const GURL page_visible_url_;
  const int navigation_item_count_;

  // Favicon status.
  FaviconStatus favicon_status_;

  // Callbacks used to load the full data about this WebState.
  WebStateStorageLoader storage_loader_;
  NativeSessionFetcher session_fetcher_;

  // Serialized representation of the session; only available when the
  // session serialization optimisation feature is disabled.
  // TODO(crbug.com/40245950): remove once the feature is fully launched.
  __strong CRWSessionStorage* session_storage_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_
