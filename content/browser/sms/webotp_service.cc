// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/sms/sms_metrics.h"
#include "content/browser/sms/user_consent_handler.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-shared.h"

using blink::WebOTPServiceDestroyedReason;
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
bool ValidateAndCollectUniqueOrigins(RenderFrameHost* rfh,
                                     OriginList& origin_list) {
  url::Origin current_origin = rfh->GetLastCommittedOrigin();
  origin_list.push_back(current_origin);

  RenderFrameHost* parent = rfh->GetParent();
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

bool IsCrossOriginFrame(RenderFrameHost* rfh) {
  if (!rfh->GetParent())
    return false;
  url::Origin current_origin = rfh->GetLastCommittedOrigin();
  RenderFrameHost* parent = rfh->GetParent();
  while (parent) {
    url::Origin parent_origin = parent->GetLastCommittedOrigin();
    if (!parent_origin.IsSameOriginWith(current_origin))
      return true;
    parent = parent->GetParent();
  }
  return false;
}

}  // namespace

WebOTPService::WebOTPService(
    SmsFetcher* fetcher,
    const OriginList& origin_list,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver)
    : FrameServiceBase(host, std::move(receiver)),
      fetcher_(fetcher),
      origin_list_(origin_list),
      timeout_timer_(FROM_HERE,
                     blink::kWebOTPRequestTimeout,
                     this,
                     &WebOTPService::OnTimeout) {
  DCHECK(fetcher_);
}

WebOTPService::~WebOTPService() {
  // Resolve any pending callback and invoke clean up to unsubscribe this
  // service from fetcher.
  CompleteRequest(SmsStatus::kUnhandledRequest);

  DCHECK(!callback_);
}

// static
bool WebOTPService::Create(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver) {
  DCHECK(host);

  OriginList origin_list;
  if (!ValidateAndCollectUniqueOrigins(host, origin_list))
    return false;

  // WebOTPService owns itself. It will self-destruct when a mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new WebOTPService(fetcher, origin_list, host, std::move(receiver));
  static_cast<RenderFrameHostImpl*>(host)->OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kWebOTPService);
  return true;
}

void WebOTPService::Receive(ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(majidvp): The comment below seems incorrect. This flow is used for
  // both prompted and unprompted backends so it is not clear if we should
  // always cancel early. Also I don't believe that we are actually silently
  // dropping the sms but in fact the logic cancels the request once
  // an sms comes in and there is no delegate.

  // This flow relies on the delegate to display an infobar for user
  // confirmation. Cancelling the call early if no delegate is available is
  // easier to debug then silently dropping SMSes later on.
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  if (!web_contents->GetDelegate()) {
    std::move(callback).Run(SmsStatus::kCancelled, base::nullopt);
    return;
  }

  DCHECK(!origin_list_.empty());
  // Abort the last request if there is we have not yet handled it.
  if (callback_) {
    std::move(callback_).Run(SmsStatus::kCancelled, base::nullopt);
    fetcher_->Unsubscribe(origin_list_, this);
  }

  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);
  timeout_timer_.Reset();
  prompt_failure_.reset();

  // |one_time_code_| and prompt are still present from the previous request so
  // a new subscription is unnecessary. Note that it is only safe for us to use
  // the in flight otp with the new request since both requests belong to the
  // same origin.
  // TODO(majidvp): replace is_active() check with a check on existence of the
  // handler.
  auto* consent_handler = GetConsentHandler();
  if (consent_handler && consent_handler->is_active())
    return;

  fetcher_->Subscribe(origin_list_, this, render_frame_host());
}

void WebOTPService::OnReceive(const OriginList& origin_list,
                              const std::string& one_time_code,
                              UserConsent consent_requirement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!one_time_code_);
  DCHECK(!start_time_.is_null());
  DCHECK(!origin_list.empty());

  receive_time_ = base::TimeTicks::Now();
  RecordSmsReceiveTime(receive_time_ - start_time_,
                       render_frame_host()->GetPageUkmSourceId());
  RecordSmsParsingStatus(SmsParsingStatus::kParsed,
                         render_frame_host()->GetPageUkmSourceId());

  one_time_code_ = one_time_code;

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

  switch (failure_type) {
    case FailureType::kPromptTimeout:
    case FailureType::kPromptCancelled:
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
      prompt_failure_ = failure_type;
      fetcher_->Unsubscribe(origin_list_, this);
      return;
    case FailureType::kBackendNotAvailable:
      CompleteRequest(SmsStatus::kBackendNotAvailable);
      return;
    default: /* do nothing as it is handled below. */
      break;
  }

  // Records Sms parsing failures.
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
    case FailureType::kBackendNotAvailable:
    case FailureType::kNoFailure:
      NOTREACHED();
      break;
  }
  DCHECK(status != SmsParsingStatus::kParsed);
  RecordSmsParsingStatus(status, render_frame_host()->GetPageUkmSourceId());
}

void WebOTPService::Abort() {
  DCHECK(callback_);
  CompleteRequest(SmsStatus::kAborted);
}

void WebOTPService::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  switch (load_details.type) {
    case NavigationType::NAVIGATION_TYPE_NEW_ENTRY:
      RecordDestroyedReason(WebOTPServiceDestroyedReason::kNavigateNewPage);
      break;
    case NavigationType::NAVIGATION_TYPE_EXISTING_ENTRY:
      RecordDestroyedReason(
          WebOTPServiceDestroyedReason::kNavigateExistingPage);
      break;
    default:
      // Ignore cases we don't care about.
      break;
  }
}

void WebOTPService::CompleteRequest(blink::mojom::SmsStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Optional<std::string> code = base::nullopt;
  if (status == SmsStatus::kSuccess) {
    DCHECK(one_time_code_);
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
  prompt_failure_.reset();
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
  ukm::SourceId source_id = render_frame_host()->GetPageUkmSourceId();
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();

  // For privacy, metrics from inner frames are recorded with the top frame's
  // origin. Given that WebOTP is supported in cross-origin iframes, it's better
  // to indicate such information in the |Outcome| metrics to understand the
  // impact and implications. e.g. does user decline more often if the API is
  // used in an cross-origin iframe.
  bool is_cross_origin_frame = IsCrossOriginFrame(render_frame_host());

  if (status == SmsStatus::kSuccess) {
    RecordSmsOutcome(Outcome::kSuccess, source_id, recorder,
                     is_cross_origin_frame);
    RecordSmsSuccessTime(base::TimeTicks::Now() - start_time_, source_id,
                         recorder);
  } else if (status == SmsStatus::kUnhandledRequest) {
    RecordSmsOutcome(Outcome::kUnhandledRequest, source_id, recorder,
                     is_cross_origin_frame);
  } else if (status == SmsStatus::kAborted) {
    RecordSmsOutcome(Outcome::kAborted, source_id, recorder,
                     is_cross_origin_frame);
  } else if (status == SmsStatus::kCancelled) {
    RecordSmsOutcome(Outcome::kCancelled, source_id, recorder,
                     is_cross_origin_frame);
    RecordSmsCancelTime(base::TimeTicks::Now() - start_time_);
  } else if (status == SmsStatus::kTimeout) {
    if (prompt_failure_ &&
        prompt_failure_.value() == FailureType::kPromptCancelled) {
      RecordSmsOutcome(Outcome::kUserCancelled, source_id, recorder,
                       is_cross_origin_frame);
      RecordSmsUserCancelTime(base::TimeTicks::Now() - start_time_, source_id,
                              recorder);
    } else {
      RecordSmsOutcome(Outcome::kTimeout, source_id, recorder,
                       is_cross_origin_frame);
    }
  } else if (status == SmsStatus::kBackendNotAvailable) {
    // Records when the backend is not available AND the request gets cancelled.
    // i.e. client specifies GmsBackend.VERIFICATION but it's unavailable. If
    // client specifies GmsBackend.AUTO and the verification backend is not
    // available, we fall back to the user consent backend and the request will
    // be handled accordingly. e.g. if the user declined the prompt, we record
    // it as |kUserCancelled|.
    RecordSmsOutcome(Outcome::kBackendNotAvailable, source_id, recorder,
                     is_cross_origin_frame);
  }

  // Record ContinueOn timing values only if we are using an asynchronous
  // consent handler (i.e. showing user prompts).
  auto* consent_handler = GetConsentHandler();
  if (consent_handler && consent_handler->is_async()) {
    if (status == SmsStatus::kSuccess) {
      DCHECK(!receive_time_.is_null());
      RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    } else if (prompt_failure_ &&
               prompt_failure_.value() == FailureType::kPromptCancelled) {
      DCHECK(!receive_time_.is_null());
      RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    }
  }
}

void WebOTPService::OnUserConsentComplete(UserConsentResult result) {
  switch (result) {
    case UserConsentResult::kApproved:
      CompleteRequest(SmsStatus::kSuccess);
      break;
    case UserConsentResult::kNoDelegate:
      CompleteRequest(SmsStatus::kCancelled);
      break;
    case UserConsentResult::kDenied:
      OnFailure(FailureType::kPromptCancelled);
      break;
  }
}

}  // namespace content
