// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "base/functional/callback.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

const char kSmsFetcherImplKeyName[] = "sms_fetcher";

SmsFetcherImpl::SmsFetcherImpl(SmsProvider* provider) : provider_(provider) {
  if (provider_)
    provider_->AddObserver(this);
}

SmsFetcherImpl::~SmsFetcherImpl() {
  if (provider_)
    provider_->RemoveObserver(this);
}

// static
SmsFetcher* SmsFetcher::Get(BrowserContext* context) {
  if (!context->GetUserData(kSmsFetcherImplKeyName)) {
    auto fetcher = std::make_unique<SmsFetcherImpl>(
        BrowserMainLoop::GetInstance()->GetSmsProvider());
    context->SetUserData(kSmsFetcherImplKeyName, std::move(fetcher));
  }

  return static_cast<SmsFetcherImpl*>(
      context->GetUserData(kSmsFetcherImplKeyName));
}

void SmsFetcherImpl::Subscribe(const OriginList& origin_list,
                               SmsQueue::Subscriber& subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Should not be called multiple times for the same subscriber and origin.
  DCHECK(!subscribers_.HasSubscriber(origin_list, &subscriber));

  subscribers_.Push(origin_list, &subscriber);
  if (provider_)
    provider_->Retrieve(nullptr, SmsFetchType::kRemote);
}

void SmsFetcherImpl::Subscribe(const OriginList& origin_list,
                               SmsQueue::Subscriber& subscriber,
                               RenderFrameHost& render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This function cannot get called during prerendering because
  // WebOTPService::Receive() calls this, but WebOTPService is deferred during
  // prerendering by MojoBinderPolicyApplier. This DCHECK proves we don't have
  // to worry about prerendering when using WebContents::FromRenderFrameHost()
  // below (see function comments for WebContents::FromRenderFrameHost() for
  // more details).
  DCHECK_NE(render_frame_host.GetLifecycleState(),
            RenderFrameHost::LifecycleState::kPrerendering);
  // Should not be called multiple times for the same subscriber.
  DCHECK(!remote_cancel_callbacks_.count(&subscriber));
  DCHECK(!subscribers_.HasSubscriber(origin_list, &subscriber));

  subscribers_.Push(origin_list, &subscriber);

  // Fetches a remote SMS.
  base::OnceClosure cancel_callback =
      GetContentClient()->browser()->FetchRemoteSms(
          WebContents::FromRenderFrameHost(&render_frame_host), origin_list,
          base::BindOnce(&SmsFetcherImpl::OnRemote,
                         weak_ptr_factory_.GetWeakPtr()));
  if (cancel_callback)
    remote_cancel_callbacks_[&subscriber] = std::move(cancel_callback);

  // Fetches a local SMS.
  if (provider_)
    provider_->Retrieve(&render_frame_host, SmsFetchType::kLocal);
}

void SmsFetcherImpl::Unsubscribe(const OriginList& origin_list,
                                 SmsQueue::Subscriber* subscriber) {
  // Unsubscribe does not make a call to the provider because currently there
  // is no mechanism to cancel a subscription.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscribers_.Remove(origin_list, subscriber);
  // A subscriber does not have a remote cancel callback in the map when
  //   1. it has been unsubscribed before. e.g. we unsubscribe a subscriber when
  //     a verification flow is successful and when the subscriber is destroyed.
  //   2. TODO(crbug.com/40103792): no need to keep cancel callback when we
  //   don't
  //     fetch a remote sms. e.g. when kWebOTPCrossDevice is disabled.
  auto it = remote_cancel_callbacks_.find(subscriber);
  if (it == remote_cancel_callbacks_.end())
    return;
  auto cancel_callback = std::move(it->second);
  remote_cancel_callbacks_.erase(it);

  std::move(cancel_callback).Run();
}

bool SmsFetcherImpl::Notify(const OriginList& origin_list,
                            const std::string& one_time_code,
                            UserConsent consent_requirement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The received OTP is returned to the first subscriber for the origin.
  auto* subscriber = subscribers_.Pop(origin_list);

  if (!subscriber)
    return false;

  subscriber->OnReceive(origin_list, one_time_code, consent_requirement);
  return true;
}

void SmsFetcherImpl::OnRemote(std::optional<OriginList> origin_list,
                              std::optional<std::string> one_time_code,
                              std::optional<FailureType> failure_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (failure_type) {
    OnFailure(failure_type.value());
    return;
  }
  if (!origin_list || !one_time_code)
    return;

  Notify(origin_list.value(), one_time_code.value(), UserConsent::kObtained);
}

bool SmsFetcherImpl::OnReceive(const OriginList& origin_list,
                               const std::string& one_time_code,
                               UserConsent consent_requirement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Notify(origin_list, one_time_code, consent_requirement);
}

bool SmsFetcherImpl::OnFailure(FailureType failure_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return subscribers_.NotifyFailure(failure_type);
}

bool SmsFetcherImpl::HasSubscribers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return subscribers_.HasSubscribers();
}

}  // namespace content
