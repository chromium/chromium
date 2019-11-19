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
#include "url/origin.h"

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

void SmsFetcherImpl::Subscribe(const url::Origin& origin,
                               SmsQueue::Subscriber* subscriber) {
  subscribers_.Push(origin, subscriber);

  // Fetches a remote SMS.
  GetContentClient()->browser()->FetchRemoteSms(
      context_, origin,
      base::BindOnce(&SmsFetcherImpl::OnRemote,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetches a local SMS.
  if (provider_)
    provider_->Retrieve();
}

void SmsFetcherImpl::Unsubscribe(const url::Origin& origin,
                                 SmsQueue::Subscriber* subscriber) {
  subscribers_.Remove(origin, subscriber);
}

bool SmsFetcherImpl::Notify(const url::Origin& origin,
                            const std::string& one_time_code,
                            const std::string& sms) {
  auto* subscriber = subscribers_.Pop(origin);

  if (!subscriber)
    return false;

  subscriber->OnReceive(one_time_code, sms);

  return true;
}

void SmsFetcherImpl::OnRemote(base::Optional<std::string> sms) {
  if (!sms)
    return;

  base::Optional<SmsParser::Result> result = SmsParser::Parse(*sms);
  if (!result)
    return;

  Notify(result->origin, result->one_time_code, *sms);
}

bool SmsFetcherImpl::OnReceive(const url::Origin& origin,
                               const std::string& one_time_code,
                               const std::string& sms) {
  return Notify(origin, one_time_code, sms);
}

bool SmsFetcherImpl::HasSubscribers() {
  return subscribers_.HasSubscribers();
}

}  // namespace content
