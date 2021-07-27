// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/idle/idle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::IdleManagerError;
using blink::mojom::IdleState;
using blink::mojom::PermissionStatus;

constexpr base::TimeDelta kMinimumThreshold = base::TimeDelta::FromSeconds(60);

}  // namespace

IdleManagerImpl::IdleManagerImpl(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

IdleManagerImpl::~IdleManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!monitors_.empty()) {
    IdleMonitor* monitor = monitors_.head()->value();
    monitor->RemoveFromList();
    delete monitor;
  }
}

void IdleManagerImpl::CreateService(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void IdleManagerImpl::SetIdleOverride(
    blink::mojom::UserIdleState user_state,
    blink::mojom::ScreenIdleState screen_state) {
  state_override_ = IdleState::New(user_state, screen_state);
  OnIdleStateChange(IdlePollingService::GetInstance()->GetIdleState());
}

void IdleManagerImpl::ClearIdleOverride() {
  state_override_ = nullptr;
  OnIdleStateChange(IdlePollingService::GetInstance()->GetIdleState());
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

  if (!HasPermission()) {
    std::move(callback).Run(IdleManagerError::kPermissionDisabled, nullptr);
    return;
  }

  if (monitors_.empty()) {
    observer_.Observe(IdlePollingService::GetInstance());
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

  std::move(callback).Run(IdleManagerError::kSuccess,
                          std::move(response_state));
}

bool IdleManagerImpl::HasPermission() {
  PermissionController* permission_controller =
      render_frame_host_->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);
  PermissionStatus status = permission_controller->GetPermissionStatusForFrame(
      PermissionType::IDLE_DETECTION, render_frame_host_,
      render_frame_host_->GetMainFrame()->GetLastCommittedURL().GetOrigin());
  return status == PermissionStatus::GRANTED;
}

void IdleManagerImpl::RemoveMonitor(IdleMonitor* monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  monitor->RemoveFromList();
  delete monitor;

  if (monitors_.empty()) {
    observer_.Reset();
  }
}

blink::mojom::IdleStatePtr IdleManagerImpl::CheckIdleState(
    base::TimeDelta threshold) {
  if (state_override_) {
    return state_override_->Clone();
  }

  const IdlePollingService::State& state =
      IdlePollingService::GetInstance()->GetIdleState();

  blink::mojom::UserIdleState user;
  if (state.idle_time >= threshold) {
    user = blink::mojom::UserIdleState::kIdle;
  } else {
    user = blink::mojom::UserIdleState::kActive;
  }

  blink::mojom::ScreenIdleState screen;
  if (state.locked) {
    screen = blink::mojom::ScreenIdleState::kLocked;
  } else {
    screen = blink::mojom::ScreenIdleState::kUnlocked;
  }

  return IdleState::New(user, screen);
}

void IdleManagerImpl::OnIdleStateChange(
    const IdlePollingService::State& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* node = monitors_.head(); node != monitors_.end();
       node = node->next()) {
    IdleMonitor* monitor = node->value();
    monitor->SetLastState(CheckIdleState(monitor->threshold()));
  }
}

}  // namespace content
