// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_keepalive.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "services/service_manager/public/cpp/service_receiver.h"

namespace service_manager {

class ServiceKeepaliveRefImpl : public ServiceKeepaliveRef {
 public:
  ServiceKeepaliveRefImpl(
      base::WeakPtr<ServiceKeepalive> keepalive,
      scoped_refptr<base::SequencedTaskRunner> keepalive_task_runner)
      : keepalive_(std::move(keepalive)),
        keepalive_task_runner_(std::move(keepalive_task_runner)) {
    // This object is not thread-safe but may be used exclusively on a different
    // thread from the one which constructed it.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ServiceKeepaliveRefImpl(const ServiceKeepaliveRefImpl&) = delete;
  ServiceKeepaliveRefImpl& operator=(const ServiceKeepaliveRefImpl&) = delete;

  ~ServiceKeepaliveRefImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (keepalive_task_runner_->RunsTasksInCurrentSequence() && keepalive_) {
      keepalive_->ReleaseRef();
    } else {
      keepalive_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ServiceKeepalive::ReleaseRef, keepalive_));
    }
  }

 private:
  // ServiceKeepaliveRef:
  std::unique_ptr<ServiceKeepaliveRef> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (keepalive_task_runner_->RunsTasksInCurrentSequence() && keepalive_) {
      keepalive_->AddRef();
    } else {
      keepalive_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ServiceKeepalive::AddRef, keepalive_));
    }

    return std::make_unique<ServiceKeepaliveRefImpl>(keepalive_,
                                                     keepalive_task_runner_);
  }

  base::WeakPtr<ServiceKeepalive> keepalive_;
  scoped_refptr<base::SequencedTaskRunner> keepalive_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceKeepalive::ServiceKeepalive(ServiceReceiver* receiver,
                                   std::optional<base::TimeDelta> idle_timeout)
    : receiver_(receiver), idle_timeout_(idle_timeout) {}

ServiceKeepalive::~ServiceKeepalive() = default;

std::unique_ptr<ServiceKeepaliveRef> ServiceKeepalive::CreateRef() {
  AddRef();
  return std::make_unique<ServiceKeepaliveRefImpl>(
      weak_ptr_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

bool ServiceKeepalive::HasNoRefs() {
  return ref_count_ == 0;
}

void ServiceKeepalive::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceKeepalive::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceKeepalive::AddRef() {
  ++ref_count_;

  if (!idle_timer_)
    return;

  // We had begun an idle timeout countdown, but a ref was added before the time
  // expired.
  idle_timer_.reset();
  for (auto& observer : observers_)
    observer.OnIdleTimeoutCancelled();
}

void ServiceKeepalive::ReleaseRef() {
  if (--ref_count_ > 0 || !idle_timeout_.has_value())
    return;

  // Ref count hit zero and we're configured with an idle timeout. Start the
  // doomsday clock!
  idle_timer_.emplace();
  idle_timer_->Start(FROM_HERE, *idle_timeout_,
                     base::BindOnce(&ServiceKeepalive::OnTimerExpired,
                                    base::Unretained(this)));
}

void ServiceKeepalive::OnTimerExpired() {
  // We were configured with a timeout and we have now been idle for that long.

  for (auto& observer : observers_)
    observer.OnIdleTimeout();

  // NOTE: We allow for a null |receiver_| because it's convenient in some
  // testing scenarios and adds no real complexity to this implementation.
  if (receiver_)
    receiver_->RequestClose();
}

}  // namespace service_manager
