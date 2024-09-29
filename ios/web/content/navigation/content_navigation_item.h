// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_ITEM_H_
#define IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_ITEM_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/https_upgrade_type.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/security/ssl_status.h"
#import "url/gurl.h"

namespace content {
class NavigationEntry;
}

namespace web {

class NavigationItemHolder;

// Wraps a content::NavigationEntry. The lifetime of instances is tied to that
// of the corresponding NavigationEntry (via SharedUserData).
class ContentNavigationItem : public NavigationItem {
 public:
  static NavigationItem* GetOrCreate(content::NavigationEntry* entry);
  ~ContentNavigationItem() override;

  int GetUniqueID() const override;

  void SetOriginalRequestURL(const GURL& url) override;
  const GURL& GetOriginalRequestURL() const override;

  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;

  void SetReferrer(const Referrer& referrer) override;
  const Referrer& GetReferrer() const override;

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

  HttpsUpgradeType GetHttpsUpgradeType() const override;
  void SetHttpsUpgradeType(HttpsUpgradeType https_upgrade_type) override;

 private:
  friend class NavigationItemHolder;

  explicit ContentNavigationItem(content::NavigationEntry* entry);
  raw_ptr<content::NavigationEntry> entry_ = nullptr;

  // We lazily update these in the corresponding getter. Since the value on
  // NavigationEntry isn't changed, the functions are still semantically
  // const.
  mutable Referrer referrer_;
  mutable HttpRequestHeaders* headers_ = nil;
  mutable FaviconStatus favicon_status_;
  mutable SSLStatus ssl_status_;

  UserAgentType user_agent_type_ = UserAgentType::MOBILE;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_ITEM_H_
