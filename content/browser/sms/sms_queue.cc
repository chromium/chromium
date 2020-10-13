// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_queue.h"

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"

namespace content {

SmsQueue::SmsQueue() = default;
SmsQueue::~SmsQueue() = default;

void SmsQueue::Push(const url::Origin& origin, Subscriber* subscriber) {
  subscribers_[origin].AddObserver(subscriber);
  // We expect that in most cases there should be only one pending origin and in
  // rare cases there may be a few more (<10).
  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Sms.PendingOriginCount",
                             subscribers_.size(), 10);
}

SmsQueue::Subscriber* SmsQueue::Pop(const url::Origin& origin) {
  auto it = subscribers_.find(origin);
  if (it == subscribers_.end())
    return nullptr;
  base::ObserverList<Subscriber>& subscribers = it->second;

  Subscriber& subscriber = *(subscribers.begin());
  Remove(origin, &subscriber);
  return &subscriber;
}

void SmsQueue::Remove(const url::Origin& origin, Subscriber* subscriber) {
  auto it = subscribers_.find(origin);
  if (it == subscribers_.end())
    return;
  base::ObserverList<Subscriber>& queue = it->second;
  queue.RemoveObserver(subscriber);

  if (queue.begin() == queue.end())
    subscribers_.erase(it);
}

bool SmsQueue::HasSubscribers() {
  return !subscribers_.empty();
}

bool SmsQueue::HasSubscriber(const url::Origin& origin,
                             const Subscriber* subscriber) {
  return (subscribers_.find(origin) != subscribers_.end()) &&
         subscribers_[origin].HasObserver(subscriber);
}

void SmsQueue::NotifyParsingFailure(SmsParsingStatus status) {
  FailureType failure_type;
  switch (status) {
    case SmsParsingStatus::kOTPFormatRegexNotMatch:
      failure_type = FailureType::kSmsNotParsed_OTPFormatRegexNotMatch;
      break;
    case SmsParsingStatus::kHostAndPortNotParsed:
      failure_type = FailureType::kSmsNotParsed_HostAndPortNotParsed;
      break;
    case SmsParsingStatus::kGURLNotValid:
      failure_type = FailureType::kSmsNotParsed_kGURLNotValid;
      break;
    default:
      NOTREACHED();
      break;
  }
  for (auto& origin_to_subscriber_list : subscribers_) {
    base::ObserverList<Subscriber>& subscribers =
        origin_to_subscriber_list.second;
    for (auto& subscriber : subscribers) {
      subscriber.OnFailure(failure_type);
    }
  }
}

}  // namespace content
