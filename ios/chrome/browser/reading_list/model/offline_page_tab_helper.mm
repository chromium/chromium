// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"

#import "base/base64.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/memory/ptr_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/timer/timer.h"
#import "components/reading_list/core/offline_url_utils.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

namespace {
// Gets the offline data at `offline_path`. The result is a single std::string
// with all resources inlined.
// This method access file system and cannot be called on UI thread.
// TODO(crbug.com/40164221): Remove backwards compatibility after M95
std::string GetOfflineData(base::FilePath offline_root,
                           base::FilePath offline_path) {
  base::FilePath absolute_path =
      reading_list::OfflineURLAbsolutePathFromRelativePath(offline_root,
                                                           offline_path);

  std::string content;
  if (!base::ReadFileToString(absolute_path, &content)) {
    return std::string();
  }

  if (offline_path.Extension() != ".html") {
    // If page is not html (in the current version, it means it is PDF), there
    // are no resources to inline.
    return content;
  }
  base::FileEnumerator images_enumerator(absolute_path.DirName(), false,
                                         base::FileEnumerator::FILES);
  for (base::FilePath image_path = images_enumerator.Next();
       !image_path.empty(); image_path = images_enumerator.Next()) {
    if (image_path == absolute_path) {
      // Skip the root file.
      continue;
    }
    std::string file_name = image_path.BaseName().value();
    if (file_name.size() != 32) {
      // Resource file names are hashes with 32 hexadecimal characters.
      continue;
    }
    std::string image;
    if (!base::ReadFileToString(image_path, &image)) {
      continue;
    }
    image = base::Base64Encode(image);
    std::string src_with_file = base::StringPrintf("%s", file_name.c_str());
    std::string src_with_data =
        base::StringPrintf("data:image/png;base64,%s", image.c_str());
    base::ReplaceSubstringsAfterOffset(&content, 0, src_with_file,
                                       src_with_data);
  }
  return content;
}
}  // namespace

OfflinePageTabHelper::OfflinePageTabHelper(web::WebState* web_state,
                                           ReadingListModel* model)
    : web_state_(web_state), reading_list_model_(model) {
  web_state_->AddObserver(this);
  reading_list_model_->AddObserver(this);
}

OfflinePageTabHelper::~OfflinePageTabHelper() {
  Detach();
}

void OfflinePageTabHelper::Detach() {
  StopCheckingLoadingProgress();
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void OfflinePageTabHelper::LoadData(int offline_navigation,
                                    const GURL& url,
                                    const std::string& extension,
                                    const std::string& data) {
  if (!web_state_ || !web_state_->GetNavigationManager() ||
      !web_state_->GetNavigationManager()->GetLastCommittedItem()) {
    // It is possible that the web_state_ has been detached during the page
    // loading.
    return;
  }
  if (last_navigation_started_ != offline_navigation) {
    return;
  }

  NSString* mime = nil;
  if (extension == ".html") {
    mime = @"text/html";
  } else if (extension == ".pdf") {
    mime = @"application/pdf";
  }
  DCHECK(mime);
  presenting_offline_page_ = true;
  NSData* ns_data = [NSData dataWithBytes:data.c_str() length:data.size()];
  offline_navigation_triggered_ = reading_list::OfflineReloadURLForURL(url);
  dont_reload_online_on_next_navigation_ = true;
  web_state_->LoadData(ns_data, mime, offline_navigation_triggered_);
  // LoadData replace the last committed item and will set the URL to
  // `offline_navigation_triggered_`. Set the VirtualURL to `url` so it is
  // displayed in the omnibox.
  web_state_->GetNavigationManager()->GetLastCommittedItem()->SetVirtualURL(
      url);
}

void OfflinePageTabHelper::LoadOfflineData(web::WebState* web_state,
                                           const GURL& url,
                                           bool is_pdf,
                                           const std::string& data) {
  presenting_offline_page_ = true;
  offline_navigation_triggered_ = url;

  if (is_pdf) {
    NSData* ns_data = [NSData dataWithBytes:data.data() length:data.length()];
    web_state->LoadSimulatedRequest(url, ns_data, @"application/pdf");
  } else {
    NSString* path = [NSBundle.mainBundle pathForResource:@"error_page_reloaded"
                                                   ofType:@"html"];
    // Script which reloads the page if the page is being served from the
    // browser cache.
    NSString* reload_page_html_template =
        [NSString stringWithContentsOfFile:path
                                  encoding:NSUTF8StringEncoding
                                     error:nil];
    NSString* html = base::SysUTF8ToNSString(data);
    NSString* injected_html =
        [reload_page_html_template stringByAppendingString:html];
    web_state->LoadSimulatedRequest(url, injected_html);
  }
}

void OfflinePageTabHelper::DidStartNavigation(web::WebState* web_state,
                                              web::NavigationContext* context) {
  if (context->GetUrl() == offline_navigation_triggered_ ||
      context->IsSameDocument()) {
    // This is the navigation triggered by loadData or loadSimulatedRequest.
    // Ignore it, to not reset the presenting_offline_page_ flag.
    offline_navigation_triggered_ = GURL();
    return;
  }
  offline_navigation_triggered_ = GURL();
  initial_navigation_url_ = context->GetUrl();
  loading_slow_or_failed_ = false;
  navigation_committed_ = false;
  last_navigation_started_++;

  if (!reloading_from_offline_) {
    is_reload_navigation_ = ui::PageTransitionTypeIncludingQualifiersIs(
        context->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
    is_offline_navigation_ =
        reading_list::IsOfflineEntryURL(initial_navigation_url_);
    navigation_transition_type_ = context->GetPageTransition();
    is_new_navigation_ =
        ui::PageTransitionIsNewNavigation(navigation_transition_type_);
    navigation_is_renderer_initiated_ = context->IsRendererInitiated();
  }

  if (reading_list::IsOfflineReloadURL(context->GetUrl())) {
    return;
  }
  reloading_from_offline_ = false;
  if (!is_reload_navigation_ || !presenting_offline_page_) {
    StartCheckingLoadingProgress(initial_navigation_url_);
  }
  presenting_offline_page_ = false;
}

void OfflinePageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  navigation_committed_ = navigation_context->HasCommitted();

  if (reading_list::IsOfflineReloadURL(navigation_context->GetUrl())) {
    if (dont_reload_online_on_next_navigation_) {
      dont_reload_online_on_next_navigation_ = false;
    } else if (reloading_from_offline_) {
      ReplaceLocationUrlAndReload(
          reading_list::ReloadURLForOfflineURL(navigation_context->GetUrl()));
      return;
    }
  }
  if (!presenting_offline_page_) {
    PresentOfflinePageForOnlineUrl(initial_navigation_url_);
  }
}

void OfflinePageTabHelper::ReplaceLocationUrlAndReload(const GURL& url) {
  DCHECK(presenting_offline_page_);
  web_state_->GetNavigationManager()->GetLastCommittedItem()->SetVirtualURL(
      url);
  reloading_from_offline_ = true;
  std::string encoded_url;
  if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
    encoded_url = base::Base64Encode(url.spec());
  } else {
    encoded_url = base::Base64Encode("about:blank");
  }
  NSString* js =
      [NSString stringWithFormat:@"window.location.replace(atob('%@'));",
                                 base::SysUTF8ToNSString(encoded_url)];
  web_state_->ExecuteUserJavaScript(js);
}

GURL OfflinePageTabHelper::GetOnlineURLFromNavigationURL(
    const GURL& url) const {
  if (url.host() == kChromeUIOfflineHost) {
    return reading_list::EntryURLForOfflineURL(url);
  }
  return url;
}

void OfflinePageTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (reading_list::IsOfflineReloadURL(initial_navigation_url_)) {
    return;
  }
  StopCheckingLoadingProgress();
  // If the offline page was loaded directly, the initial_navigation_url_ is
  // chrome://offline?... and triggers a load error. Extract the meaningful
  // URL from the loading URL.
  GURL url = GetOnlineURLFromNavigationURL(initial_navigation_url_);

  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE) {
    loading_slow_or_failed_ = true;
    PresentOfflinePageForOnlineUrl(url);
    return;
  }

  if (!url.is_valid() || !reading_list_model_->loaded() ||
      !reading_list_model_->GetEntryByURL(url)) {
    return;
  }
  reading_list_model_->SetReadStatusIfExists(url, true);
  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed",
                        presenting_offline_page_);
}

void OfflinePageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK(web_state_ == nullptr || web_state_ == web_state);
  Detach();
}

void OfflinePageTabHelper::ReadingListModelLoaded(
    const ReadingListModel* model) {
  if (navigation_committed_ && loading_slow_or_failed_) {
    PresentOfflinePageForOnlineUrl(initial_navigation_url_);
  }
}

void OfflinePageTabHelper::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(reading_list_model_ == nullptr || reading_list_model_ == model);

  // Detach will nullify web_state_, this keeps it local a bit longer
  // to allow removing user data below.
  web::WebState* webState = web_state_;

  Detach();

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  webState->RemoveUserData(UserDataKey());
}

void OfflinePageTabHelper::PresentOfflinePageForOnlineUrl(const GURL& url) {
  // As presenting the offline version will replace the content of the committed
  // page, the offline version can only be presented if the navigation currently
  // tracked by the OfflinePageTabHelper is the last committed one.
  // This is the case in three scenarios:
  // - the navigation has been committed (here, navigation_committed_ is true),
  // - the navigation is a reload, which means it has been committed on the
  //   previous load (here, is_reload is true),
  // - if the navigation is a new navigation (here, is_new_navigation is true).
  //   In this case, it will be cancel and a new placeholder navigation will be
  //   triggerred that will always becommitted. This new navigation will be
  //   replaced by offline version.
  bool can_work_on_not_committed_navigation =
      is_reload_navigation_ || is_new_navigation_;
  bool can_load_offline =
      navigation_committed_ || can_work_on_not_committed_navigation;
  if (!can_load_offline) {
    return;
  }
  if (!loading_slow_or_failed_) {
    return;
  }
  if (!HasDistilledVersionForOnlineUrl(url)) {
    return;
  }
  GURL entry_url = GetOnlineURLFromNavigationURL(url);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(entry_url);
  if (!is_offline_navigation_ && is_new_navigation_ && !navigation_committed_) {
    // If the current navigation was not committed, but it was a new navigation,
    // a new placeholder navigation with a chrome://offline URL can be created
    // which will be replaced by the offline version on load failure.
    GURL offlineURL = reading_list::OfflineURLForURL(entry_url);

    web::NavigationManager::WebLoadParams params(offlineURL);
    params.transition_type = navigation_transition_type_;
    params.virtual_url = entry_url;
    params.is_renderer_initiated = navigation_is_renderer_initiated_;
    web_state_->GetNavigationManager()->LoadURLWithParams(params);
    return;
  }

  base::FilePath offline_path = entry->DistilledPath();
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  base::FilePath offline_root =
      ReadingListDownloadServiceFactory::GetForProfile(profile)
          ->OfflineRoot()
          .DirName();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetOfflineData, offline_root, offline_path),
      base::BindOnce(&OfflinePageTabHelper::LoadData,
                     weak_factory_.GetWeakPtr(), last_navigation_started_,
                     entry_url, offline_path.Extension()));
}

void OfflinePageTabHelper::LoadOfflinePage(const GURL& url) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  base::FilePath offline_root =
      ReadingListDownloadServiceFactory::GetForProfile(profile)
          ->OfflineRoot()
          .DirName();

  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  base::FilePath offline_path = entry->DistilledPath();
  bool is_pdf = offline_path.Extension() == ".pdf";

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetOfflineData, offline_root, offline_path),
      base::BindOnce(&OfflinePageTabHelper::LoadOfflineData,
                     weak_factory_.GetWeakPtr(), web_state_, url, is_pdf));
}

bool OfflinePageTabHelper::HasDistilledVersionForOnlineUrl(
    const GURL& online_url) const {
  if (!reading_list_model_ || !web_state_ || !reading_list_model_->loaded() ||
      !online_url.is_valid()) {
    return false;
  }

  GURL url = GetOnlineURLFromNavigationURL(online_url);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (!entry) {
    return false;
  }

  return entry->DistilledState() == ReadingListEntry::PROCESSED;
}

bool OfflinePageTabHelper::CanHandleErrorLoadingURL(const GURL& url) const {
  return HasDistilledVersionForOnlineUrl(url) ||
         reading_list::IsOfflineReloadURL(url);
}

void OfflinePageTabHelper::StartCheckingLoadingProgress(const GURL& url) {
  if (reading_list_model_->loaded() && !HasDistilledVersionForOnlineUrl(url)) {
    // No need to launch the timer if there is no distilled version.
    return;
  }

  try_number_ = 0;
  timer_.reset(new base::RepeatingTimer());
  timer_->Start(FROM_HERE, base::Milliseconds(1500),
                base::BindRepeating(&OfflinePageTabHelper::CheckLoadingProgress,
                                    weak_factory_.GetWeakPtr(), url));
}

void OfflinePageTabHelper::StopCheckingLoadingProgress() {
  timer_.reset();
}

void OfflinePageTabHelper::CheckLoadingProgress(const GURL& url) {
  if (reading_list_model_->loaded() && !HasDistilledVersionForOnlineUrl(url)) {
    StopCheckingLoadingProgress();
    return;
  }
  try_number_++;
  double progress = web_state_->GetLoadingProgress();
  const double kMinimumExpectedProgressPerStep = 0.25;
  if (progress < try_number_ * kMinimumExpectedProgressPerStep) {
    loading_slow_or_failed_ = true;
    PresentOfflinePageForOnlineUrl(url);
    StopCheckingLoadingProgress();
  } else if (try_number_ >= 3) {
    // Loading reached 75%, let the page finish normal loading.
    // Do not call StopCheckingLoadingProgress() as pending url is still needed
    // to mark the entry read on success loading or to display offline version
    // on error.
    timer_->Stop();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(OfflinePageTabHelper)
