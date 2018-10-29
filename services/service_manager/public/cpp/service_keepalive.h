// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_

#include "services/service_manager/public/cpp/export.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace service_manager {

class ServiceContext;

// Helper class which vends ServiceContextRefs from its own
// ServiceContextRefFactory. Whenever the ref count goes to zero, this starts an
// idle timer (configured at construction time). If the timer runs out before
// another ref is created, this requests clean service termination from the
// service manager on the service's behalf.
//
// Useful if you want your service to stay alive for some fixed delay after
// going idle, to insulate against frequent startup and shutdown of the service
// when used at regular intervals or in rapid but not continuous succession, as
// is fairly common.
//
// Use this in place of directly owning a ServiceContextRefFactory, to vend
// service references to different endpoints in your service.
class SERVICE_MANAGER_PUBLIC_CPP_EXPORT ServiceKeepalive {
 public:
  class TimeoutObserver {
   public:
    virtual ~TimeoutObserver() {}
    virtual void OnTimeoutExpired() = 0;
    virtual void OnTimeoutCancelled() = 0;
  };

  // Creates a keepalive which allows the service to be idle for |idle_timeout|
  // before requesting termination. If |idle_timeout| is not given, the
  // ServiceKeepalive will never request termination, i.e. the service will
  // stay alive indefinitely. Both |context| and |timeout_observer| are not
  // owned and must outlive the ServiceKeepalive instance.
  ServiceKeepalive(ServiceContext* context,
                   base::Optional<base::TimeDelta> idle_timeout,
                   TimeoutObserver* timeout_observer = nullptr);
  ~ServiceKeepalive();

  std::unique_ptr<ServiceContextRef> CreateRef();
  bool HasNoRefs();

 private:
  void OnRefAdded();
  void OnRefCountZero();
  void OnTimerExpired();

  ServiceContext* const context_;
  const base::Optional<base::TimeDelta> idle_timeout_;
  TimeoutObserver* const timeout_observer_;
  base::OneShotTimer idle_timer_;
  ServiceContextRefFactory ref_factory_;
  base::WeakPtrFactory<ServiceKeepalive> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceKeepalive);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
