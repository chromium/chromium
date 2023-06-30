// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "base/time/time.h"
#include "ios/web/navigation/navigation_initiation_type.h"
#include "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/security/ssl_status.h"
#include "url/gurl.h"

namespace web {
namespace proto {
class NavigationItemStorage;
}  // namespace proto

enum class NavigationInitiationType;

// Implementation of NavigationItem.
class NavigationItemImpl : public web::NavigationItem {
 public:
  // Creates a default NavigationItemImpl.
  NavigationItemImpl();
  ~NavigationItemImpl() override;

  // Creates a NavigationItemImpl from serialized representation.
  explicit NavigationItemImpl(const proto::NavigationItemStorage& storage);

  // Serializes the NavigationItemImpl into `storage`.
  void SerializeToProto(proto::NavigationItemStorage& storage) const;

  // Clones the current object.
  std::unique_ptr<NavigationItemImpl> Clone();

  // NavigationItem implementation:
  int GetUniqueID() const override;
  void SetOriginalRequestURL(const GURL& url) override;
  const GURL& GetOriginalRequestURL() const override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;
  void SetReferrer(const web::Referrer& referrer) override;
  const web::Referrer& GetReferrer() const override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() const override;
  void SetTitle(const std::u16string& title) override;
  const std::u16string& GetTitle() const override;
  const std::u16string& GetTitleForDisplay() const override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() const override;
  const FaviconStatus& GetFaviconStatus() const override;
  void SetFaviconStatus(const FaviconStatus& favicon_status) override;
  const SSLStatus& GetSSL() const override;
  SSLStatus& GetSSL() override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() const override;
  void SetUserAgentType(UserAgentType type) override;
  UserAgentType GetUserAgentType() const override;
  bool HasPostData() const override;
  HttpRequestHeaders* GetHttpRequestHeaders() const override;
  void AddHttpRequestHeaders(HttpRequestHeaders* additional_headers) override;
  void SetHttpsUpgradeType(HttpsUpgradeType https_upgrade_type) override;
  HttpsUpgradeType GetHttpsUpgradeType() const override;

  // Serialized representation of the state object that was used in conjunction
  // with a JavaScript window.history.pushState() or
  // window.history.replaceState() call that created or modified this
  // NavigationItem. Intended to be used for JavaScript history operations and
  // will be nil in most cases.
  void SetSerializedStateObject(NSString* serialized_state_object);
  NSString* GetSerializedStateObject() const;

  // Whether this navigation is the result of a hash change.
  void SetIsCreatedFromHashChange(bool hash_change);
  bool IsCreatedFromHashChange() const;

  // Initiation type of this pending navigation. Resets to NONE after commit.
  void SetNavigationInitiationType(
      web::NavigationInitiationType navigation_initiation_type);
  web::NavigationInitiationType NavigationInitiationType() const;

  // Whether or not to bypass serializing this item to session storage.  Set to
  // YES to skip saving this page (and therefore restoring this page).
  void SetShouldSkipSerialization(bool skip);

  // Returns whether the page should be skipped when serializing. Will return
  // true if `SetShouldSkipSerialization(YES)` was called but may return true
  // in other circumstances (e.g. URL too long, ...).
  bool ShouldSkipSerialization() const;

  // Data submitted with a POST request, persisted for resubmits.
  void SetPostData(NSData* post_data);
  NSData* GetPostData() const;

  // Removes the header for `key` from `http_request_headers_`.
  void RemoveHttpRequestHeaderForKey(NSString* key);

  // Removes all http headers from `http_request_headers_`.
  void ResetHttpRequestHeaders();

  // Once a navigation item is committed, we should no longer track
  // non-persisted state, as documented on the members below.
  void ResetForCommit();

  // Returns the title string to be used for a page with `url` if that page
  // doesn't specify a title.
  static std::u16string GetDisplayTitleForURL(const GURL& url);

  // Used only by NavigationManagerImpl.  SetUntrusted() is only used for
  // Visible or LastCommitted NavigationItems where the `url_` may be incorrect
  // due to timining problems or bugs in WKWebView.
  void SetUntrusted();
  bool IsUntrusted();

  // Restores the state of the `other` navigation item in this item.
  void RestoreStateFromItem(NavigationItem* other);

#ifndef NDEBUG
  // Returns a human-readable description of the state for debugging purposes.
  NSString* GetDescription() const;
#endif

 private:
  // Explicit copy constructor since the super class is not copyable.
  // Used to implement Clone().
  NavigationItemImpl(const NavigationItemImpl& item);

  const int unique_id_;
  GURL original_request_url_;
  GURL url_;
  Referrer referrer_;
  GURL virtual_url_;
  std::u16string title_;
  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;
  FaviconStatus favicon_status_;
  SSLStatus ssl_;
  base::Time timestamp_;
  UserAgentType user_agent_type_ = UserAgentType::NONE;
  NSMutableDictionary* http_request_headers_ = nil;

  NSString* serialized_state_object_ = nil;
  bool is_created_from_hash_change_ = false;
  bool should_skip_serialization_ = false;
  NSData* post_data_ = nil;

  // The navigation initiation type of the item.  This decides whether the URL
  // should be displayed before the navigation commits.  It is cleared in
  // `ResetForCommit` and not persisted.
  web::NavigationInitiationType navigation_initiation_type_ =
      web::NavigationInitiationType::NONE;

  // Used only by NavigationManagerImpl.  `is_untrusted_` is only `true` for
  // Visible or LastCommitted NavigationItems where the `url_` may be incorrect
  // due to timining problems or bugs in WKWebView.
  bool is_untrusted_ = false;

  // This is a cached version of the result of GetTitleForDisplay. When the URL,
  // virtual URL, or title is set, this should be cleared to force a refresh.
  mutable std::u16string cached_display_title_;

  // Type of the HTTPS upgrade applied to this navigation, if any.
  HttpsUpgradeType https_upgrade_type_ = HttpsUpgradeType::kNone;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
