// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

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
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-shared.h"

using blink::SmsReceiverDestroyedReason;
using blink::mojom::SmsStatus;

namespace content {

SmsService::SmsService(
    SmsFetcher* fetcher,
    std::unique_ptr<UserConsentHandler> consent_handler,
    const url::Origin& origin,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : FrameServiceBase(host, std::move(receiver)),
      fetcher_(fetcher),
      consent_handler_(std::move(consent_handler)),
      origin_(origin) {
  DCHECK(fetcher_);
}

SmsService::SmsService(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : SmsService(fetcher,
                 nullptr,
                 host->GetLastCommittedOrigin(),
                 host,
                 std::move(receiver)) {
  bool needs_user_prompt =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebOtpBackend) == switches::kWebOtpBackendSmsVerification;

  if (needs_user_prompt) {
    consent_handler_ = std::make_unique<PromptBasedUserConsentHandler>(
        render_frame_host(), origin_);
  } else {
    consent_handler_ = std::make_unique<NoopUserConsentHandler>();
  }
}

SmsService::~SmsService() {
  if (callback_)
    CompleteRequest(SmsStatus::kUnhandledRequest);
  DCHECK(!callback_);
}

// static
void SmsService::Create(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  DCHECK(host);

  // SmsService owns itself. It will self-destruct when a mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new SmsService(fetcher, host, std::move(receiver));
  static_cast<RenderFrameHostImpl*>(host)->OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kSmsService);
}

void SmsService::Receive(ReceiveCallback callback) {
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

  // Abort the last request if there is we have not yet handled it.
  if (callback_) {
    std::move(callback_).Run(SmsStatus::kCancelled, base::nullopt);
    fetcher_->Unsubscribe(origin_, this);
  }

  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);

  // |one_time_code_| and prompt are still present from the previous request so
  // a new subscription is unnecessary. Note that it is only safe for us to use
  // the in flight otp with the new request since both requests belong to the
  // same origin.
  if (consent_handler_->is_active())
    return;

  fetcher_->Subscribe(origin_, this, render_frame_host());
}

void SmsService::OnReceive(const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!one_time_code_);
  DCHECK(!start_time_.is_null());

  receive_time_ = base::TimeTicks::Now();
  RecordSmsReceiveTime(receive_time_ - start_time_,
                       render_frame_host()->GetPageUkmSourceId());
  RecordSmsParsingStatus(SmsParsingStatus::kParsed,
                         render_frame_host()->GetPageUkmSourceId());

  one_time_code_ = one_time_code;

  consent_handler_->RequestUserConsent(
      one_time_code, base::BindOnce(&SmsService::CompleteRequest,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void SmsService::OnFailure(FailureType failure_type) {
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
      // TODO(yigu): Land implementation in crrev.com/2427560.
      NOTIMPLEMENTED();
      break;
  }
  DCHECK(status != SmsParsingStatus::kParsed);
  RecordSmsParsingStatus(status, render_frame_host()->GetPageUkmSourceId());
}

void SmsService::Abort() {
  DCHECK(callback_);
  CompleteRequest(SmsStatus::kAborted);
}

void SmsService::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  switch (load_details.type) {
    case NavigationType::NAVIGATION_TYPE_NEW_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateNewPage);
      break;
    case NavigationType::NAVIGATION_TYPE_EXISTING_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateExistingPage);
      break;
    case NavigationType::NAVIGATION_TYPE_SAME_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateSamePage);
      break;
    default:
      // Ignore cases we don't care about.
      break;
  }
}

void SmsService::CompleteRequest(blink::mojom::SmsStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Optional<std::string> code = base::nullopt;
  if (status == SmsStatus::kSuccess) {
    DCHECK(one_time_code_);
    code = one_time_code_;
  }

  // Record ContinueOn timing values only if we are using an asynchronous
  // consent handler (i.e. showing user prompts).
  if (consent_handler_->is_async()) {
    if (status == SmsStatus::kSuccess) {
      DCHECK(!receive_time_.is_null());
      RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    } else if (status == SmsStatus::kCancelled) {
      DCHECK(!receive_time_.is_null());
      RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);
    }
  }

  if (callback_) {
    std::move(callback_).Run(status, code);
  }

  CleanUp();
}

void SmsService::CleanUp() {
  // Skip resetting |one_time_code_|, |sms| and |receive_time_| while prompt is
  // still open in case it needs to be returned to the next incoming request
  // upon prompt confirmation.
  if (!consent_handler_->is_active()) {
    one_time_code_.reset();
    receive_time_ = base::TimeTicks();
  }
  start_time_ = base::TimeTicks();
  callback_.Reset();
  fetcher_->Unsubscribe(origin_, this);
}

}  // namespace content
