// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <iterator>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/sms/sms_metrics.h"
#include "content/browser/sms/user_consent_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-shared.h"

using blink::mojom::SmsStatus;
using Outcome = blink::WebOTPServiceOutcome;

namespace content {

namespace {

// Only |kMaxUniqueOriginInAncestorChainForWebOTP| unique origins in the chain
// is considered valid. In addition, the unique origins must be consecutive.
// e.g. the following are valid:
// A.com (calls WebOTP API)
// A.com -> A.com (calls WebOTP API)
// A.com -> A.com -> B.com (calls WebOTP API)
// A.com -> B.com -> B.com (calls WebOTP API)
// while the following are invalid:
// A.com -> B.com -> A.com (calls WebOTP API)
// A.com -> B.com -> C.com (calls WebOTP API)
bool ValidateAndCollectUniqueOrigins(RenderFrameHost& rfh,
                                     OriginList& origin_list) {
  url::Origin current_origin = rfh.GetLastCommittedOrigin();
  origin_list.push_back(current_origin);

  RenderFrameHost* parent = rfh.GetParent();
  while (parent) {
    url::Origin parent_origin = parent->GetLastCommittedOrigin();
    if (!parent_origin.IsSameOriginWith(current_origin)) {
      origin_list.push_back(parent_origin);
      current_origin = parent_origin;
    }
    if (origin_list.size() > blink::kMaxUniqueOriginInAncestorChainForWebOTP)
      return false;
    parent = parent->GetParent();
  }
  return true;
}

bool IsCrossOriginFrame(RenderFrameHost& rfh) {
  if (!rfh.GetParent())
    return false;
  url::Origin current_origin = rfh.GetLastCommittedOrigin();
  RenderFrameHost* parent = rfh.GetParent();
  while (parent) {
    url::Origin parent_origin = parent->GetLastCommittedOrigin();
    if (!parent_origin.IsSameOriginWith(current_origin))
      return true;
    parent = parent->GetParent();
  }
  return false;
}

Outcome FailureTypeToOutcome(SmsFetchFailureType failure_type) {
  switch (failure_type) {
    case SmsFetchFailureType::kPromptTimeout:
      return Outcome::kTimeout;
    case SmsFetchFailureType::kPromptCancelled:
      return Outcome::kUserCancelled;
    case SmsFetchFailureType::kCrossDeviceFailure:
      return Outcome::kCrossDeviceFailure;
    default:
      NOTREACHED_IN_MIGRATION();
      return Outcome::kTimeout;
  }
}

Outcome SmsStatusToOutcome(SmsStatus status) {
  switch (status) {
    case SmsStatus::kSuccess:
      return Outcome::kSuccess;
    case SmsStatus::kUnhandledRequest:
      return Outcome::kUnhandledRequest;
    case SmsStatus::kAborted:
      return Outcome::kAborted;
    case SmsStatus::kCancelled:
      return Outcome::kCancelled;
    case SmsStatus::kBackendNotAvailable:
      // Records when the backend is not available AND the request gets
      // cancelled. i.e. client specifies GmsBackend.VERIFICATION but it's
      // unavailable. If client specifies GmsBackend.AUTO and the verification
      // backend is not available, we fall back to the user consent backend and
      // the request will be handled accordingly. e.g. if the user declined the
      // prompt, we record it as |kUserCancelled|.
      return Outcome::kBackendNotAvailable;
    case SmsStatus::kTimeout:
      return Outcome::kTimeout;
  }
}

ukm::SourceId GetPageUkmSourceId(RenderFrameHost& render_frame_host) {
  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As WebOTPService runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(!render_frame_host.IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  return render_frame_host.GetPageUkmSourceId();
}

}  // namespace

WebOTPService::WebOTPService(
    SmsFetcher* fetcher,
    const OriginList& origin_list,
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver)
    : DocumentService(host, std::move(receiver)),
      fetcher_(fetcher),
      origin_list_(origin_list),
      timeout_timer_(FROM_HERE,
                     blink::kWebOTPRequestTimeout,
                     this,
                     &WebOTPService::OnTimeout) {
  CHECK(fetcher_);
}

WebOTPService::~WebOTPService() {
  CHECK(!callback_);
}

// static
bool WebOTPService::Create(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver) {
  CHECK(host);

  OriginList origin_list;
  if (!ValidateAndCollectUniqueOrigins(*host, origin_list))
    return false;

  // WebOTPService owns itself. It will self-destruct when a mojo interface
  // error occurs, the RenderFrameHost is deleted, or the RenderFrameHost
  // navigates to a new document.
  new WebOTPService(fetcher, origin_list, *host, std::move(receiver));
  static_cast<RenderFrameHostImpl*>(host)
      ->OnBackForwardCacheDisablingStickyFeatureUsed(
          blink::scheduler::WebSchedulerTrackedFeature::kWebOTPService);
  return true;
}

// static
WebOTPService& WebOTPService::CreateForTesting(
    SmsFetcher* fetcher,
    const OriginList& origins,
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver) {
  return *new WebOTPService(fetcher, origins, frame_host, std::move(receiver));
}

void WebOTPService::WillBeDestroyed(DocumentServiceDestructionReason) {
  // Resolve any pending callback and invoke clean up to unsubscribe this
  // service from fetcher.
  //
  // TODO(crbug.com/40222530): Previously, running the callbacks in the
  // destructor was required to avoid triggering CHECKs since the
  // mojo::Receiver was (incorrectly) not yet reset in the destructor.
  //
  // The destruction order is fixed so running the reply callbacks should no
  // longer be necessary; however, there are now unit test-only dependencies on
  // this behavior. Remove those test dependencies and migrate any remaining
  // cleanup logic that is still needed to the destructor and delete this
  // `WillBeDestroyed()` override.
  CompleteRequest(SmsStatus::kUnhandledRequest);
}

void WebOTPService::Receive(ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!origin_list_.empty());
  // Cancels the last request if there is we have not yet handled it.
  if (callback_)
    CompleteRequest(SmsStatus::kCancelled);

  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);
  timeout_timer_.Reset();
  delayed_rejection_reason_.reset();

  // |one_time_code_| and prompt are still present from the previous request so
  // a new subscription is unnecessary. Note that it is only safe for us to use
  // the in flight otp with the new request since both requests belong to the
  // same origin.
  // TODO(majidvp): replace is_active() check with a check on existence of the
  // handler.
  auto* consent_handler = GetConsentHandler();
  if (consent_handler && consent_handler->is_active())
    return;

  fetcher_->Subscribe(origin_list_, *this, render_frame_host());
}

void WebOTPService::OnReceive(const OriginList& origin_list,
                              const std::string& one_time_code,
                              UserConsent consent_requirement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!one_time_code_);
  CHECK(!start_time_.is_null());
  CHECK(!origin_list.empty());

  receive_time_ = base::TimeTicks::Now();
  RecordSmsReceiveTime(receive_time_ - start_time_,
                       GetPageUkmSourceId(render_frame_host()));
  RecordSmsParsingStatus(SmsParsingStatus::kParsed,
                         GetPageUkmSourceId(render_frame_host()));

  one_time_code_ = one_time_code;
  // This function cannot get called during prerendering because WebOTPService
  // is deferred during prerendering by MojoBinderPolicyApplier. This CHECK
  // proves we don't have to worry about prerendering when using
  // WebContents::FromRenderFrameHost() below (see function comments for
  // WebContents::FromRenderFrameHost() for more details).
  CHECK_NE(render_frame_host().GetLifecycleState(),
           RenderFrameHost::LifecycleState::kPrerendering);
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  // With UserConsent API, users can see and interact with the permission prompt
  // when they are on the different page other than the one that calls WebOTP.
  // This is considered as a bad UX and we should measure how many successful
  // verifications are exercising the UserConsent backend which is implied by
  // UserConsent::kObtained.
  if (consent_requirement == UserConsent::kObtained) {
    RecordWebContentsVisibilityOnReceive(web_contents->GetVisibility() ==
                                         Visibility::VISIBLE);
  }

  // Create a new consent handler for each OTP request. While we could
  // potentially cache these across request but they are lightweight enought to
  // not be worth the complexity associate with caching them.
  UserConsentHandler* consent_handler =
      CreateConsentHandler(consent_requirement);
  consent_handler->RequestUserConsent(
      one_time_code, base::BindOnce(&WebOTPService::OnUserConsentComplete,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void WebOTPService::OnFailure(FailureType failure_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SmsParser::SmsParsingStatus status = SmsParsingStatus::kParsed;
  switch (failure_type) {
    case FailureType::kSmsNotParsed_OTPFormatRegexNotMatch:
      status = SmsParsingStatus::kOTPFormatRegexNotMatch;
      break;
    case FailureType::kSmsNotParsed_HostAndPortNotParsed:
      status = SmsParsingStatus::kHostAndPortNotParsed;
      break;
    case FailureType::kSmsNotParsed_kGURLNotValid:
      status = SmsParsingStatus::kGURLNotValid;
      break;
    case FailureType::kPromptTimeout:
    case FailureType::kPromptCancelled:
    case FailureType::kCrossDeviceFailure:
      // We do not complete the request here and instead rely on |OnTimeout| to
      // complete the request. This delays the promise resolution for privacy
      // reasons. e.g. if a promise gets resolved right after a user declines
      // the prompt, sites would know that the SMS did reach the user and they
      // could use such information for targeting. By using a timeout in all
      // cases, it is not possible to distinguish between sms not being received
      // and received but not shared.
      // Note that we still unsubscribe it from the fetcher and |Unsubscribe|
      // will be called again during the normal |CompleteRequest| process but it
      // should be no-op.
      delayed_rejection_reason_ = failure_type;
      fetcher_->Unsubscribe(origin_list_, this);
      return;
    case FailureType::kBackendNotAvailable:
      CompleteRequest(SmsStatus::kBackendNotAvailable);
      return;
    case FailureType::kNoFailure:
      NOTREACHED_IN_MIGRATION();
  }

  // Records Sms parsing failures.
  CHECK(status != SmsParsingStatus::kParsed);
  RecordSmsParsingStatus(status, GetPageUkmSourceId(render_frame_host()));
}

void WebOTPService::Abort() {
  if (!callback_) {
    mojo::ReportBadMessage(
        "The abort controller must be used after initiating an SMS request.");
    return;
  }
  CompleteRequest(SmsStatus::kAborted);
}

void WebOTPService::CompleteRequest(blink::mojom::SmsStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<std::string> code = std::nullopt;
  if (status == SmsStatus::kSuccess) {
    CHECK(one_time_code_);
    code = one_time_code_;
  }

  if (callback_) {
    RecordMetrics(status);
    std::move(callback_).Run(status, code);
  }

  CleanUp();
}

void WebOTPService::CleanUp() {
  // Skip resetting |one_time_code_|, |sms| and |receive_time_| while prompt is
  // still open in case it needs to be returned to the next incoming request
  // upon prompt confirmation.
  // TODO(majidvp): replace is_active() check with a check on existence of the
  // handler.
  auto* consent_handler = GetConsentHandler();
  bool consent_in_progress = consent_handler && consent_handler->is_active();
  if (!consent_in_progress) {
    one_time_code_.reset();
    receive_time_ = base::TimeTicks();
    // Clear the consent handler to avoid reusing it by mistake.
    consent_handler_.reset();
  }
  start_time_ = base::TimeTicks();
  callback_.Reset();
  delayed_rejection_reason_.reset();
  fetcher_->Unsubscribe(origin_list_, this);
}

UserConsentHandler* WebOTPService::CreateConsentHandler(
    UserConsent consent_requirement) {
  if (consent_handler_for_test_)
    return consent_handler_for_test_;

  if (consent_requirement == UserConsent::kNotObtained) {
    consent_handler_ = std::make_unique<PromptBasedUserConsentHandler>(
        render_frame_host(), origin_list_);
  } else {
    consent_handler_ = std::make_unique<NoopUserConsentHandler>();
  }

  return consent_handler_.get();
}

UserConsentHandler* WebOTPService::GetConsentHandler() {
  if (consent_handler_for_test_)
    return consent_handler_for_test_;

  return consent_handler_.get();
}

void WebOTPService::SetConsentHandlerForTesting(UserConsentHandler* handler) {
  consent_handler_for_test_ = handler;
}

void WebOTPService::OnTimeout() {
  CompleteRequest(SmsStatus::kTimeout);
}

void WebOTPService::RecordMetrics(blink::mojom::SmsStatus status) {
  // Record ContinueOn timing values only if we are using an asynchronous
  // consent handler (i.e. showing user prompts).
  auto* consent_handler = GetConsentHandler();
  if (consent_handler && consent_handler->is_async()) {
    if (status == SmsStatus::kSuccess) {
      CHECK(!receive_time_.is_null());
      RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    } else if (delayed_rejection_reason_ && delayed_rejection_reason_.value() ==
                                                FailureType::kPromptCancelled) {
      CHECK(!receive_time_.is_null());
      RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    }
  }

  ukm::SourceId source_id = GetPageUkmSourceId(render_frame_host());
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();

  // For privacy, metrics from inner frames are recorded with the top frame's
  // origin. Given that WebOTP is supported in cross-origin iframes, it's better
  // to indicate such information in the |Outcome| metrics to understand the
  // impact and implications. e.g. does user decline more often if the API is
  // used in an cross-origin iframe.
  bool is_cross_origin_frame = IsCrossOriginFrame(render_frame_host());
  // For privacy, we do not reject the request immediately when user declines
  // the permission prompt. Therefore the recording of such outcome is also
  // delayed. We record it at one of the following scenarios:
  //   1. at the timeout when the delayed timer fires
  //   2. before the timeout if the request is aborted
  //   3. before the timeout if |this| gets destroyed (e.g. website navigates)
  //   4. before the timeout if the request is cancelled in favor of a new
  //   request by the website.
  // In 2, 3 and 4, there is a different SmsStatus when trying to record metrics
  // so we need to do it based on delayed_rejection_reason_.
  if (delayed_rejection_reason_) {
    CHECK_NE(status, SmsStatus::kSuccess);
    // Records Outcome for requests which we reject with delay.
    RecordSmsOutcome(FailureTypeToOutcome(delayed_rejection_reason_.value()),
                     source_id, recorder, is_cross_origin_frame);

    if (delayed_rejection_reason_.value() == FailureType::kPromptCancelled) {
      RecordSmsUserCancelTime(base::TimeTicks::Now() - start_time_, source_id,
                              recorder);
    }
    delayed_rejection_reason_.reset();
    return;
  }

  // Records Outcome for requests which we resolve / reject immediately.
  RecordSmsOutcome(SmsStatusToOutcome(status), source_id, recorder,
                   is_cross_origin_frame);
  if (status == SmsStatus::kSuccess) {
    RecordSmsSuccessTime(base::TimeTicks::Now() - start_time_, source_id,
                         recorder);
  } else if (status == SmsStatus::kCancelled) {
    RecordSmsCancelTime(base::TimeTicks::Now() - start_time_);
  }
}

void WebOTPService::OnUserConsentComplete(UserConsentResult result) {
  switch (result) {
    case UserConsentResult::kApproved:
      CompleteRequest(SmsStatus::kSuccess);
      break;
    case UserConsentResult::kNoDelegate:
    case UserConsentResult::kInactiveRenderFrameHost:
      CompleteRequest(SmsStatus::kCancelled);
      break;
    case UserConsentResult::kDenied:
      OnFailure(FailureType::kPromptCancelled);
      break;
  }
}

}  // namespace content
