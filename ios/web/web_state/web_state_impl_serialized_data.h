// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_

#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/web_state/web_state_impl.h"

@class CRWNavigationItemStorage;

namespace web {

// Object storing the information needed to realize a WebState.
//
// This object is mostly a storage, but has some helper method to allow
// an "unrealized" WebState to be queried for information available in
// the serialized state.
//
// Like RealizedWebState, the WebStateImpl may forward method to this
// class. Those methods will not be documented, but instead they will
// be annotated with the name of the class that declares them.
class WebStateImpl::SerializedData {
 public:
  // Creates a SerializedData with a non-null pointer to the owning
  // WebStateImpl and a copy of the serialized state.
  SerializedData(WebStateImpl* owner,
                 const CreateParams& create_params,
                 CRWSessionStorage* session_storage);

  SerializedData(const SerializedData&) = delete;
  SerializedData& operator=(const SerializedData) = delete;

  ~SerializedData();

  // Tears down the SerializedData. The tear down *must* be called before
  // the object is destroyed because the WebStateObserver may call methods on
  // the WebState being destroyed, which will have to be forwarded via `saved_`
  // pointer (thus it must be non-null).
  void TearDown();

  // Returns a copy of `params_`.
  CreateParams GetCreateParams() const;

  // Returns the serialized representation of the session.
  CRWSessionStorage* GetSessionStorage() const;

  // WebState:
  base::Time GetLastActiveTime() const;
  base::Time GetCreationTime() const;
  BrowserState* GetBrowserState() const;
  NSString* GetStableIdentifier() const;
  SessionID GetUniqueIdentifier() const;
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

  // Returns the CRWNavigationItemStorage* corresponding to the last committed
  // navigation item from the serialized state. May return nil.
  CRWNavigationItemStorage* GetLastCommittedItem() const;

  // Owner. Never null. Owns this object.
  WebStateImpl* owner_ = nullptr;

  // Parameters to initialize the RealizedWebState.
  CreateParams create_params_;

  // Serialized representation of the session. Never nil.
  __strong CRWSessionStorage* session_storage_ = nil;

  // Favicon status.
  FaviconStatus favicon_status_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_SERIALIZED_DATA_H_
