// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "base/callback.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

const char kSmsFetcherImplKeyName[] = "sms_fetcher";

SmsFetcherImpl::SmsFetcherImpl(BrowserContext* context, SmsProvider* provider)
    : context_(context), provider_(provider) {
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
        context, BrowserMainLoop::GetInstance()->GetSmsProvider());
    context->SetUserData(kSmsFetcherImplKeyName, std::move(fetcher));
  }

  return static_cast<SmsFetcherImpl*>(
      context->GetUserData(kSmsFetcherImplKeyName));
}

// TODO(crbug.com/1015645): Add implementation.
void SmsFetcherImpl::Subscribe(const url::Origin& origin,
                               SmsQueue::Subscriber* subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void SmsFetcherImpl::Subscribe(const url::Origin& origin,
                               SmsQueue::Subscriber* subscriber,
                               RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(subscriber);
  DCHECK(render_frame_host);
  // Should not be called multiple times for the same subscriber and origin.
  DCHECK(!subscribers_.HasSubscriber(origin, subscriber));

  subscribers_.Push(origin, subscriber);

  // Fetches a remote SMS.
  GetContentClient()->browser()->FetchRemoteSms(
      context_, origin,
      base::BindOnce(&SmsFetcherImpl::OnRemote,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetches a local SMS.
  if (provider_)
    provider_->Retrieve(render_frame_host);
}

void SmsFetcherImpl::Unsubscribe(const url::Origin& origin,
                                 SmsQueue::Subscriber* subscriber) {
  // Unsubscribe does not make a call to the provider because currently there
  // is no mechanism to cancel a subscription.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscribers_.Remove(origin, subscriber);
}

bool SmsFetcherImpl::Notify(const url::Origin& origin,
                            const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The received OTP is returned to the first subscriber for the origin.
  auto* subscriber = subscribers_.Pop(origin);

  if (!subscriber)
    return false;

  subscriber->OnReceive(one_time_code);
  return true;
}

void SmsFetcherImpl::OnRemote(base::Optional<std::string> sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sms)
    return;

  // TODO(yigu): We should log when the sms cannot be parsed similar to local
  // SMSes.
  SmsParser::Result result = SmsParser::Parse(*sms);
  if (!result.IsValid())
    return;

  Notify(result.origin, result.one_time_code);
}

bool SmsFetcherImpl::OnReceive(const url::Origin& origin,
                               const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Notify(origin, one_time_code);
}

void SmsFetcherImpl::NotifyParsingFailure(SmsParser::SmsParsingStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscribers_.NotifyParsingFailure(status);
}

bool SmsFetcherImpl::HasSubscribers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return subscribers_.HasSubscribers();
}

}  // namespace content
