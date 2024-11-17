// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper.h"

#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_fetcher.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_fetcher_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

// Returns true if the `item`'s transition type is FORM_SUBMIT.
bool IsFormSubmit(const web::NavigationItem* item) {
  return ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                      ui::PAGE_TRANSITION_FORM_SUBMIT);
}

// Generates a keyword from `item`. This code is based on:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/search_engines/search_engine_tab_helper.cc
std::u16string GenerateKeywordFromNavigationItem(
    const web::NavigationItem* item) {
  // Don't autogenerate keywords for pages that are the result of form
  // submissions.
  if (IsFormSubmit(item))
    return std::u16string();

  // The code from Desktop will try NavigationEntry::GetUserTypedURL() first if
  // available since that represents what the user typed to get here, and fall
  // back on the regular URL if not.
  // TODO(crbug.com/40394195): Use GetUserTypedURL() once NavigationItem
  // supports it.
  GURL url = item->GetURL();
  if (!url.is_valid()) {
    return std::u16string();
  }

  // Don't autogenerate keywords for referrers that
  // a) are anything other than HTTP/HTTPS or
  // b) have a path.
  //
  // To relax the path constraint, make sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!url.SchemeIsHTTPOrHTTPS() || url.path().length() > 1) {
    return std::u16string();
  }

  return TemplateURL::GenerateKeyword(url);
}
}

SearchEngineTabHelper::~SearchEngineTabHelper() {}

SearchEngineTabHelper::SearchEngineTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  DCHECK(favicon::WebFaviconDriver::FromWebState(web_state));
  favicon_driver_observation_.Observe(
      favicon::WebFaviconDriver::FromWebState(web_state));
}

void SearchEngineTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
  favicon_driver_observation_.Reset();
}

// When favicon is updated, notify TemplateURLService about the change.
void SearchEngineTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  const GURL potential_search_url = driver->GetActiveURL();
  if (url_service && url_service->loaded() && potential_search_url.is_valid())
    url_service->UpdateProviderFavicons(potential_search_url, icon_url);
}

// When the page is loaded, checks if `searchable_url_` has a value generated
// from the <form> submission before the navigation. If true, and the navigation
// is successful, adds a TemplateURL by `searchable_url_`. `searchable_url_`
// will be set to empty in the end.
void SearchEngineTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!searchable_url_.is_empty()) {
    if (!navigation_context->GetError() &&
        !navigation_context->IsSameDocument()) {
      AddTemplateURLBySearchableURL(searchable_url_);
    }
    searchable_url_ = GURL();
  }
}

void SearchEngineTabHelper::SetSearchableUrl(GURL searchable_url) {
  searchable_url_ = searchable_url;
}

// Creates a new TemplateURL by OSDD. The TemplateURL will be added to
// TemplateURLService by TemplateURLFecther. This code is based on:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/search_engines/search_engine_tab_helper.cc
void SearchEngineTabHelper::AddTemplateURLByOSDD(const GURL& page_url,
                                                 const GURL& osdd_url) {
  // Checks to see if we should generate a keyword based on the OSDD, and if
  // necessary uses TemplateURLFetcher to download the OSDD and create a
  // keyword.

  // Make sure that the page is the current page and other basic checks.
  // When `page_url` has file: scheme, this method doesn't work because of
  // http://b/issue?id=863583. For that reason, this doesn't check and allow
  // urls referring to osdd urls with same schemes.
  if (!osdd_url.is_valid() || !osdd_url.SchemeIsHTTPOrHTTPS())
    return;

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if ((page_url != web_state_->GetLastCommittedURL()) ||
      (!ios::TemplateURLFetcherFactory::GetForProfile(profile)) ||
      (profile->IsOffTheRecord())) {
    return;
  }

  // If the current page is a form submit, find the last page that was not a
  // form submit and use its url to generate the keyword from.
  const web::NavigationManager* manager = web_state_->GetNavigationManager();
  const web::NavigationItem* item = nullptr;
  for (int index = manager->GetLastCommittedItemIndex(); true; --index) {
    if (index < 0)
      return;
    item = manager->GetItemAtIndex(index);
    if (!IsFormSubmit(item))
      break;
  }

  // Autogenerate a keyword for the autodetected case; in the other cases we'll
  // generate a keyword later after fetching the OSDD.
  std::u16string keyword = GenerateKeywordFromNavigationItem(item);
  if (keyword.empty())
    return;

  // Download the OpenSearch description document. If this is successful, a
  // new keyword will be created when done. For `render_frame_id` arg, it's used
  // by network::ResourceRequest::render_frame_id, we don't use Blink so leave
  // it to be the default value defined here:
  //   https://cs.chromium.org/chromium/src/services/network/public/cpp/resource_request.h?rcl=39c6fbea496641a6514e34c0ab689871d14e6d52&l=194;
  ios::TemplateURLFetcherFactory::GetForProfile(profile)->ScheduleDownload(
      keyword, osdd_url, item->GetFaviconStatus().url,
      url::Origin::Create(web_state_->GetLastCommittedURL()),
      profile->GetURLLoaderFactory(),
      /* render_frame_id */ MSG_ROUTING_NONE,
      /* request_id */ 0);
}

// Creates a TemplateURL by `searchable_url` and adds it to TemplateURLService.
// This code is based on:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/search_engines/search_engine_tab_helper.cc
void SearchEngineTabHelper::AddTemplateURLBySearchableURL(
    const GURL& searchable_url) {
  if (!searchable_url.is_valid()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  // Don't add TemplateURL under incognito mode.
  if (profile->IsOffTheRecord()) {
    return;
  }

  const web::NavigationManager* manager = web_state_->GetNavigationManager();
  int last_index = manager->GetLastCommittedItemIndex();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  if (last_index <= 0)
    return;
  const web::NavigationItem* current_item = manager->GetItemAtIndex(last_index);
  const web::NavigationItem* previous_item =
      manager->GetItemAtIndex(last_index - 1);

  std::u16string keyword(GenerateKeywordFromNavigationItem(previous_item));
  if (keyword.empty())
    return;

  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  if (!url_service)
    return;

  if (!url_service->loaded()) {
    url_service->Load();
    return;
  }

  if (!url_service->CanAddAutogeneratedKeyword(keyword, searchable_url)) {
    return;
  }

  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL(searchable_url.spec());

  // Try to get favicon url by following methods:
  //   1. Get from FaviconStatus of previous NavigationItem;
  //   2. Create by current NavigationItem's referrer if valid;
  //   3. Create by previous NavigationItem's URL if valid;
  const web::FaviconStatus& previous_item_favicon_status =
      previous_item->GetFaviconStatus();
  if (previous_item_favicon_status.url.is_valid()) {
    data.favicon_url = previous_item_favicon_status.url;
  } else if (current_item->GetReferrer().url.is_valid()) {
    data.favicon_url =
        TemplateURL::GenerateFaviconURL(current_item->GetReferrer().url);
  } else if (previous_item->GetURL().is_valid()) {
    data.favicon_url = TemplateURL::GenerateFaviconURL(previous_item->GetURL());
  }
  data.safe_for_autoreplace = true;

  // This Add() call may displace the previously auto-generated TemplateURL.
  // But it will never displace the Default Search Engine, nor will it displace
  // any OpenSearch document derived engines, which outrank this one.
  url_service->Add(std::make_unique<TemplateURL>(data));
}

WEB_STATE_USER_DATA_KEY_IMPL(SearchEngineTabHelper)
