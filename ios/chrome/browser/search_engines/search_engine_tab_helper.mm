// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_fetcher.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kCommandPrefix[] = "searchEngine";
const char kCommandOpenSearch[] = "searchEngine.openSearch";
const char kOpenSearchPageUrlKey[] = "pageUrl";
const char kOpenSearchOsddUrlKey[] = "osddUrl";
const char kCommandSearchableUrl[] = "searchEngine.searchableUrl";
const char kSearchableUrlUrlKey[] = "url";

// Returns true if the |item|'s transition type is FORM_SUBMIT.
bool IsFormSubmit(const web::NavigationItem* item) {
  return ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                      ui::PAGE_TRANSITION_FORM_SUBMIT);
}

// Generates a keyword from |item|. This code is based on:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/search_engines/search_engine_tab_helper.cc
base::string16 GenerateKeywordFromNavigationItem(
    const web::NavigationItem* item) {
  // Don't autogenerate keywords for pages that are the result of form
  // submissions.
  if (IsFormSubmit(item))
    return base::string16();

  // The code from Desktop will try NavigationEntry::GetUserTypedURL() first if
  // available since that represents what the user typed to get here, and fall
  // back on the regular URL if not.
  // TODO(crbug.com/433824): Use GetUserTypedURL() once NavigationItem supports
  // it.
  GURL url = item->GetURL();
  if (!url.is_valid()) {
    return base::string16();
  }

  // Don't autogenerate keywords for referrers that
  // a) are anything other than HTTP/HTTPS or
  // b) have a path.
  //
  // To relax the path constraint, make sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!url.SchemeIsHTTPOrHTTPS() || url.path().length() > 1) {
    return base::string16();
  }

  return TemplateURL::GenerateKeyword(url);
}
}

SearchEngineTabHelper::~SearchEngineTabHelper() {}

SearchEngineTabHelper::SearchEngineTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  subscription_ = web_state->AddScriptCommandCallback(
      base::BindRepeating(&SearchEngineTabHelper::OnJsMessage,
                          base::Unretained(this)),
      kCommandPrefix);
  DCHECK(favicon::WebFaviconDriver::FromWebState(web_state));
  favicon_driver_observer_.Add(
      favicon::WebFaviconDriver::FromWebState(web_state));
}

void SearchEngineTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
  favicon_driver_observer_.RemoveAll();
}

// When favicon is updated, notify TemplateURLService about the change.
void SearchEngineTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  const GURL potential_search_url = driver->GetActiveURL();
  if (url_service && url_service->loaded() && potential_search_url.is_valid())
    url_service->UpdateProviderFavicons(potential_search_url, icon_url);
}

// When the page is loaded, checks if |searchable_url_| has a value generated
// from the <form> submission before the navigation. If true, and the navigation
// is successful, adds a TemplateURL by |searchable_url_|. |searchable_url_|
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

void SearchEngineTabHelper::OnJsMessage(const base::DictionaryValue& message,
                                        const GURL& page_url,
                                        bool user_is_interacting,
                                        web::WebFrame* sender_frame) {
  const base::Value* cmd = message.FindKey("command");
  if (!cmd || !cmd->is_string()) {
    return;
  }
  std::string cmd_str = cmd->GetString();
  if (cmd_str == kCommandOpenSearch) {
    const base::Value* document_url = message.FindKey(kOpenSearchPageUrlKey);
    if (!document_url || !document_url->is_string())
      return;
    const base::Value* osdd_url = message.FindKey(kOpenSearchOsddUrlKey);
    if (!osdd_url || !osdd_url->is_string())
      return;
    AddTemplateURLByOSDD(GURL(document_url->GetString()),
                         GURL(osdd_url->GetString()));
  } else if (cmd_str == kCommandSearchableUrl) {
    const base::Value* url = message.FindKey(kSearchableUrlUrlKey);
    if (!url || !url->is_string())
      return;
    // Save |url| to |searchable_url_| when generated from <form> submission,
    // and create the TemplateURL when the submission did lead to a successful
    // navigation.
    searchable_url_ = GURL(url->GetString());
  }
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
  // When |page_url| has file: scheme, this method doesn't work because of
  // http://b/issue?id=863583. For that reason, this doesn't check and allow
  // urls referring to osdd urls with same schemes.
  if (!osdd_url.is_valid() || !osdd_url.SchemeIsHTTPOrHTTPS())
    return;

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  if ((page_url != web_state_->GetLastCommittedURL()) ||
      (!ios::TemplateURLFetcherFactory::GetForBrowserState(browser_state)) ||
      (browser_state->IsOffTheRecord()))
    return;

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
  base::string16 keyword = GenerateKeywordFromNavigationItem(item);
  if (keyword.empty())
    return;

  // Download the OpenSearch description document. If this is successful, a
  // new keyword will be created when done. For the last two args:
  //   1. Used by newwork::ResourceRequest::render_frame_id. We don't use Blink
  //   so leave it to be the default value defined here:
  //      https://cs.chromium.org/chromium/src/services/network/public/cpp/resource_request.h?rcl=39c6fbea496641a6514e34c0ab689871d14e6d52&l=194;
  //   2. Used by network::ResourceRequest::resource_type. It's a design defect:
  //      https://cs.chromium.org/chromium/src/services/network/public/cpp/resource_request.h?rcl=39c6fbea496641a6514e34c0ab689871d14e6d52&l=100
  //      Use the same value as the SearchEngineTabHelper for Desktop.
  ios::TemplateURLFetcherFactory::GetForBrowserState(browser_state)
      ->ScheduleDownload(keyword, osdd_url, item->GetFavicon().url,
                         url::Origin::Create(web_state_->GetLastCommittedURL()),
                         browser_state->GetURLLoaderFactory(), MSG_ROUTING_NONE,
                         /* content::ResourceType::kSubResource */ 6);
}

// Creates a TemplateURL by |searchable_url| and adds it to TemplateURLService.
// This code is based on:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/search_engines/search_engine_tab_helper.cc
void SearchEngineTabHelper::AddTemplateURLBySearchableURL(
    const GURL& searchable_url) {
  if (!searchable_url.is_valid()) {
    return;
  }

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  // Don't add TemplateURL under incognito mode.
  if (browser_state->IsOffTheRecord())
    return;

  const web::NavigationManager* manager = web_state_->GetNavigationManager();
  int last_index = manager->GetLastCommittedItemIndex();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  if (last_index <= 0)
    return;
  const web::NavigationItem* current_item = manager->GetItemAtIndex(last_index);
  const web::NavigationItem* previous_item =
      manager->GetItemAtIndex(last_index - 1);

  base::string16 keyword(GenerateKeywordFromNavigationItem(previous_item));
  if (keyword.empty())
    return;

  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  if (!url_service)
    return;

  if (!url_service->loaded()) {
    url_service->Load();
    return;
  }

  const TemplateURL* existing_url;
  if (!url_service->CanAddAutogeneratedKeyword(keyword, searchable_url,
                                               &existing_url)) {
    return;
  }

  if (existing_url) {
    if (existing_url->originating_url().is_valid()) {
      // The existing keyword was generated from an OpenSearch description
      // document, don't regenerate.
      return;
    }
    url_service->Remove(existing_url);
  }

  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL(searchable_url.spec());

  // Try to get favicon url by following methods:
  //   1. Get from FaviconStatus of previous NavigationItem;
  //   2. Create by current NavigationItem's referrer if valid;
  //   3. Create by previous NavigationItem's URL if valid;
  if (previous_item->GetFavicon().url.is_valid()) {
    data.favicon_url = previous_item->GetFavicon().url;
  } else if (current_item->GetReferrer().url.is_valid()) {
    data.favicon_url =
        TemplateURL::GenerateFaviconURL(current_item->GetReferrer().url);
  } else if (previous_item->GetURL().is_valid()) {
    data.favicon_url = TemplateURL::GenerateFaviconURL(previous_item->GetURL());
  }
  data.safe_for_autoreplace = true;
  url_service->Add(std::make_unique<TemplateURL>(data));
}

WEB_STATE_USER_DATA_KEY_IMPL(SearchEngineTabHelper)
