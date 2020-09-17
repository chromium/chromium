// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "ui/base/idle/idle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::IdleManagerError;
using blink::mojom::IdleState;
using blink::mojom::PermissionStatus;

constexpr base::TimeDelta kPollInterval = base::TimeDelta::FromSeconds(1);

constexpr base::TimeDelta kMinimumThreshold = base::TimeDelta::FromSeconds(60);

// Default provider implementation. Everything is delegated to
// ui::CalculateIdleTime and ui::CheckIdleStateIsLocked.
class DefaultIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  DefaultIdleProvider() = default;
  ~DefaultIdleProvider() override = default;

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

  return IdleState::New(user, screen);
}

}  // namespace

IdleManagerImpl::IdleManagerImpl(BrowserContext* browser_context)
    : idle_time_provider_(new DefaultIdleProvider()),
      browser_context_(browser_context) {}

IdleManagerImpl::~IdleManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!monitors_.empty()) {
    IdleMonitor* monitor = monitors_.head()->value();
    monitor->RemoveFromList();
    delete monitor;
  }
}

void IdleManagerImpl::CreateService(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver,
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver), origin);
}

void IdleManagerImpl::SetIdleOverride(
    blink::mojom::UserIdleState user_state,
    blink::mojom::ScreenIdleState screen_state) {
  state_override_ = IdleState::New(user_state, screen_state);
  UpdateIdleState();
}

void IdleManagerImpl::ClearIdleOverride() {
  state_override_ = nullptr;
  UpdateIdleState();
}

void IdleManagerImpl::AddMonitor(
    base::TimeDelta threshold,
    mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
    AddMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (threshold < kMinimumThreshold) {
    receivers_.ReportBadMessage("Minimum threshold is 1 minute.");
    return;
  }

  const url::Origin& origin = receivers_.current_context();
  if (!HasPermission(origin)) {
    std::move(callback).Run(IdleManagerError::kPermissionDisabled, nullptr);
    return;
  }

  blink::mojom::IdleStatePtr current_state = CheckIdleState(threshold);
  auto response_state = current_state->Clone();
  auto monitor = std::make_unique<IdleMonitor>(
      std::move(monitor_remote), std::move(current_state), threshold);

  // This unretained reference is safe because IdleManagerImpl owns all
  // IdleMonitor instances.
  monitor->SetErrorHandler(
      base::BindOnce(&IdleManagerImpl::RemoveMonitor, base::Unretained(this)));

  monitors_.Append(monitor.release());

  StartPolling();

  std::move(callback).Run(IdleManagerError::kSuccess,
                          std::move(response_state));
}

bool IdleManagerImpl::HasPermission(const url::Origin& origin) {
  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context_);
  DCHECK(permission_controller);
  PermissionStatus status = permission_controller->GetPermissionStatus(
      PermissionType::IDLE_DETECTION, origin.GetURL(), origin.GetURL());
  return status == PermissionStatus::GRANTED;
}

void IdleManagerImpl::RemoveMonitor(IdleMonitor* monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  monitor->RemoveFromList();
  delete monitor;

  if (monitors_.empty()) {
    StopPolling();
  }
}

void IdleManagerImpl::SetIdleTimeProviderForTest(
    std::unique_ptr<IdleTimeProvider> idle_time_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_time_provider_ = std::move(idle_time_provider);
}

bool IdleManagerImpl::IsPollingForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return poll_timer_.IsRunning();
}

void IdleManagerImpl::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE, kPollInterval,
                      base::BindRepeating(&IdleManagerImpl::UpdateIdleState,
                                          base::Unretained(this)));
  }
}

void IdleManagerImpl::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  poll_timer_.Stop();
}

blink::mojom::IdleStatePtr IdleManagerImpl::CheckIdleState(
    base::TimeDelta threshold) {
  if (state_override_) {
    return state_override_->Clone();
  }
  base::TimeDelta idle_time = idle_time_provider_->CalculateIdleTime();
  bool locked = idle_time_provider_->CheckIdleStateIsLocked();

  return IdleTimeToIdleState(locked, idle_time, threshold);
}

void IdleManagerImpl::UpdateIdleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* node = monitors_.head(); node != monitors_.end();
       node = node->next()) {
    IdleMonitor* monitor = node->value();
    monitor->SetLastState(CheckIdleState(monitor->threshold()));
  }
}

}  // namespace content
