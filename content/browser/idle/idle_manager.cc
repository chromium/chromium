// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/idle/idle_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/idle/idle_monitor.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "ui/base/idle/idle.h"

namespace content {

namespace {

constexpr base::TimeDelta kPollInterval = base::TimeDelta::FromSeconds(1);

constexpr base::TimeDelta kMinimumThreshold = base::TimeDelta::FromSeconds(60);

// Default provider implementation. Everything is delegated to
// ui::CalculateIdleState, ui::CalculateIdleTime, and
// ui::CheckIdleStateIsLocked.
class DefaultIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  DefaultIdleProvider() = default;
  ~DefaultIdleProvider() override = default;

  ui::IdleState CalculateIdleState(base::TimeDelta idle_threshold) override {
    return ui::CalculateIdleState(idle_threshold.InSeconds());
  }

  base::TimeDelta CalculateIdleTime() override {
    return base::TimeDelta::FromSeconds(ui::CalculateIdleTime());
  }

  bool CheckIdleStateIsLocked() override {
    return ui::CheckIdleStateIsLocked();
  }
};

blink::mojom::IdleStatePtr IdleTimeToIdleState(bool locked,
                                               base::TimeDelta idle_time,
                                               base::TimeDelta idle_threshold) {
  blink::mojom::UserIdleState user;
  if (idle_time >= idle_threshold)
    user = blink::mojom::UserIdleState::kIdle;
  else
    user = blink::mojom::UserIdleState::kActive;

  blink::mojom::ScreenIdleState screen;
  if (locked)
    screen = blink::mojom::ScreenIdleState::kLocked;
  else
    screen = blink::mojom::ScreenIdleState::kUnlocked;

  return blink::mojom::IdleState::New(user, screen);
}

}  // namespace

IdleManager::IdleManager() : idle_time_provider_(new DefaultIdleProvider()) {}

IdleManager::~IdleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!monitors_.empty()) {
    IdleMonitor* monitor = monitors_.head()->value();
    monitor->RemoveFromList();
    delete monitor;
  }
}

void IdleManager::CreateService(blink::mojom::IdleManagerRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void IdleManager::AddMonitor(
    base::TimeDelta threshold,
    mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
    AddMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (threshold < kMinimumThreshold) {
    bindings_.ReportBadMessage("Minimum threshold is 60 seconds.");
    return;
  }

  auto monitor = std::make_unique<IdleMonitor>(
      std::move(monitor_remote), CheckIdleState(threshold), threshold);

  // This unretained reference is safe because IdleManager owns all IdleMonitor
  // instances.
  monitor->SetErrorHandler(
      base::BindOnce(&IdleManager::RemoveMonitor, base::Unretained(this)));

  monitors_.Append(monitor.release());

  StartPolling();

  std::move(callback).Run(CheckIdleState(threshold));
}

void IdleManager::RemoveMonitor(IdleMonitor* monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  monitor->RemoveFromList();
  delete monitor;

  if (monitors_.empty()) {
    StopPolling();
  }
}

void IdleManager::SetIdleTimeProviderForTest(
    std::unique_ptr<IdleTimeProvider> idle_time_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_time_provider_ = std::move(idle_time_provider);
}

bool IdleManager::IsPollingForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return poll_timer_.IsRunning();
}

void IdleManager::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE, kPollInterval,
                      base::BindRepeating(&IdleManager::UpdateIdleState,
                                          base::Unretained(this)));
  }
}

void IdleManager::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  poll_timer_.Stop();
}

blink::mojom::IdleStatePtr IdleManager::CheckIdleState(
    base::TimeDelta threshold) {
  base::TimeDelta idle_time = idle_time_provider_->CalculateIdleTime();
  bool locked = idle_time_provider_->CheckIdleStateIsLocked();

  return IdleTimeToIdleState(locked, idle_time, threshold);
}

void IdleManager::UpdateIdleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* node = monitors_.head(); node != monitors_.end();
       node = node->next()) {
    IdleMonitor* monitor = node->value();
    monitor->SetLastState(CheckIdleState(monitor->threshold()));
  }
}

}  // namespace content
