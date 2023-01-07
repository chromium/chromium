// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_queue.h"

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

SmsQueue::SmsQueue() = default;
SmsQueue::~SmsQueue() = default;

void SmsQueue::Push(const OriginList& origin_list, Subscriber* subscriber) {
  subscribers_[origin_list].AddObserver(subscriber);
  // We expect that in most cases there should be only one pending origin and in
  // rare cases there may be a few more (<10).
  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Sms.PendingOriginCount",
                             subscribers_.size(), 10);
}

SmsQueue::Subscriber* SmsQueue::Pop(const OriginList& origin_list) {
  auto it = subscribers_.find(origin_list);
  if (it == subscribers_.end())
    return nullptr;
  base::ObserverList<Subscriber>& subscribers = it->second;

  Subscriber& subscriber = *(subscribers.begin());
  Remove(origin_list, &subscriber);
  return &subscriber;
}

void SmsQueue::Remove(const OriginList& origin_list, Subscriber* subscriber) {
  auto it = subscribers_.find(origin_list);
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

bool SmsQueue::HasSubscriber(const OriginList& origin_list,
                             const Subscriber* subscriber) {
  return (subscribers_.find(origin_list) != subscribers_.end()) &&
         subscribers_[origin_list].HasObserver(subscriber);
}

// Currently we cannot extract the origin information upon failure because it's
// not visible to the service. If we have a single origin in the queue we simply
// assume failure belongs to that origin. This assumption should hold for vast
// majority of cases given that a single pending origin is the most likely
// scenario (measured in UMA histogram  |Blink.Sms.PendingOriginCount|). However
// if there is more than one origin waiting we do not pass up the error to avoid
// over-counting failures. Similar to the success case, we only notify the first
// subscriber with that origin.
bool SmsQueue::NotifyFailure(FailureType failure_type) {
  // TODO(crbug.com/1138454): We should improve the infrastructure to be able to
  // handle failed requests when there are multiple pending origins
  // simultaneously.
  if (subscribers_.size() != 1)
    return false;

  const OriginList& implied_origin = subscribers_.begin()->first;
  Subscriber* subscriber = Pop(implied_origin);
  subscriber->OnFailure(failure_type);
  return true;
}

}  // namespace content
