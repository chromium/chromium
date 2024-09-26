// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MODEL_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MODEL_DELEGATE_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/location_bar_model_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class WebStateList;

namespace web {
class NavigationItem;
class WebState;
}  // namespace web

// Implementation of LocationBarModelDelegate which uses an instance of
// WebStateLisy in order to fulfill its duties.
class LocationBarModelDelegateIOS : public LocationBarModelDelegate {
 public:
  // `web_state_list` must outlive this LocationBarModelDelegateIOS object.
  explicit LocationBarModelDelegateIOS(WebStateList* web_state_list,
                                       ProfileIOS* profile);

  LocationBarModelDelegateIOS(const LocationBarModelDelegateIOS&) = delete;
  LocationBarModelDelegateIOS& operator=(const LocationBarModelDelegateIOS&) =
      delete;

  ~LocationBarModelDelegateIOS() override;

  // LocationBarModelDelegate implementation:
  std::u16string FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::u16string& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldDisplayURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;
  const gfx::VectorIcon* GetVectorIconOverride() const override;
  bool IsOfflinePage() const override;
  bool IsNewTabPage() const override;
  bool IsNewTabPageURL(const GURL& url) const override;
  bool IsHomePage(const GURL& url) const override;
  TemplateURLService* GetTemplateURLService() override;

 private:
  // Helper method to extract the NavigationItem from which the states are
  // retrieved. If this returns null (which can happens during initialization),
  // default values are used.
  web::NavigationItem* GetNavigationItem() const;

  // Returns the active WebState.
  web::WebState* GetActiveWebState() const;

  raw_ptr<WebStateList> web_state_list_;  // weak

  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MODEL_DELEGATE_IOS_H_
