// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/ash/power_manager_client_proxy.h"

namespace media {

PowerManagerClientProxy::PowerManagerClientProxy() = default;

void PowerManagerClientProxy::Init(
    base::WeakPtr<Observer> observer,
    const std::string& debug_info,
    scoped_refptr<base::SingleThreadTaskRunner> observer_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner) {
  observer_ = std::move(observer);
  debug_info_ = debug_info;
  observer_task_runner_ = std::move(observer_task_runner);
  dbus_task_runner_ = std::move(dbus_task_runner);

  dbus_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerClientProxy::InitOnDBusThread, this));
}

void PowerManagerClientProxy::Shutdown() {
  dbus_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerClientProxy::ShutdownOnDBusThread, this));
}

void PowerManagerClientProxy::UnblockSuspend(
    const base::UnguessableToken& unblock_suspend_token) {
  dbus_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerClientProxy::UnblockSuspendOnDBusThread, this,
                     unblock_suspend_token));
}

PowerManagerClientProxy::~PowerManagerClientProxy() = default;

void PowerManagerClientProxy::InitOnDBusThread() {
  DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

void PowerManagerClientProxy::ShutdownOnDBusThread() {
  DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerManagerClientProxy::UnblockSuspendOnDBusThread(
    const base::UnguessableToken& unblock_suspend_token) {
  DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
  chromeos::PowerManagerClient::Get()->UnblockSuspend(unblock_suspend_token);
}

void PowerManagerClientProxy::SuspendImminentOnObserverThread(
    base::UnguessableToken unblock_suspend_token) {
  DCHECK(observer_task_runner_->RunsTasksInCurrentSequence());
  // TODO(b/175168296): Ensure that the weak pointer |observer| is dereferenced
  // and invalidated on the same thread.
  if (observer_) {
    observer_->SuspendImminent();
  }
  UnblockSuspend(std::move(unblock_suspend_token));
}

void PowerManagerClientProxy::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  auto token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, debug_info_);
  observer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerClientProxy::SuspendImminentOnObserverThread,
                     this, std::move(token)));
}

void PowerManagerClientProxy::SuspendDone(base::TimeDelta sleep_duration) {
  observer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Observer::SuspendDone, observer_));
}

}  // namespace media
