// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_POLLING_SERVICE_H_
#define CONTENT_BROWSER_IDLE_IDLE_POLLING_SERVICE_H_

#include <memory>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

class IdleTimeProvider;

// Polls the system to determine whether the user is idle or the screen is
// locked and notifies observers.
class CONTENT_EXPORT IdlePollingService {
 public:
  static IdlePollingService* GetInstance();

  struct State {
    bool locked;
    base::TimeDelta idle_time;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIdleStateChange(const State& state) = 0;
  };

  IdlePollingService(const IdlePollingService&) = delete;
  IdlePollingService& operator=(const IdlePollingService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const State& GetIdleState();

  void SetProviderForTest(std::unique_ptr<IdleTimeProvider> provider);
  bool IsPollingForTest();

 private:
  friend class base::NoDestructor<IdlePollingService>;

  IdlePollingService();
  ~IdlePollingService();

  void PollIdleState();

  base::RepeatingTimer timer_;
  std::unique_ptr<IdleTimeProvider> provider_;
  State last_state_;
  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_POLLING_SERVICE_H_
