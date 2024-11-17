// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"

#import <string_view>

#import "base/check.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/prefs/pref_service.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"

LocationBarModelDelegateIOS::LocationBarModelDelegateIOS(
    WebStateList* web_state_list,
    ProfileIOS* profile)
    : web_state_list_(web_state_list), profile_(profile) {}

LocationBarModelDelegateIOS::~LocationBarModelDelegateIOS() {}

web::WebState* LocationBarModelDelegateIOS::GetActiveWebState() const {
  return web_state_list_->GetActiveWebState();
}

web::NavigationItem* LocationBarModelDelegateIOS::GetNavigationItem() const {
  web::WebState* web_state = GetActiveWebState();
  web::NavigationManager* navigation_manager =
      web_state ? web_state->GetNavigationManager() : nullptr;
  return navigation_manager ? navigation_manager->GetVisibleItem() : nullptr;
}

std::u16string
LocationBarModelDelegateIOS::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const std::u16string& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, AutocompleteSchemeClassifierImpl(), nullptr);
}

bool LocationBarModelDelegateIOS::GetURL(GURL* url) const {
  DCHECK(url);
  web::NavigationItem* item = GetNavigationItem();
  if (!item)
    return false;
  *url = item->GetVirtualURL();
  // Return `false` for about scheme pages.  This will result in the location
  // bar showing the default page, "about:blank". See crbug.com/989497 for
  // details on why.
  if (url->SchemeIs(url::kAboutScheme))
    return false;
  return true;
}

bool LocationBarModelDelegateIOS::ShouldDisplayURL() const {
  if (web::WebState* web_state = GetActiveWebState()) {
    security_interstitials::IOSBlockingPageTabHelper* tab_helper =
        security_interstitials::IOSBlockingPageTabHelper::FromWebState(
            web_state);
    if (tab_helper && tab_helper->GetCurrentBlockingPage()) {
      return tab_helper->ShouldDisplayURL();
    }
  }

  web::NavigationItem* item = GetNavigationItem();
  if (item) {
    GURL url = item->GetURL();
    GURL virtual_url = item->GetVirtualURL();
    if (url.SchemeIs(kChromeUIScheme) ||
        virtual_url.SchemeIs(kChromeUIScheme)) {
      if (!url.SchemeIs(kChromeUIScheme))
        url = virtual_url;
      std::string_view host = url.host_piece();
      return host != kChromeUINewTabHost;
    }
  }
  return true;
}

security_state::SecurityLevel LocationBarModelDelegateIOS::GetSecurityLevel()
    const {
  web::WebState* web_state = GetActiveWebState();
  return security_state::GetSecurityLevelForWebState(web_state);
}

std::unique_ptr<security_state::VisibleSecurityState>
LocationBarModelDelegateIOS::GetVisibleSecurityState() const {
  web::WebState* web_state = GetActiveWebState();
  return security_state::GetVisibleSecurityStateForWebState(web_state);
}

scoped_refptr<net::X509Certificate>
LocationBarModelDelegateIOS::GetCertificate() const {
  web::NavigationItem* item = GetNavigationItem();
  if (item)
    return item->GetSSL().certificate;
  return scoped_refptr<net::X509Certificate>();
}

const gfx::VectorIcon* LocationBarModelDelegateIOS::GetVectorIconOverride()
    const {
  return nullptr;
}

bool LocationBarModelDelegateIOS::IsOfflinePage() const {
  web::WebState* web_state = GetActiveWebState();
  if (!web_state)
    return false;
  return OfflinePageTabHelper::FromWebState(web_state)
      ->presenting_offline_page();
}

bool LocationBarModelDelegateIOS::IsNewTabPage() const {
  // This is currently only called by the OmniboxEditModel to determine if the
  // Google landing page is showing.
  //
  // TODO(crbug.com/40340113)(lliabraa): This should also check the user's
  // default search engine because if they're not using Google the Google
  // landing page is not shown.
  GURL currentURL;
  if (!GetURL(&currentURL))
    return false;
  return currentURL == kChromeUINewTabURL;
}

bool LocationBarModelDelegateIOS::IsNewTabPageURL(const GURL& url) const {
  return url.spec() == kChromeUINewTabURL;
}

bool LocationBarModelDelegateIOS::IsHomePage(const GURL& url) const {
  // iOS does not have a notion of home page.
  return false;
}

TemplateURLService* LocationBarModelDelegateIOS::GetTemplateURLService()

{
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}
