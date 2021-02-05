// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_POWER_MANAGER_CLIENT_PROXY_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_POWER_MANAGER_CLIENT_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace media {

class PowerManagerClientProxy
    : public base::RefCountedThreadSafe<PowerManagerClientProxy>,
      public chromeos::PowerManagerClient::Observer {
 public:
  class Observer {
   public:
    virtual void SuspendDone() = 0;
    virtual void SuspendImminent() = 0;
  };

  PowerManagerClientProxy();
  PowerManagerClientProxy(const PowerManagerClientProxy&) = delete;
  PowerManagerClientProxy& operator=(const PowerManagerClientProxy&) = delete;

  void Init(base::WeakPtr<Observer> observer,
            const std::string& debug_info,
            scoped_refptr<base::SingleThreadTaskRunner> observer_task_runner,
            scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner);

  void Shutdown();

  void UnblockSuspend(const base::UnguessableToken& unblock_suspend_token);

 private:
  friend class base::RefCountedThreadSafe<PowerManagerClientProxy>;

  ~PowerManagerClientProxy() override;

  void InitOnDBusThread();

  void ShutdownOnDBusThread();

  void UnblockSuspendOnDBusThread(
      const base::UnguessableToken& unblock_suspend_token);

  void SuspendImminentOnObserverThread(
      base::UnguessableToken unblock_suspend_token);

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) final;

  void SuspendDone(base::TimeDelta sleep_duration) final;

  base::WeakPtr<Observer> observer_;
  std::string debug_info_;
  scoped_refptr<base::SingleThreadTaskRunner> observer_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_POWER_MANAGER_CLIENT_PROXY_H_
