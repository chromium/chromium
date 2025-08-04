// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/containers/fixed_flat_set.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "components/google/core/common/google_util.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/dom_distiller/model/offline_page_distiller_viewer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_viewer.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"

namespace {

// These are known google websites that don't support reader mode.
static constexpr auto kGoogleWorkspaceBlocklist =
    base::MakeFixedFlatSet<std::string_view>(
        {"assistant.google.com", "calendar.google.com", "docs.google.com",
         "drive.google.com", "mail.google.com", "photos.google.com"});

// Helper function to generate the snackbar message.
NSString* GenerateSnackbarMessage(bool is_distillable_page) {
  std::string message = "\nDistillation Result: ";
  message += (is_distillable_page ? "Distillable" : "Not Distillable");
  return base::SysUTF8ToNSString(message);
}

// Returns the Readability heuristic result if it is available otherwise returns
// `kMalformedResponse`.
ReaderModeHeuristicResult GetReaderModeHeuristicResult(
    const base::Value* result) {
  if (!result) {
    return ReaderModeHeuristicResult::kMalformedResponse;
  }
  std::optional<bool> result_conversion = result->GetIfBool();
  if (result_conversion.has_value()) {
    return result_conversion.value()
               ? ReaderModeHeuristicResult::kReaderModeEligible
               : ReaderModeHeuristicResult::
                     kReaderModeNotEligibleContentAndLength;
  }
  return ReaderModeHeuristicResult::kMalformedResponse;
}

}  // namespace

ReaderModeTabHelper::ReaderModeTabHelper(web::WebState* web_state,
                                         DistillerService* distiller_service)
    : web_state_(web_state),
      distiller_service_(distiller_service),
      metrics_helper_(web_state, distiller_service->GetDistilledPagePrefs()) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

ReaderModeTabHelper::~ReaderModeTabHelper() {
  DeactivateReader(
      ReaderModeDeactivationReason::kHostTabDestructionDeactivated);
  for (auto& observer : observers_) {
    observer.ReaderModeTabHelperDestroyed(this);
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
  CHECK(CurrentPageIsEligibleForReaderMode());
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
  bool eligible = web_state_ && !web_state_->IsBeingDestroyed() &&
                  web_state_->ContentIsHTML();
  if (!eligible) {
    return false;
  }
  GURL current_url = web_state_->GetLastCommittedURL();
  return current_url.SchemeIsHTTPOrHTTPS() &&
         !google_util::IsGoogleHomePageUrl(current_url) &&
         !google_util::IsGoogleSearchUrl(current_url) &&
         !google_util::IsYoutubeDomainUrl(
             current_url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) &&
         !kGoogleWorkspaceBlocklist.contains(current_url.host_piece());
}

bool ReaderModeTabHelper::CurrentPageIsDistillable() const {
  return CurrentPageIsEligibleForReaderMode() &&
         last_committed_url_eligibility_ready_ &&
         last_committed_url_without_ref_.is_valid() &&
         last_committed_url_without_ref_.EqualsIgnoringRef(
             reader_mode_eligible_url_);
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
  if (last_committed_url_eligibility_ready_) {
    std::move(callback).Run(CurrentPageIsDistillable());
    return;
  }
  last_committed_url_eligibility_callbacks_.push_back(std::move(callback));
}

void ReaderModeTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
}

void ReaderModeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  CHECK_EQ(web_state, web_state_);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    TriggerReaderModeHeuristicAsync(web_state_->GetLastCommittedURL());
  }
}

void ReaderModeTabHelper::TriggerReaderModeHeuristicAsync(const GURL& url) {
  if (!IsReaderModeAvailable()) {
    return;
  }
  // Guarantee that there is only one trigger heuristic running at a time.
  ResetUrlEligibility(url);

  trigger_reader_mode_timer_.Start(
      FROM_HERE, ReaderModeHeuristicPageLoadDelay(),
      base::BindOnce(&ReaderModeTabHelper::TriggerReaderModeHeuristic,
                     weak_ptr_factory_.GetWeakPtr(), url));
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
  ResetUrlEligibility(navigation_context->GetUrl());
}

void ReaderModeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument() ||
      navigation_context->HasUserGesture()) {
    DeactivateReader(ReaderModeDeactivationReason::kNavigationDeactivated);
  }

  SetLastCommittedUrl(web_state->GetLastCommittedURL());
}

void ReaderModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  DeactivateReader(
      ReaderModeDeactivationReason::kHostTabDestructionDeactivated);
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ReaderModeTabHelper::ResetUrlEligibility(const GURL& url) {
  // Ensure that only one asynchronous eligibility check is running at a time.
  if (trigger_reader_mode_timer_.IsRunning()) {
    trigger_reader_mode_timer_.Stop();
    metrics_helper_.RecordReaderHeuristicCanceled();
  } else {
    // If there is no trigger in progress ensure any metrics related to a
    // past navigation have been recorded.
    metrics_helper_.Flush();
  }

  // Do not reset URL eligibility for same-page navigations.
  if (!reader_mode_eligible_url_.EqualsIgnoringRef(url)) {
    reader_mode_eligible_url_ = GURL();
    distillation_already_failed_ = false;
  }
}

void ReaderModeTabHelper::ReaderModeContentDidLoadData(
    ReaderModeContentTabHelper* reader_mode_content_tab_helper) {
  reader_mode_web_state_content_loaded_ = true;
  for (auto& observer : observers_) {
    observer.ReaderModeWebStateDidLoadContent(this);
  }
  WebViewProxyTabHelper* tab_helper =
      WebViewProxyTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->SetOverridingWebViewProxy(
        reader_mode_web_state_->GetWebViewProxy());
  }
  metrics_helper_.RecordReaderShown();
  // Generic snapshot image generation on side-swipe has a long tail latency.
  // Force update the snapshot storage to ensure that the latest snapshot is
  // presented before a transition.
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (snapshot_tab_helper) {
    snapshot_tab_helper->UpdateSnapshotWithCallback(nil);
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

void ReaderModeTabHelper::HandleReadabilityHeuristicResult(
    const GURL& url,
    const base::Value* result) {
  HandleReaderModeHeuristicResult(url, GetReaderModeHeuristicResult(result));
}

void ReaderModeTabHelper::SetFullscreenController(
    FullscreenController* fullscreen_controller) {
  if (!reader_mode_web_state_) {
    return;
  }
  ReaderModeContentTabHelper* content_tab_helper =
      ReaderModeContentTabHelper::FromWebState(reader_mode_web_state_.get());
  if (content_tab_helper) {
    content_tab_helper->SetFullscreenController(fullscreen_controller);
  }
}

void ReaderModeTabHelper::HandleReaderModeHeuristicResult(
    const GURL& url,
    ReaderModeHeuristicResult result) {
  metrics_helper_.RecordReaderHeuristicCompleted(result);

  if (url != web_state_->GetLastCommittedURL()) {
    // There has been a change in the committed URL since the last heuristic
    // run. Re-run the heuristic and reset the eligible URL.
    TriggerReaderModeHeuristicAsync(web_state_->GetLastCommittedURL());
    return;
  }
  reader_mode_eligible_url_ =
      result == ReaderModeHeuristicResult::kReaderModeEligible ? url : GURL();
  if (last_committed_url_without_ref_.EqualsIgnoringRef(url)) {
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(CurrentPageIsDistillable());
  }
}

void ReaderModeTabHelper::TriggerReaderModeHeuristic(const GURL& url) {
  if (!IsReaderModeAvailable()) {
    return;
  }
  if (!CurrentPageIsEligibleForReaderMode()) {
    // If the current page does not support running the heuristic, then the
    // eligibility of the current page is already know.
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(false);
    return;
  }

  web::WebFramesManager* web_frames_manager =
      ReaderModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  if (!web_frames_manager) {
    return;
  }
  web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  metrics_helper_.RecordReaderHeuristicTriggered();
  if (base::FeatureList::IsEnabled(kEnableReadabilityHeuristic)) {
    main_frame->ExecuteJavaScript(
        base::UTF8ToUTF16(dom_distiller::GetReadabilityTriggeringScript()),
        base::BindOnce(&ReaderModeTabHelper::HandleReadabilityHeuristicResult,
                       weak_ptr_factory_.GetWeakPtr(), url));
  } else {
    ReaderModeJavaScriptFeature::GetInstance()->TriggerReaderModeHeuristic(
        main_frame);
  }
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

  if (IsReaderModeSnackbarEnabled()) {
    // Show a snackbar with the heuristic result, latency and page distillation
    // result and latency.
    MDCSnackbarMessage* message = [MDCSnackbarMessage
        messageWithText:GenerateSnackbarMessage(is_distillable_page)];
    message.duration = MDCSnackbarMessageDurationMax;
    [snackbar_handler_ showSnackbarMessage:message];
  }

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
  metrics_helper_.RecordReaderDistillerTriggered(access_point);

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

void ReaderModeTabHelper::DestroyReaderModeContent(
    ReaderModeDeactivationReason reason) {
  metrics_helper_.Flush();

  WebViewProxyTabHelper* tab_helper =
      WebViewProxyTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->SetOverridingWebViewProxy(nil);
  }
  for (auto& observer : observers_) {
    observer.ReaderModeWebStateWillBecomeUnavailable(this, reason);
  }
  reader_mode_web_state_content_loaded_ = false;

  // Cancel any ongoing distillation task.
  distiller_viewer_.reset();
  // Update the snapshot with the original web page.
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (snapshot_tab_helper) {
    snapshot_tab_helper->UpdateSnapshotWithCallback(nil);
  }
}

void ReaderModeTabHelper::SetLastCommittedUrl(const GURL& url) {
  if (url.EqualsIgnoringRef(last_committed_url_without_ref_)) {
    return;
  }
  last_committed_url_without_ref_ = url;
  last_committed_url_eligibility_ready_ = false;
  // At this point, the only callbacks waiting for results have been added since
  // the last committed URL, before the Reader mode heuristic could determine
  // eligibility. Hence, they can all be called with nullopt (no result).
  CallLastCommittedUrlEligibilityCallbacks(std::nullopt);
}

void ReaderModeTabHelper::CallLastCommittedUrlEligibilityCallbacks(
    std::optional<bool> result) {
  for (auto& callback : last_committed_url_eligibility_callbacks_) {
    std::move(callback).Run(result);
  }
  last_committed_url_eligibility_callbacks_.clear();
}

void ReaderModeTabHelper::CancelDistillation() {
  metrics_helper_.RecordReaderDistillerTimedOut();
  RecordDistillationFailure();
  DeactivateReader(
      ReaderModeDeactivationReason::kDistillationFailureDeactivated);
}
