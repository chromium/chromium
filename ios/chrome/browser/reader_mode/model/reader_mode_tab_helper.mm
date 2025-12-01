// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/time/time.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/dom_distiller/model/offline_page_distiller_viewer.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_viewer.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_scroll_anchor_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"

namespace {

bool IsTranslateEnabled(ChromeIOSTranslateClient* translate_client) {
  return translate_client && translate_client->GetTranslateManager()
                                 ->GetLanguageState()
                                 ->IsPageTranslated();
}

// Returns the source language setting for the page in scope for
// `translate_client`.
std::string GetSourceLanguageCode(ChromeIOSTranslateClient* translate_client) {
  return translate::TranslateDownloadManager::GetLanguageCode(
      translate_client->GetTranslateManager()
          ->GetLanguageState()
          ->source_language());
}

// Returns the target language setting for `translate_client`.
std::string GetTargetLanguageCode(ChromeIOSTranslateClient* translate_client,
                                  web::WebState* web_state) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      translate_client->GetTranslatePrefs();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))
          ->GetPrimaryModel();
  return translate_client->GetTranslateManager()->GetTargetLanguageForDisplay(
      translate_prefs.get(), language_model);
}

// Removes the translate infobar from the list of tracked infobars to ensure
// that this is not reused when closing Reading Mode web state.
void RemoveTranslateInfobarIfExists(web::WebState* web_state) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!infobar_manager) {
    return;
  }

  infobars::InfoBar* old_infobar = NULL;
  translate::TranslateInfoBarDelegate* old_delegate = NULL;
  for (infobars::InfoBar* infobar : infobar_manager->infobars()) {
    old_infobar = infobar;
    old_delegate = old_infobar->delegate()->AsTranslateInfoBarDelegate();
    if (old_delegate) {
      break;
    }
  }
  if (old_delegate) {
    infobar_manager->RemoveInfoBar(old_infobar);
  }
  InfobarOverlayRequestInserter::FromWebState(web_state)
      ->SuppressNextInfobarOfType(InfobarType::kInfobarTypeTranslate);
  // Ensure that the translate badge is present if the original page is
  // translatable.
  language::IOSLanguageDetectionTabHelper::FromWebState(web_state)
      ->StartLanguageDetection();
}

}  // namespace

ReaderModeTabHelper::ReaderModeTabHelper(web::WebState* web_state,
                                         DistillerService* distiller_service)
    : web_state_(web_state),
      distiller_service_(distiller_service),
      metrics_helper_(web_state, distiller_service->GetDistilledPagePrefs()),
      eligibility_decider_(web_state, &metrics_helper_) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

ReaderModeTabHelper::~ReaderModeTabHelper() {
  DeactivateReader(
      ReaderModeDeactivationReason::kHostTabDestructionDeactivated);
  for (auto& observer : observers_) {
    observer.ReaderModeTabHelperDestroyed(this, reader_mode_web_state_.get());
  }
}

void ReaderModeTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ReaderModeTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ReaderModeTabHelper::IsActive() const {
  return active_;
}

void ReaderModeTabHelper::ActivateReader(ReaderModeAccessPoint access_point) {
  if (active_) {
    return;
  }
  // It is not expected to activate reader mode for a page that is not
  // eligible.
  if (!CurrentPageIsEligibleForReaderMode()) {
    RecordDistillationFailure();
    return;
  }
  active_ = true;

  // If Reader mode is being activated, lazily create the secondary WebState
  // where the content will be rendered and start distillation.
  CreateReaderModeContent(access_point);
}

void ReaderModeTabHelper::DeactivateReader(
    ReaderModeDeactivationReason reason) {
  if (!active_) {
    return;
  }
  active_ = false;
  // If Reader mode is being deactivated, keep the secondary WebState but make
  // the content unavailable and ensure the Reader mode UI is dismissed.
  DestroyReaderModeContent(reason);
}

web::WebState* ReaderModeTabHelper::GetReaderModeWebState() {
  if (!reader_mode_web_state_content_loaded_) {
    return nullptr;
  }
  return reader_mode_web_state_.get();
}

bool ReaderModeTabHelper::CurrentPageIsEligibleForReaderMode() const {
  return eligibility_decider_.CurrentPageIsEligibleForReaderMode();
}

bool ReaderModeTabHelper::CurrentPageIsDistillable() const {
  return eligibility_decider_.CurrentPageIsDistillable();
}

bool ReaderModeTabHelper::CurrentPageDistillationAlreadyFailed() const {
  return distillation_already_failed_;
}

void ReaderModeTabHelper::RecordDistillationFailure() {
  distillation_already_failed_ = true;
  for (auto& observer : observers_) {
    observer.ReaderModeDistillationFailed(this);
  }
}

void ReaderModeTabHelper::FetchLastCommittedUrlDistillabilityResult(
    base::OnceCallback<void(std::optional<bool>)> callback) {
  eligibility_decider_.FetchLastCommittedUrlDistillabilityResult(
      std::move(callback));
}

void ReaderModeTabHelper::SetReaderModeHandler(
    id<ReaderModeCommands> reader_mode_handler) {
  reader_mode_handler_ = reader_mode_handler;
}

id<ReaderModeCommands> ReaderModeTabHelper::GetReaderModeHandler() const {
  return reader_mode_handler_;
}

void ReaderModeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  CHECK_EQ(web_state, web_state_);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    eligibility_decider_.StartDecision(web_state_->GetLastCommittedURL());
    distillation_already_failed_ = false;
  }
}

void ReaderModeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  // A new navigation is started while the Reader Mode heuristic trigger is
  // running on the previous navigation. Stop the trigger to attach the new
  // navigation.
  eligibility_decider_.ResetDecision(navigation_context->GetUrl());
  distillation_already_failed_ = false;
}

void ReaderModeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // PageLoaded may not be called for fragment navigations or
  // pushState/replaceState. Do not reset eligibility state pre-emptively.
  if (navigation_context->IsSameDocument()) {
    return;
  }

  if (!navigation_context->IsSameDocument() ||
      navigation_context->HasUserGesture()) {
    DeactivateReader(ReaderModeDeactivationReason::kNavigationDeactivated);
  }

  eligibility_decider_.SetLastCommittedUrl(web_state->GetLastCommittedURL());
}

void ReaderModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  DeactivateReader(
      ReaderModeDeactivationReason::kHostTabDestructionDeactivated);
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ReaderModeTabHelper::ReaderModeContentDidLoadData(
    ReaderModeContentTabHelper* reader_mode_content_tab_helper) {
  reader_mode_web_state_content_loaded_ = true;
  for (auto& observer : observers_) {
    observer.ReaderModeWebStateDidLoadContent(this,
                                              reader_mode_web_state_.get());
  }

  // Apply translation to the page if it was applied on the original page.
  if (IsReaderModeTranslationAvailable()) {
    if (source_translation_state_.is_original_source_translated) {
      reader_mode_content_tab_helper->ActivateTranslateOnPage(
          source_translation_state_.source_code,
          source_translation_state_.target_code);
    }
  }

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state_.get());
  if (manager) {
    manager->RemoveAllInfoBars(/*animate=*/false);
  }

  WebViewProxyTabHelper* tab_helper =
      WebViewProxyTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->SetOverridingWebViewProxy(
        reader_mode_web_state_->GetWebViewProxy());
  }
  metrics_helper_.RecordReaderShown();

  SnapshotSourceTabHelper::FromWebState(web_state_)
      ->SetOverridingSourceWebState(reader_mode_web_state_.get());
  // Generic snapshot image generation on side-swipe has a long tail latency.
  // Force update the snapshot storage to ensure that the latest snapshot is
  // presented before a transition.
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (snapshot_tab_helper) {
    snapshot_tab_helper->UpdateSnapshotWithCallback(nil);
  }

  // If a scroll anchor was found in the original page, scroll to it.
  if (!scroll_anchor_script_.empty() && distiller_viewer_) {
    distiller_viewer_->SendJavaScript(scroll_anchor_script_);
  }
}

void ReaderModeTabHelper::ReaderModeContentDidCancelRequest(
    ReaderModeContentTabHelper* reader_mode_content_tab_helper,
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info) {
  if (!active_ || !web_state_->IsVisible() || !request_info.is_user_initiated) {
    return;
  }
  // When the Reader mode content cancels a request to navigate, load the
  // requested URL in the host WebState instead.
  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(request.URL));
  NSString* referrer_value = [request
      valueForHTTPHeaderField:web::wk_navigation_util::kReferrerHeaderName];
  if (referrer_value) {
    NSURL* referrer_url = [NSURL URLWithString:referrer_value];
    params.referrer.url = net::GURLWithNSURL(referrer_url);
    params.referrer.policy = web::ReferrerPolicyDefault;
  }
  params.transition_type = request_info.transition_type;
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
}

base::WeakPtr<ReaderModeTabHelper> ReaderModeTabHelper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ReaderModeTabHelper::HandleReaderModeHeuristicResult(
    ReaderModeHeuristicResult result) {
  eligibility_decider_.HandleReaderModeHeuristicResult(result);
}

void ReaderModeTabHelper::SetScrollAnchorScript(std::string script) {
  scroll_anchor_script_ = std::move(script);
}

void ReaderModeTabHelper::PageDistillationCompleted(
    ReaderModeAccessPoint access_point,
    const GURL& page_url,
    const std::string& html,
    const std::vector<DistillerViewerInterface::ImageInfo>& images,
    const std::string& title,
    const std::string& csp_nonce) {
  // Cancel the distillation timeout request if page distillation completes.
  reader_mode_distillation_timer_.Stop();

  // If ExecuteJavaScript completion is run after WebState is destroyed, do
  // not continue metrics collection.
  if (!web_state_ || web_state_->IsBeingDestroyed()) {
    return;
  }

  bool is_distillable_page = !html.empty();
  metrics_helper_.RecordReaderDistillerCompleted(
      access_point, is_distillable_page
                        ? ReaderModeDistillerResult::kPageIsDistillable
                        : ReaderModeDistillerResult::kPageIsNotDistillable);

  if (IsReaderModeAvailable()) {
    if (is_distillable_page) {
      // Load the Reader mode content in the Reader mode content WebState.
      NSData* content_data = [NSData dataWithBytes:html.data()
                                            length:html.length()];
      ReaderModeContentTabHelper::FromWebState(reader_mode_web_state_.get())
          ->LoadContent(page_url, content_data);
    } else {
      RecordDistillationFailure();
      // If the page could not be distilled, deactivate Reader mode in this tab.
      DeactivateReader(
          ReaderModeDeactivationReason::kDistillationFailureDeactivated);
    }
  }
}

void ReaderModeTabHelper::CreateReaderModeContent(
    ReaderModeAccessPoint access_point) {
  bool is_incognito = web_state_->GetBrowserState()->IsOffTheRecord();
  metrics_helper_.RecordReaderDistillerTriggered(access_point, is_incognito);

  if (!reader_mode_web_state_) {
    web::WebState::CreateParams create_params = web::WebState::CreateParams(
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
    reader_mode_web_state_ = web::WebState::Create(create_params);
    reader_mode_web_state_->SetWebUsageEnabled(true);
    ReaderModeContentTabHelper::CreateForWebState(reader_mode_web_state_.get());
    ReaderModeContentTabHelper* content_tab_helper =
        ReaderModeContentTabHelper::FromWebState(reader_mode_web_state_.get());
    content_tab_helper->SetDelegate(this);
    content_tab_helper->AttachSupportedTabHelpers(web_state_.get());
  }

  web::WebFramesManager* web_frames_manager =
      ReaderModeScrollAnchorJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state_);
  if (web_frames_manager) {
    web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
    if (main_frame) {
      ReaderModeScrollAnchorJavaScriptFeature::GetInstance()->FindScrollAnchor(
          main_frame);
    }
  }

  // Apply a blurring effect to the original web page as part of the translation
  // settings experiment.
  if (reader_mode_handler_ &&
      base::FeatureList::IsEnabled(kEnableReaderModeTranslationWithInfobar)) {
    [reader_mode_handler_
        showReaderModeBlurOverlay:
            base::CallbackToBlock(
                base::BindOnce(&ReaderModeTabHelper::CompleteDistillation,
                               weak_ptr_factory_.GetWeakPtr(), access_point))];
  } else {
    CompleteDistillation(access_point);
  }
}

void ReaderModeTabHelper::DestroyReaderModeContent(
    ReaderModeDeactivationReason reason) {
  metrics_helper_.Flush(reason);

  WebViewProxyTabHelper* tab_helper =
      WebViewProxyTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->SetOverridingWebViewProxy(nil);
  }
  for (auto& observer : observers_) {
    observer.ReaderModeWebStateWillBecomeUnavailable(
        this, reader_mode_web_state_.get(), reason);
  }
  reader_mode_web_state_content_loaded_ = false;

  // Cancel any ongoing distillation task.
  distiller_viewer_.reset();

  // Remove blur effect on the web page if available.
  [reader_mode_handler_ hideReaderModeBlurOverlay];

  // Ensure that any infobars created in Reading Mode state are removed prior
  // to creating new ones attached to the original web page.
  RemoveTranslateInfobarIfExists(web_state_.get());

  // Display translation badge if a translation was applied before or
  // during Reading Mode activation for active tabs.
  if (IsReaderModeTranslationAvailable()) {
    switch (reason) {
      case ReaderModeDeactivationReason::kNavigationDeactivated:
      case ReaderModeDeactivationReason::kUserDeactivated: {
        ChromeIOSTranslateClient* translate_client =
            ChromeIOSTranslateClient::FromWebState(web_state_.get());
        ApplyLanguageSettingsFromClient(translate_client);
        break;
      }
      case ReaderModeDeactivationReason::kDistillationFailureDeactivated: {
        ApplyLanguageSettingsFromSource();
        break;
      }
      case ReaderModeDeactivationReason::kHostTabDestructionDeactivated: {
        break;
      }
    }
  }
  source_translation_state_ = {};

  SnapshotSourceTabHelper::FromWebState(web_state_)
      ->SetOverridingSourceWebState(nullptr);
  // Update the snapshot with the original web page.
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (snapshot_tab_helper) {
    snapshot_tab_helper->UpdateSnapshotWithCallback(nil);
  }
}

void ReaderModeTabHelper::CancelDistillation() {
  metrics_helper_.RecordReaderDistillerTimedOut();
  RecordDistillationFailure();
  DeactivateReader(
      ReaderModeDeactivationReason::kDistillationFailureDeactivated);
}

void ReaderModeTabHelper::ApplyLanguageSettingsFromClient(
    ChromeIOSTranslateClient* translate_client) {
  ChromeIOSTranslateClient* content_translate_client =
      ChromeIOSTranslateClient::FromWebState(reader_mode_web_state_.get());
  if (IsTranslateEnabled(content_translate_client)) {
    // Suppresses the translate infobar that would be displayed following the
    // translation of the page.
    InfobarOverlayRequestInserter::FromWebState(web_state_.get())
        ->SuppressNextInfobarOfType(InfobarType::kInfobarTypeTranslate);

    // Ensure the language settings are updated with the latest version from
    // Reading Mode.
    const std::string source_code =
        GetSourceLanguageCode(content_translate_client);
    const std::string target_code = GetTargetLanguageCode(
        content_translate_client, reader_mode_web_state_.get());

    translate::TranslateManager* translate_manager =
        translate_client->GetTranslateManager();
    translate_manager->ShowTranslateUI(source_code, target_code,
                                       /*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }
}

void ReaderModeTabHelper::ApplyLanguageSettingsFromSource() {
  ChromeIOSTranslateClient* translate_client =
      ChromeIOSTranslateClient::FromWebState(web_state_.get());
  if (source_translation_state_.is_original_source_translated) {
    // Suppresses the translate infobar that would be displayed following the
    // translation of the page.
    InfobarOverlayRequestInserter::FromWebState(web_state_.get())
        ->SuppressNextInfobarOfType(InfobarType::kInfobarTypeTranslate);

    translate::TranslateManager* translate_manager =
        translate_client->GetTranslateManager();
    translate_manager->ShowTranslateUI(source_translation_state_.source_code,
                                       source_translation_state_.target_code,
                                       /*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }
}

void ReaderModeTabHelper::CompleteDistillation(
    ReaderModeAccessPoint access_point) {
  if (IsReaderModeTranslationAvailable()) {
    ChromeIOSTranslateClient* translate_client =
        ChromeIOSTranslateClient::FromWebState(web_state_.get());
    TranslationState source_translation_state;
    source_translation_state.is_original_source_translated =
        IsTranslateEnabled(translate_client);
    if (source_translation_state.is_original_source_translated) {
      source_translation_state.source_code =
          GetSourceLanguageCode(translate_client);
      source_translation_state.target_code =
          GetTargetLanguageCode(translate_client, web_state_.get());
      if (base::FeatureList::IsEnabled(
              kEnableReaderModeTranslationWithInfobar)) {
        translate_client->GetTranslateManager()->RevertTranslation();
      }
    }
    source_translation_state_ = source_translation_state;
  }

  std::unique_ptr<ReaderModeDistillerPage> distiller_page =
      std::make_unique<ReaderModeDistillerPage>(web_state_);
  distiller_viewer_ = std::make_unique<ReaderModeDistillerViewer>(
      reader_mode_web_state_.get(), distiller_service_,
      std::move(distiller_page), web_state_->GetLastCommittedURL(),
      base::BindOnce(&ReaderModeTabHelper::PageDistillationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), access_point));

  reader_mode_distillation_timer_.Start(
      FROM_HERE, ReaderModeDistillationTimeout(),
      base::BindOnce(&ReaderModeTabHelper::CancelDistillation,
                     weak_ptr_factory_.GetWeakPtr()));
}
