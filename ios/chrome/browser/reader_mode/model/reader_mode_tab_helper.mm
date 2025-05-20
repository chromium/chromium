// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/dom_distiller/model/offline_page_distiller_viewer.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"
#import "services/metrics/public/cpp/ukm_builders.h"

namespace {

// Records the time elapsed from the execution of the distillation JavaScript to
// the result callback.
void RecordReaderModeDistillationLatency(base::TimeDelta elapsed,
                                         ukm::SourceId source_id) {
  UMA_HISTOGRAM_TIMES(kReaderModeDistillerLatencyHistogram, elapsed);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Latency(source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}

// Records whether the given source ID for a navigation is distillable or not.
void RecordReaderModeDistillationResult(bool is_distillable,
                                        ukm::SourceId source_id) {
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Result(source_id)
        .SetResult(static_cast<int64_t>(
            is_distillable ? ReaderModeDistillerResult::kPageIsDistillable
                           : ReaderModeDistillerResult::kPageIsNotDistillable))
        .Record(ukm::UkmRecorder::Get());
  }
}

bool IsKnownAmpCache(web::WebFrame* web_frame) {
  url::Origin origin = web_frame->GetSecurityOrigin();
  // Source:
  // https://github.com/ampproject/amphtml/blob/main/build-system/global-configs/caches.json
  return origin.DomainIs("cdn.ampproject.org") ||
         origin.DomainIs("bing-amp.com");
}

// Records whether any available web frames in the current web state use AMP.
// This metric will help determine whether AMP special casing will affect
// distillation success.
void RecordReaderModeForAmpDistill(bool is_distillable_page,
                                   web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  web::WebFramesManager* page_world_manager =
      web_state->GetPageWorldWebFramesManager();
  auto web_frames = page_world_manager->GetAllWebFrames();
  bool is_amp =
      std::any_of(web_frames.begin(), web_frames.end(), IsKnownAmpCache);

  ReaderModeAmpClassification classification;
  if (is_amp) {
    classification = is_distillable_page
                         ? ReaderModeAmpClassification::kPopulatedDistillWithAmp
                         : ReaderModeAmpClassification::kEmptyDistillWithAmp;
  } else {
    classification = is_distillable_page
                         ? ReaderModeAmpClassification::kPopulatedDistillNoAmp
                         : ReaderModeAmpClassification::kEmptyDistillNoAmp;
  }

  UMA_HISTOGRAM_ENUMERATION(kReaderModeAmpClassificationHistogram,
                            classification);
}

// Helper function to generate the snackbar message.
NSString* GenerateSnackbarMessage(base::TimeDelta heuristic_latency,
                                  bool is_distillable_page,
                                  base::TimeDelta distillation_latency) {
  std::string message =
      "Heuristic Latency: " +
      base::NumberToString(heuristic_latency.InMilliseconds()) + "ms";
  message += "\nDistillation Result: ";
  message += (is_distillable_page ? "Distillable" : "Not Distillable");
  message += "\nDistillation Latency: " +
             base::NumberToString(distillation_latency.InMilliseconds()) + "ms";
  return base::SysUTF8ToNSString(message);
}

}  // namespace

ReaderModeTabHelper::ReaderModeTabHelper(web::WebState* web_state,
                                         DistillerService* distiller_service)
    : web_state_(web_state), distiller_service_(distiller_service) {
  CHECK(web_state_);
  web_state_->AddObserver(this);
}

ReaderModeTabHelper::~ReaderModeTabHelper() = default;

void ReaderModeTabHelper::SetDelegate(ReaderModeTabHelperDelegate* delegate) {
  delegate_ = delegate;
}

bool ReaderModeTabHelper::IsActive() const {
  return !!reader_mode_web_state_;
}

void ReaderModeTabHelper::SetActive(bool active) {
  if (active == IsActive()) {
    return;
  }
  if (active) {
    // If Reader mode is being activated, create the secondary WebState where
    // the content will be rendered and start distillation.
    CreateReaderModeWebState();
  } else {
    // If Reader mode is being deactivated, destroy the secondary WebState and
    // ensure the Reader mode UI is dismissed.
    DestroyReaderModeWebState();
  }
}

bool ReaderModeTabHelper::IsReaderModeContentAvailable() const {
  // TODO(crbug.com/417685203): Try to remove this parameter once decoupling is
  // completed e.g. instead check whether there is a ReaderModeContentTabHelper
  // attached and displaying content.
  return reader_mode_content_available_;
}

UIView* ReaderModeTabHelper::GetReaderModeContentView() {
  CHECK(IsReaderModeContentAvailable());
  return reader_mode_web_state_->GetView();
}

bool ReaderModeTabHelper::CurrentPageSupportsReaderMode() const {
  if (!web_state_ || web_state_->IsBeingDestroyed()) {
    return false;
  }
  // TODO(crbug.com/416226085): Maybe return false if the page is not
  // distillable.
  return !IsUrlNtp(web_state_->GetVisibleURL()) && web_state_->ContentIsHTML();
}

void ReaderModeTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
}

void ReaderModeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  CHECK_EQ(web_state, web_state_);
  // TODO(crbug.com/409940117): If `IsReaderModeAvailable()` then Reader mode is
  // being debugged, so the heuristic shouldn't be started automatically on page
  // load. Remove this check when debugging code is cleaned up.
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS &&
      !IsReaderModeAvailable()) {
    // Guarantee that there is only one trigger heuristic running at a time.
    if (trigger_reader_mode_timer_.IsRunning()) {
      trigger_reader_mode_timer_.Stop();
    }
    trigger_reader_mode_timer_.Start(
        FROM_HERE, ReaderModeDistillerPageLoadDelay(),
        base::BindOnce(&ReaderModeTabHelper::TriggerReaderModeHeuristic,
                       weak_ptr_factory_.GetWeakPtr()));
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
  if (trigger_reader_mode_timer_.IsRunning()) {
    trigger_reader_mode_timer_.Stop();
  }
}

void ReaderModeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument() ||
      navigation_context->HasUserGesture()) {
    SetActive(false);
  }
}

void ReaderModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  SetActive(false);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void ReaderModeTabHelper::ReaderModeContentDidCancelRequest(
    ReaderModeContentTabHelper* reader_mode_content_tab_helper,
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info) {
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

void ReaderModeTabHelper::HandleReaderModeHeuristicResult(
    const GURL& url,
    ReaderModeHeuristicResult result) {
  UMA_HISTOGRAM_ENUMERATION(kReaderModeHeuristicResultHistogram, result);

  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Result(source_id)
        .SetResult(static_cast<int64_t>(result))
        .Record(ukm::UkmRecorder::Get());
  }
}

void ReaderModeTabHelper::RecordReaderModeHeuristicLatency(
    const base::TimeDelta& delta) {
  heuristic_latency_ = delta;
  UMA_HISTOGRAM_TIMES(kReaderModeHeuristicLatencyHistogram, delta);
  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Latency(source_id)
        .SetLatency(delta.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}

bool ReaderModeTabHelper::CanTriggerReaderModeHeuristic() {
  if (IsReaderModeAvailable()) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(
          kEnableReaderModeDistillerHeuristicForMetrics)) {
    return false;
  }
  const double page_load_probability =
      kReaderModeDistillerPageLoadProbability.Get();
  if (page_load_probability <= 0.0 || page_load_probability > 1.0) {
    // Invalid probability range. Disable the Reader Mode feature.
    return false;
  }

  const double rand_double = base::RandDouble();
  return rand_double < page_load_probability;
}

void ReaderModeTabHelper::TriggerReaderModeHeuristic() {
  if (!CanTriggerReaderModeHeuristic()) {
    return;
  }
  web::WebFramesManager* web_frames_manager =
      ReaderModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  if (!web_frames_manager) {
    return;
  }
  web::WebFrame* web_frame = web_frames_manager->GetMainWebFrame();
  if (!web_frame) {
    return;
  }
  ReaderModeJavaScriptFeature::GetInstance()->TriggerReaderModeHeuristic(
      web_frame);
}

void ReaderModeTabHelper::PageDistillationCompleted(
    base::TimeTicks start_time,
    const GURL& page_url,
    const std::string& html,
    const std::vector<DistillerViewerInterface::ImageInfo>& images,
    const std::string& title,
    const std::string& csp_nonce) {
  // If ExecuteJavaScript completion is run after WebState is destroyed, do
  // not continue metrics collection.
  if (!web_state_ || web_state_->IsBeingDestroyed()) {
    return;
  }
  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  const base::TimeDelta distillation_latency =
      base::TimeTicks::Now() - start_time;
  RecordReaderModeDistillationLatency(distillation_latency, source_id);

  bool is_distillable_page = !html.empty();
  RecordReaderModeDistillationResult(is_distillable_page, source_id);
  RecordReaderModeForAmpDistill(is_distillable_page, web_state_);

  if (IsReaderModeSnackbarEnabled()) {
    // Show a snackbar with the heuristic result, latency and page distillation
    // result and latency.
    MDCSnackbarMessage* message = [MDCSnackbarMessage
        messageWithText:GenerateSnackbarMessage(heuristic_latency_,
                                                is_distillable_page,
                                                distillation_latency)];
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
      reader_mode_content_available_ = true;
      if (delegate_) {
        delegate_->ReaderModeContentDidBecomeAvailable(this);
      }
    } else {
      // If the page could not be distilled, deactivate Reader mode in this tab.
      SetActive(false);
    }
  }
}

void ReaderModeTabHelper::CreateReaderModeWebState() {
  web::WebState::CreateParams create_params = web::WebState::CreateParams(
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())
          ->GetOffTheRecordProfile());
  reader_mode_web_state_ = web::WebState::Create(create_params);
  ReaderModeContentTabHelper::CreateForWebState(reader_mode_web_state_.get());
  ReaderModeContentTabHelper::FromWebState(reader_mode_web_state_.get())
      ->SetDelegate(this);
  reader_mode_web_state_->SetWebUsageEnabled(true);

  std::unique_ptr<ReaderModeDistillerPage> distiller_page =
      std::make_unique<ReaderModeDistillerPage>(web_state_);
  distiller_viewer_.reset(new OfflinePageDistillerViewer(
      distiller_service_, std::move(distiller_page),
      web_state_->GetLastCommittedURL(),
      base::BindRepeating(&ReaderModeTabHelper::PageDistillationCompleted,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::TimeTicks::Now())));
}

void ReaderModeTabHelper::DestroyReaderModeWebState() {
  if (delegate_) {
    delegate_->ReaderModeContentWillBecomeUnavailable(this);
  }
  reader_mode_content_available_ = false;
  reader_mode_web_state_.reset();
  // Cancel any ongoing distillation task.
  distiller_viewer_.reset();
}
