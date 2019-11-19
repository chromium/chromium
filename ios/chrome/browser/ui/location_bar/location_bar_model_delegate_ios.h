// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MODEL_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MODEL_DELEGATE_IOS_H_

#include "base/macros.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"

class WebStateList;

namespace web {
class NavigationItem;
class WebState;
}  // namespace web

// Implementation of LocationBarModelDelegate which uses an instance of
// TabModel in order to fulfill its duties.
class LocationBarModelDelegateIOS : public LocationBarModelDelegate {
 public:
  // |web_state_list| must outlive this LocationBarModelDelegateIOS object.
  explicit LocationBarModelDelegateIOS(WebStateList* web_state_list);
  ~LocationBarModelDelegateIOS() override;

  // LocationBarModelDelegate implementation:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldDisplayURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;
  const gfx::VectorIcon* GetVectorIconOverride() const override;
  bool IsOfflinePage() const override;
  bool IsInstantNTP() const override;
  bool IsNewTabPage(const GURL& url) const override;
  bool IsHomePage(const GURL& url) const override;

 private:
  // Helper method to extract the NavigationItem from which the states are
  // retrieved. If this returns null (which can happens during initialization),
  // default values are used.
  web::NavigationItem* GetNavigationItem() const;

  // Returns the active WebState.
  web::WebState* GetActiveWebState() const;

  WebStateList* web_state_list_;  // weak

  DISALLOW_COPY_AND_ASSIGN(LocationBarModelDelegateIOS);
};

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MODEL_DELEGATE_IOS_H_
