// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_keepalive.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

ServiceKeepalive::ServiceKeepalive(ServiceContext* context,
                                   base::Optional<base::TimeDelta> idle_timeout,
                                   TimeoutObserver* timeout_observer)
    : context_(context),
      idle_timeout_(idle_timeout),
      timeout_observer_(timeout_observer),
      ref_factory_(base::BindRepeating(&ServiceKeepalive::OnRefCountZero,
                                       base::Unretained(this))),
      weak_ptr_factory_(this) {
  ref_factory_.SetRefAddedCallback(base::BindRepeating(
      &ServiceKeepalive::OnRefAdded, base::Unretained(this)));
}

ServiceKeepalive::~ServiceKeepalive() = default;

std::unique_ptr<ServiceContextRef> ServiceKeepalive::CreateRef() {
  return ref_factory_.CreateRef();
}

bool ServiceKeepalive::HasNoRefs() {
  return ref_factory_.HasNoRefs();
}

void ServiceKeepalive::OnRefAdded() {
  if (idle_timer_.IsRunning() && timeout_observer_)
    timeout_observer_->OnTimeoutCancelled();
  idle_timer_.Stop();
}

void ServiceKeepalive::OnRefCountZero() {
  if (!idle_timeout_.has_value())
    return;
  idle_timer_.Start(FROM_HERE, idle_timeout_.value(),
                    base::BindRepeating(&ServiceKeepalive::OnTimerExpired,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ServiceKeepalive::OnTimerExpired() {
  if (timeout_observer_)
    timeout_observer_->OnTimeoutExpired();
  context_->CreateQuitClosure().Run();
}

}  // namespace service_manager
