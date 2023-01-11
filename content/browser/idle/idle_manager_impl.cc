// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

namespace {

using blink::mojom::IdleManagerError;
using blink::mojom::IdleState;
using blink::mojom::PermissionStatus;

constexpr base::TimeDelta kUserInputThreshold =
    base::Milliseconds(blink::mojom::IdleManager::kUserInputThresholdMs);

}  // namespace

IdleManagerImpl::IdleManagerImpl(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  monitors_.set_disconnect_handler(base::BindRepeating(
      &IdleManagerImpl::OnMonitorDisconnected, base::Unretained(this)));
}

IdleManagerImpl::~IdleManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IdleManagerImpl::CreateService(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void IdleManagerImpl::SetIdleOverride(bool is_user_active,
                                      bool is_screen_unlocked) {
  state_override_ = true;
  observer_.Reset();

  last_state_ = IdleState::New();
  if (!is_user_active)
    last_state_->idle_time = base::Seconds(0);
  last_state_->screen_locked = !is_screen_unlocked;

  for (const auto& monitor : monitors_) {
    monitor->Update(last_state_->Clone(), /*is_overridden_by_devtools=*/true);
  }
}

void IdleManagerImpl::ClearIdleOverride() {
  state_override_ = false;

  if (monitors_.empty()) {
    return;
  }

  observer_.Observe(ui::IdlePollingService::GetInstance());
  last_state_ = CheckIdleState();
  for (const auto& monitor : monitors_) {
    monitor->Update(last_state_->Clone(), /*is_overridden_by_devtools=*/false);
  }
}

void IdleManagerImpl::AddMonitor(
    mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
    AddMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasPermission()) {
    std::move(callback).Run(IdleManagerError::kPermissionDisabled, nullptr);
    return;
  }

  if (monitors_.empty() && !state_override_) {
    observer_.Observe(ui::IdlePollingService::GetInstance());
    last_state_ = CheckIdleState();
  }

  monitors_.Add(std::move(monitor_remote));

  std::move(callback).Run(IdleManagerError::kSuccess, last_state_->Clone());
}

bool IdleManagerImpl::HasPermission() {
  PermissionController* permission_controller =
      render_frame_host_->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);
  PermissionStatus status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::IDLE_DETECTION, render_frame_host_);
  return status == PermissionStatus::GRANTED;
}

void IdleManagerImpl::OnMonitorDisconnected(mojo::RemoteSetElementId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (monitors_.empty()) {
    observer_.Reset();
  }
}

void IdleManagerImpl::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::mojom::IdleStatePtr new_state = CreateIdleState(state);
  if (new_state == last_state_) {
    return;
  }

  last_state_ = std::move(new_state);
  for (const auto& monitor : monitors_) {
    monitor->Update(last_state_->Clone(), /*is_overridden_by_devtools=*/false);
  }
}

blink::mojom::IdleStatePtr IdleManagerImpl::CreateIdleState(
    const ui::IdlePollingService::State& state) {
  auto result = IdleState::New();
  if (state.idle_time >= kUserInputThreshold) {
    result->idle_time = state.idle_time - kUserInputThreshold;
  }
  result->screen_locked = state.locked;
  return result;
}

blink::mojom::IdleStatePtr IdleManagerImpl::CheckIdleState() {
  return CreateIdleState(ui::IdlePollingService::GetInstance()->GetIdleState());
}

}  // namespace content
