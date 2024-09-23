// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/model/history_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "components/history/core/browser/history_constants.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/strings/grit/components_strings.h"
#import "components/translate/core/common/language_detection_details.h"
#import "ios/chrome/browser/complex_tasks/model/ios_content_record_task_id.h"
#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/http/http_response_headers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

std::optional<std::u16string> GetPageTitle(const web::NavigationItem& item) {
  const std::u16string& title = item.GetTitleForDisplay();
  if (title.empty() ||
      title == l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE)) {
    return std::nullopt;
  }

  return std::optional<std::u16string>(title);
}

}  // namespace

HistoryTabHelper::~HistoryTabHelper() {
  DCHECK(!web_state_);
}

void HistoryTabHelper::UpdateHistoryForNavigation(
    const history::HistoryAddPageArgs& add_page_args) {
  history::HistoryService* history_service = GetHistoryService();
  if (!history_service)
    return;

  // Update the previous navigation's end time.
  if (cached_navigation_state_) {
    history_service->UpdateWithPageEndTime(
        GetContextID(), cached_navigation_state_->nav_entry_id,
        cached_navigation_state_->url, base::Time::Now());
  }
  // Cache the relevant fields of the current navigation, so we can later update
  // its end time too.
  cached_navigation_state_ = {add_page_args.nav_entry_id, add_page_args.url};

  // Now, actually add the new navigation to history.
  history_service->AddPage(add_page_args);
}

void HistoryTabHelper::UpdateHistoryPageTitle(const web::NavigationItem& item) {
  DCHECK(!delay_notification_);

  const std::optional<std::u16string> title = GetPageTitle(item);
  // Don't update the history if current entry has no title.
  if (!title) {
    return;
  }

  history::HistoryService* history_service = GetHistoryService();
  if (history_service) {
    history_service->SetPageTitle(item.GetVirtualURL(), title.value());
  }
}

history::HistoryAddPageArgs HistoryTabHelper::CreateHistoryAddPageArgs(
    web::NavigationItem* last_committed_item,
    web::NavigationContext* navigation_context) {
  const GURL& url = last_committed_item->GetURL();

  const ui::PageTransition transition =
      last_committed_item->GetTransitionType();

  history::RedirectList redirects;
  const GURL& original_url = last_committed_item->GetOriginalRequestURL();
  const GURL& referrer_url = last_committed_item->GetReferrer().url;
  if (original_url != url) {
    // Simulate a valid redirect chain in case of URLs that have been modified
    // by CRWWebController -finishHistoryNavigationFromEntry:.
    if (transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT ||
        url.EqualsIgnoringRef(original_url)) {
      redirects.push_back(referrer_url);
    }
    // TODO(crbug.com/40511880): the redirect chain is not constructed the same
    // way as desktop so this part needs to be revised.
    redirects.push_back(original_url);
    redirects.push_back(url);
  }

  // Navigations originating from New Tab Page or Reading List should not
  // contribute to Most Visited.
  const bool content_suggestions_navigation = ui::PageTransitionCoreTypeIs(
      transition, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  const bool consider_for_ntp_most_visited =
      !content_suggestions_navigation &&
      referrer_url != kReadingListReferrerURL;

  const int http_response_code =
      navigation_context->GetResponseHeaders()
          ? navigation_context->GetResponseHeaders()->response_code()
          : 0;

  // Hide navigations that result in an error in order to prevent the omnibox
  // from suggesting URLs that have never been navigated to successfully.
  // (If a navigation to the URL succeeds at some point, the URL will be
  // unhidden and thus eligible to be suggested by the omnibox.)
  const bool hidden = (http_response_code >= 400);

  history::VisitContextAnnotations::OnVisitFields context_annotations;

  context_annotations.browser_type =
      history::VisitContextAnnotations::BrowserType::kTabbed;

  IOSChromeSessionTabHelper* session_tab_helper =
      IOSChromeSessionTabHelper::FromWebState(web_state_);
  if (session_tab_helper) {
    context_annotations.window_id = session_tab_helper->window_id();
    context_annotations.tab_id = session_tab_helper->session_id();
  }

  IOSTaskTabHelper* task_tab_helper =
      IOSTaskTabHelper::FromWebState(web_state_);
  if (task_tab_helper) {
    const IOSContentRecordTaskId* content_record_task_id =
        task_tab_helper->GetContextRecordTaskId(
            last_committed_item->GetUniqueID());
    if (content_record_task_id) {
      context_annotations.task_id = content_record_task_id->task_id();
      context_annotations.root_task_id = content_record_task_id->root_task_id();
      context_annotations.parent_task_id =
          content_record_task_id->parent_task_id();
    }
  }

  context_annotations.response_code = http_response_code;

  return history::HistoryAddPageArgs(
      url, last_committed_item->GetTimestamp(), GetContextID(),
      last_committed_item->GetUniqueID(), navigation_context->GetNavigationId(),
      referrer_url, redirects, transition, hidden, history::SOURCE_BROWSED,
      /*did_replace_entry=*/false, consider_for_ntp_most_visited,
      navigation_context->IsSameDocument() ? GetPageTitle(*last_committed_item)
                                           : std::nullopt,
      // TODO(crbug.com/40279742): due to WebKit constraints, iOS does not
      // support triple-key partitioning. Once supported, we need to populate
      // `top_level_url` with the correct value. Until then, :visited history on
      // iOS is unpartitioned.
      /*top_level_url=*/std::nullopt,
      /*opener=*/std::nullopt,
      /*bookmark_id=*/std::nullopt,
      /*app_id=*/std::nullopt,
      /*context_annotations=*/std::move(context_annotations));
}

void HistoryTabHelper::SetDelayHistoryServiceNotification(
    bool delay_notification) {
  delay_notification_ = delay_notification;
  if (delay_notification_) {
    return;
  }

  for (const auto& add_page_args : recorded_navigations_) {
    UpdateHistoryForNavigation(add_page_args);
  }

  std::vector<history::HistoryAddPageArgs> empty_vector;
  std::swap(recorded_navigations_, empty_vector);

  web::NavigationItem* last_committed_item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  if (last_committed_item) {
    UpdateHistoryPageTitle(*last_committed_item);
  }
}

void HistoryTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (history::HistoryService* hs = GetHistoryService()) {
    web::NavigationItem* last_committed_item =
        web_state_->GetNavigationManager()->GetLastCommittedItem();
    if (last_committed_item) {
      hs->SetPageLanguageForVisit(
          GetContextID(), last_committed_item->GetUniqueID(),
          web_state_->GetLastCommittedURL(), details.adopted_language);
    }
  }
}

HistoryTabHelper::HistoryTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);

  // A translate client is not always attached to a web state (e.g. tests).
  if (ChromeIOSTranslateClient* translate_client =
          ChromeIOSTranslateClient::FromWebState(web_state)) {
    translate_observation_.Observe(translate_client->GetTranslateDriver());
  }
}

void HistoryTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (web_state_->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  // Do not record failed navigation nor 404 to the history (to prevent them
  // from showing up as Most Visited tiles on NTP).
  if (navigation_context->GetError()) {
    return;
  }

  if (navigation_context->GetResponseHeaders() &&
      navigation_context->GetResponseHeaders()->response_code() == 404) {
    return;
  }

  // TODO(crbug.com/41441240): Remove GetLastCommittedItem nil check once
  // HasComitted has been fixed.
  if (!navigation_context->HasCommitted() ||
      !web_state_->GetNavigationManager()->GetLastCommittedItem()) {
    // Navigation was replaced or aborted.
    return;
  }

  web::NavigationItem* last_committed_item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  DCHECK(!last_committed_item->GetTimestamp().is_null());

  // Do not update the history database for back/forward navigations.
  // TODO(crbug.com/40491761): on iOS the navigation is not currently tagged
  // with a ui::PAGE_TRANSITION_FORWARD_BACK transition.
  const ui::PageTransition transition =
      last_committed_item->GetTransitionType();
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return;
  }

  // Do not update the history database for data: urls. This diverges from
  // desktop, but prevents dumping huge view-source urls into the history
  // database. Keep it NDEBUG only because view-source:// URLs are enabled
  // on NDEBUG builds only.
#ifndef NDEBUG
  const GURL& url = last_committed_item->GetURL();
  if (url.SchemeIs(url::kDataScheme)) {
    return;
  }
#endif

  num_title_changes_ = 0;

  history::HistoryAddPageArgs add_page_args =
      CreateHistoryAddPageArgs(last_committed_item, navigation_context);

  if (delay_notification_) {
    recorded_navigations_.push_back(std::move(add_page_args));
  } else {
    DCHECK(recorded_navigations_.empty());

    UpdateHistoryForNavigation(add_page_args);
    UpdateHistoryPageTitle(*last_committed_item);
  }
}

void HistoryTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  last_load_completion_ = base::TimeTicks::Now();
}

void HistoryTabHelper::TitleWasSet(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  if (delay_notification_) {
    return;
  }

  // Protect against pages changing their title too often during page load.
  if (num_title_changes_ >= history::kMaxTitleChanges)
    return;

  // Only store page titles into history if they were set while the page was
  // loading or during a brief span after load is complete. This fixes the case
  // where a page uses a title change to alert a user of a situation but that
  // title change ends up saved in history.
  if (web_state->IsLoading() ||
      (base::TimeTicks::Now() - last_load_completion_ <
       history::GetTitleSettingWindow())) {
    web::NavigationItem* last_committed_item =
        web_state_->GetNavigationManager()->GetLastCommittedItem();
    if (last_committed_item) {
      UpdateHistoryPageTitle(*last_committed_item);
      ++num_title_changes_;
    }
  }
}

void HistoryTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  translate_observation_.Reset();

  history::HistoryService* history_service = GetHistoryService();
  if (history_service) {
    // If there is a current history-eligible navigation in this tab (i.e.
    // `cached_navigation_state_` exists), that visit is concluded now, so
    // update its end time.
    if (cached_navigation_state_) {
      history_service->UpdateWithPageEndTime(
          GetContextID(), cached_navigation_state_->nav_entry_id,
          cached_navigation_state_->url, base::Time::Now());
    }

    history_service->ClearCachedDataForContextID(GetContextID());
  }

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

history::HistoryService* HistoryTabHelper::GetHistoryService() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  return ios::HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_STATE_USER_DATA_KEY_IMPL(HistoryTabHelper)
