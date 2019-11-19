// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_queue.h"

#include "base/callback.h"
#include "base/optional.h"

namespace content {

SmsQueue::SmsQueue() = default;
SmsQueue::~SmsQueue() = default;

void SmsQueue::Push(const url::Origin& origin, Subscriber* subscriber) {
  subscribers_[origin].AddObserver(subscriber);
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

}  // namespace content
